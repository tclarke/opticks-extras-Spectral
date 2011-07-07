/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "DataRequest.h"
#include "DgFormatsRasterPager.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "SpectralVersion.h"
#include "XercesIncludes.h"
#include "xmlreader.h"

XERCES_CPP_NAMESPACE_USE 

REGISTER_PLUGIN_BASIC(SpectralDgFormats, DgFormatsRasterPager);

DgFormatsRasterPager::DgFormatsRasterPager() : mDataType(DgUtilities::DG_RAW_DATA)
{
   setName("DgFormats Raster Pager");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setCreator("Ball Aerospace & Technologies Corp.");
   setDescription("Provides access to on-disk QuickBird-2, WorldView-1 and WorldView-2 data");
   setDescriptorId("{EAE2E253-E067-4B93-B3ED-04AEF4ED7D9F}");
   setShortDescription("DgFormats Raster Pager");
}

DgFormatsRasterPager::~DgFormatsRasterPager()
{
   for (std::vector<std::pair<DgFileTile, RasterPager*> >::iterator iter = mTilePagers.begin();
        iter != mTilePagers.end();
        ++iter)
   {
      PlugIn* pPlugin = dynamic_cast<PlugIn*>(iter->second);
      if (pPlugin != NULL)
      {
         Service<PlugInManagerServices>()->destroyPlugIn(pPlugin);
      }
   }
   mTilePagers.clear();
}

bool DgFormatsRasterPager::openFile(const std::string& filename)
{
   const RasterElement* pRasterElement = getRasterElement();
   VERIFY(pRasterElement != NULL);
   const RasterDataDescriptor* pDescriptor = 
      dynamic_cast<const RasterDataDescriptor*>(pRasterElement->getDataDescriptor());
   VERIFY(pDescriptor != NULL);
   const RasterFileDescriptor* pFileDescriptor = 
      dynamic_cast<const RasterFileDescriptor*>(pDescriptor->getFileDescriptor());
   VERIFY(pFileDescriptor != NULL);

   if (pFileDescriptor->getDatasetLocation() == "radiance")
   {
      mDataType = DgUtilities::DG_RADIANCE_DATA;
   }
   else if (pFileDescriptor->getDatasetLocation() == "reflectance")
   {
      mDataType = DgUtilities::DG_REFLECTANCE_DATA;
   }
   else
   {
      mDataType = DgUtilities::DG_RAW_DATA;
   }

   XmlReader xml(Service<MessageLogMgr>()->getLog(), false);
   if (filename.empty())
   {
      return false;
   }

   XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* pDoc = xml.parse(filename);
   if (pDoc == NULL)
   {
      return false;
   }

   DOMElement* pRoot = pDoc->getDocumentElement();

   FactoryResource<DynamicObject> pMetadata = DgUtilities::parseMetadata(pDoc);
   if (pMetadata.get() == NULL || pMetadata->getNumAttributes() == 0)
   {
      return false;
   }
   unsigned int bandCount = pFileDescriptor->getBandCount();
   DgUtilities::handleSpecialMetadata(pMetadata.get(), bandCount);
   mConversionFactors = determineConversionFactors(pMetadata.get(), mDataType);
   if (mDataType != DgUtilities::DG_RAW_DATA && mConversionFactors.size() != bandCount)
   {
      return false;
   }

   if (pRoot == NULL || !XMLString::equals(pRoot->getNodeName(), X("isd")))
   {
      return false;
   }

   unsigned int height = 0;
   unsigned int width = 0;
   std::vector<DgFileTile> tiles = DgFileTile::getTiles(pDoc, filename, height, width);

   if (tiles.empty())
   {
      return false;
   }

   unsigned int startRowNum = pDescriptor->getRows().front().getOnDiskNumber();
   unsigned int stopRowNum = pDescriptor->getRows().back().getOnDiskNumber();
   unsigned int startColNum = pDescriptor->getColumns().front().getOnDiskNumber();
   unsigned int stopColNum = pDescriptor->getColumns().back().getOnDiskNumber();

   InterleaveFormatType interleave = pFileDescriptor->getInterleaveFormat();
   unsigned int tileBytesPerElement = (pFileDescriptor->getBitsPerElement() / 8);
   for (std::vector<DgFileTile>::iterator iter = tiles.begin();
        iter != tiles.end();
        ++iter)
   {

      DgFileTile theTile = *iter;
      if (startRowNum > theTile.mEndRow || stopRowNum < theTile.mStartRow ||
          startColNum > theTile.mEndCol || stopColNum < theTile.mStartCol)
      {
         continue;
      }
      std::string tiffFile = theTile.mTilFilename;

      FactoryResource<Filename> pFilename;
      pFilename->setFullPathAndName(tiffFile);

      ExecutableResource pagerPlugIn("GeoTiffPager", std::string(), NULL);
      unsigned int rowCount = iter->mEndRow - iter->mStartRow + 1;
      unsigned int colCount = iter->mEndCol - iter->mStartCol + 1;
      pagerPlugIn->getInArgList().setPlugInArgValue<InterleaveFormatType>("interleave", &interleave);
      pagerPlugIn->getInArgList().setPlugInArgValue<unsigned int>("numRows", &rowCount);
      pagerPlugIn->getInArgList().setPlugInArgValue<unsigned int>("numColumns", &colCount);
      pagerPlugIn->getInArgList().setPlugInArgValue<unsigned int>("numBands", &bandCount);
      pagerPlugIn->getInArgList().setPlugInArgValue<unsigned int>("bytesPerElement", &tileBytesPerElement);
      pagerPlugIn->getInArgList().setPlugInArgValue<unsigned int>("cacheBlocks", 0);
      pagerPlugIn->getInArgList().setPlugInArgValue("Filename", pFilename.get());
      bool success = pagerPlugIn->execute();

      RasterPager* pPager = dynamic_cast<RasterPager*>(pagerPlugIn->getPlugIn());
      if ((pPager == NULL) || (success == false))
      {
         return false;
      }
      pagerPlugIn.release();
      mTilePagers.push_back(std::make_pair(theTile, pPager));
   }

   return !mTilePagers.empty();
}

CachedPage::UnitPtr DgFormatsRasterPager::fetchUnit(DataRequest* pOriginalRequest)
{
   // load entire rows for the request
   const RasterDataDescriptor* pDesc = 
      dynamic_cast<const RasterDataDescriptor*>(getRasterElement()->getDataDescriptor());
   const RasterFileDescriptor* pFileDesc = dynamic_cast<const RasterFileDescriptor*>(pDesc->getFileDescriptor());
   unsigned int bytesPerElement = pDesc->getBytesPerElement();
   unsigned int tileBytesPerElement = (pFileDesc->getBitsPerElement() / 8);
   InterleaveFormatType interleave = pOriginalRequest->getInterleaveFormat();

   if (mDataType == DgUtilities::DG_RAW_DATA && bytesPerElement != tileBytesPerElement)
   {
      return CachedPage::UnitPtr();
   }

   // calculate the rows we are loading
   DimensionDescriptor startRow = pOriginalRequest->getStartRow();
   DimensionDescriptor stopRow = pOriginalRequest->getStopRow();
   unsigned int concurrentRows = pOriginalRequest->getConcurrentRows();
   if (startRow.getActiveNumber() + concurrentRows - 1 >= stopRow.getActiveNumber())
   {
      concurrentRows = stopRow.getActiveNumber()-startRow.getActiveNumber()+1;
   }
   unsigned int startRowNum = startRow.getOnDiskNumber(); //we support row subsets
   unsigned int stopRowNum = std::min(startRowNum + concurrentRows, pFileDesc->getRowCount()) - 1;
   unsigned int numRows = stopRowNum - startRowNum + 1;
   if (numRows == 0)
   {
      return CachedPage::UnitPtr();
   }

   // calculate the columns we are loading
   // ignore the request because for cache purposes we always load full rows
   const std::vector<DimensionDescriptor>& cols = pDesc->getColumns();
   unsigned int startColNum = cols.front().getOnDiskNumber();
   unsigned int stopColNum = cols.back().getOnDiskNumber();
   unsigned int numCols = stopColNum - startColNum + 1;

   // calculate the bands we are loading
   const std::vector<DimensionDescriptor>& bands = pDesc->getBands();
   unsigned int startBandNum = bands.front().getOnDiskNumber();
   unsigned int stopBandNum = bands.back().getOnDiskNumber();
   unsigned int numBands = stopBandNum - startBandNum + 1;
   if (interleave == BSQ)
   {
      //cached pager only ever requests 1 band when BSQ is used.
      startBandNum = stopBandNum = pOriginalRequest->getStartBand().getOnDiskNumber();
      numBands = 1;
   }

   uint64_t bufSize = static_cast<uint64_t>(numRows) * numCols * numBands * bytesPerElement;
   ArrayResource<char> pBuffer(bufSize, true);
   if (pBuffer.get() == NULL)
   {
      return CachedPage::UnitPtr();
   }
   if (mDataType == DgUtilities::DG_REFLECTANCE_DATA)
   {
      memset(pBuffer.get(), 0xff, bufSize); //equivalent to numeric_limits<unsigned short>::max()
   }
   else if (mDataType == DgUtilities::DG_RADIANCE_DATA)
   {
      float* pFloatBuffer = reinterpret_cast<float*>(pBuffer.get());
      for (uint64_t pos = 0; pos < bufSize; pos += sizeof(float))
      {
         *pFloatBuffer = -1;
         pFloatBuffer++;
      }
   }
   else
   {
      memset(pBuffer.get(), 0, bufSize);
   }

   DimensionDescriptor fetchStartColumn;
   fetchStartColumn.setActiveNumber(0);
   fetchStartColumn.setOnDiskNumber(0);

   DimensionDescriptor fetchStartBand;
   fetchStartBand.setActiveNumber(startBandNum);
   fetchStartBand.setOnDiskNumber(startBandNum);

   DimensionDescriptor fetchStopBand;
   fetchStopBand.setActiveNumber(stopBandNum);
   fetchStopBand.setOnDiskNumber(stopBandNum);
   for (std::vector<std::pair<DgFileTile, RasterPager*> >::iterator iter = mTilePagers.begin();
        iter != mTilePagers.end();
        ++iter)
   {
      DgFileTile theTile = iter->first;
      bool topTile = (startRowNum >= theTile.mStartRow && startRowNum <= theTile.mEndRow);
      bool bottomTile = (stopRowNum >= theTile.mStartRow && stopRowNum <= theTile.mEndRow);
      bool insideTile = (theTile.mStartRow >= startRowNum && theTile.mEndRow <= stopRowNum);
      if  (!topTile && !bottomTile && !insideTile)
      {
         continue;
      }
      FactoryResource<DataRequest> request;
      DimensionDescriptor fetchStartRow;
      DimensionDescriptor fetchStopRow;
      DimensionDescriptor fetchStopColumn;
      fetchStopColumn.setActiveNumber(theTile.mEndCol - theTile.mStartCol);
      fetchStopColumn.setOnDiskNumber(theTile.mEndCol - theTile.mStartCol);
      if (topTile)
      {
         fetchStartRow.setActiveNumber(startRowNum - theTile.mStartRow);
         fetchStartRow.setOnDiskNumber(startRowNum - theTile.mStartRow);
      }
      else
      {
         fetchStartRow.setActiveNumber(0);
         fetchStartRow.setOnDiskNumber(0);
      }
      if (bottomTile)
      {
         fetchStopRow.setActiveNumber(stopRowNum - theTile.mStartRow);
         fetchStopRow.setOnDiskNumber(stopRowNum - theTile.mStartRow);
      }
      else
      {
         fetchStopRow.setActiveNumber(theTile.mEndRow - theTile.mStartRow);
         fetchStopRow.setOnDiskNumber(theTile.mEndRow - theTile.mStartRow);
      }
      unsigned int fetchNumRows = fetchStopRow.getActiveNumber() - fetchStartRow.getActiveNumber() + 1;
      unsigned int fetchNumCols = fetchStopColumn.getActiveNumber() - fetchStartColumn.getActiveNumber() + 1;
      request->setRows(fetchStartRow, fetchStopRow, fetchNumRows);
      request->setColumns(fetchStartColumn, fetchStopColumn, fetchNumCols);
      request->setBands(fetchStartBand, fetchStopBand, numBands);
      request->setInterleaveFormat(interleave);
      request->setWritable(false);
      RasterPage* pPage = iter->second->getPage(request.get(), fetchStartRow, fetchStartColumn, fetchStartBand);
      if (pPage == NULL || pPage->getRawData() == NULL)
      {
         return CachedPage::UnitPtr();
      }

      //copy from tile into larger buffer
      char* pTileData = reinterpret_cast<char*>(pPage->getRawData());
      char* pData = pBuffer.get();
      uint64_t blockRowSize = static_cast<uint64_t>(numCols) * numBands * bytesPerElement;
      uint64_t tileRowSize = static_cast<uint64_t>(fetchNumCols) * numBands * tileBytesPerElement;
      unsigned int tileColStart = 0;
      if (startColNum > theTile.mStartCol)
      {
         tileColStart = startColNum - theTile.mStartCol;
      }
      unsigned int tileColEnd = fetchNumCols - 1;
      if (stopColNum <= theTile.mEndCol)
      {
         tileColEnd = stopColNum - theTile.mStartCol;
      }
      unsigned int tileColumns = tileColEnd - tileColStart + 1;
      if ((insideTile || bottomTile) && !topTile)
      {
         pData += ((theTile.mStartRow - startRowNum) * blockRowSize);
      }
      if (tileColStart > 0)
      {
         pTileData += (tileColStart * numBands * tileBytesPerElement);
      }
      if (theTile.mStartCol > startColNum)
      {
         pData += (static_cast<uint64_t>(theTile.mStartCol) - startColNum) * numBands * bytesPerElement;
      }
      if (mDataType == DgUtilities::DG_RAW_DATA)
      {
         for (unsigned int curRow = 0; curRow < fetchNumRows; ++curRow)
         {
            memcpy(pData, pTileData, static_cast<uint64_t>(tileColumns) * numBands * bytesPerElement);
            pData += blockRowSize;
            pTileData += tileRowSize;
         }
      }
      else if (mDataType == DgUtilities::DG_RADIANCE_DATA || mDataType == DgUtilities::DG_REFLECTANCE_DATA)
      {
         char* pDataToUpdate = NULL;
         char* pTileDataToUpdate = NULL;
         double conversionFactor = 1.0;
         unsigned short reflectanceBadValue = std::numeric_limits<unsigned short>::max();
         float radianceBadValue = -1.0;
         if (interleave == BSQ)
         {
            conversionFactor = mConversionFactors[startBandNum];
            for (unsigned int curRow = 0; curRow < fetchNumRows; ++curRow)
            {
               pDataToUpdate = pData;
               pTileDataToUpdate = pTileData;
               for (unsigned int curCol = 0; curCol < tileColumns; ++curCol)
               {
                  unsigned short orgValue = 0;
                  if (tileBytesPerElement == 2)
                  {
                     orgValue = *(reinterpret_cast<unsigned short*>(pTileDataToUpdate));
                  }
                  else if (tileBytesPerElement == 1)
                  {
                     orgValue = *(reinterpret_cast<unsigned char*>(pTileDataToUpdate));
                  }
                  double newValue = conversionFactor * orgValue;
                  if (mDataType == DgUtilities::DG_REFLECTANCE_DATA)
                  {
                     unsigned short copyValue = newValue * 10000;
                     if (copyValue == 0)
                     {
                        copyValue = reflectanceBadValue;
                     }
                     memcpy(pDataToUpdate, &copyValue, sizeof(unsigned short));
                  }
                  else
                  {
                     float copyValue = newValue;
                     if (orgValue == 0)
                     {
                        copyValue = radianceBadValue;
                     }                     
                     memcpy(pDataToUpdate, &copyValue, sizeof(float));
                  }
                  pDataToUpdate += bytesPerElement;
                  pTileDataToUpdate += tileBytesPerElement;
               }
               pData += blockRowSize;
               pTileData += tileRowSize;
            }
         }
         else if (interleave == BIP)
         {
            for (unsigned int curRow = 0; curRow < fetchNumRows; ++curRow)
            {
               pDataToUpdate = pData;
               pTileDataToUpdate = pTileData;
               for (unsigned int curCol = 0; curCol < tileColumns; ++curCol)
               {
                  for (unsigned int curBand = 0; curBand < numBands; ++curBand)
                  {
                     conversionFactor = mConversionFactors[curBand];
                     unsigned short orgValue = 0;
                     if (tileBytesPerElement == 2)
                     {
                        orgValue = *(reinterpret_cast<unsigned short*>(pTileDataToUpdate));
                     }
                     else if (tileBytesPerElement == 1)
                     {
                        orgValue = *(reinterpret_cast<unsigned char*>(pTileDataToUpdate));
                     }
                     double newValue = conversionFactor * orgValue;
                     if (mDataType == DgUtilities::DG_REFLECTANCE_DATA)
                     {
                        unsigned short copyValue = newValue * 10000;
                        if (copyValue == 0)
                        {
                           copyValue = reflectanceBadValue;
                        }
                        memcpy(pDataToUpdate, &copyValue, sizeof(unsigned short));
                     }
                     else
                     {
                        float copyValue = newValue;
                        if (orgValue == 0)
                        {
                           copyValue = radianceBadValue;
                        }                     
                        memcpy(pDataToUpdate, &copyValue, sizeof(float));
                     }
                     pDataToUpdate += bytesPerElement;
                     pTileDataToUpdate += tileBytesPerElement;
                  }
               }
               pData += blockRowSize;
               pTileData += tileRowSize;
            }
         }
      }
      iter->second->releasePage(pPage);
   }

   return CachedPage::UnitPtr(new CachedPage::CacheUnit(
      pBuffer.release(), pOriginalRequest->getStartRow(), numRows, bufSize,
      (interleave == BSQ ? pOriginalRequest->getStartBand() : CachedPage::CacheUnit::ALL_BANDS)));
}

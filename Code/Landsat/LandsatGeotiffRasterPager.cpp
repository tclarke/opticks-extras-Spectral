/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "ComplexData.h"
#include "DataRequest.h"
#include "LandsatGeotiffRasterPager.h"
#include "LandsatUtilities.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "RasterPage.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "SpectralVersion.h"
#include "switchOnEncoding.h"
#include "XercesIncludes.h"
#include "xmlreader.h"

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <utility>
#include <vector>

REGISTER_PLUGIN_BASIC(SpectralLandsat, LandsatGeotiffRasterPager);

XERCES_CPP_NAMESPACE_USE 

LandsatGeotiffRasterPager::LandsatGeotiffRasterPager()
   : mDataType(Landsat::LANDSAT_RAW_DATA), mImageType(), mK1(1.0), mK2(1.0)
{
   setName("Landsat GeoTIFF Raster Pager");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setCreator("Ball Aerospace & Technologies Corp.");
   setDescription("Provides access to on-disk Landsat GeoTIFF files.");
   setDescriptorId("{BB39BF0E-6676-48DB-B8A5-81BAB5508907}");
   setShortDescription("Landsat GeoTIFF Raster Pager");
}

LandsatGeotiffRasterPager::~LandsatGeotiffRasterPager()
{
   for (std::vector<RasterPager*>::iterator iter = mBandPagers.begin();
        iter != mBandPagers.end();
        ++iter)
   {
      PlugIn* pPlugin = dynamic_cast<PlugIn*>(*iter);
      if (pPlugin != NULL)
      {
         Service<PlugInManagerServices>()->destroyPlugIn(pPlugin);
      }
   }
   mBandPagers.clear();
}

bool LandsatGeotiffRasterPager::openFile(const std::string& filename)
{
   const RasterElement* pRasterElement = getRasterElement();
   VERIFY(pRasterElement != NULL);
   const RasterDataDescriptor* pDescriptor = dynamic_cast<const RasterDataDescriptor*>(pRasterElement->getDataDescriptor());
   VERIFY(pDescriptor != NULL);
   const RasterFileDescriptor* pFileDescriptor = dynamic_cast<const RasterFileDescriptor*>(pDescriptor->getFileDescriptor());
   VERIFY(pFileDescriptor != NULL);

   QStringList results = QString::fromStdString(pFileDescriptor->getDatasetLocation()).split("-");
   if (results.empty())
   {
      return false;
   }
   std::string dataset = results[0].toStdString();
   if (dataset == "vnir")
   {
      mImageType = Landsat::LANDSAT_VNIR;
   }
   else if (dataset == "pan")
   {
      mImageType = Landsat::LANDSAT_PAN;
   }
   else if (dataset == "tir")
   {
      mImageType = Landsat::LANDSAT_TIR;
   }
   else
   {
      return false;
   }
   mDataType = Landsat::LANDSAT_RAW_DATA;
   if (results.length() > 1)
   {
      std::string typeStr = results[1].toStdString();
      if (typeStr == "radiance")
      {
         mDataType = Landsat::LANDSAT_RADIANCE_DATA;
      }
      else if (typeStr == "reflectance")
      {
         mDataType = Landsat::LANDSAT_REFLECTANCE_DATA;
      }
      else if (typeStr == "temperature")
      {
         mDataType = Landsat::LANDSAT_TEMPERATURE_DATA;
      }
   }
   bool parseSuccess = true;
   FactoryResource<DynamicObject> pMetadata = Landsat::parseMtlFile(filename, parseSuccess);
   if (!parseSuccess)
   {
      return false;
   }
   std::vector<unsigned int> validBands;
   std::vector<std::string> bandFiles = Landsat::getGeotiffBandFilenames(pMetadata.get(),
      filename, mImageType, validBands);
   if (bandFiles.size() != pFileDescriptor->getBandCount())
   {
      return false;
   }
   Landsat::fixMtlMetadata(pMetadata.get(), mImageType, validBands);
   mRadianceFactors = Landsat::determineRadianceConversionFactors(pMetadata.get(), mImageType, validBands);
   if (mDataType != Landsat::LANDSAT_RAW_DATA &&
      mRadianceFactors.size() != pFileDescriptor->getBandCount())
   {
      return false;
   }
   mReflectanceFactors = Landsat::determineReflectanceConversionFactors(pMetadata.get(), mImageType, validBands);
   if (mDataType == Landsat::LANDSAT_REFLECTANCE_DATA &&
      mRadianceFactors.size() != pFileDescriptor->getBandCount())
   {
      return false;
   }
   if (mDataType == Landsat::LANDSAT_TEMPERATURE_DATA &&
      !Landsat::getTemperatureConstants(pMetadata.get(), mImageType, mK1, mK2))
   {
      return false;
   }

   if (pFileDescriptor->getBitsPerElement() != 8)
   {
      return false;
   }

   unsigned int bytesPerElement = RasterUtilities::bytesInEncoding(INT1UBYTE);
   unsigned int numRows = pFileDescriptor->getRowCount();
   unsigned int numColumns = pFileDescriptor->getColumnCount();
   unsigned int numBands = 1;
   InterleaveFormatType interleave = BSQ;
   for (std::vector<std::string>::iterator iter = bandFiles.begin();
        iter != bandFiles.end();
        ++iter)
   {
      std::string tiffFile = *iter;

      FactoryResource<Filename> pFilename;
      pFilename->setFullPathAndName(tiffFile);

      ExecutableResource pagerPlugIn("GeoTiffPager", std::string(), NULL);
      pagerPlugIn->getInArgList().setPlugInArgValue<InterleaveFormatType>("interleave", &interleave);
      pagerPlugIn->getInArgList().setPlugInArgValue<unsigned int>("numRows", &numRows);
      pagerPlugIn->getInArgList().setPlugInArgValue<unsigned int>("numColumns", &numColumns);
      pagerPlugIn->getInArgList().setPlugInArgValue<unsigned int>("numBands", &numBands);
      pagerPlugIn->getInArgList().setPlugInArgValue<unsigned int>("bytesPerElement", &bytesPerElement);
      pagerPlugIn->getInArgList().setPlugInArgValue<unsigned int>("cacheBlocks", 0);
      pagerPlugIn->getInArgList().setPlugInArgValue("Filename", pFilename.get());
      bool success = pagerPlugIn->execute();

      RasterPager* pPager = dynamic_cast<RasterPager*>(pagerPlugIn->getPlugIn());
      if ((pPager == NULL) || (success == false))
      {
         return false;
      }
      pagerPlugIn.release();
      mBandPagers.push_back(pPager);
   }

   return !mBandPagers.empty();
}

CachedPage::UnitPtr LandsatGeotiffRasterPager::fetchUnit(DataRequest* pOriginalRequest)
{
   // load entire rows for the request
   const RasterDataDescriptor* pDesc = dynamic_cast<const RasterDataDescriptor*>(getRasterElement()->getDataDescriptor());
   const RasterFileDescriptor* pFileDesc = dynamic_cast<const RasterFileDescriptor*>(pDesc->getFileDescriptor());
   unsigned int bytesPerElement = pDesc->getBytesPerElement();
   if(pFileDesc->getBitsPerElement() != 8)
   {
      return CachedPage::UnitPtr();
   }
   unsigned int originalBytesPerElement = RasterUtilities::bytesInEncoding(INT1UBYTE);

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

   //cached pager only ever requests 1 band when BSQ is used.
   unsigned int bandNum = pOriginalRequest->getStartBand().getOnDiskNumber();
   
   uint64_t bufSize = static_cast<uint64_t>(numRows) * numCols * bytesPerElement;
   ArrayResource<char> pBuffer(bufSize, true);
   if (pBuffer.get() == NULL)
   {
      return CachedPage::UnitPtr();
   }
   memset(pBuffer.get(), 0, bufSize);

   DimensionDescriptor fetchStartColumn;
   fetchStartColumn.setActiveNumber(0);
   fetchStartColumn.setOnDiskNumber(0);
   DimensionDescriptor fetchStopColumn;
   fetchStopColumn.setActiveNumber(numCols-1);
   fetchStopColumn.setOnDiskNumber(numCols-1);
   DimensionDescriptor fetchBand;
   fetchBand.setActiveNumber(0);
   fetchBand.setOnDiskNumber(0);
   RasterPager* pPager = NULL;
   if (bandNum < mBandPagers.size())
   {
      pPager = mBandPagers[bandNum];
   }
   if (pPager == NULL)
   {
      return CachedPage::UnitPtr();
   }
   FactoryResource<DataRequest> request;
   DimensionDescriptor fetchStartRow;
   fetchStartRow.setActiveNumber(startRowNum);
   fetchStartRow.setOnDiskNumber(startRowNum);
   DimensionDescriptor fetchStopRow;
   fetchStopRow.setActiveNumber(stopRowNum);
   fetchStopRow.setOnDiskNumber(stopRowNum);
   request->setRows(fetchStartRow, fetchStopRow, numRows);
   request->setColumns(fetchStartColumn, fetchStopColumn, numCols);
   request->setBands(fetchBand, fetchBand, 1);
   request->setInterleaveFormat(BSQ);
   request->setWritable(false);
   RasterPage* pPage = pPager->getPage(request.get(), fetchStartRow, fetchStartColumn, fetchBand);
   if (pPage == NULL || pPage->getRawData() == NULL)
   {
      return CachedPage::UnitPtr();
   }

   //copy from pager and convert if necessary
   char* pOriginalData = reinterpret_cast<char*>(pPage->getRawData());
   char* pData = pBuffer.get();
   uint64_t rowSize = static_cast<uint64_t>(numCols) * bytesPerElement;
   uint64_t originalRowSize = static_cast<uint64_t>(numCols) * originalBytesPerElement;
   //RAW_DATA
   if (mDataType == Landsat::LANDSAT_RAW_DATA)
   {
      memcpy(pData, pOriginalData, rowSize * numRows);
   }
   else if (mDataType == Landsat::LANDSAT_RADIANCE_DATA)
   {
      convertToRadiance(bandNum, numRows, numCols, pOriginalData, pData);
   }
   else if (mDataType == Landsat::LANDSAT_REFLECTANCE_DATA)
   {
      convertToReflectance(bandNum, numRows, numCols, pOriginalData, pData);
   }
   else if (mDataType == Landsat::LANDSAT_TEMPERATURE_DATA)
   {
      convertToTemperature(bandNum, numRows, numCols, pOriginalData, pData);
   }
   else
   {
      return CachedPage::UnitPtr();
   }

   pPager->releasePage(pPage);

   return CachedPage::UnitPtr(new CachedPage::CacheUnit(
      pBuffer.release(), pOriginalRequest->getStartRow(), numRows, bufSize,
      pOriginalRequest->getStartBand()));
}

void LandsatGeotiffRasterPager::convertToRadiance(unsigned int band,
   unsigned int numRows, unsigned int numCols, char* pOriginalData, char* pData)
{
   unsigned char* pOrgDataChar = reinterpret_cast<unsigned char*>(pOriginalData);
   float* pDataFloat = reinterpret_cast<float*>(pData);
   std::pair<double, double> gainBiases = mRadianceFactors[band];
   double gain = gainBiases.first;
   double bias = gainBiases.second;
   float badValue = -100.0;
   for (unsigned int curRow = 0; curRow < numRows; ++curRow)
   {
      for (unsigned int curCol = 0; curCol < numCols; ++curCol)
      {
         if (*pOrgDataChar != 0)
         {
            *pDataFloat = *pOrgDataChar * gain + bias;
         }
         else
         {
            *pDataFloat = badValue;
         }
         pDataFloat++;
         pOrgDataChar++;
      }
   }
}

void LandsatGeotiffRasterPager::convertToReflectance(unsigned int band,
   unsigned int numRows, unsigned int numCols, char* pOriginalData, char* pData)
{
   unsigned char* pOrgDataChar = reinterpret_cast<unsigned char*>(pOriginalData);
   short* pDataShort = reinterpret_cast<short*>(pData);
   std::pair<double, double> gainBiases = mRadianceFactors[band];
   double gain = gainBiases.first;
   double bias = gainBiases.second;
   double reflectanceFactor = mReflectanceFactors[band];
   short badValue = std::numeric_limits<short>::max();
   for (unsigned int curRow = 0; curRow < numRows; ++curRow)
   {
      for (unsigned int curCol = 0; curCol < numCols; ++curCol)
      {
         if (*pOrgDataChar != 0)
         {
            *pDataShort = ((*pOrgDataChar * gain + bias) * reflectanceFactor) * 10000;
         }
         else
         {
            *pDataShort = badValue;
         }
         pDataShort++;
         pOrgDataChar++;
      }
   }
}

void LandsatGeotiffRasterPager::convertToTemperature(unsigned int band,
   unsigned int numRows, unsigned int numCols, char* pOriginalData, char* pData)
{
   unsigned char* pOrgDataChar = reinterpret_cast<unsigned char*>(pOriginalData);
   float* pDataFloat = reinterpret_cast<float*>(pData);
   std::pair<double, double> gainBiases = mRadianceFactors[band];
   double gain = gainBiases.first;
   double bias = gainBiases.second;
   float badValue = -1.0;
   for (unsigned int curRow = 0; curRow < numRows; ++curRow)
   {
      for (unsigned int curCol = 0; curCol < numCols; ++curCol)
      {
         if (*pOrgDataChar != 0)
         {
            *pDataFloat = mK2 / log((mK1 / (*pOrgDataChar * gain + bias)) + 1);
         }
         else
         {
            *pDataFloat = badValue;
         }
         pDataFloat++;
         pOrgDataChar++;
      }
   }
}
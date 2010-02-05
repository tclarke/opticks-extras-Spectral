/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppAssert.h"
#include "AppVerify.h"
#include "BandResamplePager.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "Filename.h"
#include "ImportDescriptor.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "SpectralVersion.h"

using namespace std;

REGISTER_PLUGIN_BASIC(SpectralLandsat, BandResamplePager);

BandResampleRasterPage::BandResampleRasterPage(void* pData,
                                               unsigned int rows,
                                               unsigned int columns):
         mpData(pData), mRows(rows), mColumns(columns)
{
}

BandResampleRasterPage::~BandResampleRasterPage()
{
}

void* BandResampleRasterPage::getRawData()
{
   return mpData;
}

unsigned int BandResampleRasterPage::getNumRows()
{
   return mRows;
}

unsigned int BandResampleRasterPage::getNumColumns()
{
   return mColumns;
}

unsigned int BandResampleRasterPage::getNumBands()
{
   return 1;
}

unsigned int BandResampleRasterPage::getInterlineBytes()
{
   return 0;
}

BandResamplePager::BandResamplePager() : mpElement(NULL), mpFileDescriptor(NULL),
                                         mpMemoryMappedPager(NULL), mpRemapData(NULL)
{
   setName("BandResamplePager");
   setCopyright(SPECTRAL_COPYRIGHT);
   setCreator("Ball Aerospace & Technologies Corp.");
   setDescription("Uses Memory Mapped Pager to access on-disk data but resamples a subset of "
                  "bands which have lower spatial resolution than the data set.");
   setDescriptorId("{93F9A2AD-7583-4cca-B747-42ED96F385FA}");
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

BandResamplePager::~BandResamplePager()
{
   delete mpRemapData;
}

bool BandResamplePager::getInputSpecification(PlugInArgList*& pArgList)
{
   VERIFY((pArgList = Service<PlugInManagerServices>()->getPlugInArgList()) != NULL);
   VERIFY(pArgList->addArg<RasterElement>("Raster Element"));
   VERIFY(pArgList->addArg<Filename>("Filename"));
   VERIFY(pArgList->addArg<bool>("isWritable"));
   VERIFY(pArgList->addArg<bool>("Use Data Descriptor"));
   VERIFY(pArgList->addArg<unsigned int>("Band", "Original band number which needs resampling."));
   VERIFY(pArgList->addArg<unsigned int>("Rows", "Number of rows in the band to resample."));
   VERIFY(pArgList->addArg<unsigned int>("Columns", "Number of columns in the band to resample."));
   return true;
}

bool BandResamplePager::execute(PlugInArgList* pInputArgList, PlugInArgList* pOutputArgList)
{
   mpMemoryMappedPager = NULL;
   VERIFY(pInputArgList != NULL);
   mpElement = pInputArgList->getPlugInArgValue<RasterElement>("Raster Element");
   VERIFY(mpElement != NULL);
   DataDescriptor* pDescriptor = mpElement->getDataDescriptor();
   VERIFY(pDescriptor != NULL);
   mpFileDescriptor = static_cast<RasterFileDescriptor*>(pDescriptor->getFileDescriptor());
   VERIFY(mpFileDescriptor != NULL);
   VERIFY(!mpFileDescriptor->getBandFiles().empty());

   Filename* pFilename = pInputArgList->getPlugInArgValue<Filename>("Filename");
   VERIFY(pFilename != NULL);
   bool isWritable;
   VERIFY(pInputArgList->getPlugInArgValue<bool>("isWritable", isWritable));
   bool useDataDescriptor;
   VERIFY(pInputArgList->getPlugInArgValue<bool>("Use Data Descriptor", useDataDescriptor));
   VERIFY(pInputArgList->getPlugInArgValue<unsigned int>("Band", mBand));
   VERIFY(pInputArgList->getPlugInArgValue<unsigned int>("Rows", mRows));
   VERIFY(pInputArgList->getPlugInArgValue<unsigned int>("Columns", mColumns));

   mMemoryMappedPagerPlugin = ExecutableResource("MemoryMappedPager");
   mMemoryMappedPagerPlugin->getInArgList().setPlugInArgValue("Raster Element", mpElement);
   mMemoryMappedPagerPlugin->getInArgList().setPlugInArgValue("Filename", pFilename);
   mMemoryMappedPagerPlugin->getInArgList().setPlugInArgValue("isWritable", &isWritable);
   mMemoryMappedPagerPlugin->getInArgList().setPlugInArgValue("Use Data Descriptor", &useDataDescriptor);
   bool success = mMemoryMappedPagerPlugin->execute();
   if (success)
   {
      mpMemoryMappedPager = dynamic_cast<RasterPager*>(mMemoryMappedPagerPlugin->getPlugIn());
      success = (mpMemoryMappedPager != NULL);
   }
   return success;
}

RasterPage* BandResamplePager::getPage(DataRequest* pOriginalRequest,
                                       DimensionDescriptor startRow,
                                       DimensionDescriptor startColumn,
                                       DimensionDescriptor startBand)
{
   VERIFYRV(pOriginalRequest != NULL && mpMemoryMappedPager != NULL, NULL);
   VERIFYRV(pOriginalRequest->getConcurrentBands() == 1, NULL);
   RasterPage* pPage = NULL;
   if (startBand.getOriginalNumber() == mBand)
   {
      RasterDataDescriptor* pDstDescriptor = static_cast<RasterDataDescriptor*>(mpElement->getDataDescriptor());
      unsigned int elementSize = pDstDescriptor->getBytesPerElement();
      unsigned int dstRowSize = pDstDescriptor->getColumnCount() * elementSize;
      if (mpRemapData == NULL)
      {
         // load the data with generic
         ImporterResource remapImporter("Generic Importer", mpFileDescriptor->getBandFiles()[startBand.getOnDiskNumber()]->getFullPathAndName());
         RasterDataDescriptor* pSrcDescriptor = static_cast<RasterDataDescriptor*>(
            remapImporter->getImportDescriptors().front()->getDataDescriptor());
         pSrcDescriptor->setProcessingLocation(ON_DISK_READ_ONLY);
         RasterFileDescriptor* pSrcFileDescriptor = static_cast<RasterFileDescriptor*>(pSrcDescriptor->getFileDescriptor());
         pSrcFileDescriptor->setEndian(mpFileDescriptor->getEndian());
         pSrcFileDescriptor->setBitsPerElement(mpFileDescriptor->getBitsPerElement());
         pSrcFileDescriptor->setRows(RasterUtilities::generateDimensionVector(mRows, true, true, true));
         pSrcFileDescriptor->setColumns(RasterUtilities::generateDimensionVector(mColumns, true, true, true));
         pSrcFileDescriptor->setBands(RasterUtilities::generateDimensionVector(1, true, true, true));
         pSrcFileDescriptor->setPostlineBytes(mpFileDescriptor->getPostlineBytes());
         pSrcFileDescriptor->setPrelineBytes(mpFileDescriptor->getPrelineBytes());
         pSrcDescriptor->setDataType(pDstDescriptor->getDataType());
         pSrcDescriptor->setRows(pSrcFileDescriptor->getRows());
         pSrcDescriptor->setColumns(pSrcFileDescriptor->getColumns());
         pSrcDescriptor->setBands(pSrcFileDescriptor->getBands());
         if (remapImporter->execute())
         {
            ModelResource<RasterElement> pSrcElement(static_cast<RasterElement*>(remapImporter->getImportedElements().front()));
            FactoryResource<DataRequest> pSrcRequest;
            DataAccessor pSrcAccessor = pSrcElement->getDataAccessor(pSrcRequest.release());
            unsigned int srcRowSize = pSrcDescriptor->getColumnCount() * elementSize;
            unsigned int columnStep = pDstDescriptor->getColumnCount() / mColumns + (pDstDescriptor->getColumnCount() % mColumns == 0 ? 0 : 1);
            unsigned int rowStep = pDstDescriptor->getRowCount() / mRows + (pDstDescriptor->getRowCount() % mRows == 0 ? 0 : 1);
            unsigned int dstRow = 0, dstColumn = 0;
            mpRemapData = new char[pDstDescriptor->getRowCount() * dstRowSize];
            for (; pSrcAccessor.isValid(); pSrcAccessor->nextRow())
            {
               for (unsigned int srcColumn = 0; pSrcAccessor.isValid() && srcColumn < pSrcDescriptor->getColumnCount(); srcColumn++)
               {
                  char* pSrcLocation = reinterpret_cast<char*>(pSrcAccessor->getColumn());
                  for (unsigned int step = 0; step < columnStep && dstColumn < pDstDescriptor->getColumnCount(); step++)
                  {
                     char* pDstLocation = mpRemapData + (dstRow * dstRowSize + dstColumn++ * elementSize);
                     memcpy(pDstLocation, pSrcLocation, elementSize);
                  }
                  pSrcAccessor->nextColumn();
               }
               dstColumn = 0;
               char* pDstSrcLocation = mpRemapData + dstRow * dstRowSize;
               for (unsigned int step = 0; step < rowStep && dstRow < pDstDescriptor->getRowCount()-1; step++)
               {
                  char* pDstLocation = mpRemapData + ++dstRow * dstRowSize;
                  memcpy(pDstLocation, pDstSrcLocation, dstRowSize);
               }
            }
         }
      }
      unsigned int offset = startRow.getOnDiskNumber() * dstRowSize + startColumn.getOnDiskNumber() * elementSize;
      pPage = new BandResampleRasterPage(mpRemapData + offset, pDstDescriptor->getRowCount(), pDstDescriptor->getColumnCount());
   }
   else
   {
      pPage = mpMemoryMappedPager->getPage(pOriginalRequest, startRow, startColumn, startBand);
   }
   return pPage;
}

void BandResamplePager::releasePage(RasterPage* pPage)
{
   VERIFYNRV(mpMemoryMappedPager != NULL);
   BandResampleRasterPage* pTmpPage = dynamic_cast<BandResampleRasterPage*>(pPage);
   if (pTmpPage != NULL)
   {
      delete pTmpPage;
   }
   else
   {
      mpMemoryMappedPager->releasePage(pPage);
   }
}

int BandResamplePager::getSupportedRequestVersion() const
{
   VERIFYRV(mpMemoryMappedPager != NULL, -1);
   return mpMemoryMappedPager->getSupportedRequestVersion();
}

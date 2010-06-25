/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "DataAccessorImpl.h"
#include "DataElement.h"
#include "DataRequest.h"
#include "DesktopServices.h"
#include "DynamicObject.h"
#include "GcpList.h"
#include "Filename.h"
#include "FileResource.h"
#include "MatrixFunctions.h"
#include "MnfInverse.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "SpecialMetadata.h"
#include "SpectralVersion.h"
#include "Undo.h"
#include "Units.h"

#include <QtCore/QString>
#include <QtGui/QFileDialog>

#include <list>
#include <vector>

REGISTER_PLUGIN_BASIC(SpectralMnf, MnfInverse);

MnfInverse::MnfInverse() :
   mpRaster(NULL),
   mbDisplayResults(true),
   mNumColumns(0),
   mNumRows(0),
   mNumBands(0)
{
   setName("Minimum Noise Fraction Inverse Transform");
   setVersion(SPECTRAL_VERSION_NUMBER);
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setShortDescription("Run Inverse MNF");
   setDescription("Apply Minimum Noise Fraction Inverse Transform to data cube.");
   setMenuLocation("[Spectral]\\Transforms\\Minimum Noise Fraction\\Inverse Transform");
   setDescriptorId("{84306449-C853-4254-B4B9-ADBCD5DF4432}");
   setAbortSupported(true);
   allowMultipleInstances(true);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

MnfInverse::~MnfInverse()
{
}

bool MnfInverse::getInputSpecification(PlugInArgList*& pArgList)
{
   // Set up list
   pArgList = Service<PlugInManagerServices>()->getPlugInArgList();
   VERIFY(pArgList != NULL);
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   VERIFY(pArgList->addArg<RasterElement>(Executable::DataElementArg(), NULL));

   if (isBatch()) // need additional info in batch mode
   {
      VERIFY(pArgList->addArg<Filename>("Transform Filename", NULL));
      VERIFY(pArgList->addArg<bool>("Display Results", false));
   }

   return true;
}

bool MnfInverse::getOutputSpecification(PlugInArgList*& pArgList)
{
   // Set up list
   pArgList = Service<PlugInManagerServices>()->getPlugInArgList();
   VERIFY(pArgList != NULL);
   VERIFY(pArgList->addArg<RasterElement>("Inverse MNF Data Cube", NULL));
   return true;
}

bool MnfInverse::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   VERIFY(pInArgList != NULL);
   mProgress.initialize(pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg()),
      "Perform Inverse MNF", "app", "A1E227B6-1480-4596-8FAB-8AD17C4243B5");

   if (!extractInputArgs(pInArgList))
   {
      mProgress.report("Unable to extract arguments.", 0, ERRORS, true);
      return false;
   }

   if (mpRaster == NULL)
   {
      mProgress.report("No raster element available.", 0, ERRORS, true);
      return false;
   }

   const RasterDataDescriptor* pDescriptor =
      dynamic_cast<const RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   VERIFY(pDescriptor != NULL);

   mNumRows = pDescriptor->getRowCount();
   mNumColumns = pDescriptor->getColumnCount();
   mNumBands = pDescriptor->getBandCount();

   if (isBatch() == false)
   {
      QString filename = QString::fromStdString(mpRaster->getFilename());
      if (filename.isEmpty())
      {
         filename = QString::fromStdString(mpRaster->getName());
      }

      FactoryResource<Filename> pFilename;
      pFilename->setFullPathAndName(filename.toStdString());
      filename = QFileDialog::getOpenFileName(Service<DesktopServices>()->getMainWidget(),
         "Select MNF Transform File", QString::fromStdString(pFilename->getPath()), 
         "MNF files (*.mnf);;All Files (*)");
      if (filename.isEmpty())
      {
         mProgress.report("MNF Inverse aborted by user", 0, ABORT, true);
         return false;
      }

      mTransformFilename = filename.toStdString();
   }

   if (mTransformFilename.empty())
   {
      mProgress.report("No MNF transform filename provided.", 0, ERRORS, true);
      return false;
   }
   
   mProgress.getCurrentStep()->addProperty("Transform Filename", mTransformFilename);

   unsigned int bandsInTransform(0);
   unsigned int numComponents(0);
   if (!getInfoFromTransformFile(mTransformFilename, bandsInTransform, numComponents))
   {
      mMessage = "The transform file is invalid.";
      mProgress.report(mMessage, 0, ERRORS, true);
      return false;
   }

   MatrixFunctions::MatrixResource<double> pTransformMatrix(bandsInTransform, numComponents);
   double** pMatrix = pTransformMatrix;

   std::vector<double> wavelengths;
   wavelengths.reserve(bandsInTransform);
   if (!readInMnfTransform(mTransformFilename, pTransformMatrix, wavelengths))
   {
      // error message sent in readInTransform
      return false;
   }

   mProgress.report("Inverting Transform Matrix. This will take some time and "
      "no progress updates will occur....", 0, NORMAL);
   MatrixFunctions::MatrixResource<double> pInverseMatrix(bandsInTransform, numComponents);
   double** pInverse = pInverseMatrix;
   if (!MatrixFunctions::invertSquareMatrix2D(pInverse, const_cast<const double**>(pMatrix), bandsInTransform))
   {
      mMessage = "Error occurred computing inverse of the MNF transform.";
      mProgress.report(mMessage, 0, ERRORS, true);
      return false;
   }
   mProgress.report("Inverting Transform Matrix finished", 100, NORMAL);

   FactoryResource<Filename> pInvRasterName;
   pInvRasterName->setFullPathAndName(mpRaster->getName());
   std::string invRasterName = pInvRasterName->getPath() + "/" + pInvRasterName->getTitle() +
      "_inverse." + pInvRasterName->getExtension();
   ModelResource<RasterElement> pInverseRaster(createInverseRaster(invRasterName,
      mNumRows, mNumColumns, bandsInTransform));

   if (pInverseRaster.get() == NULL)
   {
      if (mMessage.empty())
      {
         mMessage = "Unable to create the inverse raster element";
      }
      mProgress.report(mMessage, 0, ERRORS);
      return false;
   }

   // copy classification from mpRaster
   pInverseRaster->copyClassification(mpRaster);

   // add wavelengths if available to inverse raster
   if (wavelengths.empty() == false)
   {
      std::string pCenterWavelengthPath[] = { SPECIAL_METADATA_NAME, BAND_METADATA_NAME, 
         CENTER_WAVELENGTHS_METADATA_NAME, END_METADATA_NAME };
      pInverseRaster->getMetadata()->setAttributeByPath(pCenterWavelengthPath, wavelengths);
   }

   // compute the inverse data values
   if (!computeInverse(pInverseRaster.get(), pInverse, bandsInTransform, numComponents))
   {
      if (isAborted())
      {
         mProgress.report("Inverse MNF transform canceled", 0, ABORT);
         return false;
      }
      if (mMessage.empty())
      {
         mMessage = "Unable to compute the inverse.";
      }
      mProgress.report(mMessage, 0, ERRORS);
      return false;
   }

   if (mbDisplayResults)
   {
      if (!createInverseView(pInverseRaster.get()))
      {
         if (mMessage.empty())
         {
            mMessage = "Unable to create the Spatial Data View.";
         }
         mProgress.report(mMessage, 0, ERRORS);
         return false;
      }
   }

   pOutArgList->setPlugInArgValue<RasterElement>("Inverse MNF Data Cube", pInverseRaster.release());
   return true;
}

bool MnfInverse::extractInputArgs(const PlugInArgList* pArgList)
{
   VERIFY(pArgList != NULL);

   mpRaster = pArgList->getPlugInArgValue<RasterElement>(Executable::DataElementArg());
   if (mpRaster == NULL)
   {
      mMessage = "The input raster element was null";
      mProgress.report(mMessage, 0, ERRORS, true);
      return false;
   }

   const RasterDataDescriptor* pDescriptor = dynamic_cast<const RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   VERIFY(pDescriptor != NULL);

   EncodingType dataType = pDescriptor->getDataType();
   std::string unitName = pDescriptor->getUnits()->getUnitName();
   if (dataType != FLT8BYTES || unitName != "MNF Value")
   {
      mMessage = "This is not a valid MNF data set!";
      mProgress.report(mMessage, 0, ERRORS, true);
      return false;
   }

   if (isBatch())
   {
      Filename* pFilename = pArgList->getPlugInArgValue<Filename>("Transform Filename");
      if (pFilename == NULL)
      {
         mMessage = "The filename of the MNF transform file to use was not provided.";
         mProgress.report(mMessage, 0, ERRORS, true);
         return false;
      }

      mTransformFilename = pFilename->getFullPathAndName();
      if (mTransformFilename.empty())
      {
         mMessage = "The filename of the MNF transform file to use was blank.";
         mProgress.report(mMessage, 0, ERRORS, true);
         return false;
      }

      pArgList->getPlugInArgValue<bool>("Display Results", mbDisplayResults);
   }

   return true;
}

bool MnfInverse::getInfoFromTransformFile(std::string& filename, unsigned int& numBands,
                                                  unsigned int& numComponents)
{
   if (filename.empty())
   {
      return false;
   }

   FileResource pFile(filename.c_str(), "rt");
   if (pFile.get() == NULL)
   {
      mMessage = "Unable to open transform file: " + filename;
      mProgress.report(mMessage, 0, ERRORS, true);
      return false;
   }

   int numFieldsRead = fscanf(pFile, "%d\n", &numBands);
   if (numFieldsRead != 1)
   {
      mMessage = "Error reading number of bands from MNF transform file:\n" + filename;
      mProgress.report(mMessage, 0, ERRORS, true);
      return false;
   }

   numFieldsRead = fscanf(pFile, "%d\n", &numComponents);
   if (numFieldsRead != 1)
   {
      mMessage = "Error reading number of components from MNF transform file:\n" + filename;
      mProgress.report(mMessage, 0, ERRORS, true);
      return false;
   }

   return true;
}

bool MnfInverse::readInMnfTransform(const std::string& filename, double** pTransform, std::vector<double>& wavelengths)
{
   if (filename.empty())
   {
      return false;
   }

   FileResource pFile(filename.c_str(), "rt");
   if (pFile.get() == NULL)
   {
      mMessage = "Unable to read MNF transform from file " + filename;
      mProgress.report(mMessage, 0, ERRORS, true);
      return false;
   }

   mMessage = "Reading MNF transform from file " + filename;
   mProgress.report(mMessage, 0, NORMAL);

   unsigned int numBands = 0;
   unsigned int numComponents = 0;
   int numFieldsRead = fscanf(pFile, "%d\n", &numBands);
   if (numFieldsRead != 1)
   {
      mMessage = "Error reading number of bands from MNF transform file:\n" + filename;
      mProgress.report(mMessage, 0, ERRORS, true);
      return false;
   }

   numFieldsRead = fscanf(pFile, "%d\n", &numComponents);
   if (numFieldsRead != 1)
   {
      mMessage = "Error reading number of components from MNF transform file:\n" + filename;
      mProgress.report(mMessage, 0, ERRORS, true);
      return false;
   }

   if (numComponents < mNumBands)
   {
      mMessage = "Mismatch between number of bands in cube to invert and number of components in MNF transform file.";
      mProgress.report(mMessage, 0, ERRORS, true);
      return false;
   }

   bool success(true);

   // read in transform
   for (unsigned int row = 0; row < numBands; ++row)
   {
      for (unsigned int col = 0; col < numComponents; ++col)
      {
         numFieldsRead = fscanf(pFile, "%lg ", &(pTransform[row][col]));

         if (numFieldsRead != 1)
         {
            success = false;
            break;
         }
      }
      if (!success)
      {
         break;
      }
      mProgress.report(mMessage, 100 * row / mNumBands, NORMAL);
   }
   if (!success)
   {
      mMessage = "Error reading MNF transform from disk.";
      mProgress.report(mMessage, 0, ERRORS);
      return false;
   }

   // now read in wavelengths if present
   char line[512];
   numFieldsRead = fscanf(pFile, "%s", line);  // "Wavelengths" caption

   if (numFieldsRead == 1)
   {
      double wavelength(0.0);
      for (unsigned int band = 0; band < numBands; ++band)
      {
         numFieldsRead = fscanf(pFile, "%lg", &wavelength);
         if (numFieldsRead < 1)
         {
            wavelengths.clear();
            break;
         }
         wavelengths.push_back(wavelength);
      }
   }

   // make sure the number of wavelengths read in is equal to the number of bands in transform
   if (wavelengths.empty() == false && wavelengths.size() != numBands)
   {
      wavelengths.clear();
   }

   mMessage = "MNF transform successfully read from disk";
   if (wavelengths.empty())
   {
      mMessage += " however no center wavelength information is available";
      mProgress.report(mMessage, 100, WARNING);
   }
   else
   {
      mProgress.report(mMessage, 100, NORMAL);
   }

   return success;
}

RasterElement* MnfInverse::createInverseRaster(std::string name, unsigned int numRows,
                                               unsigned int numColumns, unsigned int numBands)
{
   if (name.empty() || numRows == 0 || numColumns == 0 || numBands == 0)
   {
      mMessage = "Invalid specifications for creating the inverse raster element.";
      return NULL;
   }

   DataElement* pElem = Service<ModelServices>()->getElement(name, TypeConverter::toString<RasterElement>(), NULL);
   if (pElem != NULL)
   {
      Service<ModelServices>()->destroyElement(pElem);
   }
   RasterElement* pInverseRaster = RasterUtilities::createRasterElement(name, numRows, numColumns,
      numBands, FLT8BYTES, BIP, true, NULL);

   // if it wasn't created in memory, try to create on disk
   if (pInverseRaster == NULL)
   {
      pInverseRaster = RasterUtilities::createRasterElement(name, numRows, numColumns,
         numBands, FLT8BYTES, BIP, false, NULL);
   }

   if (pInverseRaster == NULL)
   {
      mMessage = "Unable to create the inverse raster element.";
   }

   return pInverseRaster;
}

bool MnfInverse::computeInverse(RasterElement* pInvRaster, double** pInvTransform,
                    unsigned int numBands, unsigned int numComponents)
{
   if (pInvRaster == NULL || pInvTransform == NULL || numBands == 0 || numComponents == 0)
   {
      mMessage = "Input parameters are invalid.";
      return false;
   }

   RasterDataDescriptor* pInvDesc = dynamic_cast<RasterDataDescriptor*>(pInvRaster->getDataDescriptor());
   VERIFY(pInvDesc != NULL);
   unsigned int numInvRows = pInvDesc->getRowCount();
   unsigned int numInvCols = pInvDesc->getColumnCount();
   unsigned int numInvBands = pInvDesc->getBandCount();

   if (numInvCols != mNumColumns || numInvRows != mNumRows)
   {
      mMessage = "The dimensions of the source raster element and the inverse raster element do not match.";
      return false;
   }

   FactoryResource<DataRequest> pOrigRqt;
   pOrigRqt->setInterleaveFormat(BIP);
   DataAccessor origAcc = mpRaster->getDataAccessor(pOrigRqt.release());
   FactoryResource<DataRequest> pInvRqt;
   pInvRqt->setWritable(true);
   DataAccessor invAcc = pInvRaster->getDataAccessor(pInvRqt.release());

   double* pOrigData(NULL);
   double* pInvData(NULL);
   for (unsigned int row = 0; row < mNumRows; ++row)
   {
      for (unsigned int col = 0; col < mNumColumns; ++col)
      {
         if (isAborted())
         {
            return false;
         }
         VERIFY(origAcc.isValid());
         VERIFY(invAcc.isValid());
         pOrigData = reinterpret_cast<double*>(origAcc->getColumn());
         pInvData = reinterpret_cast<double*>(invAcc->getColumn());
         for (unsigned int comp = 0; comp < numInvBands; ++comp)
         {
            for (unsigned int band = 0; band < mNumBands; ++band)
            {
               pInvData[comp] += pOrigData[band] * pInvTransform[band][comp];
            }
         }
         origAcc->nextColumn();
         invAcc->nextColumn();
      }
      origAcc->nextRow();
      invAcc->nextRow();
      mProgress.report("Computing Inverse data values...", (row + 1) * 100 / mNumRows, NORMAL);
   }

   return true;
}


bool MnfInverse::createInverseView(RasterElement* pInvRaster)
{
   VERIFY(pInvRaster != NULL);
   mProgress.report("Creating view...", 0, NORMAL);

   std::string filename = pInvRaster->getName();

   Service<DesktopServices> pDesktop;
   SpatialDataWindow* pWindow = static_cast<SpatialDataWindow*>(
      pDesktop->createWindow(filename, SPATIAL_DATA_WINDOW));
   if (pWindow == NULL)
   {
      mMessage = "Could not create new window!";
      mProgress.report(mMessage, 0, ERRORS, true);
      return false;
   }

   mProgress.report("Creating view...", 25, NORMAL);
   SpatialDataView* pView(NULL);
   if (pWindow != NULL)
   {
      pView = pWindow->getSpatialDataView();
   }

   if (pView == NULL)
   {
      mMessage = "Could not obtain new view!";
      mProgress.report(mMessage, 25, ERRORS, true);
      return false;
   }

   pView->setPrimaryRasterElement(pInvRaster);

   mProgress.report("Creating view...", 50, NORMAL);

   UndoLock lock(pView);
   if (pView->createLayer(RASTER, pInvRaster) == NULL)
   {
      pDesktop->deleteWindow(pWindow);
      mMessage = "Could not access raster properties for view!";
      mProgress.report(mMessage, 50, ERRORS, true);
      return false;
   }

   mProgress.report("Creating view...", 75, NORMAL);

   // Create a GCP layer if available
   if (mpRaster != NULL)
   {
      const RasterDataDescriptor* pDescriptor =
         dynamic_cast<const RasterDataDescriptor*>(mpRaster->getDataDescriptor());
      if (pDescriptor != NULL)
      {
         const RasterFileDescriptor* pFileDescriptor =
            dynamic_cast<const RasterFileDescriptor*>(pDescriptor->getFileDescriptor());
         if (pFileDescriptor != NULL)
         {
            Service<ModelServices> pModel;
            if (pModel.get() != NULL)
            {
               std::list<GcpPoint> gcps;
               if ((mNumRows == pFileDescriptor->getRowCount()) &&
                  (mNumColumns == pFileDescriptor->getColumnCount()))
               {
                  gcps = pFileDescriptor->getGcps();
               }

               if (gcps.empty() == true)
               {
                  if (mpRaster->isGeoreferenced())
                  {
                     GcpPoint gcp;

                     // Lower left
                     gcp.mPixel.mX = 0.0;
                     gcp.mPixel.mY = 0.0;
                     gcp.mCoordinate = mpRaster->convertPixelToGeocoord(gcp.mPixel);
                     gcps.push_back(gcp);

                     // Lower right
                     gcp.mPixel.mX = mNumColumns - 1;
                     gcp.mPixel.mY = 0.0;
                     gcp.mCoordinate = mpRaster->convertPixelToGeocoord(gcp.mPixel);
                     gcps.push_back(gcp);

                     // Upper left
                     gcp.mPixel.mX = 0.0;
                     gcp.mPixel.mY = mNumRows - 1;
                     gcp.mCoordinate = mpRaster->convertPixelToGeocoord(gcp.mPixel);
                     gcps.push_back(gcp);

                     // Upper right
                     gcp.mPixel.mX = mNumColumns - 1;
                     gcp.mPixel.mY = mNumRows - 1;
                     gcp.mCoordinate = mpRaster->convertPixelToGeocoord(gcp.mPixel);
                     gcps.push_back(gcp);

                     // Center
                     gcp.mPixel.mX = mNumColumns / 2.0;
                     gcp.mPixel.mY = mNumRows / 2.0;
                     gcp.mCoordinate = mpRaster->convertPixelToGeocoord(gcp.mPixel);
                     gcps.push_back(gcp);
                  }
               }

               if (gcps.empty() == false)
               {
                  DataDescriptor* pGcpDescriptor = Service<ModelServices>()->createDataDescriptor("Corner Coordinates",
                     "GcpList", pInvRaster);
                  if (pGcpDescriptor != NULL)
                  {
                     GcpList* pGcpList = static_cast<GcpList*>(pModel->createElement(pGcpDescriptor));
                     if (pGcpList != NULL)
                     {
                        // Add the GCPs to the GCP list
                        pGcpList->addPoints(gcps);

                        // Create the GCP list layer
                        pView->createLayer(GCP_LAYER, pGcpList);
                     }
                  }
               }
               else
               {
                  mMessage = "Geocoordinates are not available and will not be added to the new MNF cube!";
                  mProgress.report(mMessage, 0, WARNING, true);
               }
            }
         }
      }
   }

   if (isAborted() == false)
   {
      mProgress.report("Finished creating view", 100, NORMAL);
   }
   else
   {
      pDesktop->deleteWindow(pWindow);
      mProgress.report("Create view aborted", 100, ABORT, true);
      return false;
   }

   return true;
}

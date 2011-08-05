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
#include "MessageLogResource.h"
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
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL, Executable::ProgressArgDescription()));
   VERIFY(pArgList->addArg<RasterElement>(Executable::DataElementArg(), NULL, "Raster element over which the MNF "
      "inverse transform will be performed."));

   if (isBatch()) // need additional info in batch mode
   {
      VERIFY(pArgList->addArg<Filename>("Transform Filename", NULL, "Location of the results from a previously "
         "performed MNF transform."));
      VERIFY(pArgList->addArg<bool>("Display Results", false, "Flag for whether the results of the MNF inverse "
         "transform should be displayed."));
   }

   return true;
}

bool MnfInverse::getOutputSpecification(PlugInArgList*& pArgList)
{
   // Set up list
   pArgList = Service<PlugInManagerServices>()->getPlugInArgList();
   VERIFY(pArgList != NULL);
   VERIFY(pArgList->addArg<RasterElement>("Inverse MNF Data Cube", NULL, "Raster element resulting from the MNF "
      "inverse transform operation."));
   return true;
}

bool MnfInverse::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   StepResource pStep("Perform Inverse MNF", "spectral", "A1E227B6-1480-4596-8FAB-8AD17C4243B5");
   mpStep = pStep.get();

   VERIFY(pInArgList != NULL);
   mpProgress = pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg());

   if (!extractInputArgs(pInArgList))
   {
      if (mMessage.empty())
      {
         mMessage = "Unable to extract arguments.";
      }
      updateProgress(mMessage, 0, ERRORS);
      pStep->finalize(Message::Failure, mMessage);
      return false;
   }

   if (mpRaster == NULL)
   {
      mMessage = "No raster element available.";
      updateProgress(mMessage, 0, ERRORS);
      pStep->finalize(Message::Failure, mMessage);
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
         mMessage = "MNF Inverse aborted by user";
         pStep->finalize(Message::Abort, mMessage);
         return false;
      }

      mTransformFilename = filename.toStdString();
   }

   if (mTransformFilename.empty())
   {
      mMessage = "No MNF transform filename provided.";
      updateProgress(mMessage, 0, ERRORS);
      pStep->finalize(Message::Failure, mMessage);
      return false;
   }

   pStep->addProperty("Transform Filename", mTransformFilename);

   unsigned int bandsInTransform(0);
   unsigned int numComponents(0);
   if (!getInfoFromTransformFile(mTransformFilename, bandsInTransform, numComponents))
   {
      if (mMessage.empty())
      {
         mMessage = "The transform file is invalid.";
      }
      updateProgress(mMessage, 0, ERRORS);
      pStep->finalize(Message::Failure, mMessage);
      return false;
   }

   MatrixFunctions::MatrixResource<double> pTransformMatrix(bandsInTransform, numComponents);
   double** pMatrix = pTransformMatrix;

   std::vector<double> wavelengths;
   wavelengths.reserve(bandsInTransform);
   if (!readInMnfTransform(mTransformFilename, pTransformMatrix, wavelengths))
   {
      updateProgress(mMessage, 0, ERRORS);
      pStep->finalize(Message::Failure, mMessage);
      return false;
   }

   std::string localMsg = "Inverting Transform Matrix. This will take some time and no progress updates will occur...";
   updateProgress(localMsg, 0, NORMAL);
   MatrixFunctions::MatrixResource<double> pInverseMatrix(bandsInTransform, numComponents);
   double** pInverse = pInverseMatrix;
   if (!MatrixFunctions::invertSquareMatrix2D(pInverse, const_cast<const double**>(pMatrix), bandsInTransform))
   {
      mMessage = "Error occurred computing inverse of the MNF transform.";
      updateProgress(mMessage, 0, ERRORS);
      pStep->finalize(Message::Failure, mMessage);
      return false;
   }
   localMsg = "Inverting Transform Matrix finished.";
   updateProgress(localMsg, 100, NORMAL);

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
      updateProgress(mMessage, 0, ERRORS);
      pStep->finalize(Message::Failure, mMessage);
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
         localMsg = "Inverse MNF transform canceled";
         updateProgress(localMsg, 0, ABORT);
         pStep->finalize(Message::Abort, localMsg);
         return false;
      }
      if (mMessage.empty())
      {
         mMessage = "Unable to compute the inverse.";
      }
      updateProgress(mMessage, 0, ERRORS);
      pStep->finalize(Message::Failure, mMessage);
      return false;
   }

   if (mbDisplayResults)
   {
      if (!createInverseView(pInverseRaster.get()))
      {
         if (isAborted())
         {
            localMsg = "Inverse MNF transform canceled";
            updateProgress(localMsg, 0, ABORT);
            pStep->finalize(Message::Abort, localMsg);
            return false;
         }
         if (mMessage.empty())
         {
            mMessage = "Unable to create the Spatial Data View.";
         }
         updateProgress(mMessage, 0, ERRORS);
         pStep->finalize(Message::Failure, mMessage);
         return false;
      }
   }

   pOutArgList->setPlugInArgValue<RasterElement>("Inverse MNF Data Cube", pInverseRaster.release());
   localMsg = "MNF Inverse transform finished";
   updateProgress(localMsg, 100, NORMAL);
   pStep->finalize(Message::Success);
   return true;
}

bool MnfInverse::extractInputArgs(const PlugInArgList* pArgList)
{
   VERIFY(pArgList != NULL);

   mpRaster = pArgList->getPlugInArgValue<RasterElement>(Executable::DataElementArg());
   if (mpRaster == NULL)
   {
      mMessage = "The input raster element was invalid.";
      return false;
   }

   const RasterDataDescriptor* pDescriptor = dynamic_cast<const RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   VERIFY(pDescriptor != NULL);

   EncodingType dataType = pDescriptor->getDataType();
   std::string unitName = pDescriptor->getUnits()->getUnitName();
   if (dataType != FLT8BYTES || unitName != "MNF Value")
   {
      mMessage = "This is not a valid MNF data set!";
      return false;
   }

   if (isBatch())
   {
      Filename* pFilename = pArgList->getPlugInArgValue<Filename>("Transform Filename");
      if (pFilename == NULL)
      {
         mMessage = "The filename of the MNF transform file to use was not provided.";
         return false;
      }

      mTransformFilename = pFilename->getFullPathAndName();
      if (mTransformFilename.empty())
      {
         mMessage = "The filename of the MNF transform file to use was blank.";
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
      mMessage = "The transform filename was invalid.";
      return false;
   }

   FileResource pFile(filename.c_str(), "rt");
   if (pFile.get() == NULL)
   {
      mMessage = "Unable to open transform file: " + filename;
      return false;
   }

   int numFieldsRead = fscanf(pFile, "%d\n", &numBands);
   if (numFieldsRead != 1)
   {
      mMessage = "Error reading number of bands from MNF transform file:\n" + filename;
      return false;
   }

   numFieldsRead = fscanf(pFile, "%d\n", &numComponents);
   if (numFieldsRead != 1)
   {
      mMessage = "Error reading number of components from MNF transform file:\n" + filename;
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
      return false;
   }

   std::string msg = "Reading MNF transform from file " + filename;
   updateProgress(msg, 0, NORMAL);

   unsigned int numBands = 0;
   unsigned int numComponents = 0;
   int numFieldsRead = fscanf(pFile, "%d\n", &numBands);
   if (numFieldsRead != 1)
   {
      mMessage = "Error reading number of bands from MNF transform file:\n" + filename;
      return false;
   }

   numFieldsRead = fscanf(pFile, "%d\n", &numComponents);
   if (numFieldsRead != 1)
   {
      mMessage = "Error reading number of components from MNF transform file:\n" + filename;
      return false;
   }

   if (numComponents < mNumBands)
   {
      mMessage = "Mismatch between number of bands in cube to invert and number of components in MNF transform file.";
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
      updateProgress(msg, 100 * row / mNumBands, NORMAL);
   }
   if (!success)
   {
      mMessage = "Error reading MNF transform from disk.";
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

   msg = "MNF transform successfully read from disk";
   if (wavelengths.empty())
   {
      msg += " however no center wavelength information is available";
      updateProgress(mMessage, 100, WARNING);
   }
   else
   {
      updateProgress(msg, 100, NORMAL);
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
      updateProgress("Computing Inverse data values...", (row + 1) * 100 / mNumRows, NORMAL);
   }

   return true;
}


bool MnfInverse::createInverseView(RasterElement* pInvRaster)
{
   VERIFY(pInvRaster != NULL);
   std::string msg = "Creating view...";
   updateProgress(msg, 0, NORMAL);

   std::string filename = pInvRaster->getName();

   Service<DesktopServices> pDesktop;
   SpatialDataWindow* pWindow = static_cast<SpatialDataWindow*>(
      pDesktop->createWindow(filename, SPATIAL_DATA_WINDOW));
   if (pWindow == NULL)
   {
      mMessage = "Could not create new window!";
      return false;
   }

   updateProgress(msg, 25, NORMAL);
   SpatialDataView* pView = pWindow->getSpatialDataView();

   if (pView == NULL)
   {
      mMessage = "Could not obtain new view!";
      pDesktop->deleteWindow(pWindow);
      return false;
   }

   if (isAborted())
   {
      pDesktop->deleteWindow(pWindow);
      return false;
   }

   pView->setPrimaryRasterElement(pInvRaster);

   updateProgress(msg, 50, NORMAL);

   if (!createLayers(pView, pInvRaster))
   {
      pDesktop->deleteWindow(pWindow);
      return false;
   }

   if (isAborted())
   {
      pDesktop->deleteWindow(pWindow);
      return false;
   }

   msg = "Finished creating view";
   updateProgress(msg, 100, NORMAL);

   return true;
}

bool MnfInverse::createLayers(SpatialDataView* pView, RasterElement* pInvRaster)
{
   UndoLock lock(pView);
   if (pView->createLayer(RASTER, pInvRaster) == NULL)
   {
      mMessage = "Could not access raster properties for view!";
      return false;
   }

   if (isAborted())
   {
      return false;
   }

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

               if (isAborted())
               {
                  return false;
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
                  updateProgress(mMessage, 0, WARNING);
               }
            }
         }
      }
   }
   return true;
}

void MnfInverse::updateProgress(const std::string& msg, int percent, ReportingLevel level)
{
   if (mpProgress != NULL)
   {
      mpProgress->updateProgress(msg, percent, level);
   }
}
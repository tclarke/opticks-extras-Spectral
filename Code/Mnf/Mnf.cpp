/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include "AoiElement.h"
#include "AppAssert.h"
#include "AppConfig.h"
#include "ApplicationServices.h"
#include "AppVerify.h"
#include "BitMaskIterator.h"
#include "ConfigurationSettings.h"
#include "DataAccessorImpl.h"
#include "DataElement.h"
#include "DataRequest.h"
#include "DifferenceImageDlg.h"
#include "DimensionDescriptor.h"
#include "DynamicObject.h"
#include "EigenPlotDlg.h"
#include "Filename.h"
#include "FileResource.h"
#include "MatrixFunctions.h"
#include "MessageLogResource.h"
#include "Mnf.h"
#include "MnfDlg.h"
#include "ModelServices.h"
#include "ObjectResource.h"
#include "PlugInArg.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterLayer.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "SpectralVersion.h"
#include "Statistics.h"
#include "StatisticsDlg.h"
#include "switchOnEncoding.h"
#include "TypeConverter.h"
#include "Undo.h"
#include "Units.h"
#include "Wavelengths.h"

#include <fstream>
#include <limits>
#include <math.h>
#include <sstream>
#include <typeinfo>

using namespace std;

namespace
{
   template<class T>
   void sumBandValues(T* pDummy, void* pData, double* pSums, unsigned int numBands)
   {
      for (unsigned int band = 0; band < numBands; ++band)
      {
         pSums[band] += static_cast<double>(reinterpret_cast<T*>(pData)[band]);
      }
   }

   template<class T>
   void computeDifferencePixel(T* pDummy, void* pData1, void* pData2, double* pResults,
                               unsigned int numBands)
   {
      T* pTmp1 = reinterpret_cast<T*>(pData1);
      T* pTmp2 = reinterpret_cast<T*>(pData2);
      for (unsigned int band = 0; band < numBands; ++band)
      {
         pResults[band] = static_cast<double>(pTmp1[band]) - static_cast<double>(pTmp2[band]);
      }
   }

   template<class T>
   void computeCovarValue(T* pData, double* pMeans, double** pValues, unsigned int numBands)
   {
      if (pData != NULL && pMeans != NULL && pValues != NULL)
      {
         for (unsigned int band1 = 0; band1 < numBands; ++band1)
         {
            for (unsigned int band2 = 0; band2 < numBands; ++ band2)
            {
               pValues[band1][band2] += (static_cast<double>(pData[band1]) - pMeans[band1]) *
                        (static_cast<double>(pData[band2]) - pMeans[band2]);
            }
         }
      }
   }

   template<class T>
   void computeMnfColumn(T *pData, double* pMnfData, double** pCoefficients, unsigned int numBands,
      unsigned int numComponents)
   {
      for (unsigned int comp = 0; comp < numComponents; ++comp)
      {
         pMnfData[comp] = 0.0;
         for (unsigned int band = 0; band < numBands; ++band)
         {
            pMnfData[comp] += pCoefficients[band][comp] * static_cast<double>(pData[band]);
         }
      }
   }

   template <class T>
   void computeMnfRow(T* pData, double* pMnfData,  double** pCoefficients, unsigned int numCols,
      unsigned int numBands, unsigned int numComponents)
   {
      T* pColumn = pData;
      double* pValue = pMnfData;

      for (unsigned int col = 0; col < numCols; ++col)
      {
         for (unsigned int comp = 0; comp < numComponents; ++comp)
         {
            pValue[comp] = 0.0;
            for (unsigned int band = 0; band < numBands; ++band)
            {
               pValue[comp] += (pCoefficients[band][comp] * static_cast<double>(pColumn[band]));
            }
         }
         pValue += numComponents;
         pColumn += numBands;
      }
   }
}

REGISTER_PLUGIN_BASIC(SpectralMnf, Mnf);

Mnf::Mnf() :
   mNumRows(0),
   mNumColumns(0),
   mNumBands(0),
   mpProgress(NULL),
   mpView(NULL),
   mpRaster(NULL),
   mpMnfRaster(static_cast<RasterElement*>(NULL)),
   mpNoiseRaster(static_cast<RasterElement*>(NULL)),
   mpStep(NULL),
   mpMnfTransformMatrix(NULL),
   mpNoiseCovarMatrix(NULL),
   mbUseTransformFile(false),
   mbUseAoi(false),
   mpProcessingAoi(NULL),
   mpNoiseAoi(NULL),
   mNumComponentsToUse(0),
   mbUseSnrValPlot(false),
   mbDisplayResults(true),
   mNoiseStatisticsMethod(DIFFDATA)
{
   setName("Minimum Noise Fraction Transform");
   setVersion(SPECTRAL_VERSION_NUMBER);
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setShortDescription("Run MNF");
   setDescription("Apply Minimum Noise Fraction Transform to data cube.");
   setMenuLocation("[Spectral]\\Transforms\\Minimum Noise Fraction\\Forward Transform");
   setDescriptorId("{D84BCC57-8450-4ba0-B1CC-2F59EE25C0BE}");
   setAbortSupported(true);
   allowMultipleInstances(true);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);

   initializeNoiseMethods();
}

Mnf::~Mnf()
{}

bool Mnf::getInputSpecification(PlugInArgList*& pArgList)
{
   // Set up list
   pArgList = mpPlugInMgr->getPlugInArgList();
   VERIFY(pArgList != NULL);
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   VERIFY(pArgList->addArg<RasterElement>(Executable::DataElementArg(), NULL));

   if (isBatch()) // need additional info in batch mode
   {
      VERIFY(pArgList->addArg<bool>("Use AOI", false));
      VERIFY(pArgList->addArg<AoiElement>("AOI Element", NULL));
      VERIFY(pArgList->addArg<bool>("Use Transform File", false));
      VERIFY(pArgList->addArg<Filename>("Transform Filename", NULL));
      string methodDesc = "Valid methods are:\n";
      methodDesc += mNoiseEstimationMethods.join(",\n").toStdString();
      VERIFY(pArgList->addArg<string>("Noise Estimation Method", "", methodDesc));
      VERIFY(pArgList->addArg<RasterElement>("Dark Current Element", NULL));
      VERIFY(pArgList->addArg<Filename>("Noise Statistics Filename", NULL));
      VERIFY(pArgList->addArg<AoiElement>("NoiseStatistics AOI", NULL));
      VERIFY(pArgList->addArg<unsigned int>("Number of Components", 0));
      VERIFY(pArgList->addArg<bool>("Display Results", false));
   }

   return true;
}

bool Mnf::getOutputSpecification(PlugInArgList*& pArgList)
{
   // Set up list
   pArgList = mpPlugInMgr->getPlugInArgList();
   VERIFY(pArgList != NULL);
   VERIFY(pArgList->addArg<RasterElement>("MNF Data Cube", NULL));
   return true;
}

bool Mnf::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   StepResource pStep("Perform MNF", "spectral", "FF6FBA92-88F5-4856-83D5-6FB69F262A54");
   mpStep = pStep.get();

   try
   {
      QString message;
      if (!extractInputArgs(pInArgList))
      {
         pStep->finalize(Message::Failure, "Unable to extract arguments.");
         return false;
      }

      const RasterDataDescriptor* pDescriptor =
         dynamic_cast<const RasterDataDescriptor*>(mpRaster->getDataDescriptor());

      mNumRows = pDescriptor->getRowCount();
      mNumColumns = pDescriptor->getColumnCount();
      mNumBands = pDescriptor->getBandCount();

      int iResult = 0;

      if (isBatch() == false)
      {
         vector<string> aoiNames = mpModel->getElementNames(mpRaster, TypeConverter::toString<AoiElement>());
         MnfDlg dlg(aoiNames, mNumBands, mpDesktop->getMainWidget());
         dlg.setNoiseStatisticsMethods(mNoiseEstimationMethods);

         bool inputValid = false;
         while (!inputValid)
         {
            if (dlg.exec() == QDialog::Rejected)
            {
               pStep->finalize(Message::Abort);
               return false;
            }

            mTransformFilename = dlg.getTransformFilename();
            if (!mTransformFilename.empty())
            {
               mbUseTransformFile = true;
            }
            else
            {
               mNoiseStatisticsMethod = getNoiseEstimationMethodType(dlg.getNoiseStatisticsMethod());
            }

            mbUseSnrValPlot = dlg.selectNumComponentsFromPlot();
            if (!mbUseSnrValPlot)
            {
               mNumComponentsToUse = dlg.getNumComponents();
            }

            inputValid = true;
            string aoiName = dlg.getRoiName();
            if (aoiName.empty())  // use whole image
            {
               mbUseAoi = false;
            }
            else
            {
               mbUseAoi = true;

               // check if any pixels are selected in the AOI
               mpProcessingAoi = getAoiElement(aoiName, mpRaster);
               VERIFY(mpProcessingAoi != NULL);
               const BitMask* pMask = mpProcessingAoi->getSelectedPoints();
               VERIFY(pMask != NULL);
               BitMaskIterator it(pMask, mpRaster);
               int numPoints = it.getCount();
               if (numPoints < static_cast<int>(mNumBands))
               {
                  message = "There are fewer pixels selected in the AOI than the number of bands in the dataset. "
                     "The MNF algorithm requires at least as many pixels to analyze as there are bands.";
                  QMessageBox::critical( NULL, "MNF", message);
                  inputValid = false;
               }
            }
         }
      }

      // log MNF options
      if (mbUseAoi)
      {
         pStep->addProperty("AOI", mpProcessingAoi->getName());
      }
      if (mbUseTransformFile)
      {
         pStep->addProperty("MNF Transform File", mTransformFilename);
      }
      else
      {
         pStep->addProperty("Noise Estimation Method", getNoiseEstimationMethodString(mNoiseStatisticsMethod));
      }

      // create matrices for noise covariance and component coefficients
      MatrixFunctions::MatrixResource<double> pNoiseCovarMatrix(mNumBands, mNumBands);
      mpNoiseCovarMatrix = pNoiseCovarMatrix;
      if (mpNoiseCovarMatrix == NULL)
      {
         pStep->finalize(Message::Failure, "Unable to obtain memory needed to calculate noise covariance matrix");
         return false;
      }
      MatrixFunctions::MatrixResource<double> pTransform(mNumBands, mNumBands);
      mpMnfTransformMatrix = pTransform;
      if (mpMnfTransformMatrix == NULL)
      {
         pStep->finalize(Message::Failure, "Unable to obtain memory needed to calculate MNF coefficients");
         return false;
      }
      if (mbUseTransformFile)
      {
         if (!readInMnfTransform(mTransformFilename))
         {
            if (isAborted())
            {
               pStep->finalize(Message::Abort);
            }
            else
            {
               pStep->finalize(Message::Failure, "Error loading transform file");
            }

            return false;
         }
      }
      else
      {
         // generate statistics to use for MNF
         if (!generateNoiseStatistics())
         {
            if (isAborted())
            {
               pStep->finalize(Message::Abort);
            }
            else
            {
               if (mpProgress != NULL)
               {
                  mpProgress->updateProgress(mMessage, 0, ERRORS);
               }
               pStep->finalize(Message::Failure, mMessage);
            }
            return false;
         }

         // Calculate MNF coefficients
         if (!calculateEigenValues())
         {
            if (isAborted())
            {
               mMessage = "MNF Aborted";
               pStep->finalize(Message::Abort);
               if (mpProgress != NULL)
               {
                  mpProgress->updateProgress(mMessage, 0, ABORT);
               }
            }
            else
            {
               // mMessage set in called methods
               pStep->finalize(Message::Failure, mMessage);
               if (mpProgress != NULL)
               {
                  mpProgress->updateProgress(mMessage, 0, ERRORS);
               }
            }
            return false;
         }

         // Save MNF transform
         QString filename = QString::fromStdString(mpRaster->getFilename());
         filename += ".mnf";
         if (isBatch() == false)
         {
            filename = QFileDialog::getSaveFileName(NULL, "Choose filename to save MNF Transform",
               filename, "MNF files (*.mnf);;All Files (*)");
         }

         if (filename.isEmpty() == false)
         {
            writeOutMnfTransform(filename.toStdString());
         }
      }

      // Create the MNF sensor data
      if (!createMnfCube())
      {
         if (isAborted())
         {
            pStep->finalize(Message::Abort);
         }
         else
         {
            pStep->finalize(Message::Failure, "Error allocating result cube");
         }

         return false;
      }

      // compute MNF components
      bool bSuccess;
      bSuccess = computeMnfValues();

      if (!bSuccess)
      {
         if (isAborted())
         {
            pStep->finalize(Message::Abort);
         }
         else
         {
            pStep->finalize(Message::Failure, "Error computing MNF components");
         }

         return false;
      }

      // Create the spectral data window
      if (mbDisplayResults)
      {
         if (!createMnfView())
         {
            if (mpMnfRaster.get() != NULL)
            {
               if (isAborted())
               {
                  pStep->finalize(Message::Abort);
               }
               else
               {
                  pStep->finalize(Message::Failure, "Error creating view");
               }

               return false;
            }
         }
      }

      // Set the values in the output arg list
      if (pOutArgList != NULL)
      {
         VERIFY(pOutArgList->setPlugInArgValue("MNF Data Cube", mpMnfRaster.get()));
      }

      if (mpProgress != NULL)
      {
         mpProgress->updateProgress("Minimum Noise Fraction Transform completed", 100, NORMAL);
      }
   }
   catch (const bad_alloc&)
   {
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress("Out of memory", 0, ERRORS);
      }
      pStep->finalize(Message::Failure, "Out of memory");

      return false;
   }

   pStep->finalize(Message::Success);
   mpMnfRaster.release();
   return true;
}

bool Mnf::extractInputArgs(const PlugInArgList* pArgList)
{
   if (pArgList == NULL)
   {
      mpStep->finalize(Message::Failure, "MNF received a null input argument list");
      return false;
   }

   mpProgress = pArgList->getPlugInArgValue<Progress>(Executable::ProgressArg());
   mpRaster = pArgList->getPlugInArgValue<RasterElement>(Executable::DataElementArg());
   if (mpRaster == NULL)
   {
      mMessage = "The input raster element was null";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   const RasterDataDescriptor* pDescriptor = dynamic_cast<const RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   if (pDescriptor == NULL)
   {
      mMessage = "Unable to access data descriptor for original data set!";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   EncodingType dataType = pDescriptor->getDataType();
   if ((dataType == INT4SCOMPLEX) || (dataType == FLT8COMPLEX))
   {
      mMessage = "Complex data is not supported!";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   unsigned int numBands = pDescriptor->getBandCount();
   if (numBands == 1)
   {
      mMessage = "Cannot perform MNF on 1 band data!";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }
      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   if (isBatch())
   {
      VERIFY(pArgList->getPlugInArgValue<bool>("Use AOI", mbUseAoi));
      if (mbUseAoi)
      {
         mpProcessingAoi = pArgList->getPlugInArgValue<AoiElement>("AOI Element");
         if (mpProcessingAoi == NULL)
         {
            mMessage = "The AOI of the data set to process was not provided.";
            mpStep->finalize(Message::Failure, mMessage);
            return false;
         }
      }

      VERIFY(pArgList->getPlugInArgValue<bool>("Use Transform File", mbUseTransformFile));
      if (mbUseTransformFile)
      {
         Filename* pFilename = pArgList->getPlugInArgValue<Filename>("Transform Filename");
         if (pFilename == NULL)
         {
            mMessage = "The filename of the MNF transform file to use was not provided.";
            mpStep->finalize(Message::Failure, mMessage);
            return false;
         }

         mTransformFilename = pFilename->getFullPathAndName();
         if (mTransformFilename.empty())
         {
            mMessage = "The filename of the MNF transform file to use was blank.";
            mpStep->finalize(Message::Failure, mMessage);
            return false;
         }
      }

      string noiseStr;
      VERIFY(pArgList->getPlugInArgValue<string>("Noise Estimation Method", noiseStr));
      mNoiseStatisticsMethod = getNoiseEstimationMethodType(noiseStr);
      if (mNoiseStatisticsMethod.isValid() == false)
      {
         mMessage = "The Noise Estimation Method is invalid.";
         mpStep->finalize(Message::Failure, mMessage);
         return false;
      }

      Filename* pName(NULL);
      switch (mNoiseStatisticsMethod)
      {
      case DARKCURRENT:
         {
            RasterElement* pRaster = pArgList->getPlugInArgValue<RasterElement>("Dark Current Element");
            if (pRaster == NULL)
            {
               mMessage = "The Dark Current data element was NULL.";
               mpStep->finalize(Message::Failure, mMessage);
               return false;
            }
            mpNoiseRaster = ModelResource<RasterElement>(pRaster);
         }
         break;

      case PREVIOUS:
         pName = pArgList->getPlugInArgValue<Filename>("Noise Statistics Filename");
         if (pName == NULL)
         {
            mMessage = "The filename of the previously computed noise statistics was invalid.";
            mpStep->finalize(Message::Failure, mMessage);
            return false;
         }
         mPreviousNoiseFilename = pName->getFullPathAndName();
         if (mPreviousNoiseFilename.empty())
         {
            mMessage = "The filename for the previously computed noise statistics was blank.";
            mpStep->finalize(Message::Failure, mMessage);
            return false;
         }
         break;

      case DIFFDATA:  // fall through
      default:
         break;
      }

      mpNoiseAoi = pArgList->getPlugInArgValue<AoiElement>("NoiseStatistics AOI");

      VERIFY(pArgList->getPlugInArgValue<unsigned int>("Number of Components", mNumComponentsToUse));
      if (mNumComponentsToUse > numBands || mNumComponentsToUse < 1)
      {
         mMessage = "The number of components to use is invalid.";
         mpStep->finalize(Message::Failure, mMessage);
         return false;
      }

      VERIFY(pArgList->getPlugInArgValue<bool>("Display Results", mbDisplayResults));
   }

   return true;
}

bool Mnf::createMnfCube()
{
   string outputName = mpRaster->getFilename();
   if (outputName.empty() == true)
   {
      outputName = mpRaster->getName();
      if (outputName.empty() == true)
      {
         mMessage = "Could not access the cube's name!";
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress(mMessage, 0, ERRORS);
         }

         mpStep->finalize(Message::Failure, mMessage);
         return false;
      }
   }

   StepResource pStep(mMessage, "spectral", "52F415BA-2C48-42af-B59C-059B7223FC5A", "Can't create spectral cube");

   int loc = outputName.rfind('.');
   if (loc == string::npos)
   {
      loc = outputName.length();
   }
   outputName = outputName.insert(loc, "_mnf");

   unsigned int numRows = mNumRows;
   unsigned int numCols = mNumColumns;
   if (mpProcessingAoi != NULL)
   {
      const BitMask* pMask = mpProcessingAoi->getSelectedPoints();
      VERIFY(pMask != NULL);
      BitMaskIterator it(pMask, mpRaster);
      numCols = it.getNumSelectedColumns();
      numRows = it.getNumSelectedRows();
   }

   RasterElement* pMnfRaster = RasterUtilities::createRasterElement(outputName, numRows, numCols,
      mNumComponentsToUse, FLT8BYTES, BIP, true, NULL);

   // if can't create in memory, then try on_disk
   if (pMnfRaster == NULL)
   {
      pMnfRaster = RasterUtilities::createRasterElement(outputName, numRows, numCols,
         mNumComponentsToUse, FLT8BYTES, BIP, false, NULL);
   }

   if (pMnfRaster == NULL)
   {
      mMessage = "Unable to create a new raster element!";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }

      pStep->finalize(Message::Failure, mMessage);
      return false;
   }
   
   mpMnfRaster = ModelResource<RasterElement>(pMnfRaster);
   RasterDataDescriptor* pRdd = dynamic_cast<RasterDataDescriptor*>(mpMnfRaster->getDataDescriptor());
   if (pRdd == NULL)
   {
      mMessage = "Unable to create a new raster element!";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }

      pStep->finalize(Message::Failure, mMessage);
      return false;
   }

   // copy classification from mpRaster
   mpMnfRaster->copyClassification(mpRaster);

   // Bad values
   vector<int>badValue(1);
   badValue[0] = 0;
   pRdd->setBadValues(badValue);

   // Units
   Units* pUnits = pRdd->getUnits();
   if (pUnits != NULL)
   {
      pUnits->setUnitType(CUSTOM_UNIT);
      pUnits->setUnitName("MNF Value");
      pUnits->setScaleFromStandard(1.0);
   }

   pStep->finalize(Message::Success);
   return true;
}

bool Mnf::computeMnfValues()
{
   QString message;
   VERIFY(mpMnfRaster.get() != NULL)
   const RasterDataDescriptor* pMnfDesc = dynamic_cast<RasterDataDescriptor*>(mpMnfRaster->getDataDescriptor());
   VERIFY(pMnfDesc != NULL);
   EncodingType mnfDataType = pMnfDesc->getDataType();
   unsigned int mnfNumRows = pMnfDesc->getRowCount();
   unsigned int mnfNumCols = pMnfDesc->getColumnCount();
   unsigned int mnfNumBands = pMnfDesc->getBandCount();

   const BitMask* pMask(NULL);
   if (mpProcessingAoi != NULL)
   {
      pMask = mpProcessingAoi->getSelectedPoints();
   }
   BitMaskIterator it(pMask, mpRaster);
   unsigned int colOffset = it.getColumnOffset();
   unsigned int rowOffset = it.getRowOffset();

   if (mnfNumBands != mNumComponentsToUse)
   {
      mMessage = "The dimensions of the MNF RasterElement are not correct.";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }

      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   const RasterDataDescriptor* pOrigDesc = dynamic_cast<const RasterDataDescriptor*>
      (mpRaster->getDataDescriptor());
   if (pOrigDesc == NULL)
   {
      mMessage = "MNF received null pointer to the source data descriptor";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }

      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   EncodingType eDataType = pOrigDesc->getDataType();
   if (!eDataType.isValid())
   {
      mMessage = "MNF received invalid value for source data encoding type";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }

      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }
   // Initialize progress bar variables
   int currentProgress = 0;
   int count = 0;

   void* pMnfData = NULL;
   void* pOrigData = NULL;

   FactoryResource<DataRequest> pBipRequest;
   pBipRequest->setInterleaveFormat(BIP);
   pBipRequest->setRows(pOrigDesc->getActiveRow(rowOffset), pOrigDesc->getActiveRow(rowOffset + mnfNumRows - 1), 1);
   pBipRequest->setColumns(pOrigDesc->getActiveColumn(colOffset),
      pOrigDesc->getActiveColumn(colOffset + mnfNumCols - 1), mnfNumCols);
   DataAccessor origAccessor = mpRaster->getDataAccessor(pBipRequest.release());
   if (!origAccessor.isValid())
   {
      mMessage = "Could not get the pixels in the original cube!";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, currentProgress, ERRORS);
      }

      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   FactoryResource<DataRequest> pBipWritableRequest;
   pBipWritableRequest->setWritable(true);
   pBipWritableRequest->setInterleaveFormat(BIP);
   pBipWritableRequest->setRows(pMnfDesc->getActiveRow(0), pMnfDesc->getActiveRow(mnfNumRows - 1), 1);
   pBipWritableRequest->setColumns(pMnfDesc->getActiveColumn(0),
      pMnfDesc->getActiveColumn(mnfNumCols - 1), mnfNumCols);
   DataAccessor mnfAccessor = mpMnfRaster->getDataAccessor(pBipWritableRequest.release());
   if (!mnfAccessor.isValid())
   {
      mMessage = "MNF could not obtain an accessor to the MNF RasterElement";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }

      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   double* pValues = NULL;
   for (unsigned int row = 0; row < mnfNumRows; ++row)
   {
      if (isAborted())
      {
         break;
      }

      VERIFY(origAccessor.isValid());
      VERIFY(mnfAccessor.isValid());
      if (pMask == NULL)
      {
         pOrigData = origAccessor->getRow();
         pValues = reinterpret_cast<double*>(mnfAccessor->getRow());
         switchOnEncoding(eDataType, computeMnfRow, pOrigData, pValues,
            mpMnfTransformMatrix, mNumColumns, mNumBands, mNumComponentsToUse);
      }
      else
      {
         for (unsigned int col = 0; col < mnfNumCols; ++col)
         {
            if (pMask->getPixel(col + colOffset, row + rowOffset))
            {
               pOrigData = origAccessor->getColumn();
               pValues = reinterpret_cast<double*>(mnfAccessor->getColumn());
               switchOnEncoding(eDataType, computeMnfColumn, pOrigData, pValues,
                  mpMnfTransformMatrix, mNumBands, mNumComponentsToUse);
            }
            origAccessor->nextColumn();
            mnfAccessor->nextColumn();
         }
      }
      origAccessor->nextRow();
      mnfAccessor->nextRow();

      currentProgress = 100 * (row + 1) / mnfNumRows;
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress("Generating MNF data cube...", currentProgress, NORMAL);
      }
   }

   if (isAborted())
   {
      mpProgress->updateProgress("MNF aborted!", currentProgress, ABORT);
      mpStep->finalize(Message::Abort);
      return false;
   }
   if (mpProgress != NULL)
   {
      mpProgress->updateProgress("MNF computations complete!", 100, NORMAL);
   }

   return true;
}

bool Mnf::createMnfView()
{
   if (mbDisplayResults)
   {
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress("Creating view...", 0, NORMAL);
      }

      string filename = mpMnfRaster->getName();

      if (mpProgress != NULL)
      {
         mpProgress->updateProgress("Creating view...", 25, NORMAL);
      }

      SpatialDataWindow* pWindow = static_cast<SpatialDataWindow*>(
         mpDesktop->createWindow(filename.c_str(), SPATIAL_DATA_WINDOW));
      if (pWindow == NULL)
      {
         mMessage = "Could not create new window!";
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress(mMessage, 25, ERRORS);
         }

         mpStep->finalize(Message::Failure, mMessage);
         return false;
      }

      if (pWindow != NULL)
      {
         mpView = pWindow->getSpatialDataView();
      }

      if (mpView == NULL)
      {
         mMessage = "Could not obtain new view!";
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress(mMessage, 25, ERRORS);
         }

         mpStep->finalize(Message::Failure, mMessage);
         return false;
      }

      mpView->setPrimaryRasterElement(mpMnfRaster.get());

      if (mpProgress != NULL)
      {
         mpProgress->updateProgress("Creating view...", 50, NORMAL);
      }

      Layer* pLayer = NULL;
      {
         UndoLock lock(mpView);
         pLayer = mpView->createLayer(RASTER, mpMnfRaster.get());
      }
      if (pLayer == NULL)
      {
         mpMnfRaster.release();  // element will be destroyed when window is deleted
         mpDesktop->deleteWindow(pWindow);
         mMessage = "Could not access raster properties for view!";
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress(mMessage, 50, ERRORS);
         }

         mpStep->finalize(Message::Failure, mMessage);
         return false;
      }

      if (mpProgress != NULL)
      {
         mpProgress->updateProgress("Creating view...", 75, NORMAL);
      }

      // Create a GCP layer if available
      if (mpRaster != NULL)
      {
         UndoLock lock(mpView);
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
                  list<GcpPoint> gcps;
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
                     DataDescriptor* pGcpDescriptor = pModel->createDataDescriptor("Corner Coordinates",
                        "GcpList", mpMnfRaster.get());
                     if (pGcpDescriptor != NULL)
                     {
                        GcpList* pGcpList = static_cast<GcpList*>(pModel->createElement(pGcpDescriptor));
                        if (pGcpList != NULL)
                        {
                           // Add the GCPs to the GCP list
                           pGcpList->addPoints(gcps);

                           // Create the GCP list layer
                           mpView->createLayer(GCP_LAYER, pGcpList);
                        }
                     }
                  }
                  else
                  {
                     string message = "Geocoordinates are not available and will not be added to the new MNF cube!";
                     if (mpProgress != NULL)
                     {
                        mpProgress->updateProgress(message, 0, WARNING);
                     }

                     if (mpStep != NULL)
                     {
                        mpStep->addMessage(message, "spectral", "C53FFFA6-7283-48c7-A67B-C780860588F0", true);
                     }
                  }
               }
            }
         }
      }

      if (isAborted() == false)
      {
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress("Finished creating view...", 100, NORMAL);
         }
      }
      else
      {
         mpMnfRaster.release();  // element will be destroyed when window is deleted
         mpDesktop->deleteWindow(pWindow);
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress("Create view aborted", 100, NORMAL);
         }

         mpStep->finalize(Message::Abort);
         return false;
      }
   }

   return true;
}

bool Mnf::calculateEigenValues()
{
   StepResource pStep("Calculate Eigen Values", "spectral", "B762334E-4184-4dff-83E6-A2F6327E8976");

   unsigned int lBandIndex;
   vector<double> eigenValues(mNumBands);
   double* pEigenValues = &eigenValues.front();

   if (mpProgress != NULL)
   {
      mpProgress->updateProgress("Calculating Eigen Values...", 0, NORMAL);
   }

   // get signal covariance matrix
   MatrixFunctions::MatrixResource<double> signalCovarMatrix(mNumBands, mNumBands);
   double** pSigCovar = signalCovarMatrix;
   if (!computeCovarianceMatrix(mpRaster, pSigCovar,"Signal Data" , mpProcessingAoi))
   {
      // mMessage set in called method;
      pStep->finalize(Message::Failure, mMessage);
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 100, ERRORS);
      }
      return false;
   }

   if (!performCholeskyDecomp(pSigCovar, pEigenValues, mNumBands, mNumBands))
   {
      // mMessage set in performCholeskyDecomp
      pStep->finalize(Message::Failure, mMessage);
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 100, ERRORS);
      }
      return false;
   }

   // set diagonal terms and zero out upper triangle
   for (unsigned int i = 0; i < mNumBands; ++i)
   {
      for (unsigned int j = i; j < mNumBands; ++j)
      {
         if (i == j)
         {
            pSigCovar[i][j] = pEigenValues[i];
         }
         else
         {
            pSigCovar[i][j] = 0.0;
         }
      }
   }

   // compute Li (inverse of lower triangle)
   MatrixFunctions::MatrixResource<double> lowerInverse(mNumBands, mNumBands);
   double** pLi = lowerInverse;
   int numRows = static_cast<int>(mNumBands);
   if (!MatrixFunctions::invertSquareMatrix2D(pLi, const_cast<const double**>(pSigCovar), numRows))
   {
      mMessage = "Unable to invert matrix.";
      pStep->finalize(Message::Failure, mMessage);
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 100, ERRORS);
      }
      return false;
   }

   // make sure upper triangle of pLi is zeroed out.
   for (unsigned int i = 0; i < mNumBands; ++i)
   {
      for (unsigned int j = i; j < mNumBands; ++j)
      {
         if (i != j)
         {
            pLi[i][j] = 0.0;
         }
      }
   }

   double sum(0.0);
   {
   // scope existence of these matrices so can recover memory
      MatrixFunctions::MatrixResource<double> lowerInverseTranspose(mNumBands, mNumBands);
      double** pLiT = lowerInverseTranspose;
      MatrixFunctions::MatrixResource<double> intermediateMatrix(mNumBands, mNumBands);
      double** pIntermed = intermediateMatrix;
      for (unsigned int i = 0; i < mNumBands; ++i)
      {
         for (unsigned int j = 0; j < mNumBands; ++j)
         {
            pLiT[i][j] = pLi[j][i];
         }
      }

      // compute Li * mpNoiseCovar, reuse pSigCov to store intermediate results
      for (unsigned int row = 0; row < mNumBands; ++row)
      {
         for (unsigned int col = 0; col < mNumBands; ++col)
         {
            sum = 0.0;
            for (unsigned int index = 0; index < mNumBands; ++index)
            {
               sum += pLi[row][index] * mpNoiseCovarMatrix[index][col];
            }
            pSigCovar[row][col] = sum;
         }
      }
      // and now multiple above result (currently in pSigCover) by transpose of Li
      for (unsigned int row = 0; row < mNumBands; ++row)
      {
         for (unsigned int col = 0; col < mNumBands; ++col)
         {
            sum = 0.0;
            for (unsigned int index = 0; index < mNumBands; ++index)
            {
               sum += pSigCovar[row][index] * pLiT[index][col];
            }
            pIntermed[row][col] = sum;
         }
      }

      // make sure matrix is symmetrical
      if (MatrixFunctions::isMatrixSymmetric(const_cast<const double**>(pIntermed), mNumBands, 0.00000001) == false)
      {
         for (unsigned int row = 0; row < mNumBands; ++row)
         {
            for (unsigned int col = 0; col < mNumBands; ++col)
            {
               double avg = (pIntermed[row][col] + pIntermed[col][row]) / 2.0;
               pIntermed[row][col] = avg;
               pIntermed[col][row] = avg;
            }
         }
      }

      // Get the eigenvalues and eigenvectors. Store the eigenvectors in mpMnfTransformMatrix for future use.
      try
      {
         if (MatrixFunctions::getEigenvalues(const_cast<const double**>(pIntermed),
            pEigenValues, mpMnfTransformMatrix, mNumBands) == false)
         {
            pStep->finalize(Message::Failure, "Unable to calculate eigenvalues.");
            if (mpProgress != NULL)
            {
               mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
            }
            return false;
         }
      }
      catch (...)
      {
         pStep->finalize(Message::Failure, "Unable to calculate eigenvalues.");
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
         }
         return false;
      }
   }

   // Now a little more manipulation - reuse pSigCovar to hold intermediate results.
   // We also need to use the transpose of the eigen vectors from mpMnfTransformMatrix,
   // so instead of accessing [row][index], we'll access [index][row]
   for (unsigned int row = 0; row < mNumBands; ++row)
   {
      for (unsigned int col = 0; col < mNumBands; ++col)
      {
         sum = 0.0;
         for (unsigned int index = 0; index < mNumBands; ++index)
         {
            sum += mpMnfTransformMatrix[index][row] * pLi[index][col];
         }
         pSigCovar[row][col] = sum;
      }
   }

   // a final transpose
   for (unsigned int i = 0; i < mNumBands; ++i)
   {
      for (unsigned int j = 0; j < mNumBands; ++j)
      {
         mpMnfTransformMatrix[i][j] = pSigCovar[j][i];
      }
   }

   if (mpProgress != NULL)
   {
      mpProgress->updateProgress("Calculating Eigen Values...", 80, NORMAL);
   }

   double dEigen_Sum = 0.0;
   double dEigen_Current = 0.0;
   double dTemp = 0.0;
   int lNoise_Cutoff = 1;
   for (lBandIndex = 0; lBandIndex < mNumBands; ++lBandIndex)
   {
      dEigen_Sum += pEigenValues[lBandIndex];
   }

   if (mpProgress != NULL)
   {
      mpProgress->updateProgress("Calculating Eigen Values...", 90, NORMAL);
   }

   for (lBandIndex = 0; lBandIndex < mNumBands; ++lBandIndex)
   {
      dEigen_Current += pEigenValues[lBandIndex];
      dTemp = 100.0 * dEigen_Current / dEigen_Sum;
      if (dTemp < 99.99)
      {
         lNoise_Cutoff++;
      }
   }
   pStep->addProperty("Noise cutoff", lNoise_Cutoff);

   if (mpProgress != NULL)
   {
      mpProgress->updateProgress("Calculation of Eigen Values completed", 100, NORMAL);
   }

   // check if user wanted to select num components based on SNR value plot
   if (mbUseSnrValPlot)
   {
      vector<double> snrValues(mNumBands);
      for (unsigned int i = 0; i < mNumBands; ++i)
      {
         snrValues[mNumBands -1 - i] = 2.0 / pEigenValues[i];
      }
      EigenPlotDlg plotDlg(mpDesktop->getMainWidget());
      plotDlg.setEigenValues(&snrValues.front(), mNumBands);
      if (plotDlg.exec() == QDialog::Rejected)
      {
         abort();
         return false;
      }
      mNumComponentsToUse = plotDlg.getNumComponents();
   }

   // and finally reverse the order of the components since it currently has most noisy as first component
   double tmpDbl(0.0);
   for (unsigned int comp = 0; comp < mNumBands / 2; ++comp)
   {
      for (unsigned int index = 0; index < mNumBands; ++index)
      {
         tmpDbl = mpMnfTransformMatrix[index][comp];
         mpMnfTransformMatrix[index][comp] = mpMnfTransformMatrix[index][mNumBands - comp -1];
         mpMnfTransformMatrix[index][mNumBands - comp -1] = tmpDbl;
      }
   }

   pStep->finalize(Message::Success);

   return true;
}

bool Mnf::generateNoiseStatistics()
{
   VERIFY(mpRaster != NULL);
   vector<string> aoiNames;
   double* pdTemp = NULL;
   QString strFilename;

   string filename = mpRaster->getFilename();
   if (filename.empty() == false)
   {
      strFilename = QString::fromStdString(filename);
   }

   QString message;
   bool success = false;
   switch (mNoiseStatisticsMethod)
   {
   case DIFFDATA:
      {
         bool useAutoSelection(true);
         float bandFracThres(0.8f);
         if (isBatch() == false)
         {
            DifferenceImageDlg dDlg(mpRaster, mpDesktop->getMainWidget());
            if (dDlg.exec() == QDialog::Rejected)
            {
               abort();
               mpStep->finalize(Message::Abort);
               return false;
            }

            if (dDlg.useAutomaticSelection())
            {
               bandFracThres = dDlg.getBandFractionThreshold();
            }
            else
            {
               useAutoSelection = false;
               mpNoiseAoi = getAoiElement(dDlg.getAoiName(), mpRaster);
            }
         }

         mpNoiseRaster = ModelResource<RasterElement>(createDifferenceRaster(mpNoiseAoi));
         if (mpNoiseRaster.get() == NULL)
         {
            // mMessage set in called method
            mpStep->finalize(Message::Failure, mMessage);
            return false;
         }

         // Need to create aoi for mpNoiseRaster from selected points in mpNoiseAoi since points need to be
         // relative to mpNoiseRaster
         ModelResource<AoiElement> pDiffAoi(createDifferenceAoi(mpNoiseAoi, mpNoiseRaster.get()));

         success = computeCovarianceMatrix(mpNoiseRaster.get(), mpNoiseCovarMatrix,
            "Noise Estimation Data", pDiffAoi.get());

         if (success)
         {
            strFilename += ".mnfcvm";
            writeMatrixToFile(strFilename, const_cast<const double**>(mpNoiseCovarMatrix),
               mNumBands, "Noise Covariance");
         }
      }
      break;

   case DARKCURRENT:
      {
         int rowSkip(1);
         int colSkip(1);
         if (isBatch() == false)
         {
            StatisticsDlg sDlg(mpRaster->getName(), mpDesktop->getMainWidget());
            if (sDlg.exec() == QDialog::Rejected)
            {
               abort();
               mpStep->finalize(Message::Abort);
               return false;
            }

            mpNoiseRaster = ModelResource<RasterElement>(dynamic_cast<RasterElement*>(
               mpModel->getElement(sDlg.getDarkCurrentDataName(), TypeConverter::toString<RasterElement>(), NULL)));
            if (mpNoiseRaster.get() == NULL)
            {
               mpStep->finalize(Message::Failure, "Could not get access to the dark current raster element");
               return false;
            }
            mpNoiseAoi = getAoiElement(sDlg.getAoiName(), mpNoiseRaster.get());
            rowSkip = sDlg.getRowFactor();
            colSkip = sDlg.getColumnFactor();
         }

         success = computeCovarianceMatrix(mpNoiseRaster.get(), mpNoiseCovarMatrix,
            "Dark Current Data", mpNoiseAoi, rowSkip, colSkip);

         if (success)
         {
            strFilename += ".mnfcvm";
            writeMatrixToFile(strFilename, const_cast<const double**>(mpNoiseCovarMatrix),
               mNumBands, "Noise Covariance");
         }
      }
      break;

   case PREVIOUS:
      if (isBatch() == false)
      {
         strFilename += ".mnfcvm";
         strFilename = QFileDialog::getOpenFileName(mpDesktop->getMainWidget(), "Select Noise Covariance File",
            strFilename, "Matrices (*.mnfcvm)");
      }

      if (strFilename.isEmpty())
      {
         abort();
         mpStep->finalize(Message::Abort);
         return false;
      }

      if (!readMatrixFromFile(strFilename, mpNoiseCovarMatrix, mNumBands, "Noise Covariance"))
      {
         return false; // error logged in readMatrixFromFile routine
      }
      success = true;
      break;

   default:
      break;
   }

   return success;
}

bool Mnf::writeMatrixToFile(QString filename, const double **pData, int numBands, const string &caption)
{
   FileResource pFile(filename.toStdString().c_str(), "wt");
   if (pFile.get() == NULL)
   {
      mMessage = "Unable to save " + caption + " matrix to disk as " + filename.toStdString();
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 100, ERRORS);
      }

      mpStep->addMessage(mMessage, "spectral", "A0478959-21AF-4e64-B9DA-C17D7363F1BB", true);
   }
   else
   {
      fprintf(pFile, "%d\n", numBands);
      for (int row = 0; row < numBands; ++row)
      {
         for (int col = 0; col < numBands; ++col)
         {
            fprintf(pFile, "%.15e ", pData[row][col]);
         }
         fprintf(pFile, "\n");
      }

      mMessage = caption + " matrix saved to disk as " + filename.toStdString();
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 100, NORMAL);
      }
   }

   return true;
}

bool Mnf::readMatrixFromFile(QString filename, double **pData, int numBands, const string &caption)
{
   FileResource pFile(filename.toStdString().c_str(), "rt");
   if (pFile.get() == NULL)
   {
      mMessage = "Unable to read " + caption + " matrix from file " + filename.toStdString();
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }

      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }
   mMessage = "Reading "  + caption + " matrix from file " + filename.toStdString();
   mpProgress->updateProgress(mMessage, 0, NORMAL);

   int lnumBands = 0;
   int numFieldsRead = fscanf(pFile, "%d\n", &lnumBands);
   if (numFieldsRead != 1)
   {
      mMessage = "Unable to read matrix file\n" + filename.toStdString();
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }

      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }
   if (lnumBands != numBands)
   {
      mMessage = "Mismatch between number of bands in cube and in matrix file.";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }

      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }
   for (int row = 0; row < numBands; ++row)
   {
      for (int col = 0; col < numBands; ++col)
      {
         numFieldsRead = fscanf(pFile, "%lg ", &(pData[row][col]));
         if (numFieldsRead != 1)
         {
            mMessage = "Error reading " + caption + " matrix from disk.";
            if (mpProgress != NULL)
            {
               mpProgress->updateProgress(mMessage, 0, ERRORS);
            }

            mpStep->finalize(Message::Failure, mMessage);
            return false;
         }
      }
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 100 * row / numBands, NORMAL);
      }
   }
   mMessage = caption + " matrix successfully read from disk";
   if (mpProgress != NULL)
   {
      mpProgress->updateProgress(mMessage, 100, NORMAL);
   }

   return true;
}

bool Mnf::readInMnfTransform(const string& filename)
{
   FileResource pFile(filename.c_str(), "rt");
   if (pFile.get() == NULL)
   {
      mMessage = "Unable to read MNF transform from file " + filename;
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }

      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   mMessage = "Reading MNF transform from file " + filename;
   if (mpProgress != NULL)
   {
      mpProgress->updateProgress(mMessage, 0, NORMAL);
   }

   unsigned int lnumBands = 0;
   unsigned int lnumComponents = 0;
   int numFieldsRead = fscanf(pFile, "%u\n", &lnumBands);
   if (numFieldsRead != 1)
   {
      mMessage = "Error reading number of bands from MNF transform file:\n" + filename;
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }

      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   numFieldsRead = fscanf(pFile, "%u\n", &lnumComponents);
   if (numFieldsRead != 1)
   {
      mMessage = "Error reading number of components from MNF transform file:\n" + filename;
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }

      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   if (lnumBands != mNumBands)
   {
      mMessage = "Mismatch between number of bands in cube and in MNF transform file.";
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 0, ERRORS);
      }

      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }
   bool success = true;
   if (lnumComponents < mNumComponentsToUse)
   {
      if (isBatch() == false)
      {
         QString message(QString("This file only contains definitions for %1 components, not %2.").
            arg(lnumComponents).arg(mNumComponentsToUse));
         success = !QMessageBox::warning(NULL, "MNF", message, "Continue", "Cancel");
      }
   }

   if (success)
   {
      double junk = 0.0;
      for (unsigned int row = 0; row < mNumBands; ++row)
      {
         for (unsigned int col = 0; col < lnumComponents; ++col)
         {
            if (col < mNumComponentsToUse)
            {
               numFieldsRead = fscanf(pFile, "%lg ", &(mpMnfTransformMatrix[row][col]));
            }
            else
            {
               numFieldsRead = fscanf(pFile, "%lg ", &(junk));
            }

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
         mpProgress->updateProgress(mMessage, 100 * row / mNumBands, NORMAL);
      }
      if (success)
      {
         mMessage = "MNF transform successfully read from disk";
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress(mMessage, 100, NORMAL);
         }
      }
      else
      {
         mMessage = "Error reading MNF transform from disk.";
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress(mMessage, 0, ERRORS);
         }

         mpStep->finalize(Message::Failure, mMessage);
      }
   }

   return success;
}

bool Mnf::writeOutMnfTransform(const string& filename)
{
   FileResource pFile(filename.c_str(), "wt");
   if (pFile.get() == NULL)
   {
      mMessage = "Unable to save MNF transform to disk as " + filename;
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(mMessage, 100, ERRORS);
      }

      mpStep->finalize(Message::Failure, mMessage);
      return false;
   }

   // write out entire transform, not just the number of components used in this run
   fprintf(pFile, "%u\n", mNumBands);
   fprintf(pFile, "%u\n", mNumBands);
   for (unsigned int row = 0; row < mNumBands; ++row)
   {
      for (unsigned int col = 0; col < mNumBands; ++col)
      {
         fprintf(pFile, "%.15e ", mpMnfTransformMatrix[row][col]);
      }
      fprintf(pFile, "\n");
   }

   DynamicObject* pMetadata = mpRaster->getMetadata();
   FactoryResource<Wavelengths> pWavelengths;
   pWavelengths->initializeFromDynamicObject(pMetadata, false);

   const std::vector<double>& centerWavelengths = pWavelengths->getCenterValues();
   if (centerWavelengths.size() == mNumBands)
   {
      fprintf(pFile, "\nWavelengths\n");
      for (unsigned int band = 0; band < mNumBands; ++band)
      {
         fprintf(pFile, "%.8g\n", centerWavelengths[band]);
      }
   }

   mMessage = "MNF transform saved to disk as " + filename;
   if (mpProgress != NULL)
   {
      mpProgress->updateProgress(mMessage, 100, NORMAL);
   }

   return true;
}

AoiElement* Mnf::getAoiElement(const std::string& aoiName, RasterElement* pRaster)
{
   AoiElement* pAoi = dynamic_cast<AoiElement*>(mpModel->getElement(aoiName,
      TypeConverter::toString<AoiElement>(), pRaster));
   if (pAoi == NULL)
   {
      pAoi = dynamic_cast<AoiElement*>(mpModel->getElement(aoiName,
         TypeConverter::toString<AoiElement>(), NULL));
   }

   return pAoi;
}

void Mnf::initializeNoiseMethods()
{
   mNoiseEstimationMethods << QString("Estimate from Data");
   mNoiseEstimationMethods << QString("Derive from Dark Current Data");
   mNoiseEstimationMethods << QString("Use previous statistics");
}

string Mnf::getNoiseEstimationMethodString(NoiseEstimateType noiseType)
{
   string noiseStr = "Invalid Method";
   if (noiseType.isValid())
   {
      noiseStr = mNoiseEstimationMethods[static_cast<int>(noiseType)].toStdString();
   }

   return noiseStr;
}

Mnf::NoiseEstimateType Mnf::getNoiseEstimationMethodType(string noiseStr)
{
   NoiseEstimateType noiseType;

   // keep cases in sync with values in NoiseEstimateTypeEnum
   switch (mNoiseEstimationMethods.indexOf(QString::fromStdString(noiseStr)))
   {
   case 2:
      noiseType = PREVIOUS;
      break;

   case 1:
      noiseType = DARKCURRENT;
      break;

   case 0:
      noiseType = DIFFDATA;
      break;

   default:  // leave noiseType invalid if no match to strings in mNoiseEstimationMethods
      break;
   }

   return noiseType;
}

RasterElement* Mnf::createDifferenceRaster(AoiElement* pAoi)
{
   if (mpRaster == NULL)
   {
      return NULL;
   }

   RasterDataDescriptor* pDesc = dynamic_cast<RasterDataDescriptor*>(mpRaster->getDataDescriptor());
   VERIFYRV(pDesc != NULL, NULL);
   const BitMask* pSelectedPixels(NULL);
   if (pAoi != NULL)
   {
      pSelectedPixels = pAoi->getSelectedPoints();
   }
   BitMaskIterator it(pSelectedPixels, mpRaster);

   unsigned int numRows = it.getNumSelectedRows();
   unsigned int numCols = it.getNumSelectedColumns();
   unsigned int numPixels = numCols * numRows;
   if (numRows <= 1 || numCols <= 1)
   {
      mMessage = "AOI for difference image is invalid.";
      return NULL;
   }

   string diffName = "MnfDiffData";

   // check if left over from previous run
   DataElement* pElement = mpModel->getElement(diffName, TypeConverter::toString<RasterElement>(),
      mpRaster);
   if (pElement != NULL)
   {
      mpModel->destroyElement(pElement);
   }
   ModelResource<RasterElement> pDiffRaster(RasterUtilities::createRasterElement(diffName, numRows, numCols,
      mNumBands, FLT8BYTES, BIP, true, mpRaster));
   if (pDiffRaster.get() == NULL)
   {
      // try on-disk
      pDiffRaster = ModelResource<RasterElement>(RasterUtilities::createRasterElement(diffName, numRows,
         numCols, mNumBands, FLT8BYTES, BIP, false, mpRaster));
   }

   if (pDiffRaster.get() == NULL)
   {
      mMessage = "Unable to create the difference raster element.";
      return NULL;
   }

   FactoryResource<DataRequest> pDiffRequest;
   pDiffRequest->setInterleaveFormat(BIP);
   pDiffRequest->setRows(pDesc->getActiveRow(0), pDesc->getActiveRow(numRows - 1), 1);
   pDiffRequest->setColumns(pDesc->getActiveColumn(0), pDesc->getActiveColumn(numCols -1), numCols);
   pDiffRequest->setWritable(true);

   // Use copy of pDiffRequest here since we'll need another accessor with same request at end of method
   DataAccessor diffAcc = pDiffRaster->getDataAccessor(pDiffRequest->copy());
   VERIFYRV(diffAcc.isValid(), NULL);

   // increment diffAcc since we'll start on second row (row2col1 - row1col2)
   diffAcc->nextRow();

   // The values from the BitMaskIterator can't be negative
   unsigned int firstRow = it.getBoundingBoxStartRow();
   unsigned int lastRow = it.getBoundingBoxEndRow();
   unsigned int firstColumn = it.getBoundingBoxStartColumn();
   unsigned int lastColumn = it.getBoundingBoxEndColumn();

   // scope these accessors so we can recover memory
   {
      FactoryResource<DataRequest> pRequest1;
      pRequest1->setInterleaveFormat(BIP);
      pRequest1->setRows(pDesc->getActiveRow(firstRow +1), pDesc->getActiveRow(lastRow), 1);
      pRequest1->setColumns(pDesc->getActiveColumn(firstColumn), pDesc->getActiveColumn(lastColumn - 1), numCols - 1);
      DataAccessor orig1Acc = mpRaster->getDataAccessor(pRequest1.release());
      FactoryResource<DataRequest> pRequest2;
      pRequest2->setInterleaveFormat(BIP);
      pRequest2->setRows(pDesc->getActiveRow(firstRow), pDesc->getActiveRow(lastRow - 1), 1);
      pRequest2->setColumns(pDesc->getActiveColumn(firstColumn + 1), pDesc->getActiveColumn(lastColumn), numCols - 1);
      DataAccessor orig2Acc = mpRaster->getDataAccessor(pRequest2.release());

      if (orig1Acc.isValid() == false || orig2Acc.isValid() == false || diffAcc.isValid() == false)
      {
         mMessage = "Unable to access original raster data or the difference raster data.";
         return NULL;
      }

      // compute differences
      EncodingType eType = pDesc->getDataType();
      for (unsigned int row = firstRow + 1; row <= lastRow; ++row)
      {
         VERIFYRV(diffAcc.isValid(), NULL);
         VERIFYRV(orig1Acc.isValid(), NULL);
         VERIFYRV(orig2Acc.isValid(), NULL);
         for (unsigned int col = firstColumn; col < lastColumn; ++col)
         {
            if (pSelectedPixels == NULL || pSelectedPixels->getPixel(col, row))
            {
               void* pData1 = orig1Acc->getColumn();
               void* pData2 = orig2Acc->getColumn();
               double* pDiffs = reinterpret_cast<double*>(diffAcc->getColumn());
               switchOnEncoding(eType, computeDifferencePixel, NULL, pData1, pData2, pDiffs, mNumBands);
            }
            orig1Acc->nextColumn();
            orig2Acc->nextColumn();
            diffAcc->nextColumn();
         }
         orig1Acc->nextRow();
         orig2Acc->nextRow();
         diffAcc->nextRow();
      }
   }

   // now the first row and last column in diff raster are still unset
   DataAccessor diffAcc2 = pDiffRaster->getDataAccessor(pDiffRequest.release());

   diffAcc->toPixel(0,0);
   diffAcc2->toPixel(numRows - 1, 0);
   VERIFYRV(diffAcc.isValid(), NULL);
   VERIFYRV(diffAcc2.isValid(), NULL);

   // copy last row into first row
   memcpy(diffAcc->getRow(), diffAcc2->getRow(), numCols * mNumBands * sizeof(double));

   // now copy first column into last column
   for (unsigned int row = 0; row < numRows; ++row)
   {
      diffAcc->toPixel(row, 0);
      diffAcc2->toPixel(row, numCols - 1);
      VERIFYRV(diffAcc.isValid(), NULL);
      VERIFYRV(diffAcc2.isValid(), NULL);
      memcpy(diffAcc2->getColumn(), diffAcc->getColumn(), mNumBands * sizeof(double));
   }

   return pDiffRaster.release();
}

bool Mnf::performCholeskyDecomp(double** pMatrix, double* pVector, int numRows, int numCols)
{
   if (pMatrix == NULL || pVector == NULL || numRows < 1 || numRows != numCols)
   {
      mMessage = "Invalid input to the Cholesky Decomposition method.";
         return false;
   }

   double sum(0.0);
   for (int i = 0; i < numRows; ++i)
   {
      for (int j = i; j < numCols; ++j)
      {
         sum = pMatrix[i][j];
         for (int k = i - 1; k >= 0; --k)
         {
            sum -= pMatrix[i][k] * pMatrix[j][k];
         }
         if (i == j)
         {
            if (sum <= 0.0)
            {
               sum = 0.0001;
            }
            pVector[i] = sqrt(sum);
         }
         else
         {
            pMatrix[j][i] = sum / pVector[i];
         }
      }
   }

   return true;
}

bool Mnf::computeCovarianceMatrix(RasterElement* pRaster, double **pMatrix, std::string info,
                                       AoiElement* pAoi, int rowFactor, int columnFactor)
{
   VERIFY(pRaster != NULL);
   VERIFY(pMatrix != NULL);

   const RasterDataDescriptor* pDesc = dynamic_cast<const RasterDataDescriptor*>(pRaster->getDataDescriptor());
   VERIFY(pDesc != NULL);
   unsigned int numRows = pDesc->getRowCount();
   unsigned int numCols = pDesc->getColumnCount();
   unsigned int numBands = pDesc->getBandCount();
   EncodingType eType = pDesc->getDataType();

   const BitMask* pMask(NULL);
   if (pAoi != NULL)
   {
      pMask = pAoi->getSelectedPoints();
      VERIFY(pMask != NULL);

      // check that AOI is not outside the image data
      int x1(0);
      int y1(0);
      int x2(0);
      int y2(0);
      BitMaskIterator it(pMask, mpRaster);
      it.getBoundingBox(x1, y1, x2, y2);

      if (x2 == 0 || y2 == 0)
      {
         mMessage = "AOI for " + info + " covariance computation is invalid.";
         return false;
      }
   }

   unsigned int row, col;
   unsigned int band1, band2;

   if (rowFactor < 1)
   {
      rowFactor = 1;
   }
   if (columnFactor < 1)
   {
      columnFactor = 1;
   }

   vector<double> means(numBands, 0.0);
   double* pMeans = &means.front();

   // calculate mean band values
   float progScale = 100.0f / static_cast<float>(numRows);
   FactoryResource<DataRequest> pRequest;
   pRequest->setInterleaveFormat(BIP);
   DataAccessor accessor = pRaster->getDataAccessor(pRequest.release());
   unsigned int pixCount = 0;
   void* pData(NULL);
   unsigned int currentRow = 0;
   unsigned int currentCol = 0;
   for (row = 0; row < numRows; row += rowFactor)
   {
      while (row > currentRow)  // need to account for skip factor
      {
         accessor->nextRow();
         ++currentRow;
      }
      VERIFY(accessor.isValid());
      if (mpProgress != NULL)
      {
         if (isAborted() == false)
         {
            mpProgress->updateProgress ("Computing mean band values for " + info +
               "...", int(progScale*row), NORMAL);
         }
         else
         {
            break;
         }
      }
      currentCol = 0;
      for (col = 0; col < numCols; col += columnFactor)
      {
         while (col > currentCol)  // need to account for skip factor
         {
            accessor->nextColumn();
            ++currentCol;
         }

         if (pMask == NULL || pMask->getPixel(col, row))
         {
            ++pixCount;
            pData = accessor->getColumn();
            switchOnEncoding(eType, sumBandValues, NULL, pData, pMeans, numBands);
         }
      }
   }

   unsigned int pixCount2 = 0;
   if (isAborted() == false)
   {
      // divide band sums by number of pixels
      for (band1 = 0; band1 < numBands; ++band1)
      {
         pMeans[band1] /= static_cast<double>(pixCount);
      }
   }

   // check if aborted
   if (isAborted() == false)
   {
      // compute the covariance
      FactoryResource<DataRequest> pRequest2;
      pRequest2->setInterleaveFormat(BIP);
      accessor = pRaster->getDataAccessor(pRequest2.release());

      pixCount2 = 0;
      currentRow = 0;
      currentCol = 0;
      for (row = 0; row < numRows; row += rowFactor)
      {
         while (row > currentRow)  // need to account for skip factor
         {
            accessor->nextRow();
            ++currentRow;
         }
         VERIFY(accessor.isValid());
         if (mpProgress != NULL)
         {
            if (isAborted() == false)
            {
               mpProgress->updateProgress("Computing Covariance Matrix for " + info +
                  "...", int(progScale * row), NORMAL);
            }
            else
            {
               break;
            }
         }
         currentCol = 0;
         for (col = 0; col < numCols; col += columnFactor)
         {
            while (col > currentCol)  // need to account for skip factor
            {
               accessor->nextColumn();
               ++currentCol;
            }
            if (pMask == NULL || pMask->getPixel(col, row))
            {
               ++pixCount2;
               pData = accessor->getColumn();
               switchOnEncoding(eType, computeCovarValue,
                  pData, pMeans, pMatrix, numBands);
            }
         }
      }
   }

   // check if aborted
   if (isAborted() == false)
   {
      // Check pixel count still same
      if (pixCount != pixCount2)
      {
         mMessage = "Error occurred in computing the covariance - mismatch in number of pixels to sample.";
         return false;
      }

      // Get mean covariances
      for (band2 = 0; band2 < numBands; ++band2)
      {
         for (band1 = band2; band1 < numBands; ++band1)
         {
            pMatrix[band2][band1] /= pixCount2 - 1;
         }
      }

      // Fill other half of triangle
      for (band2 = 0; band2 < numBands; ++band2)
      {
         for (band1 = band2 + 1; band1 < numBands; ++band1)
         {
            pMatrix[band1][band2] = pMatrix[band2][band1];
         }
      }
   }

   // if calculating for mpRaster, then save the band means
   if (pRaster == mpRaster)
   {
      mSignalBandMeans.swap(means);
   }

   if (mpProgress != NULL)
   {
      if (isAborted() == false)
      {
         mpProgress->updateProgress("Covariance Matrix Complete", 100, NORMAL);
      }
      else
      {
         mpProgress->updateProgress("Aborted computing Covariance Matrix", 0, ABORT);
      }
   }

   return true;
}

AoiElement* Mnf::createDifferenceAoi(AoiElement* pAoi, RasterElement* pParent)
{
   if (pAoi == NULL || pParent == NULL)
   {
      return NULL;
   }

   ModelResource<AoiElement> pDiffAoi("DiffAoi", pParent);
   if (pDiffAoi.get() != NULL)
   {
      int x1(0);
      int y1(0);
      int x2(0);
      int y2(0);
      const BitMask* pMask = pAoi->getSelectedPoints();
      if (pMask == NULL)
      {
         return NULL;
      }

      RasterDataDescriptor* pDesc = dynamic_cast<RasterDataDescriptor*>(pParent->getDataDescriptor());
      VERIFYRV(pDesc != NULL, NULL);
      unsigned int numRows = pDesc->getRowCount();
      unsigned int numCols = pDesc->getColumnCount();
      BitMaskIterator it(pMask, mpRaster);
      it.getBoundingBox(x1, y1, x2, y2);
      FactoryResource<BitMask> pNewMask;
      const bool** pPoints = const_cast<BitMask*>(pMask)->getRegion(x1, y1, x2, y2);
      for (int y = 0; y < static_cast<int>(numRows); ++y)
      {
         for (int x = 0; x < static_cast<int>(numCols); ++x)
         {
            pNewMask->setPixel(x, y, pPoints[y][x]);
         }
      }
      pDiffAoi->addPoints(pNewMask.get());
   }

   return pDiffAoi.release();
}
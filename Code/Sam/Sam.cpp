/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AoiElement.h"
#include "AppConfig.h"
#include "AppAssert.h"
#include "AppVerify.h"
#include "BitMask.h"
#include "BitMaskIterator.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DesktopServices.h"
#include "DynamicObject.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "ProgressTracker.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "Resampler.h"
#include "Sam.h"
#include "SamDlg.h"
#include "SamErr.h"
#include "Signature.h"
#include "SpectralUtilities.h"
#include "SpectralVersion.h"
#include "Statistics.h"
#include "switchOnEncoding.h"
#include "Wavelengths.h"

using namespace std;

REGISTER_PLUGIN_BASIC(SpectralSam, Sam);

Sam::Sam() : AlgorithmPlugIn(&mInputs), mpSamGui(NULL), mpSamAlg(NULL), mpProgress(NULL)
{
   setDescriptorId("{D202C405-0F25-46A9-9C1D-A436EC5D3210}");
   setName("SAM");
   setCreator("Ball Aerospace & Technologies Corp.");
   setShortDescription("Spectral Angle Mapper");
   setDescription("Compute spectral angles for material identification against signatures or AOIs");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setMenuLocation("[Spectral]\\Material ID\\SAM");
   setAbortSupported(true);
}

Sam::~Sam()
{
}

bool Sam::populateBatchInputArgList(PlugInArgList* pInArgList)
{
   VERIFY(pInArgList != NULL);
   populateInteractiveInputArgList(pInArgList);
   VERIFY(pInArgList->addArg<Signature>("Target Signatures", NULL, "Target signatures to be used by SAM."));
   VERIFY(pInArgList->addArg<double>("Threshold", mInputs.mThreshold, "Threshold for pixels that will be "
      "automatically flagged in the resulting threshold layer."));
   VERIFY(pInArgList->addArg<AoiElement>("AOI", mInputs.mpAoi, "AOI over which SAM will be performed. If not "
      "specified, the entire cube is used in processing."));
   VERIFY(pInArgList->addArg<bool>("Display Results", mInputs.mbDisplayResults, "Flag for whether the results of the "
      "SAM operation should be displayed."));
   VERIFY(pInArgList->addArg<string>("Results Name", mInputs.mResultsName, "Name of the raster element resulting from "
      "SAM."));
   return true;
}

bool Sam::populateInteractiveInputArgList(PlugInArgList* pInArgList)
{
   VERIFY(pInArgList != NULL);
   VERIFY(pInArgList->addArg<Progress>(Executable::ProgressArg(), NULL, Executable::ProgressArgDescription()));
   VERIFY(pInArgList->addArg<RasterElement>(Executable::DataElementArg(), NULL, "Raster element on which SAM will "
      "be performed."));
   return true;
}

bool Sam::populateDefaultOutputArgList(PlugInArgList* pOutArgList)
{
   VERIFY(pOutArgList->addArg<RasterElement>("Sam Results", NULL, "Raster element resulting from the "
      "SAM operation."));
   return true;
}

bool Sam::parseInputArgList(PlugInArgList* pInArgList)
{
   mpProgress = pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg());
   mProgress = ProgressTracker(mpProgress, "Spectral Angle Mapper",
                              "spectral", "82DDE066-46DA-4241-94EE-5E3D16B5358E");
   RasterElement* pElement = pInArgList->getPlugInArgValue<RasterElement>(Executable::DataElementArg());
   if (pElement == NULL)
   {
      mProgress.report(SAMERR001, 0, ERRORS, true);
      return false;
   }

   const RasterDataDescriptor* pElementDescriptor = static_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());
   EncodingType dataType = pElementDescriptor->getDataType();
   if (dataType == INT4SCOMPLEX || dataType == FLT8COMPLEX)
   {
      mProgress.report(SAMERR013, 0, ERRORS, true);
      return false;
   }

   // sensor is non-null and only one band -> bail out!
   if (pElementDescriptor->getBandCount() == 1)
   {
      mProgress.report(SAMERR014, 0, ERRORS, true);
      return false;
   }

   Signature* pSignatures = NULL;
   if (!isInteractive())
   {
      pSignatures = pInArgList->getPlugInArgValue<Signature>("Target Signatures");
      VERIFY(pInArgList->getPlugInArgValue("Threshold", mInputs.mThreshold));
      mInputs.mpAoi = pInArgList->getPlugInArgValue<AoiElement>("AOI");
      VERIFY(pInArgList->getPlugInArgValue("Display Results", mInputs.mbDisplayResults));
      VERIFY(pInArgList->getPlugInArgValue("Results Name", mInputs.mResultsName));

      mInputs.mSignatures = SpectralUtilities::extractSignatures(vector<Signature*>(1, pSignatures));
   }
   const BitMask* pBitMask = NULL;
   if (mInputs.mpAoi != NULL)
   {
      pBitMask = mInputs.mpAoi->getSelectedPoints();
   }
   mpSamAlg = new SamAlgorithm(pElement, mpProgress, isInteractive(), pBitMask);
   setAlgorithmPattern(Resource<AlgorithmPattern>(mpSamAlg));
   return true;
}

bool Sam::setActualValuesInOutputArgList(PlugInArgList* pOutArgList)
{
   VERIFY(pOutArgList->setPlugInArgValue("Sam Results", mpSamAlg->getResults()));
   mProgress.upALevel(); // make sure the top-level step is successfull
   return true;
}

QDialog* Sam::getGui(void* pAlgData)
{
   // Currently this dialog will be deleted by AlgorithmPattern::execute before it exits. If this
   // changes in the future or the execute method is overridden in Sam, Sam will need to delete
   // mpSamGui.
   mpSamGui = new SamDlg(mpSamAlg->getRasterElement(), this, mpProgress,
      mInputs.mResultsName, mInputs.mbCreatePseudocolor, false, Sam::hasSettingSamHelp(),
      Service<DesktopServices>()->getMainWidget());
   mpSamGui->setWindowTitle("Spectral Angle Mapper");

   return mpSamGui;
}

void Sam::propagateAbort()
{
   if (mpSamGui != NULL)
   {
      mpSamGui->abortSearch();
   }
}

bool Sam::extractFromGui()
{
   if (mpSamGui == NULL)
   {
      return false;
   }
   mInputs.mThreshold = mpSamGui->getThreshold();
   mInputs.mSignatures = mpSamGui->getExtractedSignatures();
   mInputs.mResultsName = mpSamGui->getResultsName();
   mInputs.mpAoi = mpSamGui->getAoi();
   mInputs.mbCreatePseudocolor = mpSamGui->isPseudocolorLayerUsed();

   if (mInputs.mResultsName.empty())
   {
      mInputs.mResultsName = "Sam Results";
   }
   return true;
}

SamAlgorithm::SamAlgorithm(RasterElement* pElement, Progress* pProgress, bool interactive, const BitMask* pAoi) :
               AlgorithmPattern(pElement, pProgress, interactive, pAoi),
               mpResults(NULL),
               mAbortFlag(false)
{
}

bool SamAlgorithm::preprocess()
{
   return true;
}

bool SamAlgorithm::processAll()
{
   FactoryResource<Wavelengths> pWavelengths;

   ProgressTracker progress(getProgress(), "Starting SAM", "spectral", "C4320027-6359-4F5B-8820-8BC72BF1B8F0");
   progress.getCurrentStep()->addProperty("Interactive", isInteractive());

   RasterElement* pElement = getRasterElement();
   if (pElement == NULL)
   {
      progress.report(SAMERR012, 0, ERRORS, true);
      return false;
   }
   progress.getCurrentStep()->addProperty("Cube", pElement->getName());
   const RasterDataDescriptor* pDescriptor = static_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());
   VERIFY(pDescriptor != NULL);

   BitMaskIterator iter(getPixelsToProcess(), pElement);
   unsigned int numRows = iter.getNumSelectedRows();
   unsigned int numColumns = iter.getNumSelectedColumns();
   Opticks::PixelOffset layerOffset(iter.getColumnOffset(), iter.getRowOffset());

   // get cube wavelengths
   DynamicObject* pMetadata = pElement->getMetadata();
   if (pMetadata != NULL)
   {
      pWavelengths->initializeFromDynamicObject(pMetadata, false);
   }

   int sig_index = 0;
   bool bSuccess = true;

   if (mInputs.mSignatures.empty())
   {
      progress.report(SAMERR005, 0, ERRORS, true);
      return false;
   }
   int iSignatureCount = mInputs.mSignatures.size();

   // Get colors for all the signatures
   vector<ColorType> layerColors, excludeColors;
   excludeColors.push_back(ColorType(0, 0, 0));
   excludeColors.push_back(ColorType(255, 255, 255));
   ColorType::getUniqueColors(iSignatureCount, layerColors, excludeColors);

   // Create a vector for the signature names
   vector<string> sigNames;

   // Create a pseudocolor results matrix if necessary
   ModelResource<RasterElement> pPseudocolorMatrix(reinterpret_cast<RasterElement*>(NULL));
   ModelResource<RasterElement> pLowestSAMValueMatrix(reinterpret_cast<RasterElement*>(NULL));
   // Check for multiple Signatures and if the user has selected
   // to combined multiple results in one pseudocolor output layer
   if (iSignatureCount > 1 && mInputs.mbCreatePseudocolor)
   {
      pPseudocolorMatrix = ModelResource<RasterElement>(createResults(numRows, numColumns, mInputs.mResultsName));
      pLowestSAMValueMatrix = ModelResource<RasterElement>(createResults(numRows, numColumns, "LowestSAMValue"));

      if (pPseudocolorMatrix.get() == NULL || pLowestSAMValueMatrix.get() == NULL )
      {
         progress.report(SAMERR007, 0, ERRORS, true);
         return false;
      }

      FactoryResource<DataRequest> pseudoRequest;
      pseudoRequest->setWritable(true);
      string failedDataRequestErrorMessage =
         SpectralUtilities::getFailedDataRequestErrorMessage(pseudoRequest.get(), pPseudocolorMatrix.get());
      DataAccessor pseudoAccessor = pPseudocolorMatrix->getDataAccessor(pseudoRequest.release());
      if (!pseudoAccessor.isValid())
      {
         string msg = "Unable to access results.";
         if (!failedDataRequestErrorMessage.empty())
         {
            msg += "\n" + failedDataRequestErrorMessage;
         }

         progress.report(msg, 0, ERRORS, true);
         return false;
      }

      FactoryResource<DataRequest> lsvRequest;
      lsvRequest->setWritable(true);
      failedDataRequestErrorMessage =
         SpectralUtilities::getFailedDataRequestErrorMessage(lsvRequest.get(), pLowestSAMValueMatrix.get());
      DataAccessor lowestSamValueAccessor = pLowestSAMValueMatrix->getDataAccessor(lsvRequest.release());
      if (!lowestSamValueAccessor.isValid())
      {
         string msg = "Unable to access results.";
         if (!failedDataRequestErrorMessage.empty())
         {
            msg += "\n" + failedDataRequestErrorMessage;
         }

         progress.report(msg, 0, ERRORS, true);
         return false;
      }

      //Lets zero out all the results incase we connect to an existing matrix.
      float* pPseudoValue = NULL;
      float* pLowestValue = NULL;

      for (unsigned int row_ctr = 0; row_ctr < numRows; row_ctr++)
      {
         for (unsigned int col_ctr = 0; col_ctr < numColumns; col_ctr++)
         {
            if (!pseudoAccessor.isValid() || !lowestSamValueAccessor.isValid())
            {
               progress.report("Unable to access results.", 0, ERRORS, true);
               return false;
            }

            pLowestValue = reinterpret_cast<float*>(lowestSamValueAccessor->getColumn());
            pPseudoValue = reinterpret_cast<float*>(pseudoAccessor->getColumn());

            //Initialize the matrices
            *pPseudoValue = 0.0f;
            *pLowestValue = 180.0f;

            pseudoAccessor->nextColumn();
            lowestSamValueAccessor->nextColumn();
         }
         pseudoAccessor->nextRow();
         lowestSamValueAccessor->nextRow();
      }
   }
   ModelResource<RasterElement> pResults(reinterpret_cast<RasterElement*>(NULL));

   // Processes each selected signature one at a time and
   // accumulates results
   for (sig_index = 0; bSuccess && (sig_index < iSignatureCount) && !mAbortFlag; sig_index++)
   {
      // Get the spectrum
      Signature* pSignature = mInputs.mSignatures[sig_index];

      // Create the results matrix
      sigNames.push_back(pSignature->getName());
      std::string rname = mInputs.mResultsName;
      if (iSignatureCount > 1 && !mInputs.mbCreatePseudocolor)
      {
         rname += " " + sigNames.back();
      }
      else if (iSignatureCount > 1)
      {
         rname += "SamTemp";
      }

      if (mInputs.mbCreatePseudocolor == false || pResults.get() == NULL)
      {
         pResults = ModelResource<RasterElement>(createResults(numRows, numColumns, rname));
      }
      if (pResults.get() == NULL)
      {
         bSuccess = false;
         break;
      }

      //Send the message to the progress object
      QString messageSigNumber = QString("Processing Signature %1 of %2 : SAM running on signature %3")
         .arg(sig_index+1).arg(iSignatureCount).arg(QString::fromStdString(sigNames.back()));
      string message = messageSigNumber.toStdString();

      vector<double> spectrumValues;
      vector<int> resampledBands;
      bSuccess = resampleSpectrum(pSignature, spectrumValues, pWavelengths.get(), resampledBands);

      // Check for limited spectral coverage and warning log 
      if (bSuccess && pWavelengths->hasCenterValues() &&
         resampledBands.size() != pWavelengths->getCenterValues().size())
      {
         QString buf = QString("Warning SamAlg014: The spectrum only provides spectral coverage for %1 of %2 bands.")
            .arg(resampledBands.size()).arg(pWavelengths->getCenterValues().size());
         progress.report(buf.toStdString(), 0, WARNING, true);
      }

      if (bSuccess)
      {
         BitMaskIterator iterChecker(getPixelsToProcess(), pElement);

         SamAlgInput samInput(pElement, pResults.get(), spectrumValues, &mAbortFlag, iterChecker, resampledBands);

         //Output Structure
         SamAlgOutput samOutput;

         // Reports current Spectrum SAM is running on
         mta::ProgressObjectReporter reporter(message, getProgress());

         // Initializes all threads
         mta::MultiThreadedAlgorithm<SamAlgInput, SamAlgOutput, SamThread>
            mtaSam(mta::getNumRequiredThreads(numRows),
            samInput, 
            samOutput, 
            &reporter);

         // Calculates spectral angle for current signature
         mtaSam.run();
         if (mAbortFlag)
         {
            progress.report("User aborted the operation.", 0, ABORT, true);
            mAbortFlag = false;
            return false;
         }

         if (samInput.mpResultsMatrix == NULL)
         {
            progress.report(SAMERR006, 0, ERRORS, true);
            return false;
         }

         if (isInteractive() || mInputs.mbDisplayResults)
         {
            if (iSignatureCount > 1 && mInputs.mbCreatePseudocolor)
            {
               // Merges results in to one output layer if a Pseudocolor
               // output layer has been selected
               FactoryResource<DataRequest> pseudoRequest, currentRequest, lowestRequest;
               pseudoRequest->setWritable(true);
               string failedDataRequestErrorMessage =
                  SpectralUtilities::getFailedDataRequestErrorMessage(pseudoRequest.get(), pPseudocolorMatrix.get());
               DataAccessor daPseudoAccessor = pPseudocolorMatrix->getDataAccessor(pseudoRequest.release());
               if (!daPseudoAccessor.isValid())
               {
                  string msg = "Unable to access data.";
                  if (!failedDataRequestErrorMessage.empty())
                  {
                     msg += "\n" + failedDataRequestErrorMessage;
                  }

                  progress.report(msg, 0, ERRORS, true);
                  return false;
               }

               DataAccessor daCurrentAccessor = pResults->getDataAccessor(currentRequest.release());

               lowestRequest->setWritable(true);
               failedDataRequestErrorMessage =
                  SpectralUtilities::getFailedDataRequestErrorMessage(lowestRequest.get(), pLowestSAMValueMatrix.get());
               DataAccessor daLowestSAMValue = pLowestSAMValueMatrix->getDataAccessor(lowestRequest.release());
               if (!daLowestSAMValue.isValid())
               {
                  string msg = "Unable to access data.";
                  if (!failedDataRequestErrorMessage.empty())
                  {
                     msg += "\n" + failedDataRequestErrorMessage;
                  }

                  progress.report(msg, 0, ERRORS, true);
                  return false;
               }

               float* pPseudoValue = NULL;
               float* pCurrentValue = NULL;
               float* pLowestValue = NULL; 

               for (unsigned  int row_ctr = 0; row_ctr < numRows; row_ctr++)
               {
                  for (unsigned  int col_ctr = 0; col_ctr < numColumns; col_ctr++)
                  {
                     if (!daPseudoAccessor.isValid() || !daCurrentAccessor.isValid())
                     {
                        progress.report("Unable to access data.", 0, ERRORS, true);
                        return false;
                     }
                     daPseudoAccessor->toPixel(row_ctr, col_ctr);
                     daCurrentAccessor->toPixel(row_ctr, col_ctr);

                     pPseudoValue = reinterpret_cast<float*>(daPseudoAccessor->getColumn());
                     pCurrentValue = reinterpret_cast<float*>(daCurrentAccessor->getColumn());

                     daLowestSAMValue->toPixel(row_ctr, col_ctr);
                     pLowestValue = reinterpret_cast<float*>(daLowestSAMValue->getColumn());

                     if (*pCurrentValue <= mInputs.mThreshold)
                     {
                        if (*pCurrentValue < *pLowestValue)
                        {
                           *pPseudoValue = sig_index+1;
                           *pLowestValue = *pCurrentValue;
                        }
                     }
                  }
               }
            }
            else
            {
               ColorType color;
               if (sig_index <= static_cast<int>(layerColors.size()))
               {
                  color = layerColors[sig_index];
               }

               double dMaxValue = pResults->getStatistics()->getMax();

               // Displays results for current signature
               displayThresholdResults(pResults.release(), color, LOWER, mInputs.mThreshold, dMaxValue, layerOffset);
            }
         }
      }
   } //End of Signature Loop Counter

   if (bSuccess && !mAbortFlag)
   {
      // Displays final Pseudocolor output layer results
      if ((isInteractive() || mInputs.mbDisplayResults) && iSignatureCount > 1 && mInputs.mbCreatePseudocolor)
      {
         displayPseudocolorResults(pPseudocolorMatrix.release(), sigNames, layerOffset);
      }
   }

   // Aborts gracefully after clean up
   if (mAbortFlag)
   {
      progress.report("User aborted the operation.", 0, ABORT, true);
      mAbortFlag = false;
      return false;
   }

   if (bSuccess)
   {
      if (pPseudocolorMatrix.get() != NULL)
      {
         mpResults = pPseudocolorMatrix.get();
         mpResults->updateData();
      }
      else if (pResults.get() != NULL)
      {
         mpResults = pResults.get();
         mpResults->updateData();
      }
      else
      {
         progress.report(SAMERR016, 0, ERRORS, true);
         return false;
      }
      progress.report(SAMNORM200, 100, NORMAL);
   }

   progress.getCurrentStep()->addProperty("Display Layer", mInputs.mbDisplayResults);
   progress.getCurrentStep()->addProperty("Threshold", mInputs.mThreshold);
   progress.upALevel();

   return bSuccess;
}

bool SamAlgorithm::resampleSpectrum(Signature* pSignature, vector<double>& resampledAmplitude,
                                    Wavelengths* pWavelengths, vector<int>& resampledBands)
{
   StepResource pStep("Resample Signature", "spectral", "E1C6F0EA-4D00-4B0E-851F-F677A479169D");

   Progress* pProgress = getProgress();
   if ((pWavelengths == NULL) || (pWavelengths->isEmpty()))
   {
      // Check for an in-scene signature
      RasterElement* pElement = getRasterElement();
      VERIFY(pElement != NULL);

      if (pSignature->getParent() == pElement)
      {
         vector<double> sigReflectances =
            dv_cast<vector<double> >(pSignature->getData("Reflectance"), vector<double>());
         resampledAmplitude = sigReflectances;

         resampledBands.clear();
         for (vector<double>::size_type i = 0; i < sigReflectances.size(); ++i)
         {
            resampledBands.push_back(static_cast<int>(i));
         }

         pStep->finalize(Message::Success);
         return true;
      }

      string messageText = "The data set wavelengths are invalid.";
      if (pProgress != NULL) pProgress->updateProgress(messageText, 0, ERRORS);
      pStep->finalize(Message::Failure, messageText);
      return false;
   }

   vector<double> fwhm = pWavelengths->getFwhm();
   PlugInResource resampler("Resampler");
   Resampler* pResampler = dynamic_cast<Resampler*>(resampler.get());
   if (pResampler == NULL)
   {
      string messageText = "The resampler plug-in could not be created.";
      if (pProgress != NULL) pProgress->updateProgress(messageText, 0, ERRORS);
      pStep->finalize(Message::Failure, messageText);
      return false;
   }
   string err;
   try
   {
      vector<double> sigReflectance = dv_cast<vector<double> >(pSignature->getData("Reflectance"));
      resampledAmplitude.reserve(sigReflectance.size());
      resampledBands.reserve(sigReflectance.size());
      if (!pResampler->execute(sigReflectance,
                              resampledAmplitude,
                              dv_cast<vector<double> >(pSignature->getData("Wavelength")),
                              pWavelengths->getCenterValues(),
                              fwhm,
                              resampledBands,
                              err))
      {
         string messageText = "Resampling failed: " + err;
         if (pProgress != NULL) pProgress->updateProgress(messageText, 0, ERRORS);
         pStep->finalize(Message::Failure, messageText);
         return false;
      }
   }
   catch(std::bad_cast&)
   {
      string messageText = "Resampling failed: " + err;
      if (pProgress != NULL) pProgress->updateProgress(messageText, 0, ERRORS);
      pStep->finalize(Message::Failure, messageText);
      return false;
   }

   pStep->finalize(Message::Success);
   return true;
}

RasterElement* SamAlgorithm::createResults(int numRows, int numColumns, const string& sigName)
{
   RasterElement* pElement = getRasterElement();
   if (pElement == NULL)
   {
      return NULL;
   }

   // Delete an existing element to ensure that the new results element is the correct size
   Service<ModelServices> pModel;

   RasterElement* pExistingResults = static_cast<RasterElement*>(pModel->getElement(sigName,
      TypeConverter::toString<RasterElement>(), pElement));
   if (pExistingResults != NULL)
   {
      pModel->destroyElement(pExistingResults);
   }

   // Create the new results element
   ModelResource<RasterElement> pResults(RasterUtilities::createRasterElement(sigName, numRows, numColumns,
      FLT4BYTES, true, pElement));
   if (pResults.get() == NULL)
   {
      pResults = ModelResource<RasterElement>(RasterUtilities::createRasterElement(sigName, numRows, numColumns,
         FLT4BYTES, false, pElement));
      if (pResults.get() == NULL)
      {
         reportProgress(ERRORS, 0, SAMERR009);
         MessageResource(SAMERR009, "spectral", "C89D361B-DB12-43ED-B276-6D98CA3539EE");
         return NULL;
      }
   }

   FactoryResource<Units> pUnits;
   pUnits->setUnitName("degrees");

   vector<int> badValues(1, 181);

   RasterDataDescriptor* pResultsDescriptor = static_cast<RasterDataDescriptor*>(pResults->getDataDescriptor());
   VERIFYRV(pResultsDescriptor != NULL, NULL);
   pResultsDescriptor->setUnits(pUnits.get());
   pResultsDescriptor->setBadValues(badValues);

   Statistics* pStatistics = pResults->getStatistics();
   VERIFYRV(pStatistics != NULL, NULL);
   pStatistics->setBadValues(badValues);

   return pResults.release();
}

bool SamAlgorithm::postprocess()
{
   return true;
}

bool SamAlgorithm::initialize(void* pAlgorithmData)
{
   bool bSuccess = true;
   if (pAlgorithmData != NULL)
   {
      mInputs = *reinterpret_cast<SamInputs*>(pAlgorithmData);
   }

   if (mInputs.mSignatures.empty())
   {
      reportProgress(ERRORS, 0, SAMERR011);
      MessageResource(SAMERR011, "spectral", "07592D6A-50B9-48D3-86FD-329608F6537B");
      bSuccess = false;
   }

   const BitMask* pAoi = NULL;
   if (mInputs.mpAoi != NULL)
   {
      pAoi = mInputs.mpAoi->getSelectedPoints();
   }
   setRoi(pAoi);

   return bSuccess;
}

RasterElement* SamAlgorithm::getResults() const
{
   return mpResults;
}

bool SamAlgorithm::canAbort() const
{
   return true;
}

bool SamAlgorithm::doAbort()
{
   mAbortFlag = true;
   return true;
}

SamThread::SamThread(const SamAlgInput& input, 
                     int threadCount, 
                     int threadIndex, 
                     mta::ThreadReporter& reporter) : mta::AlgorithmThread(threadIndex, reporter), 
                        mInput(input),
                        mRowRange(getThreadRange(threadCount, input.mIterCheck.getNumSelectedRows()))
{
   if (input.mIterCheck.useAllPixels())
   {
      mRowRange = getThreadRange(threadCount, static_cast<const RasterDataDescriptor*>(
         input.mpCube->getDataDescriptor())->getRowCount());
   }
}

void SamThread::run()
{
   EncodingType encoding = static_cast<const RasterDataDescriptor*>(
         mInput.mpCube->getDataDescriptor())->getDataType();
   switchOnEncoding(encoding, SamThread::ComputeSam, NULL);
}

template<class T>
void SamThread::ComputeSam(const T* pDummyData)
{
   int reSamBan_index = 0, row_index = 0, col_index = 0;
   float* pResultsData = NULL;
   int oldPercentDone = -1;
   double spectrumMag = 0.0;
   const T* pData=NULL;
   const RasterDataDescriptor* pDescriptor = static_cast<const RasterDataDescriptor*>(
      mInput.mpCube->getDataDescriptor());
   unsigned int numCols = pDescriptor->getColumnCount();
   unsigned int numBands = pDescriptor->getBandCount();
   unsigned int numRows = (mRowRange.mLast - mRowRange.mFirst + 1);

   int numResultsCols = 0;
   //Sets area to apply the SAM algortihm to. Either
   //the entire cube, or a selected ROI.
   if (mInput.mIterCheck.useAllPixels())
   {
      //Total number of Columns in cube.
      numResultsCols = numCols;
   }
   else
   {
      numResultsCols = mInput.mIterCheck.getNumSelectedColumns();
   }

   if (mInput.mpResultsMatrix == NULL)
   {
      return;
   }

   const RasterDataDescriptor* pResultDescriptor = static_cast<const RasterDataDescriptor*>(
      mInput.mpResultsMatrix->getDataDescriptor());
   // Gets results matrix that was initialized in ProcessAll()
   mRowRange.mFirst = std::max(0, mRowRange.mFirst);
   mRowRange.mLast = std::min(mRowRange.mLast, static_cast<int>(pDescriptor->getRowCount()) - 1);
   FactoryResource<DataRequest> pResultRequest;
   pResultRequest->setRows(pResultDescriptor->getActiveRow(mRowRange.mFirst),
      pResultDescriptor->getActiveRow(mRowRange.mLast));
   pResultRequest->setColumns(pResultDescriptor->getActiveColumn(0),
      pResultDescriptor->getActiveColumn(numResultsCols - 1));
   pResultRequest->setWritable(true);
   DataAccessor resultAccessor = mInput.mpResultsMatrix->getDataAccessor(pResultRequest.release());
   if (!resultAccessor.isValid())
   {
      return;
   }

   // Resamples and sets search signature 
   for (reSamBan_index = 0; reSamBan_index < (int) mInput.mResampledBands.size(); ++reSamBan_index)
   {
      spectrumMag += mInput.mSpectrum[reSamBan_index] * mInput.mSpectrum[reSamBan_index];
   }

   spectrumMag = sqrt(spectrumMag);
   int rowOffset = mInput.mIterCheck.getOffset().mY;
   int startRow = (mRowRange.mFirst + rowOffset);
   int stopRow = (mRowRange.mLast + rowOffset);

   int columnOffset = mInput.mIterCheck.getOffset().mX;
   int startColumn = columnOffset;
   int stopColumn = (numResultsCols + columnOffset - 1);

   FactoryResource<DataRequest> pRequest;
   pRequest->setInterleaveFormat(BIP);
   pRequest->setRows(pDescriptor->getActiveRow(startRow), pDescriptor->getActiveRow(stopRow));
   pRequest->setColumns(pDescriptor->getActiveColumn(startColumn), pDescriptor->getActiveColumn(stopColumn));
   DataAccessor accessor = mInput.mpCube->getDataAccessor(pRequest.release());
   if (!accessor.isValid())
   {
      return;
   }

   for (row_index = startRow; row_index <= stopRow; ++row_index)
   {
      int percentDone = mRowRange.computePercent(row_index-rowOffset);
      if (percentDone > oldPercentDone)
      {
         oldPercentDone = percentDone;
         getReporter().reportProgress(getThreadIndex(), percentDone);
      }
      if (mInput.mpAbortFlag != NULL && *mInput.mpAbortFlag)
      {
         break;
      }

      for (col_index = startColumn; col_index <= stopColumn; ++col_index)
      {  
         VERIFYNRV(resultAccessor.isValid());
         VERIFYNRV(accessor.isValid());
         // Pointer to results data
         pResultsData = reinterpret_cast<float*>(resultAccessor->getColumn());
         if (pResultsData == NULL)
         {
            return;
         }
         if (mInput.mIterCheck.getPixel(col_index, row_index))
         {
            //Pointer to cube/sensor data
            pData = reinterpret_cast<T*>(accessor->getColumn());
            VERIFYNRV(pData != NULL);
            *pResultsData = 0.0f;
            double pixelMag = 0.0;
            double angle =0.0;

            //Calculates Spectral Angle and Magnitude at current location
            for (unsigned int reSam_index = 0; 
               reSam_index < mInput.mResampledBands.size(); ++reSam_index)
            {
               int resampledBand = mInput.mResampledBands[reSam_index];
               double cubeVal = pData[resampledBand];
               angle += cubeVal * mInput.mSpectrum[reSam_index];
               pixelMag += cubeVal * cubeVal;
            }
            pixelMag = sqrt(pixelMag);
            if (pixelMag != 0.0 && spectrumMag != 0.0)
            {
               angle /= (pixelMag * spectrumMag);
               if (angle < -1.0)
               {
                  angle = -1.0;
               }
               if (angle > 1.0)
               {
                  angle = 1.0;
               }

               angle = (180.0 / 3.141592654) * acos(angle);
               *pResultsData = angle;
            }
            else
            {
               *pResultsData = 181.0;
            }
         }
         else
         {
            *pResultsData = 181.0;
         }
         //Increment Columns
         resultAccessor->nextColumn();
         accessor->nextColumn();
      }
      //Increment Rows
      resultAccessor->nextRow();
      accessor->nextRow();
   }
}

/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

// Keep this include here..it uses an OpenCV macro X.
// Moving this after Opticks includes will incorrectly use the Xerces X macro.
#include <cstddef>
#include <opencv/cv.h>

#include "Ace.h"
#include "AceDlg.h"
#include "AceErr.h"
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
#include "MatrixFunctions.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "ProgressTracker.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "Resampler.h"
#include "Signature.h"
#include "SpectralUtilities.h"
#include "SpectralVersion.h"
#include "Statistics.h"
#include "switchOnEncoding.h"
#include "Units.h"
#include "Wavelengths.h"

#include <vector>

using namespace std;

REGISTER_PLUGIN_BASIC(SpectralAce, Ace);

Ace::Ace() : AlgorithmPlugIn(&mInputs), mpAceGui(NULL), mpAceAlg(NULL), mpProgress(NULL)
{
   setDescriptorId("{D9AE3D28-CFC4-4247-849D-D25FC820C2F1}");
   setName("ACE");
   setCreator("Ball Aerospace & Technologies Corp.");
   setShortDescription("Adaptive Cosine Estimator");
   setDescription("Make use of the Adaptive Cosine Estimator results for material " 
      "identification against signatures or AOIs");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setMenuLocation("[Spectral]\\Material ID\\ACE");
   setAbortSupported(true);
}

Ace::~Ace()
{}

bool Ace::populateBatchInputArgList(PlugInArgList* pInArgList)
{
   VERIFY(pInArgList != NULL);
   VERIFY(pInArgList->addArg<Progress>(Executable::ProgressArg(), NULL, Executable::ProgressArgDescription()));
   VERIFY(pInArgList->addArg<RasterElement>(Executable::DataElementArg(), 
      "Raster element on which ACE will be performed."));
   VERIFY(pInArgList->addArg<Signature>("Target Signatures", NULL, "Signatures that will be used by ACE."));
   VERIFY(pInArgList->addArg<double>("Threshold", mInputs.mThreshold,
      "Value of pixels to be flagged by default in the threshold layer resulting from the ACE operation."));
   VERIFY(pInArgList->addArg<AoiElement>("AOI", mInputs.mpAoi,
      "Area of interest over which ACE will be performed. If not specified, the entire cube is used in processing."));
   VERIFY(pInArgList->addArg<bool>("Display Results", mInputs.mbDisplayResults,
      "Flag representing whether to display the results of the ACE operation."));
   VERIFY(pInArgList->addArg<string>("Results Name", mInputs.mResultsName,
      "Name of the raster element resulting from the ACE operation."));
   return true;
}

bool Ace::populateInteractiveInputArgList(PlugInArgList* pInArgList)
{
   VERIFY(pInArgList != NULL);
   VERIFY(pInArgList->addArg<Progress>(Executable::ProgressArg(), NULL, Executable::ProgressArgDescription()));
   VERIFY(pInArgList->addArg<RasterElement>(Executable::DataElementArg(), NULL, "Raster element on which ACE will be performed."));
   return true;
}

bool Ace::populateDefaultOutputArgList(PlugInArgList* pOutArgList)
{
   VERIFY(pOutArgList->addArg<RasterElement>("ACE Results", NULL, "Raster element resulting from the ACE operation."));
   return true;
}

bool Ace::parseInputArgList(PlugInArgList* pInArgList)
{
   mpProgress = pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg());
   mProgress = ProgressTracker(mpProgress, "Adaptive Cosine Estimator",
                              "spectral", "0883C3D0-8B82-435B-92AD-5BF90A3AD39F");
   RasterElement* pElement = pInArgList->getPlugInArgValue<RasterElement>(Executable::DataElementArg());
   if (pElement == NULL)
   {
      mProgress.report(ACEERR001, 0, ERRORS, true);
      return false;
   }

   const RasterDataDescriptor* pElementDescriptor = dynamic_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());
   EncodingType dataType = pElementDescriptor->getDataType();
   if (dataType == INT4SCOMPLEX || dataType == FLT8COMPLEX)
   {
      mProgress.report(ACEERR008, 0, ERRORS, true);
      return false;
   }

   // sensor is non-null and only one band -> bail out!
   if (pElementDescriptor->getBandCount() == 1)
   {
      mProgress.report(ACEERR009, 0, ERRORS, true);
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
   mpAceAlg = new AceAlgorithm(pElement, mpProgress, isInteractive(), pBitMask);
   setAlgorithmPattern(Resource<AlgorithmPattern>(mpAceAlg));
   return true;
}

bool Ace::setActualValuesInOutputArgList(PlugInArgList* pOutArgList)
{
   VERIFY(pOutArgList->setPlugInArgValue("ACE Results", mpAceAlg->getResults()));
   mProgress.upALevel(); // make sure the top-level step is successfull
   return true;
}

QDialog* Ace::getGui(void* pAlgData)
{
   // Currently this dialog will be deleted by AlgorithmPattern::execute before it exits. If this
   // changes in the future or the execute method is overridden in Ace, Ace will need to delete
   // mpAceGui.
   mpAceGui = new AceDlg(mpAceAlg->getRasterElement(), this, mpProgress,
      mInputs.mResultsName, mInputs.mbCreatePseudocolor, false, Ace::hasSettingAceHelp(), mInputs.mThreshold,
      Service<DesktopServices>()->getMainWidget());
   mpAceGui->setWindowTitle("Adaptive Cosine Estimator");

   return mpAceGui;
}

void Ace::propagateAbort()
{
   if (mpAceGui != NULL)
   {
      mpAceGui->abortSearch();
   }
}

bool Ace::extractFromGui()
{
   if (mpAceGui == NULL)
   {
      return false;
   }
   mInputs.mThreshold = mpAceGui->getThreshold();
   mInputs.mSignatures = mpAceGui->getExtractedSignatures();
   mInputs.mResultsName = mpAceGui->getResultsName();
   mInputs.mpAoi = mpAceGui->getAoi();
   mInputs.mbCreatePseudocolor = mpAceGui->isPseudocolorLayerUsed();

   if (mInputs.mResultsName.empty())
   {
      mInputs.mResultsName = "ACE Results";
   }
   return true;
}

AceAlgorithm::AceAlgorithm(RasterElement* pElement, Progress* pProgress, bool interactive, const BitMask* pAoi) :
               AlgorithmPattern(pElement, pProgress, interactive, pAoi),
               mpResults(NULL),
               mAbortFlag(false)
{}

bool AceAlgorithm::preprocess()
{
   return true;
}

bool AceAlgorithm::processAll()
{
   FactoryResource<Wavelengths> pWavelengths;

   ProgressTracker progress(getProgress(), "Starting ACE", "spectral", "61BA4D59-B418-4467-85D8-62419A1B3249");
   progress.getCurrentStep()->addProperty("Interactive", isInteractive());

   RasterElement* pElement = getRasterElement();
   if (pElement == NULL)
   {
      progress.report(ACEERR007, 0, ERRORS, true);
      return false;
   }
   progress.getCurrentStep()->addProperty("Cube", pElement->getName());
   const RasterDataDescriptor* pDescriptor = static_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());
   VERIFY(pDescriptor != NULL);

   BitMaskIterator iter(getPixelsToProcess(), pElement);
   unsigned int numRows = iter.getNumSelectedRows();
   unsigned int numColumns = iter.getNumSelectedColumns();
   unsigned int numBands = pDescriptor->getBandCount();
   unsigned int startRow = iter.getRowOffset();
   unsigned int startCol = iter.getColumnOffset();
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
      progress.report(ACEERR002, 0, ERRORS, true);
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
   ModelResource<RasterElement> pHighestAceValueMatrix(reinterpret_cast<RasterElement*>(NULL));

   // Check for multiple Signatures and if the user has selected
   // to combined multiple results in one pseudocolor output layer
   if (iSignatureCount > 1 && mInputs.mbCreatePseudocolor)
   {
      pPseudocolorMatrix = ModelResource<RasterElement>(createResults(numRows, numColumns, 1, mInputs.mResultsName));
      pHighestAceValueMatrix = ModelResource<RasterElement>(createResults(numRows, numColumns, 1, "HighestACEValue"));

      if (pPseudocolorMatrix.get() == NULL || pHighestAceValueMatrix.get() == NULL )
      {
         progress.report(ACEERR004, 0, ERRORS, true);
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

      FactoryResource<DataRequest> havRequest;
      havRequest->setWritable(true);
      failedDataRequestErrorMessage =
         SpectralUtilities::getFailedDataRequestErrorMessage(havRequest.get(), pHighestAceValueMatrix.get());
      DataAccessor highestAceValueAccessor = pHighestAceValueMatrix->getDataAccessor(havRequest.release());
      if (!highestAceValueAccessor.isValid())
      {
         string msg = "Unable to access results.";
         if (!failedDataRequestErrorMessage.empty())
         {
            msg += "\n" + failedDataRequestErrorMessage;
         }

         progress.report(msg, 0, ERRORS, true);
         return false;
      }

      //Lets zero out all the results in case we connect to an existing matrix.
      float* pPseudoValue = NULL;
      float* pHighestValue = NULL;

      for (unsigned int row_ctr = 0; row_ctr < numRows; row_ctr++)
      {
         for (unsigned int col_ctr = 0; col_ctr < numColumns; col_ctr++)
         {
            if (!pseudoAccessor.isValid() || !highestAceValueAccessor.isValid())
            {
               progress.report("Unable to access results.", 0, ERRORS, true);
               return false;
            }

            pHighestValue = reinterpret_cast<float*>(highestAceValueAccessor->getColumn());
            pPseudoValue = reinterpret_cast<float*>(pseudoAccessor->getColumn());

            //Initialize the matrices
            *pPseudoValue = 0.0f;
            *pHighestValue = 0.0f;

            pseudoAccessor->nextColumn();
            highestAceValueAccessor->nextColumn();
         }
         pseudoAccessor->nextRow();
         highestAceValueAccessor->nextRow();
      }
   }
   
   bool success = true;
   ExecutableResource covar("Covariance", std::string(), progress.getCurrentProgress(), !isInteractive());
   success &= covar->getInArgList().setPlugInArgValue(Executable::DataElementArg(), pElement); 
   bool bInverse = true;
   success &= covar->getInArgList().setPlugInArgValue("ComputeInverse", &bInverse);
   success &= covar->execute();
   RasterElement* pCov = static_cast<RasterElement*>(
      Service<ModelServices>()->getElement("Covariance Matrix", TypeConverter::toString<RasterElement>(), pElement));
   RasterElement* pInvCov = static_cast<RasterElement*>(
      Service<ModelServices>()->getElement("Inverse Covariance Matrix", TypeConverter::toString<RasterElement>(), pElement));
   RasterElement* pMeans = static_cast<RasterElement*>(
      Service<ModelServices>()->getElement("Means", TypeConverter::toString<RasterElement>(), pElement));

   // with a small means vector (generally hundreds of doubles) there won't be a problem with getRawData()
   success &= pCov != NULL && pInvCov != NULL && pMeans != NULL && pMeans->getRawData();
   if (!success)
   {
      progress.report("Unable to calculate covariance.", 0, ERRORS, true);
      return false;
   }

   const RasterDataDescriptor* pMeansDesc = static_cast<const RasterDataDescriptor*>(pMeans->getDataDescriptor());
   if (pMeansDesc->getRowCount() != 1 || pMeansDesc->getColumnCount() != 1 || pMeansDesc->getBandCount() != numBands)
   {
      progress.report(ACEERR011, 0, ABORT, true);
      mAbortFlag = false;
      return false;
   }
   //store the mean of the dataset we processing
   cv::Mat muMat = cv::Mat(1, numBands, CV_64F, pMeans->getRawData());
   if (static_cast<const RasterDataDescriptor*>(pCov->getDataDescriptor())->getDataType() != FLT8BYTES ||
       static_cast<const RasterDataDescriptor*>(pInvCov->getDataDescriptor())->getDataType() != FLT8BYTES)
   {
      progress.report("Invalid covariance matrix.", 0, ERRORS, true);
      return false;
   }
   cv::Mat invCovMat = cv::Mat(numBands, numBands, CV_64F, pInvCov->getRawData());  

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
         rname += "AceTemp";
      }

      if (mInputs.mbCreatePseudocolor == false || pResults.get() == NULL)
      {
         pResults = ModelResource<RasterElement>(createResults(numRows, numColumns, 1, rname));
      }
      if (pResults.get() == NULL)
      {
         bSuccess = false;
         break;
      }

      //Send the message to the progress object
      QString messageSigNumber = QString("Processing Signature %1 of %2 : ACE running on signature %3")
         .arg(sig_index+1).arg(iSignatureCount).arg(QString::fromStdString(sigNames.back()));
      string message = messageSigNumber.toStdString();

      vector<double> spectrumValues;
      vector<int> resampledBands;
      bSuccess = resampleSpectrum(pSignature, spectrumValues, pWavelengths.get(), resampledBands);

      // Check for limited spectral coverage and warning log 
      if (bSuccess && pWavelengths->hasCenterValues() &&
         resampledBands.size() != pWavelengths->getCenterValues().size())
      {
         QString buf = QString("Warning AceAlg014: The spectrum only provides spectral coverage for %1 of %2 bands.")
            .arg(resampledBands.size()).arg(pWavelengths->getCenterValues().size());
         progress.report(buf.toStdString(), 0, WARNING, true);
      }

      if (bSuccess)
      {
         BitMaskIterator iterChecker(getPixelsToProcess(), pElement);
         RasterElement* pResult = NULL;

         //subtract the mean from the signature
         cv::Mat spectrum(spectrumValues.size(), 1, CV_64F);
         for (unsigned int i = 0; i < spectrumValues.size(); i++)
         {
            spectrum.at<double>(i, 0) = spectrumValues[i] - muMat.at<double>(0, resampledBands[i]);
         }
         cv::Mat invCovMatSubset(resampledBands.size(), resampledBands.size(), CV_64F);
         for (int i_idx = 0; i_idx < resampledBands.size(); ++i_idx)
         {
            for (int j_idx = 0; j_idx < resampledBands.size(); ++j_idx)
            {
               invCovMatSubset.at<double>(i_idx, j_idx) = invCovMat.at<double>(resampledBands[i_idx], resampledBands[j_idx]);
            }
         }
         cv::Mat spectrumTerm = spectrum.t() * invCovMatSubset * spectrum;
         cv::sqrt(spectrumTerm, spectrumTerm);
         AceAlgInput aceInput(pElement, pResults.get(), spectrum, &mAbortFlag, iterChecker, resampledBands, 
            muMat, invCovMatSubset, spectrumTerm);

         //Output Structure
         AceAlgOutput aceOutput;

         // Reports current Spectrum ACE is running on
         mta::ProgressObjectReporter reporter(message, getProgress());

         // Initializes all threads
         mta::MultiThreadedAlgorithm<AceAlgInput, AceAlgOutput, AceThread>
            mtaAce(mta::getNumRequiredThreads(numRows),
            aceInput, 
            aceOutput, 
            &reporter);

         // Calculates ACE for current signature
         mtaAce.run();
         if (mAbortFlag)
         {
            progress.report(ACEABORT000, 0, ABORT, true);
            mAbortFlag = false;
            return false;
         }
         if (aceInput.mpResultsMatrix == NULL)
         {
            progress.report(ACEERR003, 0, ERRORS, true);
            return false;
         }

         if (isInteractive() || mInputs.mbDisplayResults)
         {
            if (iSignatureCount > 1 && mInputs.mbCreatePseudocolor)
            {
               // Merges results in to one output layer if a Pseudocolor
               // output layer has been selected
               FactoryResource<DataRequest> pseudoRequest, currentRequest, highestRequest;
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

               highestRequest->setWritable(true);
               failedDataRequestErrorMessage =
                  SpectralUtilities::getFailedDataRequestErrorMessage(highestRequest.get(), pHighestAceValueMatrix.get());
               DataAccessor daHighestAceValue = pHighestAceValueMatrix->getDataAccessor(highestRequest.release());
               if (!daHighestAceValue.isValid())
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
               float* pHighestValue = NULL; 

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

                     daHighestAceValue->toPixel(row_ctr, col_ctr);
                     pHighestValue = reinterpret_cast<float*>(daHighestAceValue->getColumn());

                     if (*pCurrentValue >= mInputs.mThreshold)
                     {
                        if (*pCurrentValue > *pHighestValue)
                        {
                           *pPseudoValue = sig_index + 1;
                           *pHighestValue = *pCurrentValue;
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
               displayThresholdResults(pResults.release(), color, UPPER, mInputs.mThreshold, dMaxValue, layerOffset);
            }
         }
         else
         {
            pResults.release();
         }
      }
   } //End of Signature Loop Counter

   if (bSuccess && !mAbortFlag)
   {
      // Displays final Pseudocolor output layer results
      if ((isInteractive() || mInputs.mbDisplayResults) && iSignatureCount > 1 && mInputs.mbCreatePseudocolor)
      {
         displayPseudocolorResults(pPseudocolorMatrix.get(), sigNames, layerOffset);
      }
      pPseudocolorMatrix.release();
   }

   // Aborts gracefully after clean up
   if (mAbortFlag)
   {
      progress.report(ACEABORT000, 0, ABORT, true);
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
         progress.report(ACEERR010, 0, ERRORS, true);
         return false;
      }
      progress.report(ACENORM200, 100, NORMAL);
   }

   progress.getCurrentStep()->addProperty("Display Layer", mInputs.mbDisplayResults);
   progress.getCurrentStep()->addProperty("Threshold", mInputs.mThreshold);
   progress.upALevel();

   return bSuccess;
}

bool AceAlgorithm::resampleSpectrum(Signature* pSignature, vector<double>& resampledAmplitude,
                                    Wavelengths* pWavelengths, vector<int>& resampledBands)
{
   StepResource pStep("Resample Signature", "spectral", "C3B4BCAB-064E-4D9C-8EC0-0199F74AF0E0");

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

RasterElement* AceAlgorithm::createResults(int numRows, int numColumns, int numBands, const string& sigName)
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
      numBands, FLT4BYTES, BIP, true, pElement));
   if (pResults.get() == NULL)
   {
      pResults = ModelResource<RasterElement>(RasterUtilities::createRasterElement(sigName, numRows, numColumns, 
         numBands, FLT4BYTES, BIP, false, pElement));
      if (pResults.get() == NULL)
      {
         reportProgress(ERRORS, 0, ACEERR005);
         MessageResource(ACEERR005, "spectral", "D7434620-8E59-4603-99D7-E5EA60A23CFA");
         return NULL;
      }
   }

   RasterDataDescriptor* pResultsDescriptor = static_cast<RasterDataDescriptor*>(pResults->getDataDescriptor());
   return pResults.release();
}

bool AceAlgorithm::postprocess()
{
   return true;
}

bool AceAlgorithm::initialize(void* pAlgorithmData)
{
   bool bSuccess = true;
   if (pAlgorithmData != NULL)
   {
      mInputs = *reinterpret_cast<AceInputs*>(pAlgorithmData);
   }

   if (mInputs.mSignatures.empty())
   {
      reportProgress(ERRORS, 0, ACEERR006);
      MessageResource(ACEERR006, "spectral", "B98A3391-4DCB-497B-AFCB-79363FCF2760");
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

RasterElement* AceAlgorithm::getResults() const
{
   return mpResults;
}

bool AceAlgorithm::canAbort() const
{
   return true;
}

bool AceAlgorithm::doAbort()
{
   mAbortFlag = true;
   return true;
}

AceThread::AceThread(const AceAlgInput& input, 
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

void AceThread::run()
{
   EncodingType encoding = static_cast<const RasterDataDescriptor*>(
         mInput.mpCube->getDataDescriptor())->getDataType();
   switchOnEncoding(encoding, AceThread::ComputeAce, NULL);
}

template<class T>
void AceThread::ComputeAce(const T* pDummyData)
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

   //Sets area to apply the ACE algortihm to. Either
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

   int rowOffset = mInput.mIterCheck.getOffset().mY;
   int startRow = (mRowRange.mFirst + rowOffset);
   int stopRow = (mRowRange.mLast + rowOffset);

   int columnOffset = mInput.mIterCheck.getOffset().mX;
   int startColumn = columnOffset;
   int stopColumn = (numResultsCols + columnOffset - 1);

   const Units* pUnits = pDescriptor->getUnits();
   double unitScale = (pUnits == NULL) ? 1.0 : pUnits->getScaleFromStandard();

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
            cv::Mat dataSpectrum(mInput.mResampledBands.size(), 1, CV_64F);
            for (unsigned int reAce_index = 0; 
               reAce_index < mInput.mResampledBands.size(); ++reAce_index)
            {
               int resampledBand = mInput.mResampledBands[reAce_index];
               dataSpectrum.at<double>(reAce_index, 0) = (unitScale * pData[resampledBand]) - mInput.mMuMat.at<double>(resampledBand);
            }
            
            //Coherent ACE description from paper: doi:10.1117/12.893950
            //\sigma = covariance matrix of scene (should be minus anomalies)
            //\mu_b = means of scene (using same subset as \sigma)
            //S = s - \mu_b
            //X = x - \mu_b
            //y = \frac{S^T * \sigma^-1 * X}{\sqrt{S^T * \sigma^-1 * S} * \sqrt{X^T * \sigma^-1 * X}}
            cv::Mat numerator = dataSpectrum.t() * mInput.mCovMat * mInput.mSpectrum;
            cv::Mat dataTerm = dataSpectrum.t() * mInput.mCovMat * dataSpectrum;
            cv::sqrt(dataTerm, dataTerm);
            cv::Mat denominator = mInput.mSpectrumTerm * dataTerm;
            if (denominator.at<double>(0) - 0.0 > std::numeric_limits<double>::epsilon())
            {
               cv::Mat result = numerator / denominator;
               *pResultsData = result.at<double>(0);
            }
            else
            {
               *pResultsData = 0.0;
            }
         }
         else
         {
            *pResultsData = 0.0;
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

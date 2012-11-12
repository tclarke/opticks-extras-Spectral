/*
 * The information in this file is
 * Copyright(c) 2012 WangBovik Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from
 * http://www.gnu.org/licenses/lgpl.html
 */

// Keep this include here..it uses an OpenCV macro X.
// Moving this after Opticks includes will incorrectly use the Xerces X macro.
#include <cstddef>
#include <opencv/cv.h>

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
#include "WangBovik.h"
#include "WangBovikDlg.h"
#include "WangBovikErr.h"
#include "Wavelengths.h"

#include <limits>
#include <vector>

REGISTER_PLUGIN_BASIC(SpectralWangBovik, WangBovik);

namespace
{
   const float wbiBadValue(-99.0f);
};

WangBovik::WangBovik() : AlgorithmPlugIn(&mInputs), mpWangBovikGui(NULL), mpWangBovikAlg(NULL), mpProgress(NULL)
{
   setDescriptorId("{13B80A8B-4F53-48A3-8CDF-2DDDCDFC6229}");
   setName("Wang-Bovik Index");
   setCreator("Ball Aerospace & Technologies Corp.");
   setShortDescription("Wang-Bovik Image Quality Index");
   setDescription("Make use of the Wang-Bovik Image Quality Index results for material "
      "identification against signatures or AOIs");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setMenuLocation("[Spectral]\\Material ID\\Wang-Bovik Index");
   setAbortSupported(true);
}

WangBovik::~WangBovik()
{}

bool WangBovik::populateBatchInputArgList(PlugInArgList* pInArgList)
{
   VERIFY(pInArgList != NULL);
   VERIFY(pInArgList->addArg<Progress>(Executable::ProgressArg(), NULL, Executable::ProgressArgDescription()));
   VERIFY(pInArgList->addArg<RasterElement>(Executable::DataElementArg(),
      "Raster element on which WBI will be performed."));
   VERIFY(pInArgList->addArg<Signature>("Target Signatures", NULL, "Signatures that will be used by WBI."));
   VERIFY(pInArgList->addArg<double>("Threshold", mInputs.mThreshold,
      "Value of pixels to be flagged by default in the threshold layer resulting from the WBI operation."));
   VERIFY(pInArgList->addArg<AoiElement>("AOI", mInputs.mpAoi,
      "Area of interest over which WBI will be performed. If not specified, the entire cube is used in processing."));
   VERIFY(pInArgList->addArg<bool>("Display Results", mInputs.mbDisplayResults,
      "Flag representing whether to display the results of the WBI operation."));
   VERIFY(pInArgList->addArg<string>("Results Name", mInputs.mResultsName,
      "Name of the raster element resulting from the WBI operation."));
   return true;
}

bool WangBovik::populateInteractiveInputArgList(PlugInArgList* pInArgList)
{
   VERIFY(pInArgList != NULL);
   VERIFY(pInArgList->addArg<Progress>(Executable::ProgressArg(), NULL, Executable::ProgressArgDescription()));
   VERIFY(pInArgList->addArg<RasterElement>(Executable::DataElementArg(), NULL, "Raster element on which WBI will be performed."));
   return true;
}

bool WangBovik::populateDefaultOutputArgList(PlugInArgList* pOutArgList)
{
   VERIFY(pOutArgList->addArg<RasterElement>("WBI Results", NULL, "Raster element resulting from the WBI operation."));
   return true;
}

bool WangBovik::parseInputArgList(PlugInArgList* pInArgList)
{
   mpProgress = pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg());
   mProgress = ProgressTracker(mpProgress, "Wang-Bovik Index",
                              "spectral", "ACC2E556-B5F2-4795-BBFA-9C54D0914C16");
   RasterElement* pElement = pInArgList->getPlugInArgValue<RasterElement>(Executable::DataElementArg());
   if (pElement == NULL)
   {
      mProgress.report(WBIERR001, 0, ERRORS, true);
      return false;
   }

   const RasterDataDescriptor* pElementDescriptor = dynamic_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());
   EncodingType dataType = pElementDescriptor->getDataType();
   if (dataType == INT4SCOMPLEX || dataType == FLT8COMPLEX)
   {
      mProgress.report(WBIERR008, 0, ERRORS, true);
      return false;
   }

   // sensor is non-null and only one band -> bail out!
   if (pElementDescriptor->getBandCount() == 1)
   {
      mProgress.report(WBIERR009, 0, ERRORS, true);
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
   mpWangBovikAlg = new WangBovikAlgorithm(pElement, mpProgress, isInteractive(), pBitMask);
   setAlgorithmPattern(Resource<AlgorithmPattern>(mpWangBovikAlg));
   return true;
}

bool WangBovik::setActualValuesInOutputArgList(PlugInArgList* pOutArgList)
{
   VERIFY(pOutArgList->setPlugInArgValue("WBI Results", mpWangBovikAlg->getResults()));
   mProgress.upALevel(); // make sure the top-level step is successfull
   return true;
}

QDialog* WangBovik::getGui(void* pAlgData)
{
   // Currently this dialog will be deleted by AlgorithmPattern::execute before it exits. If this
   // changes in the future or the execute method is overridden in WangBovik, WangBovik will need to delete
   // mpWangBovikGui.
   mpWangBovikGui = new WangBovikDlg(mpWangBovikAlg->getRasterElement(), this, mpProgress,
      mInputs.mResultsName, mInputs.mbCreatePseudocolor, false, WangBovik::hasSettingWangBovikHelp(), mInputs.mThreshold,
      Service<DesktopServices>()->getMainWidget());
   mpWangBovikGui->setWindowTitle("Wang-Bovik Index");

   return mpWangBovikGui;
}

void WangBovik::propagateAbort()
{
   if (mpWangBovikGui != NULL)
   {
      mpWangBovikGui->abortSearch();
   }
}

bool WangBovik::extractFromGui()
{
   if (mpWangBovikGui == NULL)
   {
      return false;
   }
   mInputs.mThreshold = mpWangBovikGui->getThreshold();
   mInputs.mSignatures = mpWangBovikGui->getExtractedSignatures();
   mInputs.mResultsName = mpWangBovikGui->getResultsName();
   mInputs.mpAoi = mpWangBovikGui->getAoi();
   mInputs.mbCreatePseudocolor = mpWangBovikGui->isPseudocolorLayerUsed();

   if (mInputs.mResultsName.empty())
   {
      mInputs.mResultsName = "WBI Results";
   }
   return true;
}

WangBovikAlgorithm::WangBovikAlgorithm(RasterElement* pElement, Progress* pProgress, bool interactive, const BitMask* pAoi) :
               AlgorithmPattern(pElement, pProgress, interactive, pAoi),
               mpResults(NULL),
               mAbortFlag(false)
{}

bool WangBovikAlgorithm::preprocess()
{
   return true;
}

bool WangBovikAlgorithm::processAll()
{
   FactoryResource<Wavelengths> pWavelengths;

   ProgressTracker progress(getProgress(), "Starting WBI", "spectral", "80CEC958-3D29-4097-93E6-2F3E11874107");
   progress.getCurrentStep()->addProperty("Interactive", isInteractive());

   RasterElement* pElement = getRasterElement();
   if (pElement == NULL)
   {
      progress.report(WBIERR007, 0, ERRORS, true);
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
      progress.report(WBIERR002, 0, ERRORS, true);
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
   ModelResource<RasterElement> pHighestWangBovikValueMatrix(reinterpret_cast<RasterElement*>(NULL));

   // Check for multiple Signatures and if the user has selected
   // to combined multiple results in one pseudocolor output layer
   if (iSignatureCount > 1 && mInputs.mbCreatePseudocolor)
   {
      pPseudocolorMatrix = ModelResource<RasterElement>(createResults(numRows, numColumns, 1, mInputs.mResultsName));
      pHighestWangBovikValueMatrix = ModelResource<RasterElement>(createResults(numRows, numColumns, 1, "HighestWBIValue"));

      if (pPseudocolorMatrix.get() == NULL || pHighestWangBovikValueMatrix.get() == NULL )
      {
         progress.report(WBIERR004, 0, ERRORS, true);
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
         SpectralUtilities::getFailedDataRequestErrorMessage(havRequest.get(), pHighestWangBovikValueMatrix.get());
      DataAccessor highestWangBovikValueAccessor = pHighestWangBovikValueMatrix->getDataAccessor(havRequest.release());
      if (!highestWangBovikValueAccessor.isValid())
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
            if (!pseudoAccessor.isValid() || !highestWangBovikValueAccessor.isValid())
            {
               progress.report("Unable to access results.", 0, ERRORS, true);
               return false;
            }

            pHighestValue = reinterpret_cast<float*>(highestWangBovikValueAccessor->getColumn());
            pPseudoValue = reinterpret_cast<float*>(pseudoAccessor->getColumn());

            //Initialize the matrices
            *pPseudoValue = 0.0f;
            *pHighestValue = 0.0f;

            pseudoAccessor->nextColumn();
            highestWangBovikValueAccessor->nextColumn();
         }
         pseudoAccessor->nextRow();
         highestWangBovikValueAccessor->nextRow();
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
         rname += "WangBovikTemp";
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
      QString messageSigNumber = QString("Processing Signature %1 of %2 : WBI running on signature %3")
         .arg(sig_index+1).arg(iSignatureCount).arg(QString::fromStdString(sigNames.back()));
      string message = messageSigNumber.toStdString();

      vector<double> spectrumValues;
      vector<int> resampledBands;
      bSuccess = resampleSpectrum(pSignature, spectrumValues, pWavelengths.get(), resampledBands);

      // adjust signature values for the scaling factor
      const Units* pSigUnits = pSignature->getUnits("Reflectance");
      double scaleFactor = pSigUnits->getScaleFromStandard();
      if (pSigUnits != NULL)
      {
         for (std::vector<double>::iterator iter = spectrumValues.begin(); iter != spectrumValues.end(); ++iter)
         {
            *iter *= scaleFactor;
         }
      }

      // Check for limited spectral coverage and warning log
      if (bSuccess && pWavelengths->hasCenterValues() &&
         resampledBands.size() != pWavelengths->getCenterValues().size())
      {
         QString buf = QString("Warning WangBovikAlg014: The spectrum only provides spectral coverage for %1 of %2 bands.")
            .arg(resampledBands.size()).arg(pWavelengths->getCenterValues().size());
         progress.report(buf.toStdString(), 0, WARNING, true);
      }

      if (bSuccess)
      {
         BitMaskIterator iterChecker(getPixelsToProcess(), pElement);
         RasterElement* pResult = NULL;

         cv::Mat spectrum(spectrumValues.size(), 1, CV_64F);
         for (unsigned int i = 0; i < spectrumValues.size(); ++i)
         {
            spectrum.at<double>(i, 0) = spectrumValues[i];
         }
         cv::Scalar sigMean;
         cv::Scalar sigStdDev;
         cv::meanStdDev(spectrum, sigMean, sigStdDev);
         double sigVariance = sigStdDev[0] * sigStdDev[0];

         // subtract signature mean from the signature values
         for (unsigned int i = 0; i < spectrumValues.size(); ++i)
         {
            spectrum.at<double>(i) -= sigMean[0];
         }
         WangBovikAlgInput wbiInput(pElement, pResults.get(), spectrum, &mAbortFlag, iterChecker, resampledBands,
            sigMean[0], sigVariance);

         //Output Structure
         WangBovikAlgOutput wbiOutput;

         // Reports current Spectrum WBI is running on
         mta::ProgressObjectReporter reporter(message, getProgress());

         // Initializes all threads
         mta::MultiThreadedAlgorithm<WangBovikAlgInput, WangBovikAlgOutput, WangBovikThread>
            mtaWangBovik(mta::getNumRequiredThreads(numRows),
            wbiInput,
            wbiOutput,
            &reporter);

         // Calculates Wang-Bovik Index values for current signature
         mtaWangBovik.run();
         if (mAbortFlag)
         {
            progress.report(WBIABORT000, 0, ABORT, true);
            mAbortFlag = false;
            return false;
         }
         if (wbiInput.mpResultsMatrix == NULL)
         {
            progress.report(WBIERR003, 0, ERRORS, true);
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
                  SpectralUtilities::getFailedDataRequestErrorMessage(highestRequest.get(), pHighestWangBovikValueMatrix.get());
               DataAccessor daHighestWangBovikValue = pHighestWangBovikValueMatrix->getDataAccessor(highestRequest.release());
               if (!daHighestWangBovikValue.isValid())
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

                     daHighestWangBovikValue->toPixel(row_ctr, col_ctr);
                     pHighestValue = reinterpret_cast<float*>(daHighestWangBovikValue->getColumn());

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
      progress.report(WBIABORT000, 0, ABORT, true);
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
         progress.report(WBIERR010, 0, ERRORS, true);
         return false;
      }
      progress.report(WBINORM200, 100, NORMAL);
   }

   progress.getCurrentStep()->addProperty("Display Layer", mInputs.mbDisplayResults);
   progress.getCurrentStep()->addProperty("Threshold", mInputs.mThreshold);
   progress.upALevel();

   return bSuccess;
}

bool WangBovikAlgorithm::resampleSpectrum(Signature* pSignature, vector<double>& resampledAmplitude,
                                    Wavelengths* pWavelengths, vector<int>& resampledBands)
{
   StepResource pStep("Resample Signature", "spectral", "933ECFAA-1F9E-495B-9CF1-AACC0EF2D2E5");

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

RasterElement* WangBovikAlgorithm::createResults(int numRows, int numColumns, int numBands, const string& sigName)
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
         reportProgress(ERRORS, 0, WBIERR005);
         MessageResource(WBIERR005, "spectral", "16AE5D76-EA7C-46BB-B7E5-AFEDDF3E53C6");
         return NULL;
      }
   }

   FactoryResource<Units> pUnits;
   pUnits->setUnitType(CUSTOM_UNIT);
   pUnits->setUnitName("Index Value");

   vector<int> badValues(1, -99);

   RasterDataDescriptor* pResultsDescriptor = static_cast<RasterDataDescriptor*>(pResults->getDataDescriptor());
   VERIFYRV(pResultsDescriptor != NULL, NULL);
   pResultsDescriptor->setUnits(pUnits.get());
   pResultsDescriptor->setBadValues(badValues);

   Statistics* pStatistics = pResults->getStatistics();
   VERIFYRV(pStatistics != NULL, NULL);
   pStatistics->setBadValues(badValues);
   return pResults.release();
}

bool WangBovikAlgorithm::postprocess()
{
   return true;
}

bool WangBovikAlgorithm::initialize(void* pAlgorithmData)
{
   bool bSuccess = true;
   if (pAlgorithmData != NULL)
   {
      mInputs = *reinterpret_cast<WangBovikInputs*>(pAlgorithmData);
   }

   if (mInputs.mSignatures.empty())
   {
      reportProgress(ERRORS, 0, WBIERR006);
      MessageResource(WBIERR006, "spectral", "627C9D0A-D98A-4CE7-BBCF-C262DA3C1280");
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

RasterElement* WangBovikAlgorithm::getResults() const
{
   return mpResults;
}

bool WangBovikAlgorithm::canAbort() const
{
   return true;
}

bool WangBovikAlgorithm::doAbort()
{
   mAbortFlag = true;
   return true;
}

WangBovikThread::WangBovikThread(const WangBovikAlgInput& input, int threadCount, int threadIndex,
   mta::ThreadReporter& reporter) : mta::AlgorithmThread(threadIndex, reporter), mInput(input),
   mRowRange(getThreadRange(threadCount, input.mIterCheck.getNumSelectedRows()))
{
   if (input.mIterCheck.useAllPixels())
   {
      mRowRange = getThreadRange(threadCount, static_cast<const RasterDataDescriptor*>(
         input.mpCube->getDataDescriptor())->getRowCount());
   }
}

void WangBovikThread::run()
{
   EncodingType encoding = static_cast<const RasterDataDescriptor*>(mInput.mpCube->getDataDescriptor())->getDataType();
   switchOnEncoding(encoding, WangBovikThread::ComputeWangBovik, NULL);
}

template<class T>
void WangBovikThread::ComputeWangBovik(const T* pDummyData)
{
   const double wbiConstant(4.0);         // from Wang, Bovik, "A Universal Image Quality Index",
                                          // IEEE Signal Processing Letters, Vol 9, No. 3, March 2002
   float* pResultsData = NULL;
   int oldPercentDone = -1;
   const T* pData = NULL;
   const RasterDataDescriptor* pDescriptor = static_cast<const RasterDataDescriptor*>(
      mInput.mpCube->getDataDescriptor());
   unsigned int numCols = pDescriptor->getColumnCount();
   unsigned int numBands = pDescriptor->getBandCount();
   unsigned int numRows = (mRowRange.mLast - mRowRange.mFirst + 1);

   int numResultsCols = 0;

   //Sets area to apply the WBI algortihm to. Either
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

   for (int row_index = startRow; row_index <= stopRow; ++row_index)
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

      for (int col_index = startColumn; col_index <= stopColumn; ++col_index)
      {
         VERIFYNRV(resultAccessor.isValid());
         VERIFYNRV(accessor.isValid());

         // Pointer to results data
         pResultsData = reinterpret_cast<float*>(resultAccessor->getColumn());
         if (pResultsData == NULL)
         {
            return;
         }
         *pResultsData = wbiBadValue;
         if (mInput.mIterCheck.getPixel(col_index, row_index))
         {
            //Pointer to cube/sensor data
            pData = reinterpret_cast<T*>(accessor->getColumn());
            VERIFYNRV(pData != NULL);

            // Wang-Bovik Index description
            // covar = covariance between data spectrum and the target spectrum
            // mu_d = mean for the data spectrum
            // mu_t = mean for the target spectrum
            // var_d = variance for the data spectrum
            // var_t = variance for the target spectrum
            // WBI = (4 * covar * mu_d * mu_t) / ((mu_d^2 + mu_t^2) * (var_d + var_t))

            //Calculate mean and variance at current location
            cv::Mat dataSpectrum(mInput.mResampledBands.size(), 1, CV_64F);
            for (unsigned int index = 0; index < mInput.mResampledBands.size(); ++index)
            {
               int resampledBand = mInput.mResampledBands[index];
               dataSpectrum.at<double>(index, 0) = unitScale * pData[resampledBand];
            }
            cv::Scalar dataMean;
            cv::Scalar dataStdDev;
            cv::meanStdDev(dataSpectrum, dataMean, dataStdDev);
            double dataVariance = dataStdDev[0] * dataStdDev[0];
            
            // mean adjust the data spectrum
            for (unsigned int i = 0; i < mInput.mResampledBands.size(); ++i)
            {
               dataSpectrum.at<double>(i, 0) -= dataMean[0];
            }

            // compute the covariance - both the data and target spectra have been mean adjusted
            double covariance(0.0);
            for (unsigned int i = 0; i < mInput.mResampledBands.size(); ++i)
            {
               covariance += dataSpectrum.at<double>(i, 0) * mInput.mSpectrum.at<double>(i, 0);
            }
            covariance /= static_cast<double>(mInput.mResampledBands.size());

            // compute the WBI value
            double numerator = wbiConstant * covariance * dataMean[0] * mInput.mSpectrumMean;
            double denominator = (dataMean[0] * dataMean[0] + mInput.mSpectrumMean * mInput.mSpectrumMean) *
               (dataVariance + mInput.mSpectrumVariance);
            if (abs(denominator) > std::numeric_limits<double>::epsilon())
            {
               *pResultsData = static_cast<float>(numerator / denominator);
            }
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

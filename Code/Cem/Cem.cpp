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
#include "Cem.h"
#include "CemDlg.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
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
#include "Wavelengths.h"

using namespace std;

struct InsertReflectance : public unary_function<unsigned int,bool>
{
   InsertReflectance(const vector<double>& src, vector<double>& dest) : mSrc(src), mDest(dest) {}
   bool operator()(const DimensionDescriptor& band)
   {
      mDest.push_back(mSrc[band.getOnDiskNumber()]);
      return true;
   }

private:
   std::vector<double>& mDest;
   const std::vector<double>& mSrc;
};

static void computeSmmSubset(int numBands, 
                             const double* pSmm, 
                             double* pSmmSubset, 
                             const std::vector<int>& resampledBands)
{
   int numResampledBands = resampledBands.size();
   for (int bindex1 = 0; bindex1 < numResampledBands; ++bindex1)
   {
      for (int bindex2 = 0; bindex2 < numResampledBands; ++bindex2)
      {
         int bVecIndex1 = resampledBands[bindex1];
         int bVecIndex2 = resampledBands[bindex2];

         pSmmSubset[bindex1 * numResampledBands + bindex2] = pSmm[resampledBands[bindex1] * numBands + resampledBands[bindex2]];
      }
   }
   MatrixFunctions::invertSquareMatrix1D(pSmmSubset, pSmmSubset, numResampledBands);
}

static bool compareBands(const std::vector<int>& vec1, const std::vector<int>& vec2)
{
   if (vec1.size() != vec2.size())
   {
      return false;
   }

   std::vector<int>::const_iterator iter1, iter2;
   for (iter1 = vec1.begin(), iter2 = vec2.begin(); iter1 != vec1.end(); ++iter1, ++iter2)
   {
      if (*iter1 != *iter2)
      {
         return false;
      }
   }

   return true;
}

REGISTER_PLUGIN_BASIC(SpectralCem, Cem);

Cem::Cem() : AlgorithmPlugIn(&mInputs),
             mpCemGui(NULL),
             mpCemAlg(NULL)
{
   setDescriptorId("{D7F22E3B-967C-4C1F-BBBB-03CC329F56AE}");
   setName("CEM");
   setCreator("Ball Aerospace & Technologies Corp.");
   setShortDescription("Constrained Energy Minimization");
   setDescription("Compute Constrained Energy Minimization Material ID Algorithm");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setMenuLocation("[Spectral]\\Material ID\\CEM");
   setAbortSupported(true);
}

Cem::~Cem()
{
}

bool Cem::populateBatchInputArgList(PlugInArgList* pInArgList)
{
   if (!populateInteractiveInputArgList(pInArgList))
   {
      return false;
   }
   VERIFY(pInArgList->addArg<Signature>("Target Signatures", NULL, "Signatures that will be used by CEM."));
   VERIFY(pInArgList->addArg<double>("Threshold", 0.5, "Value of pixels to be flagged by default in the threshold layer "
      "resulting from the CEM operation."));
   VERIFY(pInArgList->addArg<AoiElement>("AOI", NULL, "Area of interest over which CEM will be performed. If not "
      "specified, the entire cube is used in processing."));
   VERIFY(pInArgList->addArg<bool>("Display Results", false, "Flag representing whether to display the results of the "
      "CEM operation."));
   VERIFY(pInArgList->addArg<string>("Results Name", "CEM Results", "Name of the raster element resulting from the "
      "CEM operation."));
   return true;
}

bool Cem::populateInteractiveInputArgList(PlugInArgList* pInArgList)
{
   VERIFY(pInArgList != NULL);
   VERIFY(pInArgList->addArg<Progress>(Executable::ProgressArg(), NULL, Executable::ProgressArgDescription()));
   VERIFY(pInArgList->addArg<RasterElement>(Executable::DataElementArg(), NULL, "Raster element on which CEM will be performed."));
   return true;
}

bool Cem::populateDefaultOutputArgList(PlugInArgList* pOutArgList)
{
   VERIFY(pOutArgList->addArg<RasterElement>("CEM Results", NULL, "Raster element resulting from the CEM operation."));
   return true;
}

bool Cem::parseInputArgList(PlugInArgList* pInArgList)
{
   mProgress.initialize(pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg()),
                                 "Constrained Energy Minimization", "spectral", "AC978BAC-A540-4808-83A3-8E6A03771C84");
   RasterElement* pElement = pInArgList->getPlugInArgValue<RasterElement>(Executable::DataElementArg());
   if (pElement == NULL)
   {
      mProgress.report("Invalid raster element", 0, ERRORS, true);
      return false;
   }

   const RasterDataDescriptor* pElementDescriptor = static_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());
   EncodingType dataType = pElementDescriptor->getDataType();
   if (dataType == INT4SCOMPLEX || dataType == FLT8COMPLEX)
   {
      mProgress.report("Complex data is not supported.", 0, ERRORS, true);
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
   mpCemAlg = new CemAlgorithm(pElement, mProgress.getCurrentProgress(), isInteractive(), pBitMask);
   setAlgorithmPattern(Resource<AlgorithmPattern>(mpCemAlg));
   return true;
}

bool Cem::setActualValuesInOutputArgList(PlugInArgList* pOutArgList)
{
   VERIFY(pOutArgList->setPlugInArgValue("CEM Results", mpCemAlg->getResults()));
   mProgress.upALevel(); // make sure the top-level step is successful
   return true;
}

QDialog* Cem::getGui(void* pAlgData)
{
   // Currently this dialog will be deleted by AlgorithmPattern::execute before it exits. If this
   // changes in the future or the execute method is overridden in Cem, Cem will need to delete
   // mpCemGui.
   mpCemGui = new CemDlg(mpCemAlg->getRasterElement(), this, mProgress.getCurrentProgress(),
      mInputs.mResultsName, mInputs.mbCreatePseudocolor, false,
      hasSettingCemHelp(), Service<DesktopServices>()->getMainWidget());
   mpCemGui->setThreshold(mInputs.mThreshold);
   mpCemGui->setWindowTitle("Constrained Energy Minimization");

   return mpCemGui;
}

void Cem::propagateAbort()
{
   if (mpCemGui != NULL)
   {
      mpCemGui->abortSearch();
   }
}

bool Cem::extractFromGui()
{
   if (mpCemGui == NULL)
   {
      return false;
   }
   mInputs.mThreshold = mpCemGui->getThreshold();
   mInputs.mSignatures = mpCemGui->getExtractedSignatures();
   mInputs.mResultsName = mpCemGui->getResultsName();
   mInputs.mpAoi = mpCemGui->getAoi();
   mInputs.mbCreatePseudocolor = mpCemGui->isPseudocolorLayerUsed();

   if (mInputs.mResultsName.empty())
   {
      mInputs.mResultsName = "CEM Results";
   }
   return true;
}

CemAlgorithm::CemAlgorithm(RasterElement* pElement, Progress* pProgress, bool interactive, const BitMask* pAoi) : 
               AlgorithmPattern(pElement, pProgress, interactive, pAoi),
               mpResults(NULL),
               mAbortFlag(false)
{
}

bool CemAlgorithm::preprocess()
{
   const RasterDataDescriptor* pDescriptor = dynamic_cast<const RasterDataDescriptor*>(getRasterElement()->getDataDescriptor());

   const Units* pUnits = pDescriptor->getUnits();
   if (pUnits != NULL && pUnits->getUnitType() != REFLECTANCE)
   {
      MessageResource msg("The cube does not contain reflectance data.", "spectral", "522B79B1-E012-4CDF-BBB7-59DD2DB48565");
      if (isInteractive())
      {
         if (Service<DesktopServices>()->showMessageBox("CEM Warning", "The cube provided does not indicate that it contains "
                              "reflectance data.\nCEM expects to operate on reflectance data.\nDo you wish to continue?",
                              "Yes", "No") != 0)
         {
            reportProgress(ABORT, 0, "The cube does not contain reflectance data.");
            return false;
         }
      }
      else
      {
         reportProgress(WARNING, 0, "The cube does not contain reflectance data.");
      }
   }
   return true;
}

bool CemAlgorithm::processAll()
{
   ProgressTracker progress(getProgress(), "Starting CEM", "spectral", "83BEAE63-DB05-4D1A-A085-D0866FD08548");
   progress.getCurrentStep()->addProperty("Interactive", isInteractive());

   RasterElement* pElement = getRasterElement();
   if (pElement == NULL)
   {
      progress.report("No cube specified.", 0, ERRORS, true);
      return false;
   }
   progress.getCurrentStep()->addProperty("Cube", pElement->getName());
   const RasterDataDescriptor* pDescriptor = static_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());
   VERIFY(pDescriptor != NULL);

   if (mInputs.mSignatures.empty())
   {
      progress.report("No valid signatures to process.", 0, ERRORS, true);
      return false;
   }

   // Total number of Bands in Cube
   unsigned int numBands = pDescriptor->getBandCount();

   BitMaskIterator it(getPixelsToProcess(), pElement);
   unsigned int numRows = it.getNumSelectedRows();
   unsigned int numColumns = it.getNumSelectedColumns();
   Opticks::PixelOffset layerOffset(it.getColumnOffset(), it.getRowOffset());
   int iSignatureCount = mInputs.mSignatures.size();

   vector<ColorType> layerColors, excludeColors;
   excludeColors.push_back(ColorType(0, 0, 0));
   excludeColors.push_back(ColorType(255, 255, 255));
   ColorType::getUniqueColors(iSignatureCount + 2, layerColors, excludeColors); // 2 for "no match" and "interminacy

   // get SMM^-1
   ExecutableResource smmPlugin("Second Moment", string(), progress.getCurrentProgress(), !isInteractive());
   if (smmPlugin->getPlugIn() == NULL)
   {
      progress.report("Second Moment Matrix plug-in not available.", 0, ERRORS, true);
      return false;
   }
   smmPlugin->getInArgList().setPlugInArgValue<RasterElement>(Executable::DataElementArg(), pElement);
   smmPlugin->getInArgList().setPlugInArgValue<AoiElement>("AOI", mInputs.mpAoi);
   RasterElement* pSmm = NULL;
   RasterElement* pInvSmm = NULL;
   if (!smmPlugin->execute() ||
      (pSmm = smmPlugin->getOutArgList().getPlugInArgValue<RasterElement>("Second Moment Matrix")) == NULL ||
      (pInvSmm = smmPlugin->getOutArgList().getPlugInArgValue<RasterElement>("Inverse Second Moment Matrix")) == NULL)
   {
      progress.report("Failed to calculate second moment matrix.", 0, ERRORS, true);
      return false;
   }

   // get cube wavelengths
   FactoryResource<Wavelengths> pWavelengths;
   pWavelengths->initializeFromDynamicObject(pElement->getMetadata(), false);

   // Create a pseudocolor results matrix if necessary
   ModelResource<RasterElement> pPseudocolorMatrix(reinterpret_cast<RasterElement*>(NULL));
   ModelResource<RasterElement> pHighestCEMValueMatrix(reinterpret_cast<RasterElement*>(NULL));
   // Check for multiple Signatures and if the user has selected
   // to combined multiple results in one pseudocolor output layer
   if (iSignatureCount > 1 && mInputs.mbCreatePseudocolor)
   {
      pPseudocolorMatrix = ModelResource<RasterElement>(createResults(numRows, numColumns, mInputs.mResultsName));
      pHighestCEMValueMatrix = ModelResource<RasterElement>(createResults(numRows, numColumns, "HighestCEMValue"));

      if (pPseudocolorMatrix.get() == NULL || pHighestCEMValueMatrix.get() == NULL )
      {
         progress.report("Unable to create pseudocolor results matrix.", 0, ERRORS, true);
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

      FactoryResource<DataRequest> hcvRequest;
      hcvRequest->setWritable(true);
      failedDataRequestErrorMessage =
         SpectralUtilities::getFailedDataRequestErrorMessage(hcvRequest.get(), pHighestCEMValueMatrix.get());
      DataAccessor highestCemValueAccessor = pHighestCEMValueMatrix->getDataAccessor(hcvRequest.release());
      if (!highestCemValueAccessor.isValid())
      {
         string msg = "Unable to access results.";
         if (!failedDataRequestErrorMessage.empty())
         {
            msg += "\n" + failedDataRequestErrorMessage;
         }

         progress.report(msg, 0, ERRORS, true);
         return false;
      }

      //Let's zero out all the results in case we connect to an existing matrix.
      float* pPseudoValue = NULL;
      float* pHighestValue = NULL;

      for (unsigned int row_ctr = 0; row_ctr < numRows; row_ctr++)
      {
         for (unsigned int col_ctr = 0; col_ctr < numColumns; col_ctr++)
         {
            if (!pseudoAccessor.isValid() || !highestCemValueAccessor.isValid())
            {
               progress.report("Unable to access results.", 0, ERRORS, true);
               return false;
            }

            pHighestValue = reinterpret_cast<float*>(highestCemValueAccessor->getColumn());
            pPseudoValue = reinterpret_cast<float*>(pseudoAccessor->getColumn());

            //Initialize the matrices
            *pPseudoValue = 0.0f;
            *pHighestValue = -10.0f;

            pseudoAccessor->nextColumn();
            highestCemValueAccessor->nextColumn();
         }
         pseudoAccessor->nextRow();
         highestCemValueAccessor->nextRow();
      }
   }

   const Units* pUnits = pDescriptor->getUnits();
   vector<string> sigNames;
   ModelResource<RasterElement> pResults(reinterpret_cast<RasterElement*>(NULL));

   // else create a result for each signature..with a unique name...INCLUDE offset!
   bool success = true;
   for (int sig_index = 0; success && sig_index < iSignatureCount && !mAbortFlag; sig_index++)
   {
      Signature* pSignature = mInputs.mSignatures[sig_index];
      sigNames.push_back(pSignature->getName());
      std::string rname = mInputs.mResultsName;
      if (iSignatureCount > 1 && !mInputs.mbCreatePseudocolor)
      {
         rname += " " + sigNames.back();
      }
      else if (iSignatureCount > 1)
      {
         rname += "CemTemp";
      }

      if (mInputs.mbCreatePseudocolor == false || pResults.get() == NULL)
      {
         pResults = ModelResource<RasterElement>(createResults(numRows, numColumns, rname));
      }
      if (pResults.get() == NULL)
      {
         success = false;
         break;
      }

      QString messageSigNumber = QString("Processing Signature %1 of %2 : CEM running on signature %3")
         .arg(sig_index+1).arg(iSignatureCount).arg(QString::fromStdString(sigNames.back()));
      string message = messageSigNumber.toStdString();

      vector<double> spectrumValues;
      vector<int> resampledBands, prevResampledBands;
      vector<double> woper(numBands);
      success = resampleSpectrum(pSignature, spectrumValues, pWavelengths.get(), resampledBands);

      // Check for limited spectral coverage and warning log 
      if (success && pWavelengths->hasCenterValues() &&
         resampledBands.size() != pWavelengths->getCenterValues().size())
      {
         QString buf = QString("The spectrum only provides spectral coverage for %1 of %2 bands.")
            .arg(resampledBands.size()).arg(pWavelengths->getCenterValues().size());
         progress.report(buf.toStdString(), 0, WARNING, true);
      }

      if (success)
      {
         const Units* pSigUnits = pSignature->getUnits("Reflectance");
         if (pSigUnits != NULL && pUnits != NULL)
         {
            if (pUnits->getUnitType() != pSigUnits->getUnitType())
            {
               progress.report("The spectrum and data have different units. CEM detections will be unpredictable.", 0, WARNING, true);
            }

            // what to multiply the spectrum by to have it in the same units as the cube
            double unitScaleRatio = 0;
            if (pUnits->getScaleFromStandard() != 0) // prevent divided by zero
            {
               unitScaleRatio = pSigUnits->getScaleFromStandard() / pUnits->getScaleFromStandard();
            }

            // scale to ensure that cube and spectrum are scaled the same:
            std::transform(spectrumValues.begin(), spectrumValues.end(), 
               spectrumValues.begin(), std::bind2nd(std::multiplies<double>(), 
               unitScaleRatio));
         }

         if (resampledBands.size() != pWavelengths->getCenterValues().size())
         {
            vector<double> smmSubset(numBands * numBands);
            if (!compareBands(resampledBands, prevResampledBands))
            {
               prevResampledBands = resampledBands;
               computeSmmSubset(numBands, reinterpret_cast<double*>(pSmm->getRawData()), &smmSubset.front(),
                  resampledBands);
            }
            computeWoper(spectrumValues, &smmSubset.front(), numBands, woper, resampledBands);
         }
         else
         {
            computeWoper(spectrumValues, reinterpret_cast<double*>(pInvSmm->getRawData()), numBands, woper, resampledBands);
         }

         BitMaskIterator iterChecker(getPixelsToProcess(), 0, 0, pDescriptor->getColumnCount() - 1,
                                     pDescriptor->getRowCount() - 1);
         CemAlgInput cemInput(pElement, pResults.get(), woper, &mAbortFlag, iterChecker, resampledBands);

         CemAlgOutput cemOutput;
         mta::ProgressObjectReporter reporter(message, progress.getCurrentProgress());
         mta::MultiThreadedAlgorithm<CemAlgInput, CemAlgOutput, CemThread>
            mtaCem(mta::getNumRequiredThreads(numRows), cemInput, cemOutput, &reporter);
         mtaCem.run();
         if (mAbortFlag)
         {
            progress.report("User aborted the operation.", 0, ABORT, true);
            mAbortFlag = false;
            return false;
         }
         if (cemInput.mpResultsMatrix == NULL)
         {
            progress.report("Error calculating CEM", 0, ERRORS, true);
            return false;
         }
         if (isInteractive() || mInputs.mbDisplayResults)
         {
            if (iSignatureCount > 1 && mInputs.mbCreatePseudocolor)
            {
               // Merges results in to one output layer if a Pseudocolor
               // output layer has been selected
               FactoryResource<DataRequest> pseudoRequest;
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

               DataAccessor daCurrentAccessor = pResults->getDataAccessor();

               FactoryResource<DataRequest> highestRequest;
               highestRequest->setWritable(true);
               failedDataRequestErrorMessage =
                  SpectralUtilities::getFailedDataRequestErrorMessage(highestRequest.get(), pHighestCEMValueMatrix.get());
               DataAccessor daHighestCEMValue = pHighestCEMValueMatrix->getDataAccessor(highestRequest.release());
               if (!daHighestCEMValue.isValid())
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

                     daHighestCEMValue->toPixel(row_ctr, col_ctr);
                     pHighestValue = reinterpret_cast<float*>(daHighestCEMValue->getColumn());

                     if (*pCurrentValue >= mInputs.mThreshold)
                     {
                        if (*pCurrentValue > *pHighestValue)
                        {
                           *pPseudoValue = sig_index+1;
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
      }
   }

   if (success && !mAbortFlag)
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

   if (success)
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
         progress.report("Unable to display CEM results.", 0, ERRORS, true);
         return false;
      }
      progress.report("CEM Complete", 100, NORMAL);
   }

   progress.getCurrentStep()->addProperty("Display Layer", mInputs.mbDisplayResults);
   progress.getCurrentStep()->addProperty("Threshold", mInputs.mThreshold);
   progress.upALevel();

   return success;
}

void CemAlgorithm::computeWoper(std::vector<double>& spectrumValues, double* pSmm,
                                int numBands, std::vector<double>& pWoper, const std::vector<int>& resampledBands)
{
   unsigned int numResampledBands = resampledBands.size();
   pWoper.resize(numResampledBands);
   VERIFYNRV(spectrumValues.size() == numResampledBands);
   VERIFYNRV(pSmm != NULL);
   
   double temp = 0.0;
   double product = 0.0;
   int index = 0;

   for (unsigned int rBands_index1 = 0; rBands_index1 < numResampledBands; ++rBands_index1)
   {
      pWoper[rBands_index1] = 0.0;
      for (unsigned int rBands_index2 = 0; rBands_index2 < numResampledBands; ++rBands_index2)
      {
         temp = pSmm[rBands_index1 * numResampledBands + rBands_index2] * spectrumValues[rBands_index2];
         pWoper[rBands_index1] += temp;
         product += temp * spectrumValues[rBands_index1];
      }
   }
   product = (product == 0.0) ? 1.0 : (1.0 / product);

   for (unsigned int rBands_index1 = 0; rBands_index1 < numResampledBands; ++rBands_index1)
   {
      pWoper[rBands_index1] *= product;
   }
}

bool CemAlgorithm::resampleSpectrum(Signature* pSignature, 
                                    vector<double>& resampledAmplitude,
                                    Wavelengths* pWavelengths, 
                                    vector<int>& resampledBands)
{
   StepResource pStep("Resample Signature", "spectral", "D201C66A-64C0-4257-928F-A6A8D4F8B3C4");

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

RasterElement* CemAlgorithm::createResults(int numRows, int numColumns, const string& sigName)
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
         reportProgress(ERRORS, 0, "Error creating results matrix.");
         MessageResource("Error creating results matrix.", "spectral", "1B653B64-A79B-4B3E-800E-EACED6EAF3F7");
         return NULL;
      }
   }

   vector<int> badValues(1, -10);

   RasterDataDescriptor* pResultsDescriptor = static_cast<RasterDataDescriptor*>(pResults->getDataDescriptor());
   VERIFYRV(pResultsDescriptor != NULL, NULL);
   pResultsDescriptor->setBadValues(badValues);

   Statistics* pStatistics = pResults->getStatistics();
   VERIFYRV(pStatistics != NULL, NULL);
   pStatistics->setBadValues(badValues);

   return pResults.release();
}

bool CemAlgorithm::postprocess()
{
   return true;
}

bool CemAlgorithm::initialize(void* pAlgorithmData)
{
   bool bSuccess = true;
   if (pAlgorithmData != NULL)
   {
      mInputs = *reinterpret_cast<CemInputs*>(pAlgorithmData);
   }

   if (mInputs.mSignatures.empty())
   {
      reportProgress(ERRORS, 0, "There are no signatures to process.");
      MessageResource("There are no signatures to process.", "spectral", "EBBAA4DC-BE41-427D-9623-1D149DB3E264");
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

RasterElement* CemAlgorithm::getResults() const
{
   return mpResults;
}

bool CemAlgorithm::canAbort() const
{
   return true;
}

bool CemAlgorithm::doAbort()
{
   mAbortFlag = true;
   return true;
}

CemThread::CemThread(const CemAlgInput& input, 
                     int threadCount, 
                     int threadIndex, 
                     mta::ThreadReporter& reporter) : mta::AlgorithmThread(threadIndex, reporter), 
                        mInput(input),
                        mRowRange(getThreadRange(threadCount, input.mCheck.getNumSelectedRows()))
{
   if (input.mCheck.useAllPixels())
   {
      mRowRange = getThreadRange(threadCount, static_cast<const RasterDataDescriptor*>(
         input.mpCube->getDataDescriptor())->getRowCount());
   }
}

void CemThread::run()
{
   EncodingType encoding = static_cast<const RasterDataDescriptor*>(
         mInput.mpCube->getDataDescriptor())->getDataType();
   switchOnEncoding(encoding, CemThread::ComputeCem, NULL);
}

template<class T>
void CemThread::ComputeCem(const T* pDummyData)
{
   const RasterDataDescriptor* pDescriptor = dynamic_cast<const RasterDataDescriptor*>(mInput.mpCube->getDataDescriptor());
   int numCols = pDescriptor->getColumnCount();
   int numBands = pDescriptor->getBandCount();
   int numRows = mRowRange.mLast - mRowRange.mFirst + 1;
   int numResultsCols = 0;

   if (mInput.mCheck.useAllPixels())
   {
      numResultsCols = numCols;
   }
   else
   {
      numResultsCols = mInput.mCheck.getNumSelectedColumns();
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

   int index = numResultsCols * mRowRange.mFirst;
   int oldPercentDone = -1;
   int rowOffset = static_cast<int>(mInput.mCheck.getOffset().mY);
   int startRow = mRowRange.mFirst + rowOffset;
   int stopRow = mRowRange.mLast + rowOffset;

   int columnOffset = static_cast<int>(mInput.mCheck.getOffset().mX);
   int startColumn = columnOffset;
   int stopColumn = numResultsCols + columnOffset - 1;

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
      int percentDone = mRowRange.computePercent(row_index - rowOffset);
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

         float* pResultsData = reinterpret_cast<float*>(resultAccessor->getColumn());
         VERIFYNRV(pResultsData != NULL);

         if (mInput.mCheck.getPixel(col_index, row_index))
         {
            T* pData = reinterpret_cast<T*>(accessor->getColumn());
            *pResultsData = 0.0f;
            for (unsigned int band_index = 0; band_index < mInput.mResampledBands.size(); ++band_index)
            {
               int resampledBand = mInput.mResampledBands[band_index];
               *pResultsData += (pData[resampledBand] * mInput.mWoper[band_index]);
            }
         }
         else
         {
            *pResultsData = -10.0;
         }
         resultAccessor->nextColumn();
         accessor->nextColumn();
      }
      resultAccessor->nextRow();
      accessor->nextRow();
   }
}

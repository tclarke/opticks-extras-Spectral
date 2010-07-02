/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtCore/QFile>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include "AoiElement.h"
#include "AppVerify.h"
#include "BitMask.h"
#include "BitMaskIterator.h"
#include "DimensionDescriptor.h"
#include "DynamicObject.h"
#include "ElmCore.h"
#include "ElmFile.h"
#include "Executable.h"
#include "Filename.h"
#include "MatrixFunctions.h"
#include "MessageLogResource.h"
#include "ModelServices.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInResource.h"
#include "Progress.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "Resampler.h"
#include "Signature.h"
#include "SpecialMetadata.h"
#include "switchOnEncoding.h"
#include "Units.h"

#include <sstream>

using namespace std;

ElmCore::ElmCore() :
   mpProgress(NULL),
   mpRasterElement(NULL),
   mpRasterDataDescriptor(NULL)
{
   // Do nothing
}

ElmCore::~ElmCore()
{
   // Do nothing
}

Progress* ElmCore::getProgress() const
{
   return mpProgress;
}

const RasterElement* ElmCore::getRasterElement() const
{
   return mpRasterElement;
}

bool ElmCore::getInputSpecification(PlugInArgList*& pArgList)
{
   pArgList = Service<PlugInManagerServices>()->getPlugInArgList();
   VERIFY(pArgList != NULL);
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg()));
   VERIFY(pArgList->addArg<RasterElement>(Executable::DataElementArg()));

   return true;
}

bool ElmCore::extractInputArgs(PlugInArgList* pInputArgList)
{
   StepResource pStep("Extract Core Input Arguments", "app", "6D6F255B-476A-49e9-94B3-35713FE7A0B1");
   VERIFY(pStep.get() != NULL);

   // Ensure that pInputArgList exists.
   if (pInputArgList == NULL)
   {
      pStep->finalize(Message::Failure, "The input argument list is invalid.");
      return false;
   }

   // Extract the input arguments from the list.
   mpProgress = pInputArgList->getPlugInArgValue<Progress>(Executable::ProgressArg());
   mpRasterElement = pInputArgList->getPlugInArgValue<RasterElement>(Executable::DataElementArg());

   pStep->finalize();
   return true;
}

bool ElmCore::isExecuting() const
{
   return mExecuting;
}

string ElmCore::getDefaultGainsOffsetsFilename() const
{
   string gainsOffsetsFilename;
   if (mpRasterElement != NULL)
   {
      const string& ext = ElmFile::getExt();
      gainsOffsetsFilename = string(mpRasterElement->getFilename());
      int extPos = gainsOffsetsFilename.find_first_of('.');
      if (extPos == string::npos)
      {
         gainsOffsetsFilename += ext;
      }
      else
      {
         gainsOffsetsFilename.resize(extPos);
         gainsOffsetsFilename += ext;
      }
   }

   return gainsOffsetsFilename;
}

bool ElmCore::executeElm(string gainsOffsetsFilename,
   const vector<Signature*>& pSignatures, const vector<AoiElement*>& pAoiElements)
{
   StepResource pStep("Execute ELM Algorithm", "app", "BD5F228F-629D-4520-BAF8-6FCBDE1A8F62");
   VERIFY(pStep.get() != NULL);
   mExecuting = true;

   // Check that all input arguments are valid.
   if (inputArgsAreValid() == false)
   {
      pStep->finalize(Message::Failure, "Input arguments are invalid.");
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
      }

      mExecuting = false;
      return false;
   }

   // Allocate a Gains/Offsets matrix.
   MatrixFunctions::MatrixResource<double> pGainsOffsets(mCenterWavelengths.size(), 2);
   if (pGainsOffsets.get() == NULL)
   {
      pStep->finalize(Message::Failure, "Unable to allocate memory for Gains/Offsets matrix.");
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
      }

      mExecuting = false;
      return false;
   }

   // If a Gains/Offsets filename was supplied, use it.
   // Otherwise, compute the results and attempt to save them.
   if (gainsOffsetsFilename.empty() == false)
   {
      if (getGainsOffsetsFromFile(gainsOffsetsFilename, pGainsOffsets) == false)
      {
         pStep->finalize(Message::Failure, "Unable to read Gains/Offsets file \"" + gainsOffsetsFilename + "\".");
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
         }

         mExecuting = false;
         return false;
      }
   }
   else if (getGainsOffsetsFromScratch(pSignatures, pAoiElements, pGainsOffsets) == false)
   {
      pStep->finalize(Message::Failure, "Unable to Compute Gains/Offsets.");
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
      }

      mExecuting = false;
      return false;
   }

   // Apply the Gains/Offsets to the View.
   if (applyResults(pGainsOffsets) == false)
   {
      pStep->finalize(Message::Failure, "Unable to Apply Gains/Offsets to View.");
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
      }

      mExecuting = false;
      return false;
   }

   pStep->finalize();
   if (mpProgress != NULL)
   {
      mpProgress->updateProgress("Done", 100, NORMAL);
   }

   mExecuting = false;
   return true;
}

bool ElmCore::getGainsOffsetsFromScratch(const vector<Signature*>& pSignatures,
   const vector<AoiElement*>& pAoiElements, double** pGainsOffsets)
{
   if (computeResults(pSignatures, pAoiElements, pGainsOffsets) == false)
   {
      return false;
   }

   ElmFile elmFile(getDefaultGainsOffsetsFilename(), mCenterWavelengths, pGainsOffsets);
   if (elmFile.saveResults() == false)
   {
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress("Unable to save Gains/Offsets to a file.", 0, WARNING);
      }
   }

   return true;
}

bool ElmCore::getGainsOffsetsFromFile(const string& filename, double** pGainsOffsets)
{
   ElmFile elmFile(filename, mCenterWavelengths, pGainsOffsets);
   return elmFile.readResults();
}

bool ElmCore::inputArgsAreValid()
{
   string errorMessage;
   if (mpRasterElement == NULL)
   {
      errorMessage += "The \"" + Executable::DataElementArg() + "\" input argument is invalid.\n";
   }
   else
   {
      mpRasterDataDescriptor = dynamic_cast<RasterDataDescriptor*>(mpRasterElement->getDataDescriptor());
      if (mpRasterDataDescriptor == NULL)
      {
         errorMessage += "Unable to access the RasterDataDescriptor from the RasterElement.\n";
      }
      else
      {
         FactoryResource<DataRequest> pRequest;
         VERIFY(pRequest.get() != NULL);
         pRequest->setWritable(true);
         const string failedDataRequestErrorMessage =
            SpectralUtilities::getFailedDataRequestErrorMessage(pRequest.get(), mpRasterElement);
         DataAccessor daAccessor = mpRasterElement->getDataAccessor(pRequest.release());
         if (daAccessor.isValid() == false)
         {
            if (failedDataRequestErrorMessage.empty() == true)
            {
               errorMessage += "Unable to obtain a writable DataAccessor.\n";
            }
            else
            {
               errorMessage += failedDataRequestErrorMessage;
            }
         }

         EncodingType dataType = mpRasterDataDescriptor->getDataType();
         if (dataType == INT4SCOMPLEX || dataType == FLT8COMPLEX)
         {
            errorMessage += "Complex data is not supported.\n";
         }

         mpUnits = mpRasterDataDescriptor->getUnits();
         if (mpUnits == NULL)
         {
            errorMessage += "Unable to access the Units from the RasterDataDescriptor.\n";
         }
         else if (mpUnits->getUnitType() == REFLECTANCE)
         {
            if (QMessageBox::question(NULL, "Empirical Line Method", "WARNING: The data is already in reflectance.\n"
                  "If the data is actually in reflectance it is not recommended to run ELM on this data.\n\n"
                  "Do you wish to continue processing?", QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
            {
               errorMessage += "The data is already in reflectance.\n";
            }
         }

         // Get all center wavelengths.
         const DynamicObject* pMetadata = mpRasterDataDescriptor->getMetadata();
         if (pMetadata == NULL)
         {
            errorMessage += "Unable to access Center Wavelengths.\n";
         }
         else
         {
            const string pCenterWavelengthPath[] = { SPECIAL_METADATA_NAME, BAND_METADATA_NAME,
               CENTER_WAVELENGTHS_METADATA_NAME, END_METADATA_NAME };
            const vector<double>* pCenterWavelengths =
               dv_cast<vector<double> >(&pMetadata->getAttributeByPath(pCenterWavelengthPath));

            if (pCenterWavelengths == NULL)
            {
               errorMessage += "No Center Wavelengths are available.\n";
            }
            else
            {
               mCenterWavelengths = *pCenterWavelengths;
            }
         }
      }
   }

   if (errorMessage.empty() == false)
   {
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(errorMessage, 100, ERRORS);
      }

      return false;
   }

   return true;
}

bool ElmCore::computeResults(const vector<Signature*>& pSignatures,
   const vector<AoiElement*>& pAoiElements, double** pGainsOffsets)
{
   StepResource pStep("Compute Gains/Offsets", "app", "E3D6FA4B-E2B3-45d0-A934-5E1BCF0EE3A6");
   VERIFY(pStep.get() != NULL);

   int totalNumPoints = 0;
   if (resultsCanBeComputed(pSignatures, pAoiElements, totalNumPoints) == false)
   {
      pStep->finalize(Message::Failure, "Results cannot be computed.");
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
      }

      return false;
   }

   string errorMessage;
   const int polynomialOrder = 2;
   const int numElements = static_cast<int>(pAoiElements.size());
   const int numWavelengths = static_cast<int>(mCenterWavelengths.size());
   vector<double> referenceValues(totalNumPoints);
   vector<double> pixelValues(totalNumPoints);
   vector<double> coefficients(polynomialOrder);

   MatrixFunctions::MatrixResource<double> pReferenceSpectra(numElements, numWavelengths);
   if (pReferenceSpectra.get() == NULL)
   {
      errorMessage += "Unable to allocate memory for computation.\n";
   }
   // Populate the reference spectrum
   else if (readSignatureFiles(pSignatures, pReferenceSpectra) == false)
   {
      errorMessage += "Unable to read Signature Files.\n";
   }
   else
   {
      double percent = 0;
      const double step = 100.0 / numWavelengths;
      Service<ModelServices> pModel;
      for (int band = 0; band < numWavelengths; ++band)
      {
         DimensionDescriptor activeBand = mpRasterDataDescriptor->getActiveBand(band);
         if (activeBand.isValid() == false)
         {
            errorMessage += "Active Band is invalid.\n";
            break;
         }

         FactoryResource<DataRequest> pRequest;
         if (pRequest.get() == NULL)
         {
            errorMessage += "FactoryResource<DataRequest> returned NULL.\n";
            break;
         }

         pRequest->setBands(activeBand, activeBand);
         DataAccessor daAccessor = mpRasterElement->getDataAccessor(pRequest.release());
         if (daAccessor.isValid() == false)
         {
            errorMessage += "Unable to obtain a DataAccessor.\n";
            break;
         }

         unsigned int numPointsProcessed = 0;
         for (int element = 0; element < numElements && daAccessor.isValid(); ++element)
         {
            const BitMask* pMask = pAoiElements[element]->getSelectedPoints();
            if (pMask == NULL)
            {
               errorMessage += "getSelectedPoints() returned NULL.\n";
               break;
            }
            BitMaskIterator it(pMask, mpRasterElement);
            int x1, y1, x2, y2;
            it.getBoundingBox(x1, y1, x2, y2);
            const int maxRow = static_cast<int>(mpRasterDataDescriptor->getRowCount());
            const int maxCol = static_cast<int>(mpRasterDataDescriptor->getColumnCount());
            if (x1 < 0 || y1 < 0 || x2 > maxCol || y2 > maxRow)
            {
               errorMessage += "The AOI cannot contain points outside the image.\n";
               break;
            }

            for (int yCount = y1; yCount <= y2 && daAccessor.isValid(); ++yCount)
            {
               for (int xCount = x1; xCount <= x2 && daAccessor.isValid(); ++xCount)
               {
                  if (it.getPixel(xCount, yCount) == true)
                  {
                     referenceValues[numPointsProcessed] = pReferenceSpectra[element][band];
                     daAccessor->toPixel(yCount, xCount);
                     if (daAccessor.isValid() == false)
                     {
                        errorMessage += "Unable to read from the DataAccessor.\n";
                        break;
                     }

                     pixelValues[numPointsProcessed++] =
                        pModel->getDataValue(mpRasterDataDescriptor->getDataType(), daAccessor->getColumn(), 0);
                  }
               }
            }
         }

         if (numPointsProcessed != totalNumPoints && errorMessage.empty() == true)
         {
            errorMessage = "Not all points could be processed.\n";
         }
         else
         {
            const int numRows = totalNumPoints;
            const int numCols = polynomialOrder;
            MatrixFunctions::MatrixResource<double> pMatrix(numRows, numCols);
            if (pMatrix.get() == NULL)
            {
               errorMessage = "Unable to allocate memory to run ELM.\n";
            }
            else
            {
               for (int row = 0; row < numRows; ++row)
               {
                  basisFunction(referenceValues[row], pMatrix[row], numCols);
               }

               if (MatrixFunctions::solveLinearEquation(&coefficients.front(),
                  pMatrix, &pixelValues.front(), numRows, numCols) == false)
               {
                  errorMessage = "Unable to solve the linear equation.\n";
               }
            }
         }

         if (errorMessage.empty() == false)
         {
            break;
         }

         pGainsOffsets[band][1] = coefficients[0]; // save offsets
         pGainsOffsets[band][0] = coefficients[1]; // save gains
         percent += step;
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress("Computing Gains/Offsets...", static_cast<int>(percent), NORMAL);
         }
      }
   }

   if (errorMessage.empty() == false)
   {
      pStep->finalize(Message::Failure, errorMessage);
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
      }

      return false;
   }

   pStep->finalize();
   return true;
}

bool ElmCore::resultsCanBeComputed(const vector<Signature*>& pSignatures,
   const vector<AoiElement*>& pAoiElements, int& totalNumPoints)
{
   string errorMessage;
   const unsigned int numElements = pAoiElements.size();
   if (numElements < 2)
   {
      errorMessage += "There must be at least two elements to run ELM.\n";
   }

   if (pSignatures.size() != numElements)
   {
      errorMessage += "There is a mismatch between the number of Signatures and the number of AOI elements.\n";
   }

   for (vector<Signature*>::const_iterator iter = pSignatures.begin(); iter != pSignatures.end(); ++iter)
   {
      if (*iter == NULL)
      {
         errorMessage += "No Signature specified.\n";
         break;
      }
   }

   // Verify that all AoiElements are not NULL and contain at least one point.
   totalNumPoints = 0;
   for (vector<AoiElement*>::const_iterator iter = pAoiElements.begin(); iter != pAoiElements.end(); ++iter)
   {
      if (*iter == NULL)
      {
         errorMessage += "Unable to process a NULL AOI Element.\n";
         break;
      }

      const BitMask* const pBitMask = (*iter)->getSelectedPoints();
      VERIFY(pBitMask != NULL);
      BitMaskIterator it(pBitMask, mpRasterElement);
      const int numSelectedPoints = it.getCount();
      if (numSelectedPoints == 0)
      {
         errorMessage += "Unable to process an empty AOI Element.\n";
         break;
      }

      totalNumPoints += numSelectedPoints;
   }

   if (errorMessage.empty() == false)
   {
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(errorMessage, 100, ERRORS);
      }

      return false;
   }

   return true;
}

bool ElmCore::readSignatureFiles(const vector<Signature*>& pSignatures, double** pReferenceSpectra)
{
   StepResource pStep("Read Signature Files", "app", "7DD1134F-BA2A-4ba6-9421-DF749E1031B2");
   VERIFY(pStep.get() != NULL);

   PlugInResource pPlugIn("Resampler");
   Resampler* pResampler = dynamic_cast<Resampler*>(pPlugIn.get());
   if (pResampler == NULL)
   {
      pStep->finalize(Message::Failure, "The Resampler plug-in is not available");
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
      }

      return false;
   }

   const unsigned int numSignatures = pSignatures.size();
   for (unsigned int i = 0; i < numSignatures; ++i)
   {
      const Signature* pSignature = pSignatures[i];
      VERIFY(pSignature != NULL);

      string errorMsg;
      vector<int> toBands;
      vector<double> toFwhm;
      vector<double> toReflectances;

      const vector<double>* pSignatureWavelengths = dv_cast<vector<double> >(&pSignature->getData("Wavelength"));
      const vector<double>* pSignatureReflectances = dv_cast<vector<double> >(&pSignature->getData("Reflectance"));
      if (pSignatureWavelengths == NULL || pSignatureReflectances == NULL)
      {
         pStep->finalize(Message::Failure, "Signature \"" + pSignature->getDisplayName() +
            "\" cannot be used.\nThe selected signature does not contain Wavelength and Reflectance information.");
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
         }

         return false;
      }

      if (pResampler->execute(*pSignatureReflectances, toReflectances, *pSignatureWavelengths,
         mCenterWavelengths, toFwhm, toBands, errorMsg) == false)
      {
         pStep->finalize(Message::Failure, "Signature \"" + pSignature->getDisplayName() +
            "\" cannot be used.\n" + errorMsg);
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
         }

         return false;
      }

      copy(toReflectances.begin(), toReflectances.end(), pReferenceSpectra[i]);
   }

   pStep->finalize();
   return true;
}

bool ElmCore::applyResults(double** pGainsOffsets)
{
   StepResource pStep("Apply Gains/Offsets to View", "app", "2560A2F6-9F72-47b7-81A7-4A5B5C8036B6");
   VERIFY(pStep.get() != NULL);

   void* pData = NULL;
   VERIFY(mpRasterDataDescriptor != NULL);
   switchOnEncoding(mpRasterDataDescriptor->getDataType(), scaleCube, pData, pGainsOffsets);

   VERIFY(mpUnits != NULL);
   mpUnits->setUnitType(REFLECTANCE);
   mpUnits->setRangeMin(0.0);
   mpUnits->setRangeMax(0.0);
   double scaleValue = 0.0;
   switchOnEncoding(mpRasterDataDescriptor->getDataType(), getScaleValue, pData, scaleValue);

   VERIFY(scaleValue != 0.0);
   mpUnits->setScaleFromStandard(1.0 / scaleValue);

   pStep->finalize();
   return true;
}

void ElmCore::basisFunction(double scale, double* pCoefficients, int numCoeffs)
{
   pCoefficients[0] = 1.0;
   for (int coeff = 1; coeff < numCoeffs; ++coeff)
   {
      pCoefficients[coeff] = pCoefficients[coeff - 1] * scale;
   }
}

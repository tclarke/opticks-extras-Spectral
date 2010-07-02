/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef ELMCORE_H
#define ELMCORE_H

#include "ConfigurationSettings.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "Progress.h"
#include "RasterDataDescriptor.h"
#include "SpectralUtilities.h"

#include <limits>
#include <string>
#include <vector>

class AoiElement;
class Filename;
class PlugInArgList;
class Progress;
class RasterDataDescriptor;
class RasterElement;
class Signature;
class Units;

class ElmCore
{
public:
   ElmCore();
   virtual ~ElmCore();
   SETTING(ElmHelp, SpectralContextSensitiveHelp, std::string, "");

   Progress* getProgress() const;
   std::string getDefaultGainsOffsetsFilename() const;

   bool executeElm(std::string gainsOffsetsFilename,
      const std::vector<Signature*>& pSignatures, const std::vector<AoiElement*>& pAoiElements);

   const RasterElement* getRasterElement() const;

protected:
   bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool extractInputArgs(PlugInArgList* pInputArgList);
   bool isExecuting() const;

   Units* mpUnits;
   Progress* mpProgress;
   RasterElement* mpRasterElement;
   RasterDataDescriptor* mpRasterDataDescriptor;

   std::vector<double> mCenterWavelengths;

private:
   bool mExecuting;
   bool inputArgsAreValid();

   bool getGainsOffsetsFromFile(const std::string& gainsOffsetsFilename, double** pGainsOffsets);
   bool getGainsOffsetsFromScratch(const std::vector<Signature*>& pSignatures,
      const std::vector<AoiElement*>& pAoiElements, double** pGainsOffsets);

   bool computeResults(const std::vector<Signature*>& pSignatures,
      const std::vector<AoiElement*>& pAoiElements, double** pGainsOffsets);

   bool resultsCanBeComputed(const std::vector<Signature*>& pSignatures,
      const std::vector<AoiElement*>& pAoiElements, int& totalNumPoints);

   bool readSignatureFiles(const std::vector<Signature*>& pSignatures, double** pReferenceSpectra);
   bool applyResults(double** pGainsOffsets);
   void basisFunction(double scale, double* pCoefficients, int numCoeffs);

   template<typename T>
   void scaleCube(T* pData, double** pGainsOffsets);

   template<typename T>
   inline void getScaleValue(T* pData, double& scaleValue);

   template<typename T>
   inline void getMaxValue(T* pData, double& maxValue);
};

template<typename T>
void ElmCore::scaleCube(T* pData, double** pGainsOffsets)
{
   VERIFYNRV(mpRasterElement != NULL);
   VERIFYNRV(mpRasterDataDescriptor != NULL);

   const unsigned int numBands = mpRasterDataDescriptor->getBandCount();
   const unsigned int numRows = mpRasterDataDescriptor->getRowCount();
   const unsigned int numCols = mpRasterDataDescriptor->getColumnCount();

   VERIFYNRV(numRows != 0 && numCols != 0 && numBands != 0);
   const double step = 100.0 / numBands;
   double percent = 0;

   double maxValue;
   double scaleValue;
   getMaxValue(pData, maxValue);
   getScaleValue(pData, scaleValue);

   for (unsigned int band = 0; band < numBands; ++band, ++pData)
   {
      if (fabs(pGainsOffsets[band][0]) > 0.0000)
      {
         const double netGain = scaleValue / pGainsOffsets[band][0];
         const double netOffset = pGainsOffsets[band][1];
         const DimensionDescriptor activeBand = mpRasterDataDescriptor->getActiveBand(band);
         VERIFYNRV(activeBand.isValid() == true);

         FactoryResource<DataRequest> pRequest;
         VERIFYNRV(pRequest.get() != NULL);
         pRequest->setWritable(true);
         pRequest->setBands(activeBand, activeBand);
         const std::string failedDataRequestErrorMessage =
            SpectralUtilities::getFailedDataRequestErrorMessage(pRequest.get(), mpRasterElement);
         DataAccessor daAccessor = mpRasterElement->getDataAccessor(pRequest.release());
         if (daAccessor.isValid() == false)
         {
            if (mpProgress != NULL)
            {
               if (failedDataRequestErrorMessage.empty() == false)
               {
                  mpProgress->updateProgress(failedDataRequestErrorMessage, 100, ERRORS);
               }
               else
               {
                  mpProgress->updateProgress("Unable to obtain a writable DataAccessor.", 100, ERRORS);
               }
            }

            break;
         }

         for (unsigned int row = 0; row < numRows && daAccessor.isValid() == true; row++, daAccessor->nextRow())
         {
            for (unsigned int col = 0; col < numCols && daAccessor.isValid() == true; col++, daAccessor->nextColumn())
            {
               pData = static_cast<T*>(daAccessor->getColumn());

               double result = static_cast<double>(*pData);
               result = (result - netOffset) * netGain;
               if (result < 0)
               {
                  result = 0;
               }
               else if (result > maxValue)
               {
                  result = maxValue;
               }

               *pData = static_cast<T>(result);
            }
         }
      }

      percent += step;

      if (mpProgress != NULL)
      {
         mpProgress->updateProgress("Applying Gains/Offsets...", static_cast<int>(percent), NORMAL);
      }
   }

   mpRasterElement->updateData();
}

template<typename T>
inline void ElmCore::getScaleValue(T* pData, double& scaleValue)
{
   scaleValue = static_cast<double>(10000);
}

template<>
inline void ElmCore::getScaleValue<unsigned char>(unsigned char* pData, double& scaleValue)
{
   scaleValue = static_cast<double>(std::numeric_limits<unsigned char>::max());
}

template<>
inline void ElmCore::getScaleValue<char>(char* pData, double& scaleValue)
{
   scaleValue = static_cast<double>(std::numeric_limits<char>::max());
}

template<>
inline void ElmCore::getScaleValue<float>(float* pData, double& scaleValue)
{
   scaleValue = static_cast<double>(1.0);
}

template<>
inline void ElmCore::getScaleValue<double>(double* pData, double& scaleValue)
{
   scaleValue = static_cast<double>(1.0);
}

template<typename T>
inline void ElmCore::getMaxValue(T* pData, double& maxValue)
{
   getScaleValue<T>(pData, maxValue);
}

template<>
inline void ElmCore::getMaxValue<float>(float* pData, double& maxValue)
{
   maxValue = static_cast<double>(1.e38);
}

template<>
inline void ElmCore::getMaxValue<double>(double* pData, double& maxValue)
{
   maxValue = static_cast<double>(1.e308);
}

#endif

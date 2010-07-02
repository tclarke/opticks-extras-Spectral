/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef IARR_H
#define IARR_H

#include "AlgorithmShell.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "MessageLogResource.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "Statistics.h"
#include "Testable.h"
#include "TypesFile.h"
#include "Units.h"

class AoiElement;
class Progress;
class RasterElement;
class RasterLayer;
class SpatialDataWindow;

#include <iomanip>
#include <math.h>
#include <sstream>
#include <string>
#include <vector>

class Iarr : public AlgorithmShell, public Testable
{
public:
   Iarr();
   ~Iarr();

   bool runOperationalTests(Progress* pProgress, std::ostream& failure);
   bool runAllTests(Progress* pProgress, std::ostream& failure);

   bool getInputSpecification(PlugInArgList*& pArgList);
   bool getOutputSpecification(PlugInArgList*& pArgList);

   bool execute(PlugInArgList* pInputArgList, PlugInArgList* pOutputArgList);

private:
   Progress* mpProgress;
   RasterElement* mpInputRasterElement;
   RasterLayer* mpInputRasterLayer;

   std::string mInputFilename;
   AoiElement* mpAoiElement;
   unsigned int mRowStepFactor;
   unsigned int mColumnStepFactor;

   EncodingType mOutputDataType;
   bool mInMemory;
   bool mDisplayResults;
   std::string mOutputFilename;

   const std::string mExtension;

   bool extractAndValidateInputArguments(PlugInArgList* pInputArgList);
   SpatialDataWindow* createWindow(const std::string& windowName);
   RasterElement* runAlgorithm();
   RasterElement* createOutputRasterElement();
   bool determineGains(std::vector<double>& gains);
   bool applyGains(const std::vector<double>& gains, RasterElement* pOutputRasterElement);

   template <typename T>
   void writeData(T* pDestination, double value);

   template<typename T>
   bool runTest(unsigned int numRows, unsigned int numColumns,
      unsigned int numBands, EncodingType inputDataType, const T* pData, std::ostream& failure);

   class ErrorLog
   {
   public:
      ErrorLog(Step* pStep, Progress* pProgress);
      ~ErrorLog();

      void aborted();
      void setError(const std::string& message);
      void addWarning(const std::string& message);

   private:
      Step* mpStep;
      Progress* mpProgress;
      bool mFinalized;
   };
};

template <typename T>
void Iarr::writeData(T* pDestination, double value)
{
   if (pDestination != NULL)
   {
      *pDestination = static_cast<T>(value);
   }
}

template<typename T>
bool Iarr::runTest(unsigned int numRows, unsigned int numColumns,
   unsigned int numBands, EncodingType inputDataType, const T* pData, std::ostream& failure)
{
   // Create a RasterElement for testing
   ModelResource<RasterElement> pRasterElement(RasterUtilities::createRasterElement(getName() + " Test",
      numRows, numColumns, numBands, inputDataType, BSQ));
   mpInputRasterElement = pRasterElement.get();
   if (mpInputRasterElement == NULL)
   {
      failure << "Unable to create a Raster Element.";
      return false;
   }

   // Copy the data into mpInputRasterElement
   memcpy(mpInputRasterElement->getRawData(), pData, numBands * numRows * numColumns * sizeof(T));

   // Set up constant input parameters
   mInputFilename.clear();
   mOutputFilename.clear();
   mpInputRasterLayer = NULL;
   mpAoiElement = NULL;
   mDisplayResults = false;

   mOutputDataType = FLT8BYTES;
   mInMemory = true;

   for (mRowStepFactor = 1; mRowStepFactor < numRows; ++mRowStepFactor)
   {
      for (mColumnStepFactor = 1; mColumnStepFactor < numColumns; ++mColumnStepFactor)
      {
         // Compute the averages of pData
         const T* pSourceData = pData;
         std::vector<double> gains(numBands);
         for (unsigned int band = 0; band < numBands; ++band)
         {
            double totalValue = 0.0;
            unsigned int totalCounted = 0;
            for (unsigned int row = 0; row < numRows; row += mRowStepFactor)
            {
               pSourceData = pData + (band * numRows * numColumns) + (row * numColumns);
               for (unsigned int column = 0; column < numColumns;
                  column += mColumnStepFactor, pSourceData += mColumnStepFactor)
               {
                  totalValue += static_cast<double>(*pSourceData);
                  ++totalCounted;
               }
            }

            if (totalValue == 0.0 || totalCounted == 0)
            {
               failure << getName() << " has received an invalid test scenario.";
               return false;
            }

            // The gain is the inverse of the average
            gains[band] = totalCounted / totalValue;
         }

         ModelResource<RasterElement> pOutputRasterElement(runAlgorithm());
         if (pOutputRasterElement.get() == NULL)
         {
            failure << getName() << " has failed execution.";
            return false;
         }

         // Compare the results to the expected results
         const double tolerance = 1e-12;
         pSourceData = pData;
         double* pResults = reinterpret_cast<double*>(pOutputRasterElement->getRawData());
         for (unsigned int band = 0; band < numBands; ++band)
         {
            for (unsigned int row = 0; row < numRows; ++row)
            {
               for (unsigned int column = 0; column < numColumns; ++column, ++pSourceData, ++pResults)
               {
                  double expectedData = static_cast<double>(*pSourceData) * gains[band];
                  if (fabs(*pResults - expectedData) > tolerance)
                  {
                     failure << getName() << " has returned incorrect values.";
                     return false;
                  }
               }
            }
         }
      }
   }

   return true;
}

#endif

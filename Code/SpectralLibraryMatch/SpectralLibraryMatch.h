/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SPECTRALLIBRAYMATCH_H
#define SPECTRALLIBRAYMATCH_H

#include "AppConfig.h"
#include "EnumWrapper.h"
#include "StringUtilities.h"
#include "TypesFile.h"

// The Intel Threading Building Blocks Library (tbb) is not supported on the Solaris Sparc platform
#ifndef SOLARIS
#include <tbb/tbb.h>
#endif

#include <string>
#include <vector>

class AoiElement;
class RasterElement;
class Signature;

namespace SpectralLibraryMatch
{
   enum MatchAlgorithmEnum
   {
      SLMA_SAM,
      SLMA_WBI
   };
   typedef EnumWrapper<MatchAlgorithmEnum> MatchAlgorithm;

   enum LocateAlgorithmEnum
   {
      SLLA_SAM,
      SLLA_CEM,
      SLLA_WBI
   };
   typedef EnumWrapper<LocateAlgorithmEnum> LocateAlgorithm;

   enum AlgorithmSortOrderEnum
   {
      ASO_ASCENDING,
      ASO_DESCENDING
   };
   typedef EnumWrapper<AlgorithmSortOrderEnum> AlgorithmSortOrder;

   struct MatchResults
   {
      MatchResults() :
         mpRaster(NULL)
      {}

      MatchResults(const MatchResults& results) :
         mpRaster(results.mpRaster),
         mTargetName(results.mTargetName),
         mTargetValues(results.mTargetValues),
         mResults(results.mResults),
         mAlgorithmUsed(results.mAlgorithmUsed)
      {}
      
      bool isValid()
      {
         return (mpRaster != NULL) && (mTargetName.empty() == false) && (mTargetValues.empty() == false)
            && mAlgorithmUsed.isValid();
      }

      const RasterElement* mpRaster;
      std::string mTargetName;
      std::vector<double> mTargetValues;
      std::vector<std::pair<Signature*, float> > mResults;
      MatchAlgorithm mAlgorithmUsed;
   };

   class MatchLimits
   {
   public:
      MatchLimits();
      virtual ~MatchLimits();

      bool getLimitByNum() const;
      void setLimitByNum(bool limit);
      unsigned int getMaxNum() const;
      void setMaxNum(int num);
      bool getLimitByThreshold() const;
      void setLimitByThreshold(bool limit);
      double getThresholdLimit() const;
      void setThresholdLimit(double threshold);
      PassArea getThresholdType() const;
      bool passesThreshold(const double value) const;

   private:
      bool mLimitByNum;
      unsigned int mMaxNum;
      bool mLimitByThreshold;
      double mThresholdLimit;
      PassArea mThresholdType;
   };

#ifndef SOLARIS
   class MatchMetrics
   {
   public:
      MatchMetrics(double* pLibData, unsigned int numSignatures,
         MatchResults* pMatchInfo, double* pResultsData);
      ~MatchMetrics();

      void operator() (tbb::blocked_range<unsigned int>& range) const;

      double* mpLibData;
      unsigned int mNumSignatures;
      MatchResults* mpMatchInfo;
      double* mpResultsData;
      unsigned int mNumBands;
   };
#endif

   static const std::string& getNameLibraryManagerPlugIn()
   {
      static std::string var = "Spectral Library Manager";
      return var;
   }

   static const std::string& getNameLibraryMatchResultsPlugIn()
   {
      static std::string var = "Spectral Library Match Results";
      return var;
   }

   static const std::string& getNameSignatureAmplitudeData()
   {
      static std::string var = "Reflectance";
      return var;
   }

   static const std::string& getNameSignatureWavelengthData()
   {
      static std::string var = "Wavelength";
      return var;
   }

   // spectral library match functions
   template<class T>
   bool getScaledPixelValues(T* pPixelData, std::vector<double>& scaledValues,
      unsigned int numBands, double scaleFactor)
   {
      VERIFY(pPixelData != NULL);
      scaledValues.resize(numBands);
      T* pData(pPixelData);
      for (unsigned int band = 0; band < numBands; ++band)
      {
         scaledValues[band] = static_cast<double>(*pData) * scaleFactor;
         ++pData;
      }
      return true;
   }

   RasterElement* getCurrentRasterElement();
   AoiElement* getCurrentAoi();

   // function uses default options limits
   bool findSignatureMatches(const RasterElement* pLib, const std::vector<Signature*>& libSignatures,
      MatchResults& theResults);

   // function requires instance of MatchLimits 
   bool findSignatureMatches(const RasterElement* pLib, const std::vector<Signature*>& libSignatures,
                             MatchResults& theResults, const MatchLimits& limits);

   bool getScaledValuesFromSignature(std::vector<double>& values, const Signature* pSignature);
}

namespace StringUtilities
{
   template<>
   std::string toDisplayString(const SpectralLibraryMatch::MatchAlgorithm& value, bool* pError);

   template<>
   std::string toXmlString(const SpectralLibraryMatch::MatchAlgorithm& value, bool* pError);

   template<>
   SpectralLibraryMatch::MatchAlgorithm fromDisplayString<SpectralLibraryMatch::MatchAlgorithm>(
      std::string valueText, bool* pError);

   template<>
   SpectralLibraryMatch::MatchAlgorithm fromXmlString<SpectralLibraryMatch::MatchAlgorithm>(
      std::string valueText, bool* pError);

   template<>
   std::string toDisplayString(const std::vector<SpectralLibraryMatch::MatchAlgorithm>& value, bool* pError);

   template<>
   std::string toXmlString(const std::vector<SpectralLibraryMatch::MatchAlgorithm>& value, bool* pError);

   template<>
   std::vector<SpectralLibraryMatch::MatchAlgorithm> fromDisplayString<
      std::vector<SpectralLibraryMatch::MatchAlgorithm> >(std::string valueText, bool* pError);

   template<>
   std::vector<SpectralLibraryMatch::MatchAlgorithm> fromXmlString<std::vector<
      SpectralLibraryMatch::MatchAlgorithm> >(std::string valueText, bool* pError);

   template<>
   std::string toDisplayString(const SpectralLibraryMatch::LocateAlgorithm& value, bool* pError);

   template<>
   std::string toXmlString(const SpectralLibraryMatch::LocateAlgorithm& value, bool* pError);

   template<>
   SpectralLibraryMatch::LocateAlgorithm fromDisplayString<SpectralLibraryMatch::LocateAlgorithm>(
      std::string valueText, bool* pError);

   template<>
   SpectralLibraryMatch::LocateAlgorithm fromXmlString<SpectralLibraryMatch::LocateAlgorithm>(
      std::string valueText, bool* pError);

   template<>
   std::string toDisplayString(const std::vector<SpectralLibraryMatch::LocateAlgorithm>& value, bool* pError);

   template<>
   std::string toXmlString(const std::vector<SpectralLibraryMatch::LocateAlgorithm>& value, bool* pError);

   template<>
   std::vector<SpectralLibraryMatch::LocateAlgorithm> fromDisplayString<
      std::vector<SpectralLibraryMatch::LocateAlgorithm> >(std::string valueText, bool* pError);

   template<>
   std::vector<SpectralLibraryMatch::LocateAlgorithm> fromXmlString<std::vector<
      SpectralLibraryMatch::LocateAlgorithm> >(std::string valueText, bool* pError);
}

#endif

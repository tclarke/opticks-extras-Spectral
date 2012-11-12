/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AoiElement.h"
#include "AoiLayer.h"
#include "AppConfig.h"
#include "AppVerify.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DesktopServices.h"
#include "DynamicObject.h"
#include "LayerList.h"
#include "ObjectResource.h"
#include "Progress.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "Signature.h"
#include "SpatialDataView.h"
#include "SpectralLibraryMatch.h"
#include "SpectralLibraryMatchOptions.h"
#include "StringUtilities.h"
#include "StringUtilitiesMacros.h"
#include "Units.h"
#include "Wavelengths.h"

#include <algorithm>
#include <numeric>

namespace StringUtilities
{
   BEGIN_ENUM_MAPPING_ALIAS(SpectralLibraryMatch::MatchAlgorithm, Match_Algorithm)
      ADD_ENUM_MAPPING(SpectralLibraryMatch::SLMA_SAM, "Spectral Angle", "spectral_angle")
      ADD_ENUM_MAPPING(SpectralLibraryMatch::SLMA_WBI, "Wang-Bovik Index", "wang_bovik_index")
   END_ENUM_MAPPING()

   BEGIN_ENUM_MAPPING_ALIAS(SpectralLibraryMatch::LocateAlgorithm, Locate_Algorithm)
      ADD_ENUM_MAPPING(SpectralLibraryMatch::SLLA_SAM, "Spectral Angle", "spectral_angle")
      ADD_ENUM_MAPPING(SpectralLibraryMatch::SLLA_CEM, "Constrained Energy Minimization",
         "constrained_energy_minimization")
      ADD_ENUM_MAPPING(SpectralLibraryMatch::SLLA_WBI, "Wang-Bovik Index", "wang_bovik_index")
   END_ENUM_MAPPING()
}

namespace SpectralLibraryMatch
{
   MatchLimits::MatchLimits() :
      mLimitByNum(SpectralLibraryMatchOptions::getSettingLimitByMaxNum()),
      mMaxNum(SpectralLibraryMatchOptions::getSettingMaxDisplayed()),
      mLimitByThreshold(SpectralLibraryMatchOptions::getSettingLimitByThreshold()),
      mThresholdType(LOWER)
   {
      SpectralLibraryMatch::MatchAlgorithm algorithm =
         StringUtilities::fromXmlString<SpectralLibraryMatch::MatchAlgorithm>(
         SpectralLibraryMatchOptions::getSettingMatchAlgorithm());
      switch (algorithm)
      {
      case SpectralLibraryMatch::SLMA_SAM:
         mThresholdLimit = SpectralLibraryMatchOptions::getSettingMatchSamThreshold();
         mThresholdType = LOWER;
         break;

      case SpectralLibraryMatch::SLMA_WBI:
         mThresholdLimit = SpectralLibraryMatchOptions::getSettingMatchWbiThreshold();
         mThresholdType = UPPER;
         break;

      default:
         mThresholdLimit = 0.0;
         break;
      }
   }

   MatchLimits::~MatchLimits()
   {}

   bool MatchLimits::getLimitByNum() const
   {
      return mLimitByNum;
   }

   void MatchLimits::setLimitByNum(bool limit)
   {
      mLimitByNum = limit;
   }

   unsigned int MatchLimits::getMaxNum() const
   {
      return mMaxNum;
   }

   void MatchLimits::setMaxNum(int num)
   {
      mMaxNum = static_cast<unsigned int>(std::max(0, num));
   }

   bool MatchLimits::getLimitByThreshold() const
   {
      return mLimitByThreshold;
   }

   void MatchLimits::setLimitByThreshold(bool limit)
   {
      mLimitByThreshold = limit;
   }

   double MatchLimits::getThresholdLimit() const
   {
      return mThresholdLimit;
   }
   PassArea MatchLimits::getThresholdType() const
   {
      return mThresholdType;
   }

   bool MatchLimits::passesThreshold(const double value) const
   {
      if (mThresholdType.isValid())
      {
         switch (mThresholdType)
         {
         case LOWER:
            return value <= mThresholdLimit;

         case UPPER:
            return value >= mThresholdLimit;

         default:
            // only UPPER and LOWER are currently used for match algorithms
            break;
         }
      }
      return false;
   }

   void MatchLimits::setThresholdLimit(double threshold)
   {
      mThresholdLimit = threshold;
   }

   double getSpectralAngle(const std::vector<double>& targetSig, const std::vector<double>& libSig)
   {
      VERIFYRV(targetSig.size() == libSig.size(), 90.0);
      double targetMag = sqrt(inner_product(targetSig.begin(),
         targetSig.end(), targetSig.begin(), 0.0));
      double libMag = sqrt(inner_product(libSig.begin(), libSig.end(),
         libSig.begin(), 0.0));
      double samValue = inner_product(targetSig.begin(), targetSig.end(),
         libSig.begin(), 0.0);
      if (targetMag <= 0.0 || libMag <= 0.0)  // set to arbitrary low values so samValue will be very large
      {
         targetMag = 0.001;
         libMag = 0.001;
      }
      samValue /= (targetMag * libMag);
      if (samValue < -1.0)
      {
         samValue = -1.0;
      }
      else if (samValue > 1.0)
      {
         samValue = 1.0;
      }
      samValue = (180.0 / PI) * acos(samValue);

      return samValue;
   }

   double getWangBovikIndex(const std::vector<double>& targetSig, const std::vector<double>& libSig)
   {
      VERIFYRV(targetSig.size() == libSig.size(), -1.0);
      const double wangBovikConst(4.0);  // from Wang, Bovik, "A Universal Image Quality Index",
                                         // IEEE Signal Processing Letters, Vol 9, No. 3, March 2002
      unsigned int numBands = static_cast<unsigned int>(targetSig.size());
      double targetMean =
         std::accumulate(targetSig.begin(), targetSig.end(), 0.0) / static_cast<double>(numBands);
      double libMean =
         std::accumulate(libSig.begin(), libSig.end(), 0.0) / static_cast<double>(numBands);
      double targetLibCovar(0.0);
      double targetVariance(0.0);
      double libVariance(0.0);
      for (unsigned int band = 0; band < numBands; ++band)
      {
         double normTarget = targetSig[band] - targetMean;
         double normLib = libSig[band] - libMean;
         targetLibCovar += normTarget * normLib;
         targetVariance += normTarget * normTarget;
         libVariance += normLib * normLib;
      }
      targetLibCovar /= static_cast<double>(numBands - 1.0);
      targetVariance /= static_cast<double>(numBands - 1.0);
      libVariance /= static_cast<double>(numBands - 1.0);
      double numerator = wangBovikConst * targetLibCovar * targetMean * libMean;
      double denominator = (targetMean * targetMean + libMean * libMean) * (targetVariance + libVariance);
      double wbiValue(-99.0);  // initialize to bad value
      if (abs(denominator) > std::numeric_limits<double>::epsilon())
      {
         wbiValue = numerator / denominator;
      }
      return wbiValue;
   }

#if defined SOLARIS  // declare older non-ttb functions since tbb not supported under Solaris
   bool computeSpectralAngle(const std::vector<double>& targetValues, const RasterElement* pLib,
      RasterElement* pResults)
   {
      VERIFY(pLib != NULL && targetValues.empty() == false && pResults != NULL);
      const RasterDataDescriptor* pLibDesc = dynamic_cast<const RasterDataDescriptor*>(pLib->getDataDescriptor());
      const RasterDataDescriptor* pResultsDesc =
         dynamic_cast<const RasterDataDescriptor*>(pResults->getDataDescriptor());
      VERIFY(pLibDesc != NULL && pResultsDesc != NULL && pLibDesc->getRowCount() == pResultsDesc->getRowCount());
      FactoryResource<DataRequest> libRqt;
      libRqt->setInterleaveFormat(BIP);
      DataAccessor libAcc = pLib->getDataAccessor(libRqt.release());
      FactoryResource<DataRequest> resultsRqt;
      resultsRqt->setWritable(true);
      DataAccessor resultsAcc = pResults->getDataAccessor(resultsRqt.release());
      unsigned int numRows = pLibDesc->getRowCount();
      unsigned int numBands = pLibDesc->getBandCount();
      std::vector<double> libValues(numBands);
      double* pData(NULL);
      for (unsigned int row = 0; row < numRows; ++row)
      {
         VERIFY(libAcc.isValid() && resultsAcc.isValid());
         memcpy(&libValues[0], libAcc->getColumn(), numBands * sizeof(double));
         double samValue = getSpectralAngle(targetValues, libValues);
         pData = reinterpret_cast<double*>(resultsAcc->getColumn());
         *pData = samValue;
         libAcc->nextColumn();
         libAcc->nextRow();
         resultsAcc->nextColumn();
         resultsAcc->nextRow();
      }

      return true;
   }
   bool computeWangBovikIndex(const std::vector<double>& targetValues, const RasterElement* pLib,
      RasterElement* pResults)
   {
      VERIFY(pLib != NULL && targetValues.empty() == false && pResults != NULL);
      const RasterDataDescriptor* pLibDesc = dynamic_cast<const RasterDataDescriptor*>(pLib->getDataDescriptor());
      const RasterDataDescriptor* pResultsDesc =
         dynamic_cast<const RasterDataDescriptor*>(pResults->getDataDescriptor());
      VERIFY(pLibDesc != NULL && pResultsDesc != NULL && pLibDesc->getRowCount() == pResultsDesc->getRowCount());
      FactoryResource<DataRequest> libRqt;
      libRqt->setInterleaveFormat(BIP);
      DataAccessor libAcc = pLib->getDataAccessor(libRqt.release());
      FactoryResource<DataRequest> resultsRqt;
      resultsRqt->setWritable(true);
      DataAccessor resultsAcc = pResults->getDataAccessor(resultsRqt.release());
      unsigned int numRows = pLibDesc->getRowCount();
      unsigned int numBands = pLibDesc->getBandCount();
      std::vector<double> libValues(numBands);
      double* pData(NULL);
      for (unsigned int row = 0; row < numRows; ++row)
      {
         VERIFY(libAcc.isValid() && resultsAcc.isValid());
         memcpy(&libValues[0], libAcc->getColumn(), numBands * sizeof(double));
         double wbiValue = getWangBovikIndex(targetValues, libValues);
         pData = reinterpret_cast<double*>(resultsAcc->getColumn());
         *pData = wbiValue;
         libAcc->nextColumn();
         libAcc->nextRow();
         resultsAcc->nextColumn();
         resultsAcc->nextRow();
      }

      return true;
   }
#else
   MatchMetrics::MatchMetrics(double* pLibData, unsigned int numSignatures,
      MatchResults* pMatchInfo, double* pResultsData) :
      mpLibData(pLibData),
      mNumSignatures(numSignatures),
      mpMatchInfo(pMatchInfo),
      mpResultsData(pResultsData),
      mNumBands(pMatchInfo->mTargetValues.size())
   {}

   MatchMetrics::~MatchMetrics()
   {}

   void MatchMetrics::operator() (tbb::blocked_range<unsigned int>& range) const
   {
      const double* pLibData = mpLibData + mNumBands * range.begin();
      double* pMetricValue = mpResultsData + range.begin();
      const std::vector<double>& targetValues = mpMatchInfo->mTargetValues;

      // set up function for metric computation
      double (*computeFunc)(const std::vector<double>& targetSig, const std::vector<double>& libSig);
      switch (mpMatchInfo->mAlgorithmUsed)
      {
      case SLMA_SAM:
         computeFunc = getSpectralAngle;
         break;

     case SLMA_WBI:
        computeFunc = getWangBovikIndex;
        break;

      default:
         return;
      }
      unsigned int sigsize(mNumBands * sizeof(double));
      std::vector<double> libValues(mNumBands);
      for (unsigned int row = range.begin(); row < range.end(); ++row)
      {
         memcpy(&libValues[0], pLibData, sigsize);
         *pMetricValue = computeFunc(mpMatchInfo->mTargetValues, libValues);
         pLibData += mNumBands;
         ++pMetricValue;
      }
   }

   bool generateMetrics(const RasterElement* pLib, MatchResults& matchInfo, RasterElement* pResults)
   {
      VERIFY(pLib != NULL && matchInfo.mTargetValues.empty() == false && pResults != NULL);

      const RasterDataDescriptor* pLibDesc = dynamic_cast<const RasterDataDescriptor*>(pLib->getDataDescriptor());
      const RasterDataDescriptor* pResultsDesc =
         dynamic_cast<const RasterDataDescriptor*>(pResults->getDataDescriptor());
      VERIFY(pLibDesc != NULL && pResultsDesc != NULL && pLibDesc->getRowCount() == pResultsDesc->getRowCount());

      // since pLib and pResults are always created in memory, we can just grab raw pointers to the blocks of doubles
      double* pLibData = reinterpret_cast<double*>(const_cast<void*>(pLib->getRawData()));
      double* pResultsData = reinterpret_cast<double*>(const_cast<void*>(pResults->getRawData()));
      VERIFY(pLibData != NULL && pResultsData != NULL);
      unsigned int numSignatures = pLibDesc->getRowCount();
      unsigned int numBands = pLibDesc->getBandCount();

      MatchMetrics metrics(pLibData, numSignatures, &matchInfo, pResultsData);

      // granularity of 50 was the optimum determined from testing using library with 464 signatures
      unsigned int granularity(50);
      tbb::parallel_for(tbb::blocked_range<unsigned int>(0, numSignatures, granularity), metrics, tbb::simple_partitioner());

      return true;
   }
#endif

   bool lessThan(std::pair<Signature*, float>& lhs, std::pair<Signature*, float>& rhs)
   {
      return lhs.second < rhs.second;
   }

   bool greaterThan(std::pair<Signature*, float>& lhs, std::pair<Signature*, float>& rhs)
   {
      return lhs.second > rhs.second;
   }

   AlgorithmSortOrder getAlgorithmSortOrder(MatchAlgorithm algType)
   {
      AlgorithmSortOrder sortOrder;
      switch (algType)
      {
      case SLMA_SAM:
         sortOrder = ASO_ASCENDING;
         break;

      case SLMA_WBI:
         sortOrder = ASO_DESCENDING;
         break;

      default:     // return invalid enum
         break;
      }

      return sortOrder;
   }

   void generateSortedResults(RasterElement* pAlgOutput, const std::vector<Signature*> libSignatures,
                              MatchResults& theResults)
   {
      theResults.mResults.clear();
      if (pAlgOutput == NULL || libSignatures.empty())
      {
         return;
      }
      bool (*sortOrderFunc)(std::pair<Signature*, float>& lhs, std::pair<Signature*, float>& rhs);
      switch (getAlgorithmSortOrder(theResults.mAlgorithmUsed))
      {
      case ASO_ASCENDING:
         sortOrderFunc = lessThan;
         break;

      case ASO_DESCENDING:
         sortOrderFunc = greaterThan;
         break;

      default:    // invalid sort order, so exit
         return;
      }

      RasterDataDescriptor* pDesc = dynamic_cast<RasterDataDescriptor*>(pAlgOutput->getDataDescriptor());
      VERIFYNRV(pDesc != NULL);
      VERIFYNRV(pDesc->getRowCount() == libSignatures.size());
      theResults.mResults.reserve(libSignatures.size());
      FactoryResource<DataRequest> pRqt;
      pRqt->setInterleaveFormat(BIP);
      pRqt->setRows(pDesc->getActiveRow(0), pDesc->getActiveRow(pDesc->getRowCount() - 1), 1);
      DataAccessor acc = pAlgOutput->getDataAccessor(pRqt.release());
      VERIFYNRV(acc.isValid());
      for (std::vector<Signature*>::const_iterator libIt = libSignatures.begin();
         libIt != libSignatures.end() && acc.isValid(); ++libIt)
      {
         double* pData = reinterpret_cast<double*>(acc->getColumn());
         theResults.mResults.push_back(std::pair<Signature*, float>(*libIt, static_cast<float>(*pData)));
         acc->nextRow();
      }

      VERIFYNRV(theResults.mResults.size() == libSignatures.size());

      std::make_heap(theResults.mResults.begin(), theResults.mResults.end(), sortOrderFunc);
      std::sort_heap(theResults.mResults.begin(), theResults.mResults.end(), sortOrderFunc);
   }

   RasterElement* getCurrentRasterElement()
   {
      RasterElement* pRaster(NULL);
      SpatialDataView* pView =
         dynamic_cast<SpatialDataView*>(Service<DesktopServices>()->getCurrentWorkspaceWindowView());
      if (pView != NULL)
      {
         LayerList* pLayerList = pView->getLayerList();
         if (pLayerList != NULL)
         {
            pRaster = pLayerList->getPrimaryRasterElement();
         }
      }

      return pRaster;
   }

   AoiElement* getCurrentAoi()
   {
      AoiElement* pAoi(NULL);
      SpatialDataView* pView =
         dynamic_cast<SpatialDataView*>(Service<DesktopServices>()->getCurrentWorkspaceWindowView());
      if (pView != NULL)
      {
         AoiLayer* pAoiLayer = dynamic_cast<AoiLayer*>(pView->getActiveLayer());
         if (pAoiLayer != NULL)
         {
            pAoi = static_cast<AoiElement*>(pAoiLayer->getDataElement());
         }
      }

      return pAoi;
   }


   void trimSortedResults(MatchResults& theResults, const MatchLimits& limits)
   {
      if (limits.getLimitByNum())
      {
         unsigned int maxToDisplay =
            std::min(limits.getMaxNum(), static_cast<unsigned int>(theResults.mResults.size()));
         theResults.mResults.erase(theResults.mResults.begin() + maxToDisplay,
            theResults.mResults.end());
      }
      if (limits.getLimitByThreshold())
      {
         VERIFYNRV_MSG(limits.getThresholdType().isValid(),
            "Invalid threshold type, results will not be limited by threshold");
         std::vector<std::pair<Signature*, float> >::iterator it = theResults.mResults.begin();
         for (; it != theResults.mResults.end(); ++it)
         {
            if (limits.passesThreshold(static_cast<double>(it->second)) == false)
            {
               break;  // results are in sorted order so when first value to fail is found, erase rest
            }
         }
         theResults.mResults.erase(it, theResults.mResults.end());
      }
   }

   bool findSignatureMatches(const RasterElement* pLib, const std::vector<Signature*>& libSignatures,
      MatchResults& theResults, const MatchLimits& limits)
   {
      const RasterDataDescriptor* pLibDesc = dynamic_cast<const RasterDataDescriptor*>(pLib->getDataDescriptor());
      VERIFY(pLibDesc != NULL);
      ModelResource<RasterElement> pResults(RasterUtilities::createRasterElement("Spectral Library Match Results",
         pLibDesc->getRowCount(), 1, FLT8BYTES, true, const_cast<RasterElement*>(pLib)));
      VERIFY(pResults.get() != NULL);

#if defined SOLARIS  // tbb not available under solaris so use earlier code
      bool (*computeFunc)(const std::vector<double>& targetValues, const RasterElement* pLib, RasterElement* pResults);
      switch (theResults.mAlgorithmUsed)
      {
      case SLMA_SAM:
         computeFunc = computeSpectralAngle;
         break;
      case SLMA_WBI:
         computeFunc = computeWangBovikIndex;
      default:
         return false;
      }

      if (computeFunc(theResults.mTargetValues, pLib, pResults.get()) == false)
      {
         return false;
      }
#else                // use newer tbb based code for other platforms
      if (generateMetrics(pLib, theResults, pResults.get()) == false)
      {
         return false;
      }
#endif

      // sort results
      generateSortedResults(pResults.get(), libSignatures, theResults);
      trimSortedResults(theResults, limits);

      return true;
   }

   bool findSignatureMatches(const RasterElement* pLib, const std::vector<Signature*>& libSignatures,
      MatchResults& theResults)
   {
      MatchLimits limits;
      return findSignatureMatches(pLib, libSignatures, theResults, limits);
   }

   bool getScaledValuesFromSignature(std::vector<double>& values, const Signature* pSignature)
   {
      VERIFY(pSignature != NULL);
      values.clear();
      DataVariant variant = pSignature->getData("Reflectance");
      VERIFY(variant.isValid());
      variant.getValue(values);
      const Units* pUnits = pSignature->getUnits("Reflectance");
      VERIFY(pUnits != NULL);
      double scaleFactor = pUnits->getScaleFromStandard();
      for (std::vector<double>::iterator it = values.begin(); it != values.end(); ++it)
      {
         *it *= scaleFactor;
      }

      return true;
   }
}

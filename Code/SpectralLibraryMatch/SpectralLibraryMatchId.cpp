/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AoiElement.h"
#include "AppVerify.h"
#include "BitMaskIterator.h"
#include "ColorType.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DesktopServices.h"
#include "DynamicObject.h"
#include "LayerList.h"
#include "MatchIdDlg.h"
#include "ModelServices.h"
#include "ObjectResource.h"
#include "PlugIn.h"
#include "PlugInArg.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "Progress.h"
#include "PseudocolorLayer.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "SessionResource.h"
#include "Signature.h"
#include "SpatialDataView.h"
#include "SpectralLibraryManager.h"
#include "SpectralLibraryMatch.h"
#include "SpectralLibraryMatchId.h"
#include "SpectralLibraryMatchOptions.h"
#include "SpectralLibraryMatchResults.h"
#include "SpectralVersion.h"
#include "SpectralUtilities.h"
#include "StringUtilities.h"
#include "switchOnEncoding.h"
#include "TypeConverter.h"
#include "Wavelengths.h"

#include <limits>
#include <map>
#include <math.h>
#include <sstream>
#include <vector>

REGISTER_PLUGIN_BASIC(SpectralSpectralLibraryMatch, SpectralLibraryMatchId);

namespace
{
   template<class T>
   void setValue(T* pData, int& classId)
   {
      *pData = static_cast<T>(classId);
   }
}

SpectralLibraryMatchId::SpectralLibraryMatchId()
{
   setName("Spectral Library Match");
   setVersion(SPECTRAL_VERSION_NUMBER);
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setShortDescription("Find matches for in-scene spectra in a spectral library");
   setDescription("Match in-scene spectra to signatures in a spectral library.");
   setMenuLocation("[Spectral]\\Material ID\\Spectral Library Match");
   setDescriptorId("{F8507730-C821-4b61-8B32-4339E5EB5460}");
   setAbortSupported(true);
   allowMultipleInstances(false);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

SpectralLibraryMatchId::~SpectralLibraryMatchId()
{}

bool SpectralLibraryMatchId::getInputSpecification(PlugInArgList*& pArgList)
{
   Service<PlugInManagerServices> pPlugInMgr;

   // Set up list
   pArgList = pPlugInMgr->getPlugInArgList();
   VERIFY(pArgList != NULL);
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   VERIFY(pArgList->addArg<RasterElement>(Executable::DataElementArg(), NULL));

   if (isBatch()) // need additional info in batch mode
   {
      VERIFY(pArgList->addArg<AoiElement>("AOI Element", NULL));
      VERIFY(pArgList->addArg<bool>("Match each Pixel", false));

      // build list of valid match algorithm names for arg description
      std::string matchAlgDesc = "Valid algorithm names are:";
      std::vector<std::string> algNames =
         StringUtilities::getAllEnumValuesAsDisplayString<SpectralLibraryMatch::MatchAlgorithm>();
      for (std::vector<std::string>::iterator it = algNames.begin(); it != algNames.end(); ++it)
      {
         matchAlgDesc += "\n";
         matchAlgDesc += *it;
      }
      VERIFY(pArgList->addArg<std::string>("Match Algorithm Name", "", matchAlgDesc));
      VERIFY(pArgList->addArg<bool>("Limit max number of matches", true));
      VERIFY(pArgList->addArg<unsigned int>("Max number of matches", 5));
      VERIFY(pArgList->addArg<bool>("Limit matches by threshold", true));
      VERIFY(pArgList->addArg<double>("Threshold cutoff for match", 5.0));
   }

   return true;
}

bool SpectralLibraryMatchId::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;

   return true;
}

bool SpectralLibraryMatchId::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   // Lock Session Save while finding matches for the AOI avg sig
   SessionSaveLock lock;

  // get progress and raster element
   mpProgress = pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg());
   RasterElement* pRaster = pInArgList->getPlugInArgValue<RasterElement>(Executable::DataElementArg());
   if (pRaster == NULL)
   {
      updateProgress("The input raster element was null", 0, ERRORS);
      return false;
   }

   // check that raster element has wavelength info
   FactoryResource<Wavelengths> pWavelengths;
   VERIFY(pWavelengths->initializeFromDynamicObject(pRaster->getMetadata(), false));
   if (pWavelengths->getNumWavelengths() < 2)  // need more than 1 band
   {
      updateProgress("Raster element does not contain sufficient wavelength information", 0, ERRORS);
      return false;
   }

   // get pointers to library manager and results window plug-ins
   Service<PlugInManagerServices> pPlugInMgr;
   std::vector<PlugIn*> plugIns = pPlugInMgr->getPlugInInstances(SpectralLibraryMatch::getNameLibraryManagerPlugIn());
   VERIFY(!plugIns.empty());
   SpectralLibraryManager* pLibMgr = dynamic_cast<SpectralLibraryManager*>(plugIns.front());
   VERIFY(pLibMgr != NULL);
   if (isBatch() && pLibMgr->isEmpty())
   {
      updateProgress("The Spectral Library is empty.", 0, ERRORS);
      return false;
   }
   plugIns =  pPlugInMgr->getPlugInInstances(SpectralLibraryMatch::getNameLibraryMatchResultsPlugIn());
   VERIFY(!plugIns.empty());
   SpectralLibraryMatchResults* pResults = dynamic_cast<SpectralLibraryMatchResults*>(plugIns.front());
   VERIFY(pResults != NULL);

   SpectralLibraryMatch::MatchResults theResults;
   theResults.mpRaster = pRaster;
   AoiElement* pAoi(NULL);
   std::string resultsLayerName;
   bool matchEachPixel(false);

   SpectralLibraryMatch::MatchLimits limits;  // c'tor initialized instance to user option settings
   if (isBatch() == false)
   {
      // check that at least one aoi exists for the raster element
      std::vector<DataElement*> aoiElements = Service<ModelServices>()->getElements(pRaster,
         TypeConverter::toString<AoiElement>());
      if (aoiElements.empty())
      {
         Service<DesktopServices>()->showMessageBox("Spectral Library Match", "This plug-in requires an AOI. Please "
            "create at least one AOI before running this plug-in.");
         return false;
      }

      MatchIdDlg dlg(pRaster, Service<DesktopServices>()->getMainWidget());
      if (dlg.exec() == QDialog::Rejected)
      {
         return false;
      }

      theResults.mAlgorithmUsed = dlg.getMatchAlgorithm();
      pAoi = dlg.getAoi();
      VERIFY(pAoi != NULL);
      limits.setLimitByNum(dlg.getLimitByNumber());
      limits.setMaxNum(dlg.getMaxMatches());
      limits.setLimitByThreshold(dlg.getLimitByThreshold());
      limits.setThresholdLimit(dlg.getThresholdLimit());
      resultsLayerName = dlg.getLayerName();
      matchEachPixel = dlg.getMatchEachPixel();
   }
   else
   {
      VERIFY(pInArgList->getPlugInArgValue("AOI Element", pAoi));
      VERIFY(pAoi != NULL);
      VERIFY(pInArgList->getPlugInArgValue("Match each Pixel", matchEachPixel));
      std::string algStr;
      VERIFY(pInArgList->getPlugInArgValue("Match Algorithm Name", algStr));
      theResults.mAlgorithmUsed = StringUtilities::fromDisplayString<SpectralLibraryMatch::MatchAlgorithm>(algStr);
      VERIFY(theResults.mAlgorithmUsed.isValid());
      bool limitByNum(SpectralLibraryMatchOptions::getSettingLimitByMaxNum());
      unsigned int maxMatches(SpectralLibraryMatchOptions::getSettingMaxDisplayed());
      bool limitByThreshold(SpectralLibraryMatchOptions::getSettingLimitByThreshold());
      double threshold(SpectralLibraryMatchOptions::getSettingMatchThreshold());
      VERIFY(pInArgList->getPlugInArgValue("Limit max number of matches", limitByNum));
      VERIFY(pInArgList->getPlugInArgValue("Max number of matches", maxMatches));
      VERIFY(pInArgList->getPlugInArgValue("Limit matches by threshold", limitByThreshold));
      VERIFY(pInArgList->getPlugInArgValue("Threshold cutoff for match", threshold));
      limits.setLimitByNum(limitByNum);
      limits.setMaxNum(maxMatches);
      limits.setLimitByThreshold(limitByThreshold);
      limits.setThresholdLimit(threshold);
      resultsLayerName = "Spectral Library Match Results for " + pAoi->getName();
   }

   // get library info
   const std::vector<Signature*> libSignatures = pLibMgr->getLibrarySignatures();
   const RasterElement* pLib = pLibMgr->getLibraryData(pRaster);
   if (libSignatures.empty() || pLib == NULL)
   {
      updateProgress("Unable to obtain library data.", 0, ERRORS);
      return false;
   }

   // now find matches
   if (matchEachPixel)  // send output to results window and generate pseudocolor layer
   {
      std::vector<SpectralLibraryMatch::MatchResults> pixelResults;
      std::vector<std::pair<Signature*, float> > bestMatches;
      std::vector<std::string> pixelNames;
      int numSigs = static_cast<int>(pAoi->getPixelCount());
      int numProcessed(0);
      bestMatches.reserve(numSigs);
      pixelResults.reserve(numSigs);
      pixelNames.reserve(numSigs);
      const RasterDataDescriptor* pDesc = dynamic_cast<RasterDataDescriptor*>(pRaster->getDataDescriptor());
      VERIFY(pDesc != NULL);

      // get scaling factor
      const Units* pUnits = pDesc->getUnits();
      VERIFY(pUnits != NULL);
      double scaleFactor = pUnits->getScaleFromStandard();

      //get number of bands
      unsigned int numBands = pDesc->getBandCount();

      // get data type
      EncodingType eType = pDesc->getDataType();

      FactoryResource<DataRequest> pRqt;
      pRqt->setInterleaveFormat(BIP);
      DataAccessor acc = pRaster->getDataAccessor(pRqt.release());
      BitMaskIterator bit(pAoi->getSelectedPoints(), pRaster);
      VERIFY(bit != bit.end());
      theResults.mTargetValues.resize(numBands);
      while (bit != bit.end())
      {
         Opticks::PixelLocation pixel(bit.getPixelColumnLocation(), bit.getPixelRowLocation());
         theResults.mTargetName = "Pixel (" + StringUtilities::toDisplayString<int>(pixel.mX + 1) + ", " +
            StringUtilities::toDisplayString<int>(pixel.mY + 1) + ")";
         acc->toPixel(pixel.mY, pixel.mX);
         VERIFY(acc.isValid());
         switchOnEncoding(eType, SpectralLibraryMatch::getScaledPixelValues, acc->getColumn(),
            theResults.mTargetValues, numBands, scaleFactor);
         if (SpectralLibraryMatch::findSignatureMatches(pLib, libSignatures, theResults, limits))
         {
            pixelResults.push_back(theResults);
         }
         if (isAborted())
         {
            updateProgress("Spectral Library Match aborted by user.", 0, ABORT);
            return false;
         }
         if (theResults.mResults.empty() == false)  // only save if there was a best match
         {
            bestMatches.push_back(theResults.mResults.front());
            pixelNames.push_back(theResults.mTargetName);
         }
         ++numProcessed;
         bit.nextPixel();
         updateProgress("Matching AOI pixels...", 100 * numProcessed / numSigs, NORMAL);
      }
      updateProgress("Finished matching AOI pixels.", 100, NORMAL);
      std::map<Signature*, ColorType> colorMap;
      generatePseudocolorLayer(bestMatches, colorMap, pixelNames, resultsLayerName);

      // now output results
      updateProgress("Outputting results for Spectral Library Match on each pixel...", 0, NORMAL);
      int numResults = static_cast<int>(pixelResults.size());
      for (unsigned int index = 0; index < pixelResults.size(); ++index)
      {
         pResults->addResults(pixelResults[index], colorMap, mpProgress);
         if (isAborted())
         {
            updateProgress("Spectral Library Match aborted by user.", 0, ABORT);
            return false;
         }
         updateProgress("Outputting results for Spectral Library Match on each pixel...", 100 * index / numResults, NORMAL);
      }
      updateProgress("Finished outputting results for Spectral Library Match on each pixel.", 100, NORMAL);
   }
   else  // match aoi average signature - just send output to results window (no pseudocolor layer generated)
   {
      std::string avgSigName = pAoi->getName() + " Average Signature";
      Service<ModelServices> pModel;
      Signature* pSignature = static_cast<Signature*>(pModel->getElement(avgSigName,
         TypeConverter::toString<Signature>(), pRaster));
      if (pSignature == NULL)
      {
         pSignature = static_cast<Signature*>(pModel->createElement(avgSigName,
            TypeConverter::toString<Signature>(), pRaster));
      }
      if (SpectralUtilities::convertAoiToSignature(pAoi, pSignature, pRaster,
         mpProgress, &mAborted) == false)
      {
         if (isAborted())
         {
            updateProgress("Spectral Library Match aborted by user.", 0, ABORT);
            return false;
         }
         updateProgress("Unable to obtain the average spectrum for the AOI.", 0, ERRORS);
         return false;
      }

      theResults.mTargetName = pSignature->getDisplayName(true);
      VERIFY(SpectralLibraryMatch::getScaledValuesFromSignature(theResults.mTargetValues, pSignature));
      if (SpectralLibraryMatch::findSignatureMatches(pLib, libSignatures, theResults, limits))
      {
         pResults->addResults(theResults, mpProgress);
      }
      if (isAborted())
      {
         updateProgress("Spectral Library Match aborted by user.", 0, ABORT);
         return false;
      }
   }

   return true;
}

void SpectralLibraryMatchId::updateProgress(const std::string& msg, int percent, ReportingLevel level)
{
   if (mpProgress != NULL)
   {
      mpProgress->updateProgress(msg, percent, level);
   }
}

bool SpectralLibraryMatchId::generatePseudocolorLayer(std::vector<std::pair<Signature*, float> >& bestMatches,
                                                      std::map<Signature*, ColorType>& colorMap,
                                                      std::vector<std::string>& pixelNames,
                                                      const std::string& layerName)
{
   VERIFY(bestMatches.size() == pixelNames.size() && layerName.empty() == false);

   SpatialDataView* pView = dynamic_cast<SpatialDataView*>(Service<DesktopServices>()->getCurrentWorkspaceWindowView());
   VERIFY(pView != NULL);

   PseudocolorLayer* pLayer(NULL);
   LayerList* pLayerList = pView->getLayerList();
   VERIFY(pLayerList != NULL);

   // check for old layer and delete it if it exists
   pLayer = static_cast<PseudocolorLayer*>(pLayerList->getLayer(PSEUDOCOLOR, NULL, layerName));
   if (pLayer != NULL)
   {
      std::string elemName = pLayer->getDataElement()->getName();
      pView->deleteLayer(pLayer);

      // check to be sure data element was destroyed
      Service<ModelServices> pModel;
      DataElement* pDataElement = pModel->getElement(elemName, TypeConverter::toString<RasterElement>(),
         pLayerList->getPrimaryRasterElement());
      pModel->destroyElement(pDataElement);
   }

   // determine number of classes
   std::map<Signature*, int> classes;
   for (std::vector<std::pair<Signature*, float> >::iterator it = bestMatches.begin(); it != bestMatches.end(); ++it)
   {
      if (classes.find(it->first) == classes.end())
      {
         classes.insert(std::pair<Signature*, int>(it->first, 1));  // we'll assign real class values after map built
      }
   }
   int classValue(1); // reserve 0 for unclassified pixels
   for (std::map<Signature*, int>::iterator it = classes.begin(); it != classes.end(); ++it)
   {
      it->second = classValue;
      ++classValue;
   }
   unsigned int numClasses = classes.size();

   // get primary raster and create data element for the pseudocolor layer
   EncodingType dataType = getSmallestType(numClasses);
   VERIFY(dataType.isValid());
   RasterElement* pPrimaryRaster = pLayerList->getPrimaryRasterElement();
   VERIFY(pPrimaryRaster != NULL);
   const RasterDataDescriptor* pDesc = dynamic_cast<RasterDataDescriptor*>(pPrimaryRaster->getDataDescriptor());
   VERIFY(pDesc != NULL);
   RasterElement* pRaster = RasterUtilities::createRasterElement(layerName, pDesc->getRowCount(), pDesc->getColumnCount(),
      dataType, true, pPrimaryRaster);
   VERIFY(pRaster != NULL);

   pLayer = static_cast<PseudocolorLayer*>(pView->createLayer(PSEUDOCOLOR, pRaster, layerName));
   VERIFY(pLayer != NULL);

   //Get colors for the classes
   std::vector<ColorType> layerColors;
   if (numClasses > 0)
   {
      std::vector<ColorType> excludeColors;
      excludeColors.push_back(ColorType(0, 0, 0));
      excludeColors.push_back(ColorType(255, 255, 255));
      VERIFY(ColorType::getUniqueColors(numClasses, layerColors, excludeColors) == numClasses);
   }

   colorMap.clear();
   for (std::map<Signature*, int>::iterator it = classes.begin(); it != classes.end(); ++it)
   {
      VERIFY(pLayer->addInitializedClass(it->first->getName(), it->second, layerColors[it->second - 1]) != -1);
      colorMap.insert(std::pair<Signature*, ColorType>(it->first, layerColors[it->second - 1]));
   }

   // now populate the layer's raster element
   FactoryResource<DataRequest> pRqt;
   pRqt->setWritable(true);
   DataAccessor pAcc = pRaster->getDataAccessor(pRqt.release());
   VERIFY(pAcc->isValid());
   bool somePixelsNotMarked(false);
   for (unsigned int index = 0; index < pixelNames.size(); ++index)
   {
      Opticks::PixelLocation pixel = getLocationFromPixelName(pixelNames[index]);
      if (pixel.mX < 0 || pixel.mY < 0)
      {
         somePixelsNotMarked = true;
         continue;
      }
      pAcc->toPixel(pixel.mX, pixel.mY);
      VERIFY(pAcc->isValid());
      std::map<Signature*, int>::iterator cit = classes.find(bestMatches[index].first);
      if (cit != classes.end())
      {
         switchOnEncoding(dataType, setValue, pAcc->getColumn(), cit->second);
      }
      else
      {
         somePixelsNotMarked = true;
      }
   }
   pView->addLayer(pLayer);

   if (somePixelsNotMarked)
   {
      updateProgress("Some pixels in the AOI could not be processed.", 99, WARNING);
   }
   return true;
}

Opticks::PixelLocation SpectralLibraryMatchId::getLocationFromPixelName(const std::string& pixelName) const
{
   Opticks::PixelLocation pixel(-1, -1);
   if (pixelName.empty())
   {
      return pixel;
   }
   std::string name = pixelName;
   std::string::size_type openParen = name.find('(');
   std::string::size_type closeParen = name.find(')');
   if (openParen == std::string::npos || closeParen == std::string::npos)
   {
      return pixel;
   }

   name = name.substr(openParen +1, closeParen - openParen - 1);
   std::vector<std::string> valStrs = StringUtilities::split(name, ',');
   if (valStrs.size() != 2)
   {
      return pixel;
   }

   std::stringstream values;
   values << valStrs.front() << valStrs.back();
   values >> pixel.mY >> pixel.mX;

   // adjust for display pixel values being 1 based and actual pixels values being 0 based
   pixel.mX -= 1;
   pixel.mY -= 1;

   return pixel;
}

EncodingType SpectralLibraryMatchId::getSmallestType(unsigned int numClasses) const
{
   EncodingType eType;
   if (numClasses < std::numeric_limits<unsigned char>::max())
   {
      eType = INT1UBYTE;
   }
   else if (numClasses < std::numeric_limits<unsigned short>::max())
   {
      eType = INT2UBYTES;
   }

   // anything more is ridiculous, return invalid enum

   return eType;
}

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
#include "DataElement.h"
#include "DataRequest.h"
#include "DesktopServices.h"
#include "DynamicObject.h"
#include "Filename.h"
#include "FileResource.h"
#include "LayerList.h"
#include "MatchIdDlg.h"
#include "MessageLogResource.h"
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
#include "SignatureSet.h"
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
#include "XercesIncludes.h"
#include "xmlwriter.h"

#include <limits>
#include <map>
#include <math.h>
#include <sstream>
#include <vector>

#include <QtCore/QDate>
#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QString>
#include <QtCore/QTextStream>
#include <QtCore/QTime>

XERCES_CPP_NAMESPACE_USE

REGISTER_PLUGIN_BASIC(SpectralSpectralLibraryMatch, SpectralLibraryMatchId);

namespace
{
   template<class T>
   void setValue(T* pData, int& classId)
   {
      *pData = static_cast<T>(classId);
   }
}

SpectralLibraryMatchId::SpectralLibraryMatchId() :
   mpProgress(NULL),
   mpStep(NULL),
   mpResultsWindow(NULL)
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
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL, Executable::ProgressArgDescription()));
   VERIFY(pArgList->addArg<RasterElement>(Executable::DataElementArg(), NULL, "The raster element to match against "
      "the signatures in the spectral library."));

   if (isBatch()) // need additional info in batch mode
   {
      VERIFY(pArgList->addArg<AoiElement>("AOI Element", NULL,
         "The AOI over which to limit spectral library matching."));
      VERIFY(pArgList->addArg<bool>("Match each Pixel", false, "Flag to match each pixel in the AOI to the library. "
         "If false, an average signature is generated for the AOI and only it is matched to the library signatures. "
         "Default is false."));

      // build list of valid match algorithm names for arg description
      std::string matchAlgDesc = "Valid algorithm names are:";
      std::vector<std::string> algNames =
         StringUtilities::getAllEnumValuesAsXmlString<SpectralLibraryMatch::MatchAlgorithm>();
      for (std::vector<std::string>::iterator it = algNames.begin(); it != algNames.end(); ++it)
      {
         matchAlgDesc += "\n";
         matchAlgDesc += *it;
      }
      VERIFY(pArgList->addArg<std::string>("Match Algorithm Name",
         SpectralLibraryMatchOptions::getSettingMatchAlgorithm(), matchAlgDesc));
      VERIFY(pArgList->addArg<bool>("Limit max number of matches", true,
         "Flag to limit the maximum number of matches returned for each pixel or an AOI average. Default is true."));
      VERIFY(pArgList->addArg<unsigned int>("Max number of matches", 5,
         "The maximum number of matches returned. Default is 5."));
      VERIFY(pArgList->addArg<bool>("Limit matches by threshold", true,
         "Flag to filter the matches returned for each pixel or an AOI average by a threshold. Default is true."));
      VERIFY(pArgList->addArg<double>("Threshold cutoff for match", 5.0,
         "The floating point value of the threshold filter. "
         "How the filter is applied is dependent on the match algorithm used. Default is 5.0."));
      VERIFY(pArgList->addArg<bool>("Clear", false,
         "Delete any current signatures in the spectral library "
         "before loading new signatures. Default is false"));
      VERIFY(pArgList->addArg<DataElement>("Signatures Data Element", NULL,
         "The SignatureSet or SignatureLibrary containing the signatures to be loaded into the Spectral Library. "
         "Optional for Opticks but it must be specified when run in OpticksBatch."));
      VERIFY(pArgList->addArg<Filename>("Match Results Filename", NULL, "Filename for saving the match results. "
         "Optional for Opticks but must be specified when run in OpticksBatch. If specified for Opticks, the match "
         "results will be saved to this file and not displayed in the Spectral Library Match Results window."));
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
   StepResource pStep("Spectral Library Match ID", "spectral", "69CC4341-1F5E-4BD6-9CF9-9C85F4CB7CF6");
   mpStep = pStep.get();

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
   plugIns =  pPlugInMgr->getPlugInInstances(SpectralLibraryMatch::getNameLibraryMatchResultsPlugIn());
   if (!plugIns.empty())
   {
      mpResultsWindow = dynamic_cast<SpectralLibraryMatchResults*>(plugIns.front()); // will be NULL for OpticksBatch
   }

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
         std::string errMsg("The Spectral Library Match plug-in requires an AOI. Please "
            "create at least one AOI for the raster element before running this plug-in.");
         updateProgress(errMsg, 0, ERRORS);
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
      pAoi = pInArgList->getPlugInArgValue<AoiElement>("AOI Element");
      if (pAoi == NULL)
      {
         std::string errMsg("The input argument \"AOI Element\" is NULL. "
            "The Spectral Library Match plug-in requires an AOI.");
         updateProgress(errMsg, 0, ERRORS);
         return false;
      }
      VERIFY(pInArgList->getPlugInArgValue("Match each Pixel", matchEachPixel));
      std::string algStr;
      VERIFY(pInArgList->getPlugInArgValue("Match Algorithm Name", algStr));
      theResults.mAlgorithmUsed = StringUtilities::fromXmlString<SpectralLibraryMatch::MatchAlgorithm>(algStr);

      // check if arg value is from older wizard (used to use the display name for the algorithm);
      if (theResults.mAlgorithmUsed.isValid() == false)
      {
         theResults.mAlgorithmUsed = StringUtilities::fromDisplayString<SpectralLibraryMatch::MatchAlgorithm>(algStr);
      }
      if (theResults.mAlgorithmUsed.isValid() == false)
      {
         updateProgress("The input match algorithm name is invalid.", 0, ERRORS);
         return false;
      }
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
      bool clearLibrary;
      VERIFY(pInArgList->getPlugInArgValue("Clear", clearLibrary));

      // check for data element and load if one specified.
      DataElement* pSigData = pInArgList->getPlugInArgValue<DataElement>("Signatures Data Element");

      // check if running from OpticksBatch that a signature source is provided
      if (Service<ApplicationServices>()->isBatch() && pSigData == NULL)
      {
         updateProgress("No source was provided for the signatures to load into the Spectral Library.", 0, ERRORS);
         return false;
      }

      if (clearLibrary)
      {
         pLibMgr->clearLibrary();
      }

      if (pSigData != NULL)
      {
         if (!loadLibraryFromDataElement(pLibMgr, pSigData) && pLibMgr->isEmpty()) // sigs may have already been loaded
         {
            updateProgress("Error occurred while trying to load signatures from data element:\n" +
               pSigData->getDisplayName(true), 0, ERRORS);
            return false;
         }
      }

      // check for match results filename
      Filename* pResultFilename = pInArgList->getPlugInArgValue<Filename>("Match Results Filename");
      if (pResultFilename != NULL)
      {
         mMatchResultsFilename = pResultFilename->getFullPathAndName();
      }
   }

   // get library info
   if (pLibMgr->isEmpty())
   {
      updateProgress("The Spectral Library is empty.", 0, ERRORS);
      return false;
   }
   const RasterElement* pLib = pLibMgr->getResampledLibraryData(pRaster);
   if (pLib == NULL)
   {
      updateProgress("Unable to obtain library data.", 0, ERRORS);
      return false;
   }
   const std::vector<Signature*>* pLibSignatures = pLibMgr->getResampledLibrarySignatures(pLib);
   VERIFY(pLibSignatures != NULL && pLibSignatures->empty() == false);

   // now find matches
   std::vector<SpectralLibraryMatch::MatchResults> pixelResults;
   std::map<Signature*, ColorType> colorMap;
   if (matchEachPixel)  // send output to results window and generate pseudocolor layer
   {
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

         // convert to original pixel values for display
         Opticks::PixelLocation display;
         display.mX = static_cast<int>(pDesc->getActiveColumn(pixel.mX).getOriginalNumber());
         display.mY = static_cast<int>(pDesc->getActiveRow(pixel.mY).getOriginalNumber());
         theResults.mTargetName = "Pixel (" + StringUtilities::toDisplayString<int>(display.mX + 1) + ", " +
            StringUtilities::toDisplayString<int>(display.mY + 1) + ")";
         acc->toPixel(pixel.mY, pixel.mX);
         VERIFY(acc.isValid());
         switchOnEncoding(eType, SpectralLibraryMatch::getScaledPixelValues, acc->getColumn(),
            theResults.mTargetValues, numBands, scaleFactor);
         if (SpectralLibraryMatch::findSignatureMatches(pLib, *pLibSignatures, theResults, limits))
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
      generatePseudocolorLayer(bestMatches, colorMap, pixelNames, resultsLayerName);

      // now output results
      bool success = outputResults(pixelResults, limits, colorMap);
      if (isAborted())
      {
         updateProgress("Spectral Library Match aborted by user.", 0, ABORT);
         return false;
      }
      return success;
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
      if (SpectralLibraryMatch::findSignatureMatches(pLib, *pLibSignatures, theResults, limits))
      {
         pixelResults.push_back(theResults);
         if (outputResults(pixelResults, limits, colorMap) == false)
         {
            return false;
         }
      }
      if (isAborted())
      {
         updateProgress("Spectral Library Match aborted by user.", 0, ABORT);
         return false;
      }
   }

   updateProgress("Spectral Library Match completed", 100, NORMAL);
   return true;
}

void SpectralLibraryMatchId::updateProgress(const std::string& msg, int percent, ReportingLevel level)
{
   if (mpProgress != NULL)
   {
      mpProgress->updateProgress(msg, percent, level);
   }
   if (mpStep != NULL)
   {
      switch (level)
      {
      case ERRORS:
         mpStep->finalize(Message::Failure, msg);
         break;

      case ABORT:
         mpStep->finalize(Message::Abort, msg);
         break;

      case NORMAL:
         if (percent == 100)
         {
            mpStep->finalize(Message::Success);
         }
         break;

      default:
         // nothing to do
         break;
      }
   }
}

bool SpectralLibraryMatchId::generatePseudocolorLayer(std::vector<std::pair<Signature*, float> >& bestMatches,
                                                      std::map<Signature*, ColorType>& colorMap,
                                                      std::vector<std::string>& pixelNames,
                                                      const std::string& layerName)
{
   VERIFY(bestMatches.size() == pixelNames.size() && layerName.empty() == false);

   SpatialDataView* pView = dynamic_cast<SpatialDataView*>(Service<DesktopServices>()->getCurrentWorkspaceWindowView());
   if (pView == NULL)
   {
      return false;
   }

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
      Opticks::PixelLocation pixel = getLocationFromPixelName(pixelNames[index], pDesc);
      if (pixel.mX < 0 || pixel.mY < 0)
      {
         somePixelsNotMarked = true;
         continue;
      }
      pAcc->toPixel(pixel.mY, pixel.mX);
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

Opticks::PixelLocation SpectralLibraryMatchId::getLocationFromPixelName(const std::string& pixelName,
                                                                        const RasterDataDescriptor* pDesc) const
{
   Opticks::PixelLocation pixel(-1, -1);
   if (pixelName.empty() || pDesc == NULL)
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
   values >> pixel.mX >> pixel.mY;

   // adjust for display pixel values being 1 based and actual pixels values being 0 based
   pixel.mX -= 1;
   pixel.mY -= 1;

   // convert values from original to active
   pixel.mX = pDesc->getOriginalColumn(pixel.mX).getActiveNumber();
   pixel.mY = pDesc->getOriginalRow(pixel.mY).getActiveNumber();

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

bool SpectralLibraryMatchId::outputResults(std::vector<SpectralLibraryMatch::MatchResults>& theResults,
                                           SpectralLibraryMatch::MatchLimits& limits,
                                           const std::map<Signature*, ColorType>& colorMap)
{
   if (theResults.empty())
   {
      updateProgress("No results to output.", 0, ERRORS);
      return false;
   }

   // now if a filename was specified, output to it, otherwise send to the results window
   if (mMatchResultsFilename.empty() == false)
   {
      return writeResultsToFile(theResults, limits, mMatchResultsFilename);
   }

   if (mpResultsWindow != NULL)
   {
      return sendResultsToWindow(theResults, colorMap);
   }

   updateProgress("No destinations are available for outputting the match results.", 0, ERRORS);
  
   return false;
}

bool SpectralLibraryMatchId::writeResultsToFile(std::vector<SpectralLibraryMatch::MatchResults>& theResults,
                                                SpectralLibraryMatch::MatchLimits& limits, const std::string& filename)
{
   if (theResults.empty() || filename.empty())
   {
      return false;
   }

   // make sure filename has extension of ".slim"
   QString actualFilename = QString::fromStdString(filename);
   QString extension(".slim");
   if (!actualFilename.endsWith(extension, Qt::CaseInsensitive))
   {
      actualFilename += extension;
   }

   // make sure we can create the file for output
   QFile file(actualFilename);
   if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
   {
      updateProgress("Unable to open file for saving Spectral Library Match results.", 0, ERRORS);
      return false;
   }

   QTextStream out(&file);

   // we will replace any embedded tabs in data set, target and signature names with 4 spaces.
   QString tabRepl("    ");

   // write header info
   out << "OID" << "\t" << "oid:/UID/Opticks/3/0/1" << "\n";
   out << "AnalysisTime" << "\t" << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
   out << "Dataset" << "\t" <<
      QString::fromStdString(theResults.front().mpRaster->getName()).replace("\t", tabRepl) << "\n";
   out << "MatchAlgorithm" << "\t" << QString::fromStdString(
      StringUtilities::toXmlString<SpectralLibraryMatch::MatchAlgorithm>(theResults.front().mAlgorithmUsed)) << "\n";
   out << "MatchAlgorithmOID" << "\t" << "oid:/UUID/Opticks/0/12/0" << "\t" << "oid:/UUID/Opticks/2/1" << "\n";
   bool limitByNum = limits.getLimitByNum();
   out << "Max number of matches" << "\t";
   if (limitByNum)
   {
      out <<  limits.getMaxNum() << "\n";
   }
   else
   {
      out << "Not limited\n";
   }
   bool limitByThres = limits.getLimitByThreshold();
   out << "Algorithm threshold" << "\t";
   if (limitByThres)
   {
      out << QString::number(limits.getThresholdLimit(), 'f', 4) << "\n";
   }
   else
   {
      out << "Not limited\n";
   }
   out << "Target Name";

   // find max number of matches for a pixel so we can output correct number of header columns
   unsigned int maxMatches(0);
   for (std::vector<SpectralLibraryMatch::MatchResults>::iterator it = theResults.begin();
      it != theResults.end(); ++it)
   {
      if (it->isValid() == false)
      {
         continue;
      }
      unsigned int numMatches = it->mResults.size();
      if (numMatches > maxMatches)
      {
         maxMatches = numMatches;
      }
   }
   for (unsigned int i = 0; i < maxMatches; ++i)
   {
      out << "\t" << "Signature Name" << "\t" << "Match Value";
   }
   out << "\n";
   for (std::vector<SpectralLibraryMatch::MatchResults>::iterator it = theResults.begin();
      it != theResults.end(); ++it)
   {
      if (it->isValid() == false)
      {
         continue;
      }

      out << QString::fromStdString(it->mTargetName).replace("\t", tabRepl);
      if (it->mResults.empty())
      {
         out << "\t" << "No matches found\n";
      }
      else
      {
         for (std::vector<std::pair<Signature*, float> >::iterator sit = it->mResults.begin();
            sit != it->mResults.end(); ++sit)
         {
            out << "\t" << QString::fromStdString(sit->first->getName()).replace("\t", tabRepl) << "\t" << sit->second;
         }
         out << "\n";
      }
   }

   file.close();

   return true;
}

bool SpectralLibraryMatchId::sendResultsToWindow(std::vector<SpectralLibraryMatch::MatchResults>& theResults,
                                                 const std::map<Signature*, ColorType>& colorMap)
{
   if (mpResultsWindow == NULL)
   {
      return false;
   }
   unsigned int numResults = theResults.size();
   unsigned int numOutput(0);
   updateProgress("Outputting results for Spectral Library Match on each pixel...", 0, NORMAL);
   for (std::vector<SpectralLibraryMatch::MatchResults>::iterator it = theResults.begin(); it != theResults.end(); ++it)
   {
      mpResultsWindow->addResults(*it, colorMap);
      ++numOutput;
      updateProgress("Outputting results for Spectral Library Match on each pixel...",
         100 * numOutput / numResults, NORMAL);
      if (isAborted())
      {
         return false;
      }
   }

   updateProgress("Finished outputting results for Spectral Library Match on each pixel.", 100, NORMAL);
   return true;
}

bool SpectralLibraryMatchId::loadLibraryFromDataElement(SpectralLibraryManager* pLibMgr,
   const DataElement* pSignatureData)
{
   VERIFY(pLibMgr != NULL && pSignatureData != NULL);

   // Note: we're only interested in the signatures so we can also cast a SignatureLibrary element to a SignatureSet.
   const SignatureSet* pSigSet = dynamic_cast<const SignatureSet*>(pSignatureData);
   if (pSigSet == NULL)
   {
      updateProgress("The value for input arg Signatures Data Element was not a valid "
         "SignatureSet or SignatureLibrary.", 0, ERRORS);
      return false;
   }

   return pLibMgr->addSignatures(pSigSet->getSignatures());
}

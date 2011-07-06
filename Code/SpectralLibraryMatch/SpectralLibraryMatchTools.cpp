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
#include "AppVerify.h"
#include "BitMaskIterator.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "Layer.h"
#include "LayerList.h"
#include "MouseMode.h"
#include "ObjectResource.h"
#include "PlugInArg.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "SessionItemDeserializer.h"
#include "SessionItemSerializer.h"
#include "SessionResource.h"
#include "Signature.h"
#include "SignatureSet.h"
#include "Slot.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "SpectralLibraryManager.h"
#include "SpectralLibraryMatch.h"
#include "SpectralLibraryMatchOptions.h"
#include "SpectralLibraryMatchResults.h"
#include "SpectralLibraryMatchTools.h"
#include "SpectralUtilities.h"
#include "SpectralVersion.h"
#include "switchOnEncoding.h"
#include "ToolBar.h"
#include "Wavelengths.h"
#include "WorkspaceWindow.h"
#include "XercesIncludes.h"
#include "xmlreader.h"
#include "xmlwriter.h"

#include <vector>

#include <QtCore/QEvent>
#include <QtGui/QAction>
#include <QtGui/QPixmap>
#include <QtGui/QMouseEvent>

XERCES_CPP_NAMESPACE_USE

REGISTER_PLUGIN_BASIC(SpectralSpectralLibraryMatch, SpectralLibraryMatchTools);

namespace
{
   const static std::string lcSpecLibMouseModeName = "SpectralLibraryMatchMode";

   const char* const AoiAvgMatchIcon[] =
   {
      "16 16 6 1",
      " 	c None",
      ".	c #000000",
      "+	c #800000",
      "@	c #00FFFF",
      "#	c #FFFFFF",
      "$	c #0000FF",
      ".               ",
      ".               ",
      ".               ",
      ".               ",
      ".    ...        ",
      ".  ..   ..    ++",
      ".  .@#@  .+ ++  ",
      ". . #   + .+    ",
      ". . @ ++  .     ",
      ".+. ++   @.     ",
      ".  .    @.      ",
      ".  ..  @.$$     ",
      ".    ... $$$    ",
      ".         $$$   ",
      ".          $$$  ",
      "........... $$ ."
   };

   const char* const AoiPixelMatchIcon[] =
   {
      "16 16 8 1",
      " 	c None",
      ".	c #000000",
      "+	c #000080",
      "@	c #008000",
      "#	c #FF0000",
      "$	c #800000",
      "%	c #00FFFF",
      "&	c #FFFFFF",
      ".               ",
      ".   ++    + +++ ",
      ".  + +++ + + @ +",
      ". + #   +   @ @@",
      ".+ # ...    @   ",
      ". #..#  ..  @ $$",
      ".# .%&%# .$ $$  ",
      ". . &   $ .$@   ",
      ". . % $$# .@  # ",
      ".$. $$   %.  # #",
      ".  . @  %.@##   ",
      ".  .. @%..+     ",
      ".@@  ... +++    ",
      ".      @@ +++   ",
      ".          +++  ",
      "........... ++ ."
   };

   const char* const PixelMatchIcon[] =
   {
      "16 16 9 1",
      " 	c None",
      ".	c #000000",
      "+	c #FFFFFF",
      "@	c #800000",
      "#	c #008000",
      "$	c #C0C0C0",
      "%	c #00FFFF",
      "&	c #000080",
      "*	c #808080",
      "   ....         ",
      "  .++++. .....  ",
      ".+.+@@@+.@++++..",
      ".+.++++...@@@+..",
      ".+.+#..$$$..++..",
      ".+.++.%+%##.#+..",
      ".+.+.$+$$$$$.+..",
      ".+.+.$%$$$$$.+..",
      ".+.........%.+..",
      ".++++.$$$$%.++..",
      ".......$$%&& ...",
      ".******.. &&& *.",
      " .......   &&&  ",
      "            &&& ",
      "             &&&",
      "              &&"
   };
}

SpectralLibraryMatchTools::SpectralLibraryMatchTools() :
   mpProgress(NULL),
   mpResults(NULL),
   mpSignatureWindow(NULL),
   mpLibMgr(NULL),
   mpSpectralLibraryMatchMode(NULL),
   mpPixelMatchAction(NULL),
   mpAoiPixelMatchAction(NULL),
   mpAoiAverageMatchAction(NULL)
{
   ExecutableShell::setName("Spectral Library Match Tools");
   setCreator("Ball Aerospace & Technologies, Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setType("Algorithm");
   setDescription("Tools for matching in-scene spectra to signatures in a spectral library.");
   setDescriptorId("{FEC861EE-3BB1-4daa-8C9B-C4D3DCFC858A}");
   allowMultipleInstances(false);
   executeOnStartup(true);
   destroyAfterExecute(false);
   setAbortSupported(true);
   setWizardSupported(false);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

SpectralLibraryMatchTools::~SpectralLibraryMatchTools()
{
   ToolBar* pToolBar = static_cast<ToolBar*>(mpDesktop->getWindow("Spectral", TOOLBAR));
   if (pToolBar != NULL)
   {
      // Remove the toolbar buttons
      if (mpPixelMatchAction != NULL)
      {
         pToolBar->removeItem(mpPixelMatchAction);
         delete mpPixelMatchAction;
      }

      if (mpAoiPixelMatchAction != NULL)
      {
         disconnect(mpAoiPixelMatchAction, SIGNAL(activated()), this, SLOT(matchAoiPixels()));
         pToolBar->removeItem(mpAoiPixelMatchAction);
         delete mpAoiPixelMatchAction;
      }
      if (mpAoiAverageMatchAction != NULL)
      {
         disconnect(mpAoiAverageMatchAction, SIGNAL(activated()), this, SLOT(matchAoiAverageSpectrum()));
         pToolBar->removeItem(mpAoiAverageMatchAction);
         delete mpAoiAverageMatchAction;
      }
   }

   // Detach from desktop services
   mpDesktop->detach(SIGNAL_NAME(DesktopServices, WindowAdded), Slot(this, &SpectralLibraryMatchTools::windowAdded));
   mpDesktop->detach(SIGNAL_NAME(DesktopServices, WindowActivated),
      Slot(this, &SpectralLibraryMatchTools::windowActivated));
   mpDesktop->detach(SIGNAL_NAME(DesktopServices, WindowRemoved),
      Slot(this, &SpectralLibraryMatchTools::windowRemoved));

   // Remove the mouse mode from the views
   std::vector<Window*> windows;
   mpDesktop->getWindows(SPATIAL_DATA_WINDOW, windows);

   for (std::vector<Window*>::iterator iter = windows.begin(); iter != windows.end(); ++iter)
   {
      SpatialDataWindow* pWindow = static_cast<SpatialDataWindow*>(*iter);
      if (pWindow != NULL)
      {
         removePixelMatchMode(pWindow->getSpatialDataView());
      }
   }

   // Delete the spectral library matching mouse mode
   if (mpSpectralLibraryMatchMode != NULL)
   {
      mpDesktop->deleteMouseMode(mpSpectralLibraryMatchMode);
   }
}

bool SpectralLibraryMatchTools::getInputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return !isBatch();
}

bool SpectralLibraryMatchTools::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return !isBatch();
}

bool SpectralLibraryMatchTools::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (isBatch() == true)
   {
      return false;
   }

   // Create the progress object and the progress dialog
   Service<PlugInManagerServices> pMgr;
   mpProgress = pMgr->getProgress(this);
   if (mpProgress != NULL)
   {
      mpDesktop->createProgressDialog(getName(), mpProgress);
   }

   // Create the pixel match action
   QPixmap pixPixelMatch(PixelMatchIcon);
   mpPixelMatchAction = new QAction(QIcon(pixPixelMatch), "&Find spectral library matches for a pixel", this);
   mpPixelMatchAction->setAutoRepeat(false);
   mpPixelMatchAction->setCheckable(true);
   mpPixelMatchAction->setStatusTip("Find the best library matches for the\n"
      "signature of a pixel selected with the mouse");

   // Create the AOI pixel match action
   QPixmap pixAoiPixelMatch(AoiPixelMatchIcon);
   mpAoiPixelMatchAction = new QAction(QIcon(pixAoiPixelMatch),
      "&Find spectral library matches for each pixel in the AOI", this);
   mpAoiPixelMatchAction->setAutoRepeat(false);
   mpAoiPixelMatchAction->setStatusTip("Find the best library matches for each "
      "pixel in the active AOI layer");
   VERIFYNR(connect(mpAoiPixelMatchAction, SIGNAL(triggered()), this, SLOT(matchAoiPixels())));

   // Create the AOI average matching action
   QPixmap pixAoiAverageMatch(AoiAvgMatchIcon);
   mpAoiAverageMatchAction = new QAction(QIcon(pixAoiAverageMatch),
      "&Find spectral library matches for the AOI Average Signature", this);
   mpAoiAverageMatchAction->setAutoRepeat(false);
   mpAoiAverageMatchAction->setStatusTip("Find the best library matches for the average signature of the "
      "selected pixels in the active AOI layer");
   VERIFYNR(connect(mpAoiAverageMatchAction, SIGNAL(triggered()), this, SLOT(matchAoiAverageSpectrum())));

   ToolBar* pToolBar = static_cast<ToolBar*>(mpDesktop->getWindow("Spectral", TOOLBAR));
   if (pToolBar != NULL)
   {
      pToolBar->addButton(mpPixelMatchAction);
      pToolBar->addButton(mpAoiPixelMatchAction);
      pToolBar->addButton(mpAoiAverageMatchAction);
   }

   // Initialization
   enableActions();

   // Connections
   mpDesktop->attach(SIGNAL_NAME(DesktopServices, WindowAdded), Slot(this, &SpectralLibraryMatchTools::windowAdded));
   mpDesktop->attach(SIGNAL_NAME(DesktopServices, WindowActivated),
      Slot(this, &SpectralLibraryMatchTools::windowActivated));
   mpDesktop->attach(SIGNAL_NAME(DesktopServices, WindowRemoved),
      Slot(this, &SpectralLibraryMatchTools::windowRemoved));

   return true;
}

bool SpectralLibraryMatchTools::setBatch()
{
   ExecutableShell::setBatch();
   return false;
}

bool SpectralLibraryMatchTools::eventFilter(QObject* pObject, QEvent* pEvent)
{
   if ((pObject != NULL) && (pEvent != NULL))
   {
      if (pEvent->type() == QEvent::MouseButtonPress)
      {
         QMouseEvent* pMouseEvent = static_cast<QMouseEvent*> (pEvent);
         if (pMouseEvent->button() == Qt::LeftButton)
         {
            // Lock Session Save while generating and displaying the pixel signature
            SessionSaveLock lock;

            SpatialDataView* pSpatialDataView =
               dynamic_cast<SpatialDataView*>(mpDesktop->getCurrentWorkspaceWindowView());
            if (pSpatialDataView != NULL)
            {
               QWidget* pViewWidget  = pSpatialDataView->getWidget();
               if (pViewWidget == pObject)
               {
                  MouseMode* pMouseMode = pSpatialDataView->getCurrentMouseMode();
                  if (pMouseMode != NULL)
                  {
                     std::string mouseMode = "";
                     pMouseMode->getName(mouseMode);
                     if (mouseMode == lcSpecLibMouseModeName)
                     {
                        QPoint ptMouse = pMouseEvent->pos();
                        ptMouse.setY(pViewWidget->height() - pMouseEvent->pos().y());

                        LocationType pixelCoord;

                        LayerList* pLayerList = pSpatialDataView->getLayerList();
                        VERIFY(pLayerList != NULL);

                        RasterElement* pRaster = pLayerList->getPrimaryRasterElement();
                        VERIFY(pRaster != NULL);

                        // check that raster has wavelength info
                        if (Wavelengths::getNumWavelengths(pRaster->getMetadata()) < 2)
                        {
                           updateProgress("Raster element does not contain sufficient wavelength "
                              "information", 0, ERRORS);
                           return false;
                        }

                        Layer* pLayer = pLayerList->getLayer(RASTER, pRaster);
                        if (pLayer != NULL)
                        {
                           pLayer->translateScreenToData(ptMouse.x(), ptMouse.y(), pixelCoord.mX, pixelCoord.mY);
                        }

                        double dMinX = 0.0;
                        double dMinY = 0.0;
                        double dMaxX = 0.0;
                        double dMaxY = 0.0;
                        pSpatialDataView->getExtents(dMinX, dMinY, dMaxX, dMaxY);

                        if ((pixelCoord.mX >= dMinX) && (pixelCoord.mX <= dMaxX) && (pixelCoord.mY >= dMinY) &&
                           (pixelCoord.mY <= dMaxY))
                        {
                           Signature* pSignature = SpectralUtilities::getPixelSignature(pRaster,
                              Opticks::PixelLocation(pixelCoord.mX, pixelCoord.mY));
                           if (pSignature != NULL)
                           {
                              if (mpLibMgr == NULL)
                              {
                                 initializeConnections();
                              }
                              VERIFY(mpLibMgr != NULL);
                              if (mpLibMgr->isEmpty())
                              {
                                 updateProgress("The Spectral Library is empty. Add signatures to the library "
                                    "by clicking on the Edit Spectral Library toolbar button.", 0, ERRORS);
                                 return false;
                              }
                              const RasterElement* pLib = mpLibMgr->getResampledLibraryData(pRaster);
                              if (pLib == NULL)
                              {
                                 updateProgress("Unable to obtain library data.", 0, ERRORS);
                                 return false;
                              }

                              // populate results struct
                              SpectralLibraryMatch::MatchResults theResults;
                              theResults.mpRaster = pRaster;
                              theResults.mTargetName = pSignature->getDisplayName(true);
                              VERIFY(SpectralLibraryMatch::getScaledValuesFromSignature(theResults.mTargetValues,
                                 pSignature));
                              theResults.mAlgorithmUsed = 
                                 StringUtilities::fromXmlString<SpectralLibraryMatch::MatchAlgorithm>(
                                 SpectralLibraryMatchOptions::getSettingMatchAlgorithm());

                              // get library signatures and generate results
                              const std::vector<Signature*>* pLibSignatures =
                                 mpLibMgr->getResampledLibrarySignatures(pLib);
                              VERIFY(pLibSignatures != NULL && pLibSignatures->empty() == false);
                              if (SpectralLibraryMatch::findSignatureMatches(pLib, *pLibSignatures, theResults))
                              {
                                 // display results in results window
                                 VERIFY(mpResults != NULL);
                                 mpResults->addResults(theResults);

                                 // plot results in signature window
                                 plotResults(pRaster, pSignature, theResults.mResults);
                              }
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   }

   return QObject::eventFilter(pObject, pEvent);
}

void SpectralLibraryMatchTools::windowAdded(Subject& subject, const std::string& signal, const boost::any& value)
{
   if (dynamic_cast<DesktopServices*>(&subject) != NULL)
   {
      Window* pWindow = boost::any_cast<Window*>(value);

      SpatialDataWindow* pSpatialDataWindow = dynamic_cast<SpatialDataWindow*>(pWindow);
      if (pSpatialDataWindow != NULL)
      {
         addPixelMatchMode(pSpatialDataWindow->getSpatialDataView());  // method can handle NULL pointers
      }
   }
}

void SpectralLibraryMatchTools::windowActivated(Subject& subject, const std::string& signal, const boost::any& value)
{
   enableActions();
}

void SpectralLibraryMatchTools::windowRemoved(Subject& subject, const std::string& signal, const boost::any& value)
{
   if (dynamic_cast<DesktopServices*>(&subject) != NULL)
   {
      Window* pWindow = boost::any_cast<Window*>(value);

      SpatialDataWindow* pSpatialDataWindow = dynamic_cast<SpatialDataWindow*>(pWindow);
      if (pSpatialDataWindow != NULL)
      {
         removePixelMatchMode(pSpatialDataWindow->getSpatialDataView());  // method can handle NULL pointers
      }
   }
}

void SpectralLibraryMatchTools::layerActivated(Subject& subject, const std::string& signal, const boost::any& value)
{
   enableActions();
}

void SpectralLibraryMatchTools::updateProgress(const std::string& msg, int percent, ReportingLevel level)
{
   if (mpProgress != NULL)
   {
      mpProgress->updateProgress(msg, percent, level);
   }
}

void SpectralLibraryMatchTools::addPixelMatchMode(SpatialDataView* pView)
{
   if (pView == NULL)
   {
      return;
   }

   pView->attach(SIGNAL_NAME(SpatialDataView, LayerActivated), Slot(this, &SpectralLibraryMatchTools::layerActivated));

   QWidget* pViewWidget = pView->getWidget();
   if (pViewWidget != NULL)
   {
      pViewWidget->installEventFilter(this);
   }

   // Create the pixel matching mouse mode
   if (mpSpectralLibraryMatchMode == NULL)
   {
      mpSpectralLibraryMatchMode = mpDesktop->createMouseMode(lcSpecLibMouseModeName, NULL, NULL, -1, -1,
         mpPixelMatchAction);
   }

   // Add the mode to the view
   if (mpSpectralLibraryMatchMode != NULL)
   {
      pView->addMouseMode(mpSpectralLibraryMatchMode);
   }
}

void SpectralLibraryMatchTools::removePixelMatchMode(SpatialDataView* pView)
{
   if (pView == NULL)
   {
      return;
   }

   pView->detach(SIGNAL_NAME(SpatialDataView, LayerActivated), Slot(this, &SpectralLibraryMatchTools::layerActivated));

   QWidget* pViewWidget = pView->getWidget();
   if (pViewWidget != NULL)
   {
      pViewWidget->removeEventFilter(this);
   }

   if (mpSpectralLibraryMatchMode != NULL)
   {
      pView->removeMouseMode(mpSpectralLibraryMatchMode);
   }
}

void SpectralLibraryMatchTools::enableActions()
{
   bool bActiveWindow = false;
   bool bAoiMode = false;

   SpatialDataWindow* pWindow = dynamic_cast<SpatialDataWindow*>(mpDesktop->getCurrentWorkspaceWindow());
   if (pWindow != NULL)
   {
      bActiveWindow = true;

      SpatialDataView* pView = pWindow->getSpatialDataView();
      if (pView != NULL)
      {
         if (dynamic_cast<AoiLayer*>(pView->getActiveLayer()) != NULL)
         {
            bAoiMode = true;
         }
      }
   }

   if (mpPixelMatchAction != NULL)
   {
      mpPixelMatchAction->setEnabled(bActiveWindow);
   }

   if (mpAoiPixelMatchAction != NULL)
   {
      mpAoiPixelMatchAction->setEnabled(bAoiMode);
   }

   if (mpAoiAverageMatchAction != NULL)
   {
      mpAoiAverageMatchAction->setEnabled(bAoiMode);
   }
}

void SpectralLibraryMatchTools::initializeConnections()
{
   Service<PlugInManagerServices> pPlugInMgr;
   std::vector<PlugIn*> plugIns =
      pPlugInMgr->getPlugInInstances(SpectralLibraryMatch::getNameLibraryMatchResultsPlugIn());
   if (!plugIns.empty())
   {
      mpResults = dynamic_cast<SpectralLibraryMatchResults*>(plugIns.front());
   }
   plugIns = pPlugInMgr->getPlugInInstances("Signature Window");
   if (!plugIns.empty())
   {
      mpSignatureWindow = plugIns.front();
   }
   plugIns = pPlugInMgr->getPlugInInstances(SpectralLibraryMatch::getNameLibraryManagerPlugIn());
   if (!plugIns.empty())
   {
      mpLibMgr = dynamic_cast<SpectralLibraryManager*>(plugIns.front());
   }
}

void SpectralLibraryMatchTools::matchAoiPixels()
{
   // Lock Session Save while finding matches for the AOI avg sig
   SessionSaveLock lock;

   // reset abort flag
   mAborted = false;

   if (mpLibMgr == NULL)
   {
      initializeConnections();
      if (mpLibMgr == NULL)
      {
         updateProgress("Unable to access the spectral library manager.", 0, ERRORS);
         return;
      }
   }
   if (mpLibMgr->isEmpty())
   {
      updateProgress("The Spectral Library is empty. Add signatures to the library by "
         "clicking on the Edit Spectral Library toolbar button.", 0, ERRORS);
      return;
   }

   // Get the current AOI
   AoiElement* pAoi = SpectralLibraryMatch::getCurrentAoi();
   if (pAoi == NULL)
   {
      updateProgress("Unable to access the current AOI element.", 0, ERRORS);
      return;
   }

   // Get the current raster element
   RasterElement* pRaster = SpectralLibraryMatch::getCurrentRasterElement();
   if (pRaster == NULL)
   {
      updateProgress("Unable to access the current raster element.", 0, ERRORS);
      return;
   }

   // check that raster has wavelength info
   if (Wavelengths::getNumWavelengths(pRaster->getMetadata()) < 2)
   {
      updateProgress("Raster element does not contain sufficient wavelength information", 0, ERRORS);
      return;
   }

   SpectralLibraryMatch::MatchResults theResults;
   theResults.mpRaster = pRaster;
   theResults.mAlgorithmUsed = StringUtilities::fromXmlString<SpectralLibraryMatch::MatchAlgorithm>(
      SpectralLibraryMatchOptions::getSettingMatchAlgorithm());

   // get library data
   const RasterElement* pLib = mpLibMgr->getResampledLibraryData(pRaster);
   if (pLib == NULL)
   {
      updateProgress("Unable to obtain library data.", 0, ERRORS);
      return;
   }
   const std::vector<Signature*>* pLibSignatures = mpLibMgr->getResampledLibrarySignatures(pLib);
   VERIFYNRV(pLibSignatures != NULL && pLibSignatures->empty() == false);

   // loop through the aoi spectra and generate sorted results
   updateProgress("Matching AOI pixels...", 0, NORMAL);
   const RasterDataDescriptor* pDesc = dynamic_cast<RasterDataDescriptor*>(pRaster->getDataDescriptor());
   VERIFYNRV(pDesc != NULL);

   // get scaling factor
   const Units* pUnits = pDesc->getUnits();
   VERIFYNRV(pUnits != NULL);
   double scaleFactor = pUnits->getScaleFromStandard();

   //get number of bands
   unsigned int numBands = pDesc->getBandCount();

   // get data type
   EncodingType eType = pDesc->getDataType();

   FactoryResource<DataRequest> pRqt;
   pRqt->setInterleaveFormat(BIP);
   DataAccessor acc = pRaster->getDataAccessor(pRqt.release());
   BitMaskIterator bit(pAoi->getSelectedPoints(), pRaster);
   if (bit == bit.end())  // empty AOI
   {
      updateProgress("There are no selected pixels in the AOI.", 0, ERRORS);
      return;
   }
   theResults.mTargetValues.resize(numBands);
   std::vector<SpectralLibraryMatch::MatchResults> pixelResults;
   std::map<Signature*, ColorType> colorMap;
   int numProcessed(0);
   int numToProcess = bit.getCount();
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
      VERIFYNRV(acc.isValid());
      switchOnEncoding(eType, SpectralLibraryMatch::getScaledPixelValues, acc->getColumn(),
         theResults.mTargetValues, numBands, scaleFactor);
      if (SpectralLibraryMatch::findSignatureMatches(pLib, *pLibSignatures, theResults))
      {
         if (isAborted())
         {
            updateProgress("Spectral Library Match aborted by user.", 0, ABORT);
            return;
         }
         pixelResults.push_back(theResults);
      }
      ++numProcessed;
      bit.nextPixel();
      updateProgress("Matching AOI pixels...", 100 * numProcessed / numToProcess, NORMAL);
   }
   VERIFYNRV(mpResults != NULL);
   mpResults->addResults(pixelResults, colorMap, mpProgress, &mAborted);
   if (isAborted())
   {
      updateProgress("User canceled adding the results from matching AOI pixels to the Results Window.", 0, ABORT);
      return;
   }
   updateProgress("Finished matching AOI pixels.", 100, NORMAL);
}

void SpectralLibraryMatchTools::matchAoiAverageSpectrum()
{
   // Lock Session Save while finding matches for the AOI avg sig
   SessionSaveLock lock;

   // reset abort flag
   mAborted = false;

   if (mpLibMgr == NULL)
   {
      initializeConnections();
      if (mpLibMgr == NULL)
      {
         updateProgress("Unable to access the spectral library manager.", 0, ERRORS);
         return;
      }
   }
   if (mpLibMgr->isEmpty())
   {
      updateProgress("The Spectral Library is empty. Add signatures to the library by "
         "clicking on the Edit Spectral Library toolbar button.", 0, ERRORS);
      return;
   }

   // Get the current AOI
   AoiElement* pAoi = SpectralLibraryMatch::getCurrentAoi();
   if (pAoi == NULL)
   {
      updateProgress("Unable to access the current AOI element.", 0, ERRORS);
      return;
   }

   // Get the current raster element
   RasterElement* pRaster = SpectralLibraryMatch::getCurrentRasterElement();
   if (pRaster == NULL)
   {
      updateProgress("Unable to access the current raster element.", 0, ERRORS);
      return;
   }

   // check that raster has wavelength info
   if (Wavelengths::getNumWavelengths(pRaster->getMetadata()) < 2)
   {
      updateProgress("Raster element does not contain sufficient wavelength information", 0, ERRORS);
      return;
   }

   // populate results struct
   SpectralLibraryMatch::MatchResults theResults;
   theResults.mpRaster = pRaster;
   theResults.mAlgorithmUsed = StringUtilities::fromXmlString<SpectralLibraryMatch::MatchAlgorithm>(
      SpectralLibraryMatchOptions::getSettingMatchAlgorithm());

   // get the aoi avg signature and set in results struct
   std::string avgSigName = pAoi->getName() + " Average Signature";
   Service<ModelServices> pModel;
   Signature* pSignature = static_cast<Signature*>(pModel->getElement(avgSigName,
      TypeConverter::toString<Signature>(), pRaster));
   if (pSignature == NULL)
   {
      pSignature = static_cast<Signature*>(pModel->createElement(avgSigName,
         TypeConverter::toString<Signature>(), pRaster));
   }
   if (SpectralUtilities::convertAoiToSignature(pAoi, pSignature, pRaster, mpProgress, &mAborted) == false)
   {
      updateProgress("Unable to obtain the average spectrum for the AOI.", 0, ERRORS);
      return;
   }
   theResults.mTargetName = pSignature->getDisplayName(true);
   VERIFYNRV(SpectralLibraryMatch::getScaledValuesFromSignature(theResults.mTargetValues, pSignature));

   // get library signatures and generate sorted results
   const RasterElement* pLib = mpLibMgr->getResampledLibraryData(pRaster);
   if (pLib == NULL)
   {
      updateProgress("Unable to obtain library data.", 0, ERRORS);
      return;
   }
   const std::vector<Signature*>* pLibSignatures = mpLibMgr->getResampledLibrarySignatures(pLib);
   VERIFYNRV(pLibSignatures != NULL && pLibSignatures->empty() == false);
   if (SpectralLibraryMatch::findSignatureMatches(pLib, *pLibSignatures, theResults))
   {
      VERIFYNRV(mpResults != NULL);
      mpResults->addResults(theResults, mpProgress);

      plotResults(pRaster, pSignature, theResults.mResults);
   }
}

void SpectralLibraryMatchTools::plotResults(const RasterElement* pRaster, const Signature* pSignature,
                                            std::vector<std::pair<Signature*, float> > matches)
{
   // since the SignatureWindow plug-in doesn't support multiple instances, was executed at start up and wasn't
   // destroyed, we can't create an ExecutableResource using its name - we have to use a pointer to the instance
   // created at start up.
   VERIFYNRV(mpSignatureWindow != NULL);
   ExecutableResource pSigWinPlugIn(mpSignatureWindow, std::string(), NULL, false);
   bool addPlot(true);
   ColorType color(255, 0, 0);   // use red for the in-scene pixel
   pSigWinPlugIn->getInArgList().setPlugInArgValue("Add Plot", &addPlot);
   pSigWinPlugIn->getInArgList().setPlugInArgValue(Executable::DataElementArg(),
      const_cast<RasterElement*>(pRaster));
   pSigWinPlugIn->getInArgList().setPlugInArgValue("Signature to add",
      const_cast<Signature*>(pSignature));
   pSigWinPlugIn->getInArgList().setPlugInArgValue("Curve color", &color);
   bool clearBeforeAdd(true);
   pSigWinPlugIn->getInArgList().setPlugInArgValue("Clear before adding", &clearBeforeAdd);
   pSigWinPlugIn->execute();
   ModelResource<SignatureSet> pSet("Match Result to Plot", const_cast<RasterElement*>(pRaster));
   for (std::vector<std::pair<Signature*, float> >::iterator it = matches.begin();
      it != matches.end(); ++it)
   {
      pSet->insertSignature(it->first);
   }
   pSigWinPlugIn->getInArgList().setPlugInArgValue("Add Plot", &addPlot);
   pSigWinPlugIn->getInArgList().setPlugInArgValue(Executable::DataElementArg(),
      const_cast<RasterElement*>(pRaster));
   pSigWinPlugIn->getInArgList().setPlugInArgValue("Signature to add", static_cast<Signature*>(pSet.get()));
   color.mRed = 0;  // change to black for matching signatures
   pSigWinPlugIn->getInArgList().setPlugInArgValue("Curve color", &color);
   clearBeforeAdd = false;
   pSigWinPlugIn->getInArgList().setPlugInArgValue("Clear before adding", &clearBeforeAdd);
   pSigWinPlugIn->execute();
   pSigWinPlugIn.release();
}

bool SpectralLibraryMatchTools::serialize(SessionItemSerializer& serializer) const
{
   XMLWriter writer("SpectralLibraryMatchTools");

   if (mpPixelMatchAction != NULL)
   {
      writer.addAttr("pixel_match_checked", mpPixelMatchAction->isChecked());
   }

   std::vector<Window*> windows;
   Service<DesktopServices>()->getWindows(SPATIAL_DATA_WINDOW, windows);
   for (std::vector<Window*>::iterator iter = windows.begin(); iter != windows.end(); ++iter)
   {
      SpatialDataWindow* pWindow = dynamic_cast<SpatialDataWindow*>(*iter);
      if (pWindow != NULL)
      {
         SpatialDataView* pView = pWindow->getSpatialDataView();
         if (pView != NULL)
         {
            writer.pushAddPoint(writer.addElement("view"));
            writer.addAttr("id", pView->getId());
            writer.popAddPoint();
         }
      }
   }

   return serializer.serialize(writer);
}

bool SpectralLibraryMatchTools::deserialize(SessionItemDeserializer& deserializer)
{
   if (isBatch() == true)
   {
      setInteractive();
   }

   bool success = execute(NULL, NULL);
   if ((success == true) && (mpPixelMatchAction != NULL))
   {
      XmlReader reader(NULL, false);
      DOMElement* pRootElement = deserializer.deserialize(reader, "SpectralLibraryMatchTools");
      if (pRootElement)
      {
         // Add the mouse mode to the spatial data views
         for (DOMNode* pNode = pRootElement->getFirstChild(); pNode != NULL; pNode = pNode->getNextSibling())
         {
            if (XMLString::equals(pNode->getNodeName(), X("view")))
            {
               DOMElement* pViewElement = static_cast<DOMElement*>(pNode);
               Service<SessionManager> pSession;

               SpatialDataView* pView =
                  dynamic_cast<SpatialDataView*>(pSession->getSessionItem(A(pViewElement->getAttribute(X("id")))));
               if (pView != NULL)
               {
                  addPixelMatchMode(pView);
               }
            }
         }

         // Initialize the menu action
         bool checked = StringUtilities::fromXmlString<bool>(A(pRootElement->getAttribute(X("pixel_match_checked"))));
         mpPixelMatchAction->setChecked(checked);
      }
   }

   return success;
}

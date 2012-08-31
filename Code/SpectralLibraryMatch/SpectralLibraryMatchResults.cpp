/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "ColorType.h"
#include "ContextMenu.h"
#include "DataAccessorImpl.h"
#include "DesktopServices.h"
#include "DynamicObject.h"
#include "LocateDialog.h"
#include "MenuBar.h"
#include "PlugIn.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "Progress.h"
#include "RasterElement.h"
#include "RasterDataDescriptor.h"
#include "ResultsPage.h"
#include "Signature.h"
#include "SignatureDataDescriptor.h"
#include "SignatureSet.h"
#include "SpectralContextMenuActions.h"
#include "SpectralLibraryManager.h"
#include "SpectralLibraryMatch.h"
#include "SpectralLibraryMatchOptions.h"
#include "SpectralLibraryMatchResults.h"
#include "SpectralVersion.h"
#include "Subject.h"
#include "ToolBar.h"
#include "Units.h"
#include "Wavelengths.h"

#include <QtGui/QAction>
#include <QtGui/QDockWidget>
#include <QtGui/QInputDialog>
#include <QtGui/QMainWindow>
#include <QtGui/QMenu>
#include <QtGui/QPixmap>
#include <QtGui/QTabWidget>
#include <QtGui/QWidget>

REGISTER_PLUGIN_BASIC(SpectralSpectralLibraryMatch, SpectralLibraryMatchResults);

namespace
{
   const char* const ShowResultsIcon[] =
   {
      "16 16 6 1",
      " 	c None",
      ".	c #808080",
      "+	c #000000",
      "@	c #C0C0C0",
      "#	c #FFFF00",
      "$	c #0000FF",
      "                ",
      "                ",
      ".++++++++++++++ ",
      "..............+ ",
      ".@#@#@#@#@#@#.+ ",
      ".@$$$$$$$$$$@.+ ",
      ".@#@#@#@#@#@#.+ ",
      ".@@#$$$$$$@#@.+ ",
      ".@#@#@#@#@#@#.+ ",
      ".@$$$$$$$$$$@.+ ",
      ".@#@#@#@#@#@#.+ ",
      ".@@@@@@@@@@@@.+ ",
      ".#@#@#@#......  ",
      " .#@#@#.        ",
      "  .....         ",
      "                "
   };
}

SpectralLibraryMatchResults::SpectralLibraryMatchResults() :
   mpExplorer(SIGNAL_NAME(SessionExplorer, AboutToShowSessionItemContextMenu),
      Slot(this, &SpectralLibraryMatchResults::updateContextMenu))
{
   DockWindowShell::setName(SpectralLibraryMatch::getNameLibraryMatchResultsPlugIn());
   setSubtype("Results");
   setVersion(SPECTRAL_VERSION_NUMBER);
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setShortDescription("Display results from matching in-scene spectra with a spectral library.");
   setDescription("Display results from matching in-scene spectra with signatures in a spectral library.");
   setDescriptorId("{0BD9C61F-1D1D-406f-B4F1-90AD1BB1BAA2}");
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

SpectralLibraryMatchResults::~SpectralLibraryMatchResults()
{
   Service<DesktopServices> pDesktop;

   // remove toolbar button and menu item
   QAction* pAction = getAction();
   if (pAction != NULL)
   {
      ToolBar* pToolBar = static_cast<ToolBar*>(pDesktop->getWindow("Spectral", TOOLBAR));
      if (pToolBar != NULL)
      {
         pToolBar->removeItem(pAction);
         MenuBar* pMenuBar = pToolBar->getMenuBar();
         if (pMenuBar != NULL)
         {
            pMenuBar->removeMenuItem(pAction);
         }
      }
   }

   // dockwindow should still exist so detach from it
   DockWindow* pWindow = getDockWindow();
   if (pWindow != NULL)
   {
      pWindow->detach(SIGNAL_NAME(DockWindow, AboutToShowContextMenu),
         Slot(this, &SpectralLibraryMatchResults::updateContextMenu));
   }
}

QWidget* SpectralLibraryMatchResults::createWidget()
{
   mpTabWidget = new QTabWidget(getDockWindow()->getWidget());
   mpTabWidget->setTabPosition(QTabWidget::South);
   mpTabWidget->setTabShape(QTabWidget::Rounded);
   mpTabWidget->setMinimumHeight(100);

   // dockwindow should exist so attach to it
   DockWindow* pWindow = getDockWindow();
   if (pWindow != NULL)
   {

      pWindow->attach(SIGNAL_NAME(DockWindow, AboutToShowContextMenu),
         Slot(this, &SpectralLibraryMatchResults::updateContextMenu));

      // Connect to the session explorer now that the window has been created
      if (mpExplorer.get() == NULL)
      {
         Service<SessionExplorer> pExplorer;
         mpExplorer.reset(pExplorer.get());
      }

      // set location of the results window
#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : Replace this block when public method to locate DockWindow is available. (rforehan)")
      QMainWindow* pMainWindow = dynamic_cast<QMainWindow*>(Service<DesktopServices>()->getMainWidget());
      if (pMainWindow != NULL)
      {
         pMainWindow->addDockWidget(Qt::LeftDockWidgetArea, dynamic_cast<QDockWidget*>(pWindow), Qt::Vertical);
      }     
   }

   return mpTabWidget;
}

QAction* SpectralLibraryMatchResults::createAction()
{
   // Add toolbar button and menu item to invoke the window
   QAction* pShowAction(NULL);
   ToolBar* pToolBar = static_cast<ToolBar*>(Service<DesktopServices>()->getWindow("Spectral", TOOLBAR));
   if (pToolBar != NULL)
   {
      MenuBar* pMenuBar = pToolBar->getMenuBar();
      if (pMenuBar != NULL)
      {
         QAction* pMenuAction = pMenuBar->getMenuItem("/Spectral/Support Tools");
         if (pMenuAction != NULL)
         {
            QMenu* pMenu = pMenuBar->getMenu(pMenuAction);
            if (pMenu != NULL)
            {
               pShowAction = pMenu->addAction("Spectral Library Match Results Window");
               if (pShowAction != NULL)
               {
                  QPixmap pixShowResults(ShowResultsIcon);
                  pShowAction->setIcon(QIcon(pixShowResults));
                  pShowAction->setCheckable(true);
                  pShowAction->setAutoRepeat(false);
                  pShowAction->setStatusTip("Toggles the display of the Spectral Library Match Results Window");
                  pToolBar->addSeparator();
                  pToolBar->addButton(pShowAction);
               }
            }
         }
      }
   }

   return pShowAction;
}

void SpectralLibraryMatchResults::addResults(const SpectralLibraryMatch::MatchResults& theResults, Progress* pProgress)
{
   std::map<Signature*, ColorType> colorMap;
   std::vector<SpectralLibraryMatch::MatchResults> results;
   results.push_back(theResults);
   addResults(results, colorMap, pProgress);
}

void SpectralLibraryMatchResults::addResults(const std::vector<SpectralLibraryMatch::MatchResults>& theResults,
                                             const std::map<Signature*, ColorType>& colorMap,
                                             Progress* pProgress, bool* pAbort)
{
   if (theResults.empty())
   {
      return;
   }

   // Since all the results in theResults are for the same raster element, we only need to get/create the page for
   // the first result.
   ResultsPage* pPage = getPage(theResults.front().mpRaster);
   if (pPage == NULL)
   {
      pPage = createPage(theResults.front().mpRaster);
      if (pPage == NULL)
      {
         if (pProgress != NULL)
         {
            pProgress->updateProgress("Error: Unable to access the results page", 0, ERRORS);
         }
         return;
      }
   }
   mpTabWidget->setCurrentWidget(pPage);

   pPage->addResults(theResults, colorMap, pProgress, pAbort);
}

ResultsPage* SpectralLibraryMatchResults::createPage(const RasterElement* pRaster)
{
   ResultsPage* pPage = getPage(pRaster);
   if (pPage != NULL)
   {
      return NULL;
   }

   QString tabName = QString::fromStdString(pRaster->getDisplayName(true));
   pPage = new ResultsPage();
   if (pPage == NULL)
   {
      return NULL;
   }

   int index = mpTabWidget->addTab(pPage, tabName);
   mpTabWidget->setTabToolTip(index, QString::fromStdString(pRaster->getName()));
   mpTabWidget->setCurrentIndex(index);
   VERIFYNR(const_cast<RasterElement*>(pRaster)->attach(SIGNAL_NAME(Subject, Deleted),
      Slot(this, &SpectralLibraryMatchResults::elementDeleted)));
   VERIFYNR(const_cast<RasterElement*>(pRaster)->attach(SIGNAL_NAME(Subject, Modified),
      Slot(this, &SpectralLibraryMatchResults::elementModified)));
   mPageMap[pRaster] = pPage;

   return pPage;
}

ResultsPage* SpectralLibraryMatchResults::getPage(const RasterElement* pRaster) const
{
   std::map<const RasterElement*, ResultsPage*>::const_iterator pit = mPageMap.find(pRaster);
   if (pit != mPageMap.end())
   {
      return pit->second;
   }
   return NULL;
}

void SpectralLibraryMatchResults::deletePage(const RasterElement* pRaster)
{
   std::map<const RasterElement*, ResultsPage*>::iterator pit = mPageMap.find(pRaster);
   if (pit != mPageMap.end())
   {
      ResultsPage* pPage = pit->second;
      mPageMap.erase(pit);
      delete pPage;

      VERIFYNR(const_cast<RasterElement*>(pRaster)->detach(SIGNAL_NAME(Subject, Deleted),
         Slot(this, &SpectralLibraryMatchResults::elementDeleted)));
      VERIFYNR(const_cast<RasterElement*>(pRaster)->detach(SIGNAL_NAME(Subject, Modified),
         Slot(this, &SpectralLibraryMatchResults::elementModified)));
   }
}

void SpectralLibraryMatchResults::updateContextMenu(Subject& subject, const std::string& signal,
                                                    const boost::any& value)
{
   ContextMenu* pMenu = boost::any_cast<ContextMenu*>(value);
   if (pMenu == NULL)
   {
      return;
   }

   // only add actions if there are some results
   if (mpTabWidget->count() > 0)
   {
      bool isSessionItem(false);
      if (dynamic_cast<SessionExplorer*>(&subject) != NULL)
      {
         std::vector<SessionItem*> items = pMenu->getSessionItems();
         if (items.size() > 1)                                        // make sure only one item selected
         {
            return;
         }
         DockWindow* pWindow = getDockWindow();
         if (items.front() != pWindow)                             // make sure it's the results window
         {
            return;
         }
         isSessionItem = true;
      }

      QObject* pParent = pMenu->getActionParent();

      // add separator
      QAction* pSeparatorAction = new QAction(pParent);
      pSeparatorAction->setSeparator(true);
      pMenu->addAction(pSeparatorAction, SPECTRAL_LIBRARY_MATCH_RESULTS_SEPARATOR_ACTION);

      QAction* pClearAction = new QAction("&Clear", pParent);
      pClearAction->setAutoRepeat(false);
      pClearAction->setStatusTip("Clears the results from the current page");
      VERIFYNR(connect(pClearAction, SIGNAL(triggered()), this, SLOT(clearPage())));
      pMenu->addAction(pClearAction, SPECTRAL_LIBRARY_MATCH_RESULTS_CLEAR_RESULTS_ACTION);

      QAction* pAutoClearAction = new QAction("&AutoClear", pParent);
      pAutoClearAction->setAutoRepeat(false);
      pAutoClearAction->setCheckable(true);
      pAutoClearAction->setStatusTip("Enable/disable clearing existing results before adding new results");
      ResultsPage* pPage = dynamic_cast<ResultsPage*>(mpTabWidget->currentWidget());
      if (pPage != NULL)
      {
         pAutoClearAction->setChecked(pPage->getAutoClear());
         VERIFYNR(connect(pAutoClearAction, SIGNAL(toggled(bool)), pPage, SLOT(setAutoClear(bool))));
         pMenu->addAction(pAutoClearAction, SPECTRAL_LIBRARY_MATCH_RESULTS_AUTOCLEAR_ACTION);
      }

      QAction* pExpandAllAction = new QAction("&Expand All", pParent);
      pExpandAllAction->setAutoRepeat(false);
      pExpandAllAction->setStatusTip("Expands all the results nodes on the current page");
      VERIFYNR(connect(pExpandAllAction, SIGNAL(triggered()), this, SLOT(expandAllPage())));
      pMenu->addAction(pExpandAllAction, SPECTRAL_LIBRARY_MATCH_RESULTS_EXPAND_ALL_ACTION);

      QAction* pCollapseAllAction = new QAction("&Collapse All", pParent);
      pCollapseAllAction->setAutoRepeat(false);
      pCollapseAllAction->setStatusTip("Collapses all the results nodes on the current page");
      VERIFYNR(connect(pCollapseAllAction, SIGNAL(triggered()), this, SLOT(collapseAllPage())));
      pMenu->addAction(pCollapseAllAction, SPECTRAL_LIBRARY_MATCH_RESULTS_COLLAPSE_ALL_ACTION);

      QAction* pDeleteTabAction = new QAction("&Delete Page", pParent);
      pDeleteTabAction->setAutoRepeat(false);
      pDeleteTabAction->setStatusTip("Deletes the current page");
      VERIFYNR(connect(pDeleteTabAction, SIGNAL(triggered()), this, SLOT(deletePage())));
      pMenu->addAction(pDeleteTabAction, SPECTRAL_LIBRARY_MATCH_RESULTS_DELETE_PAGE_ACTION);

      if (isSessionItem == false)
      {
         QAction* pLocateAction = new QAction("&Locate Signatures", pParent);
         pLocateAction->setAutoRepeat(false);
         pLocateAction->setStatusTip("Locates the selected Signatures in the spatial data view");
         VERIFYNR(connect(pLocateAction, SIGNAL(triggered()), this, SLOT(locateSignaturesInScene())));
         pMenu->addAction(pLocateAction, SPECTRAL_LIBRARY_MATCH_RESULTS_LOCATE_ACTION);
         QAction* pCreateAverageAction = new QAction("&Create average Signature", pParent);
         pCreateAverageAction->setAutoRepeat(false);
         pCreateAverageAction->setStatusTip("Creates an average Signature from the selected "
            "Signatures in the spatial data view");
         VERIFYNR(connect(pCreateAverageAction, SIGNAL(triggered()), this, SLOT(createAverageSignature())));
         pMenu->addAction(pCreateAverageAction, SPECTRAL_LIBRARY_MATCH_RESULTS_CREATE_AVERAGE_ACTION);
      }
   }
}

void SpectralLibraryMatchResults::clearPage()
{
   ResultsPage* pPage = dynamic_cast<ResultsPage*>(mpTabWidget->currentWidget());
   if (pPage != NULL)
   {
      pPage->clear();
   }
}

void SpectralLibraryMatchResults::deletePage()
{
   ResultsPage* pPage = dynamic_cast<ResultsPage*>(mpTabWidget->currentWidget());
   if (pPage != NULL)
   {
      deletePage(getRasterElementForCurrentPage());
   }
}

void SpectralLibraryMatchResults::expandAllPage()
{
   ResultsPage* pPage = dynamic_cast<ResultsPage*>(mpTabWidget->currentWidget());
   if (pPage != NULL)
   {
      pPage->expandAll();
   }
}

void SpectralLibraryMatchResults::collapseAllPage()
{
   ResultsPage* pPage = dynamic_cast<ResultsPage*>(mpTabWidget->currentWidget());
   if (pPage != NULL)
   {
      pPage->collapseAll();
   }
}

void SpectralLibraryMatchResults::elementDeleted(Subject& subject, const std::string& signal, const boost::any& value)
{
   RasterElement* pRaster = dynamic_cast<RasterElement*>(&subject);
   if (pRaster != NULL)
   {
      ResultsPage* pPage = getPage(pRaster);
      if (pPage != NULL)
      {
         deletePage(pRaster);
      }
   }
}

void SpectralLibraryMatchResults::elementModified(Subject& subject, const std::string& signal, const boost::any& value)
{
   // check for rename
   RasterElement* pRaster = dynamic_cast<RasterElement*>(&subject);
   if (pRaster != NULL)
   {
      ResultsPage* pPage = getPage(pRaster);
      if (pPage != NULL)
      {
         int index = mpTabWidget->indexOf(pPage);
         if (index != -1 && mpTabWidget->tabToolTip(index).toStdString() != pRaster->getName())
         {
            mpTabWidget->setTabText(index, QString::fromStdString(pRaster->getDisplayName(true)));
            mpTabWidget->setTabToolTip(index, QString::fromStdString(pRaster->getName()));
         }
      }
   }
}

std::vector<Signature*> SpectralLibraryMatchResults::getSelectedSignatures() const
{
   // get the selected signatures
   std::vector<Signature*> signatures;
   ResultsPage* pPage = dynamic_cast<ResultsPage*>(mpTabWidget->currentWidget());
   if (pPage == NULL)
   {
      return signatures;
   }

   pPage->getSelectedSignatures(signatures);
   return signatures;
}

const RasterElement* SpectralLibraryMatchResults::getRasterElementForCurrentPage() const
{
   ResultsPage* pPage = dynamic_cast<ResultsPage*>(mpTabWidget->currentWidget());
   if (pPage != NULL)
   {
      for (std::map<const RasterElement*, ResultsPage*>::const_iterator it = mPageMap.begin();
         it != mPageMap.end(); ++it)
      {
         if (pPage == it->second)
         {
            return it->first;
         }
      }
   }
   
   return NULL;
}

void SpectralLibraryMatchResults::locateSignaturesInScene()
{
   const RasterElement* pRaster = getRasterElementForCurrentPage();
   if (pRaster == NULL)
   {
      Service<DesktopServices>()->showMessageBox("Spectral Library Match",
         "Unable to determine the RasterElement for the current page.");
      return;
   }

   // get selected signatures
   std::vector<Signature*> signatures = getSelectedSignatures();
   if (signatures.empty())
   {
      Service<DesktopServices>()->showMessageBox("Spectral Library Match",
         "No signatures are selected to be located.");
      return;
   }

   ModelResource<SignatureSet> pSet("Match Result signatures", const_cast<RasterElement*>(pRaster));
   for (std::vector<Signature*>::iterator it = signatures.begin(); it != signatures.end(); ++it)
   {
      pSet->insertSignature(*it);
   }

   // get default algorithm
   SpectralLibraryMatch::LocateAlgorithm locAlg = StringUtilities::fromXmlString<SpectralLibraryMatch::LocateAlgorithm>(
      SpectralLibraryMatchOptions::getSettingLocateAlgorithm());

   // get default threshold
   double threshold(0.0); 
   switch (locAlg)
   {
   case SpectralLibraryMatch::SLLA_CEM:
      threshold = SpectralLibraryMatchOptions::getSettingLocateCemThreshold();
      break;

   case SpectralLibraryMatch::SLLA_SAM:
      threshold = SpectralLibraryMatchOptions::getSettingLocateSamThreshold();
      break;

   default:
      VERIFYNRV_MSG(false, "Unknown value for Spectral Library Match locate algorithm");
   }

   std::string layerName;
   AoiElement* pAoi(NULL);
   if (SpectralLibraryMatchOptions::getSettingDisplayLocateOptions())
   {
      LocateDialog dlg(pRaster, Service<DesktopServices>()->getMainWidget());
      if (dlg.exec() != QDialog::Accepted)
      {
         return;
      }
      locAlg = dlg.getLocateAlgorithm();
      threshold = dlg.getThreshold();
      layerName = dlg.getOutputLayerName();
      pAoi = dlg.getAoi(); 
   }

   std::string plugInName;
   switch (locAlg)
   {
   case SpectralLibraryMatch::SLLA_CEM:
      plugInName = "CEM";
      break;

   case SpectralLibraryMatch::SLLA_SAM:
      plugInName = "SAM";
      break;

   default:
      VERIFYNRV_MSG(false, "Unknown value for Spectral Library Match locate algorithm");
   }
   if (layerName.empty())
   {
      layerName = "Spectral Library Match Locate Results - " + plugInName;
   }

   ExecutableResource pLocate(plugInName);
   pLocate->getInArgList().setPlugInArgValue(Executable::DataElementArg(), const_cast<RasterElement*>(pRaster));
   pLocate->getInArgList().setPlugInArgValue("Target Signatures", static_cast<Signature*>(pSet.get()));
   pLocate->getInArgList().setPlugInArgValue("Threshold", &threshold);
   pLocate->getInArgList().setPlugInArgValue("AOI", pAoi);
   bool display(true);
   pLocate->getInArgList().setPlugInArgValue("Display Results", &display);
   pLocate->getInArgList().setPlugInArgValue("Results Name", &layerName);
   pLocate->execute();
}

void SpectralLibraryMatchResults::createAverageSignature()
{
   Service<DesktopServices> pDesktop;

   const RasterElement* pRaster = getRasterElementForCurrentPage();
   if (pRaster == NULL)
   {
      pDesktop->showMessageBox("Spectral Library Match",
         "Unable to determine the RasterElement for the current page.");
      return;
   }

   std::vector<Signature*> signatures = getSelectedSignatures();
   if (signatures.empty())
   {
      pDesktop->showMessageBox("Spectral Library Match",
         "No signatures are selected for use in generating an average signature.");
      return;
   }

   // now need to get the resampled sigs from the library
   SpectralLibraryManager* pLibMgr(NULL);
   std::vector<PlugIn*> plugIns =
      Service<PlugInManagerServices>()->getPlugInInstances(SpectralLibraryMatch::getNameLibraryManagerPlugIn());
   if (!plugIns.empty())
   {
      pLibMgr = dynamic_cast<SpectralLibraryManager*>(plugIns.front());
   }
   if (pLibMgr == NULL)
   {
      pDesktop->showMessageBox("Spectral Library Match",
         "Unable to access the Spectral Library Manager.");
      return;
   }

   const RasterDataDescriptor* pDesc = dynamic_cast<const RasterDataDescriptor*>(pRaster->getDataDescriptor());
   if (pDesc == NULL)
   {
      pDesktop->showMessageBox("Spectral Library Match",
         "Unable to access the RasterDataDescriptor for the RasterElement of the current page.");
      return;
   }

   unsigned int numBands = pDesc->getBandCount();
   std::vector<double> averageValues(numBands, 0);
   std::vector<double> values;
   for (std::vector<Signature*>::iterator it = signatures.begin(); it != signatures.end(); ++it)
   {
      if (pLibMgr->getResampledSignatureValues(pRaster, *it, values) == false)
      {
         pDesktop->showMessageBox("Spectral Library Match",
            "Unable to access the resampled signature values for " + (*it)->getDisplayName(true));
         return;
      }
      for (unsigned int band = 0; band < numBands; ++band)
      {
         averageValues[band] += values[band];
      }
   }
   unsigned int numSigs = signatures.size();
   for (unsigned int band = 0; band < numBands; ++band)
   {
      averageValues[band] /= static_cast<double>(numSigs);
   }

   QString avgName = QInputDialog::getText(pDesktop->getMainWidget(), "Spectral Library Match",
      "Enter the name to use for the average signature:");
   if (avgName.isEmpty())
   {
      return;
   }
   ModelResource<Signature> pAvgSig(avgName.toStdString(), const_cast<RasterElement*>(pRaster));
   pAvgSig->setData("Reflectance", averageValues);

   const DynamicObject* pMetaData = pRaster->getMetadata();
   FactoryResource<Wavelengths> pWavelengths;
   pWavelengths->initializeFromDynamicObject(pMetaData, false);
   const std::vector<double>& centerValues = pWavelengths->getCenterValues();
   pAvgSig->setData("Wavelength", centerValues);
   SignatureDataDescriptor* pSigDesc = dynamic_cast<SignatureDataDescriptor*>(pAvgSig->getDataDescriptor());
   VERIFYNRV(pSigDesc != NULL);
   FactoryResource<Units> pUnits;
   const Units* pRasterUnits = pDesc->getUnits();
   pUnits->setUnitName(pRasterUnits->getUnitName());
   pUnits->setUnitType(pRasterUnits->getUnitType());
   pUnits->setScaleFromStandard(1.0);  // the rescaled values are already corrected for the original scaling factor
   pUnits->setRangeMin(pRasterUnits->getRangeMin());
   pUnits->setRangeMax(pRasterUnits->getRangeMax());
   pSigDesc->setUnits("Reflectance", pUnits.get());
   pAvgSig.release();
}

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtGui/QAction>
#include <QtGui/QBitmap>
#include <QtGui/QMouseEvent>

#include "AoiElement.h"
#include "AoiLayer.h"
#include "AppVerify.h"
#include "BitMask.h"
#include "ContextMenu.h"
#include "ContextMenuActions.h"
#include "DesktopServices.h"
#include "LayerList.h"
#include "MenuBar.h"
#include "ModelServices.h"
#include "MouseMode.h"
#include "PlotSet.h"
#include "PlotView.h"
#include "PlotWidget.h"
#include "PlotWindow.h"
#include "PlugInRegistration.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "SessionItemDeserializer.h"
#include "SessionItemSerializer.h"
#include "SessionManager.h"
#include "Signature.h"
#include "SignaturePlotObject.h"
#include "SignatureWindow.h"
#include "SignatureWindowIcons.h"
#include "Slot.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "SpectralContextMenuActions.h"
#include "SpectralUtilities.h"
#include "SpectralVersion.h"
#include "ToolBar.h"
#include "TypeConverter.h"
#include "UtilityServices.h"
#include "XercesIncludes.h"
#include "xmlreader.h"
#include "xmlwriter.h"

XERCES_CPP_NAMESPACE_USE
using namespace std;

REGISTER_PLUGIN_BASIC(SpectralSignatureWindow, SignatureWindow);

SignatureWindow::SignatureWindow() :
   mpExplorer(SIGNAL_NAME(SessionExplorer, AboutToShowSessionItemContextMenu),
      Slot(this, &SignatureWindow::updateContextMenu)),
   mpProgress(NULL),
   mpWindowAction(NULL),
   mpPixelSignatureMode(NULL),
   mpPixelSignatureAction(NULL),
   mpAoiSignaturesAction(NULL)
{
   AlgorithmShell::setName("Signature Window");
   setCreator("Ball Aerospace & Technologies, Corp.");
   setVersion(SPECTRAL_VERSION_NUMBER);
   setCopyright(SPECTRAL_COPYRIGHT);
   setDescription("Provides access to plot sets and their plots.");
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setDescriptorId("{27F4730B-5309-45BF-AF1E-97134A911F17}");
   allowMultipleInstances(false);
   executeOnStartup(true);
   destroyAfterExecute(false);
   setAbortSupported(true);
   setWizardSupported(false);
}

SignatureWindow::~SignatureWindow()
{
   Service<DesktopServices> pDesktop;

   // Remove the window action
   if (mpWindowAction != NULL)
   {
      ToolBar* pToolBar = static_cast<ToolBar*>(pDesktop->getWindow("Spectral", TOOLBAR));
      if (pToolBar != NULL)
      {
         MenuBar* pMenuBar = pToolBar->getMenuBar();
         if (pMenuBar != NULL)
         {
            pMenuBar->removeMenuItem(mpWindowAction);
         }

         pToolBar->removeItem(mpWindowAction);
      }

      if (pDesktop->getMainWidget() != NULL)
      {
         delete mpWindowAction;
      }
   }

   // Delete the signature window
   PlotWindow* pWindow = static_cast<PlotWindow*> (pDesktop->getWindow("Signature Window", PLOT_WINDOW));
   if (pWindow != NULL)
   {
      pWindow->detach(SIGNAL_NAME(DockWindow, Shown), Slot(this, &SignatureWindow::plotWindowShown));
      pWindow->detach(SIGNAL_NAME(DockWindow, Hidden), Slot(this, &SignatureWindow::plotWindowHidden));
      pWindow->detach(SIGNAL_NAME(DockWindow, AboutToShowContextMenu),
         Slot(this, &SignatureWindow::updateContextMenu));
      pDesktop->deleteWindow(pWindow);
   }

   // Remove the toolbar buttons
   ToolBar* pToolBar = static_cast<ToolBar*>(pDesktop->getWindow("Spectral", TOOLBAR));
   if (pToolBar != NULL)
   {
      if (mpPixelSignatureAction != NULL)
      {
         pToolBar->removeItem(mpPixelSignatureAction);
         delete mpPixelSignatureAction;
      }

      if (mpAoiSignaturesAction != NULL)
      {
         disconnect(mpAoiSignaturesAction, SIGNAL(activated()), this, SLOT(displayAoiSignatures()));
         pToolBar->removeItem(mpAoiSignaturesAction);
         delete mpAoiSignaturesAction;
      }
   }

   // Detach from the session manager
   Service<SessionManager> pSessionManager;
   pSessionManager->detach(SIGNAL_NAME(SessionManager, SessionRestored),
      Slot(this, &SignatureWindow::sessionRestored));

   // Detach from desktop services
   pDesktop->detach(SIGNAL_NAME(DesktopServices, WindowAdded), Slot(this, &SignatureWindow::windowAdded));
   pDesktop->detach(SIGNAL_NAME(DesktopServices, WindowActivated), Slot(this, &SignatureWindow::windowActivated));
   pDesktop->detach(SIGNAL_NAME(DesktopServices, WindowRemoved), Slot(this, &SignatureWindow::windowRemoved));

   // Remove the mouse mode from the views
   vector<Window*> windows;
   pDesktop->getWindows(SPATIAL_DATA_WINDOW, windows);

   for (vector<Window*>::iterator iter = windows.begin(); iter != windows.end(); ++iter)
   {
      SpatialDataWindow* pWindow = static_cast<SpatialDataWindow*>(*iter);
      if (pWindow != NULL)
      {
         removePixelSignatureMode(pWindow);
      }
   }

   // Delete the pixel spectrum mouse mode
   if (mpPixelSignatureMode != NULL)
   {
      pDesktop->deleteMouseMode(mpPixelSignatureMode);
   }

   // Delete the progress object
   Service<UtilityServices> pUtilities;
   pUtilities->destroyProgress(mpProgress);
}

bool SignatureWindow::setBatch()
{
   AlgorithmShell::setBatch();
   return false;
}

bool SignatureWindow::getInputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return !isBatch();
}

bool SignatureWindow::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return !isBatch();
}

bool SignatureWindow::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (isBatch() == true)
   {
      return false;
   }

   Service<DesktopServices> pDesktop;

   QPixmap pixSignatureWindow(SignatureWindowIcons::SignatureWindowIcon);
   pixSignatureWindow.setMask(QPixmap(SignatureWindowIcons::SignatureWindowMask));
   QIcon signatureWindowIcon(pixSignatureWindow);

   // Add a menu command and toolbar button to invoke the window
   ToolBar* pToolBar = static_cast<ToolBar*>(pDesktop->getWindow("Spectral", TOOLBAR));
   if (pToolBar != NULL)
   {
      MenuBar* pMenuBar = pToolBar->getMenuBar();
      if (pMenuBar != NULL)
      {
         mpWindowAction = pMenuBar->addCommand("Spectral/Support Tools/&Signature Window", "Signature Window");
         if (mpWindowAction != NULL)
         {
            mpWindowAction->setAutoRepeat(false);
            mpWindowAction->setIcon(signatureWindowIcon);
            mpWindowAction->setCheckable(true);
            mpWindowAction->setToolTip("Signature Window");
            mpWindowAction->setStatusTip("Toggles the display of the Signature Window");
            VERIFYNR(connect(mpWindowAction, SIGNAL(triggered(bool)), this, SLOT(displaySignatureWindow(bool))));

            pToolBar->addButton(mpWindowAction);
         }
      }
   }

   if (mpWindowAction == NULL)
   {
      return false;
   }

   // Create the progress object and the progress dialog
   Service<UtilityServices> pUtilities;
   mpProgress = pUtilities->getProgress();
   if (mpProgress != NULL)
   {
      pDesktop->createProgressDialog(getName(), mpProgress);
   }

   // Create the window
   PlotWindow* pWindow = static_cast<PlotWindow*>(pDesktop->getWindow("Signature Window", PLOT_WINDOW));
   if (pWindow == NULL)
   {
      pWindow = static_cast<PlotWindow*>(pDesktop->createWindow("Signature Window", PLOT_WINDOW));
      if (pWindow == NULL)
      {
         return false;
      }
   }

   // Attach the window
   pWindow->attach(SIGNAL_NAME(DockWindow, Shown), Slot(this, &SignatureWindow::plotWindowShown));
   pWindow->attach(SIGNAL_NAME(DockWindow, Hidden), Slot(this, &SignatureWindow::plotWindowHidden));
   pWindow->attach(SIGNAL_NAME(DockWindow, AboutToShowContextMenu),
      Slot(this, &SignatureWindow::updateContextMenu));
   pWindow->attach(SIGNAL_NAME(PlotWindow, PlotSetAdded), Slot(this, &SignatureWindow::plotSetAdded));
   pWindow->attach(SIGNAL_NAME(PlotWindow, SessionItemDropped), Slot(this, &SignatureWindow::dropSessionItem));

   // Add a default plot to the window
   Service<SessionManager> pSessionManager;
   if (pSessionManager->isSessionLoading() == false)
   {
      addDefaultPlot();
   }

   // Attach to the session manager
   pSessionManager->attach(SIGNAL_NAME(SessionManager, SessionRestored),
      Slot(this, &SignatureWindow::sessionRestored));

   // Create the pixel spectrum action
   QPixmap pixPixelSignature(SignatureWindowIcons::PixelSignatureIcon);
   QBitmap bmpPixelSignatureMask(SignatureWindowIcons::PixelSignatureMask);
   pixPixelSignature.setMask(bmpPixelSignatureMask);

   mpPixelSignatureAction = new QAction(QIcon(pixPixelSignature), "&Display Pixel Signature", this);
   mpPixelSignatureAction->setAutoRepeat(false);
   mpPixelSignatureAction->setCheckable(true);
   mpPixelSignatureAction->setStatusTip("Displays the signature of a pixel selected with the mouse");

   // Create the AOI signatures action
   QPixmap pixAoiSignatures(SignatureWindowIcons::AoiSignatureIcon);
   QBitmap bmpAoiSignaturesMask(SignatureWindowIcons::AoiSignatureMask);
   pixAoiSignatures.setMask(bmpAoiSignaturesMask);

   mpAoiSignaturesAction = new QAction(QIcon(pixAoiSignatures), "&Display AOI Signatures", this);
   mpAoiSignaturesAction->setAutoRepeat(false);
   mpAoiSignaturesAction->setStatusTip("Displays the signatures of the selected pixels in the current AOI layer");
   VERIFYNR(connect(mpAoiSignaturesAction, SIGNAL(triggered()), this, SLOT(displayAoiSignatures())));

   // Add buttons to the toolbar
   if (pToolBar != NULL)
   {
      pToolBar->addButton(mpPixelSignatureAction);
      pToolBar->addButton(mpAoiSignaturesAction);
   }

   // Initialization
   enableActions();
   pWindow->setIcon(signatureWindowIcon);
   pWindow->enableSessionItemDrops(this);
   pWindow->hide();

   // Connections
   pDesktop->attach(SIGNAL_NAME(DesktopServices, WindowAdded), Slot(this, &SignatureWindow::windowAdded));
   pDesktop->attach(SIGNAL_NAME(DesktopServices, WindowActivated), Slot(this, &SignatureWindow::windowActivated));
   pDesktop->attach(SIGNAL_NAME(DesktopServices, WindowRemoved), Slot(this, &SignatureWindow::windowRemoved));

   Service<SessionExplorer> pExplorer;
   mpExplorer.reset(pExplorer.get());

   return true;
}

bool SignatureWindow::abort()
{
   for (vector<SignaturePlotObject*>::iterator iter = mPlots.begin(); iter != mPlots.end(); ++iter)
   {
      SignaturePlotObject* pPlot = *iter;
      if (pPlot != NULL)
      {
         pPlot->abort();
      }
   }

   return AlgorithmShell::abort();
}

bool SignatureWindow::accept(SessionItem* pItem) const
{
   return dynamic_cast<Signature*>(pItem) != NULL;
}

bool SignatureWindow::serialize(SessionItemSerializer& serializer) const
{
   if (mpWindowAction != NULL)
   {
      XMLWriter writer("SignatureWindow");
      writer.addAttr("shown", mpWindowAction->isChecked());

      for (vector<SignaturePlotObject*>::const_iterator iter = mPlots.begin(); iter != mPlots.end(); ++iter)
      {
         SignaturePlotObject* pPlot = *iter;
         if (pPlot != NULL)
         {
            writer.pushAddPoint(writer.addElement("SignaturePlotObject"));

            // Plot widget
            PlotWidget* pPlotWidget = pPlot->getPlotWidget();
            if (pPlotWidget != NULL)
            {
               writer.addAttr("plotWidgetId", pPlotWidget->getId());
            }

            // Signatures
            vector<Signature*> signatures = pPlot->getSignatures();
            for (vector<Signature*>::iterator iter = signatures.begin(); iter != signatures.end(); ++iter)
            {
               Signature* pSignature = *iter;
               if (pSignature != NULL)
               {
                  writer.pushAddPoint(writer.addElement("Signature"));
                  writer.addAttr("signatureId", pSignature->getId());
                  writer.popAddPoint();
               }
            }

            // Plot
            writer.addAttr("wavelengthUnits", pPlot->getWavelengthUnits());
            writer.addAttr("bandsDisplayed", pPlot->areBandNumbersDisplayed());
            writer.addAttr("clearOnAdd", pPlot->isClearOnAdd());

            // Raster layer
            RasterLayer* pRasterLayer = pPlot->getRasterLayer();
            if (pRasterLayer != NULL)
            {
               writer.addAttr("rasterLayerId", pRasterLayer->getId());
            }

            // Regions
            writer.pushAddPoint(writer.addElement("Regions"));
            writer.addAttr("displayed", pPlot->areRegionsDisplayed());
            writer.addAttr("color", QCOLOR_TO_COLORTYPE(pPlot->getRegionColor()));
            writer.addAttr("opacity", pPlot->getRegionOpacity());
            writer.popAddPoint();

            // End the signature plot object
            writer.popAddPoint();
         }
      }

      return serializer.serialize(writer);
   }

   return false;
}

bool SignatureWindow::deserialize(SessionItemDeserializer& deserializer)
{
   if (isBatch() == true)
   {
      setInteractive();
   }

   bool success = execute(NULL, NULL);
   if ((success == true) && (mpWindowAction != NULL))
   {
      // Initialize the menu action
      XmlReader reader(NULL, false);
      DOMElement* pRootElement = deserializer.deserialize(reader, "SignatureWindow");
      if (pRootElement)
      {
         bool shown = XmlReader::StringStreamAssigner<bool>()(A(pRootElement->getAttribute(X("shown"))));
         mpWindowAction->setChecked(shown);
      }

      // Signature plots
      Service<SessionManager> pSessionManager;
      for (DOMNode* pChild = pRootElement->getFirstChild(); pChild != NULL; pChild = pChild->getNextSibling())
      {
         DOMElement* pElement = static_cast<DOMElement*>(pChild);
         if (XMLString::equals(pChild->getNodeName(), X("SignaturePlotObject")))
         {
            SignaturePlotObjectInitializer initializer;

            // Plot widget
            string plotWidgetId = A(pElement->getAttribute(X("plotWidgetId")));
            if (plotWidgetId.empty() == false)
            {
               initializer.mpPlotWidget = dynamic_cast<PlotWidget*>(pSessionManager->getSessionItem(plotWidgetId));
            }

            // Signatures and regions
            for (DOMNode* pGChild = pElement->getFirstChild(); pGChild != NULL; pGChild = pGChild->getNextSibling())
            {
               DOMElement* pGChildElement = static_cast<DOMElement*>(pGChild);
               if (XMLString::equals(pGChild->getNodeName(), X("Signature")))
               {
                  string signatureId = A(pGChildElement->getAttribute(X("signatureId")));

                  Signature* pSignature = dynamic_cast<Signature*>(pSessionManager->getSessionItem(signatureId));
                  if (pSignature != NULL)
                  {
                     initializer.mSignatures.push_back(pSignature);
                  }
               }
               else if (XMLString::equals(pGChild->getNodeName(), X("Regions")))
               {
                  initializer.mRegionsDisplayed = StringUtilities::fromXmlString<bool>(
                     A(pGChildElement->getAttribute(X("displayed"))));
                  initializer.mRegionColor = COLORTYPE_TO_QCOLOR(StringUtilities::fromXmlString<ColorType>(
                     A(pGChildElement->getAttribute(X("color")))));
                  initializer.mRegionOpacity = StringUtilities::fromXmlString<int>(
                     A(pGChildElement->getAttribute(X("opacity"))));
               }
            }

            // Plot
            initializer.mWavelengthUnits = StringUtilities::fromXmlString<Wavelengths::WavelengthUnitsType>(
               A(pElement->getAttribute(X("wavelengthUnits"))));
            initializer.mBandsDisplayed = StringUtilities::fromXmlString<bool>(
               A(pElement->getAttribute(X("bandsDisplayed"))));
            initializer.mClearOnAdd = StringUtilities::fromXmlString<bool>(A(pElement->getAttribute(X("clearOnAdd"))));

            // Raster layer
            string rasterLayerId = A(pElement->getAttribute(X("rasterLayerId")));
            if (rasterLayerId.empty() == false)
            {
               initializer.mpRasterLayer = dynamic_cast<RasterLayer*>(pSessionManager->getSessionItem(rasterLayerId));
            }

            mSessionPlots.push_back(initializer);
         }
      }
   }

   return success;
}

bool SignatureWindow::eventFilter(QObject* pObject, QEvent* pEvent)
{
   if ((pObject != NULL) && (pEvent != NULL))
   {
      if (pEvent->type() == QEvent::MouseButtonPress)
      {
         QMouseEvent* pMouseEvent = static_cast<QMouseEvent*> (pEvent);
         if (pMouseEvent->button() == Qt::LeftButton)
         {
            Service<DesktopServices> pDesktop;

            SpatialDataView* pSpatialDataView =
               dynamic_cast<SpatialDataView*>(pDesktop->getCurrentWorkspaceWindowView());
            if (pSpatialDataView != NULL)
            {
               QWidget* pViewWidget  = pSpatialDataView->getWidget();
               if (pViewWidget == pObject)
               {
                  MouseMode* pMouseMode = pSpatialDataView->getCurrentMouseMode();
                  if (pMouseMode != NULL)
                  {
                     string mouseMode = "";
                     pMouseMode->getName(mouseMode);
                     if (mouseMode == "PlugInPixelSignatureMode")
                     {
                        QPoint ptMouse = pMouseEvent->pos();
                        ptMouse.setY(pViewWidget->height() - pMouseEvent->pos().y());

                        LocationType pixelCoord;

                        LayerList* pLayerList = pSpatialDataView->getLayerList();
                        VERIFY(pLayerList != NULL);

                        RasterElement* pRaster = pLayerList->getPrimaryRasterElement();
                        VERIFY(pRaster != NULL);

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
                              string sensorName = pRaster->getName();

                              SignaturePlotObject* pSignaturePlot = getSignaturePlot(pSpatialDataView, sensorName);
                              if (pSignaturePlot != NULL)
                              {
                                 pSignaturePlot->addSignature(pSignature);
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

void SignatureWindow::dropSessionItem(Subject& subject, const string& signal, const boost::any& value)
{
   Signature* pSignature = dynamic_cast<Signature*>(boost::any_cast<SessionItem*>(value));
   if (pSignature != NULL)
   {
      Service<DesktopServices> pDesktop;

      PlotWindow* pPlotWindow = static_cast<PlotWindow*>(pDesktop->getWindow("Signature Window", PLOT_WINDOW));
      if (pPlotWindow != NULL)
      {
         PlotWidget* pWidget = pPlotWindow->getCurrentPlot();
         if (pWidget != NULL)
         {
            SignaturePlotObject* pSignaturePlot = getSignaturePlot(pWidget);
            if (pSignaturePlot != NULL)
            {
               pSignaturePlot->addSignature(pSignature);
            }
         }
      }
   }
}

void SignatureWindow::updateContextMenu(Subject& subject, const string& signal, const boost::any& value)
{
   ContextMenu* pMenu = boost::any_cast<ContextMenu*>(value);
   if (pMenu == NULL)
   {
      return;
   }

   Service<DesktopServices> pDesktop;

   PlotWindow* pWindow = static_cast<PlotWindow*> (pDesktop->getWindow("Signature Window", PLOT_WINDOW));
   if (pWindow == NULL)
   {
      return;
   }

   QObject* pParent = pMenu->getActionParent();

   // Add an action to add a plot to the current plot set
   bool bAddAction = false;
   string beforeId;

   if (dynamic_cast<SessionExplorer*>(&subject) != NULL)
   {
      // Check if a single plot set item is selected
      vector<SessionItem*> items = pMenu->getSessionItems();
      if (items.size() == 1)
      {
         PlotSet* pPlotSet = dynamic_cast<PlotSet*>(items.front());
         if (pPlotSet != NULL)
         {
            if (pWindow->containsPlotSet(pPlotSet) == true)
            {
               bAddAction = true;
            }
         }
      }
   }
   else if (dynamic_cast<PlotWindow*>(&subject) == pWindow)
   {
      bAddAction = true;
      beforeId = APP_PLOTSET_DELETE_ACTION;
   }

   if (bAddAction == true)
   {
      QAction* pAddAction = new QAction("&Add Plot", pParent);
      pAddAction->setAutoRepeat(false);
      pAddAction->setStatusTip("Adds a new plot with a default name");
      VERIFYNR(connect(pAddAction, SIGNAL(triggered()), this, SLOT(addDefaultPlot())));
      pMenu->addActionBefore(pAddAction, SPECTRAL_SIGNATUREWINDOW_ADD_PLOT_ACTION, beforeId);
   }
}

void SignatureWindow::windowAdded(Subject& subject, const string& signal, const boost::any& value)
{
   if (dynamic_cast<DesktopServices*>(&subject) != NULL)
   {
      Window* pWindow = boost::any_cast<Window*>(value);

      SpatialDataWindow* pSpatialDataWindow = dynamic_cast<SpatialDataWindow*>(pWindow);
      if (pSpatialDataWindow != NULL)
      {
         addPixelSignatureMode(pSpatialDataWindow);
      }
   }
}

void SignatureWindow::windowActivated(Subject& subject, const string& signal, const boost::any& value)
{
   enableActions();
}

void SignatureWindow::windowRemoved(Subject& subject, const string& signal, const boost::any& value)
{
   if (dynamic_cast<DesktopServices*>(&subject) != NULL)
   {
      Window* pWindow = boost::any_cast<Window*>(value);

      SpatialDataWindow* pSpatialDataWindow = dynamic_cast<SpatialDataWindow*>(pWindow);
      if (pSpatialDataWindow != NULL)
      {
         removePixelSignatureMode(pSpatialDataWindow);
      }
   }
}

void SignatureWindow::layerActivated(Subject& subject, const string& signal, const boost::any& value)
{
   enableActions();
}

void SignatureWindow::plotWindowShown(Subject& subject, const string& signal, const boost::any& value)
{
   PlotWindow* pPlotWindow = dynamic_cast<PlotWindow*> (&subject);
   if (pPlotWindow != NULL)
   {
      if (mpWindowAction != NULL)
      {
         mpWindowAction->setChecked(true);
      }
   }
}

void SignatureWindow::plotWindowHidden(Subject& subject, const string& signal, const boost::any& value)
{
   PlotWindow* pPlotWindow = dynamic_cast<PlotWindow*> (&subject);
   if (pPlotWindow != NULL)
   {
      if (mpWindowAction != NULL)
      {
         mpWindowAction->setChecked(false);
      }
   }
}

void SignatureWindow::plotSetAdded(Subject& subject, const string& signal, const boost::any& value)
{
   PlotWindow* pPlotWindow = dynamic_cast<PlotWindow*> (&subject);
   if (pPlotWindow != NULL)
   {
      PlotSet* pPlotSet = boost::any_cast<PlotSet*>(value);
      if (pPlotSet != NULL)
      {
         pPlotSet->attach(SIGNAL_NAME(PlotSet, PlotAdded), Slot(this, &SignatureWindow::plotWidgetAdded));
      }
   }
}

void SignatureWindow::plotWidgetAdded(Subject& subject, const string& signal, const boost::any& value)
{
   PlotSet* pPlotSet = dynamic_cast<PlotSet*> (&subject);
   if (pPlotSet != NULL)
   {
      PlotWidget* pPlot = boost::any_cast<PlotWidget*>(value);
      if (pPlot != NULL)
      {
         PlotView* pPlotView = pPlot->getPlot();
         if (pPlotView != NULL)
         {
            if (pPlotView->getPlotType() != SIGNATURE_PLOT)
            {
               return;
            }
         }

         Service<SessionManager> pSessionManager;
         if (pSessionManager->isSessionLoading() == false)
         {
            SignaturePlotObject* pSignaturePlot = new SignaturePlotObject(pPlot, mpProgress);
            if (pSignaturePlot != NULL)
            {
               SpatialDataView* pView = dynamic_cast<SpatialDataView*> (pPlotSet->getAssociatedView());
               if (pView != NULL)
               {
                  LayerList* pLayerList = pView->getLayerList();
                  if (pLayerList != NULL)
                  {
                     RasterElement* pElement = pLayerList->getPrimaryRasterElement();
                     if (pElement != NULL)
                     {
                        RasterLayer* pRasterLayer = static_cast<RasterLayer*>(pLayerList->getLayer(RASTER, pElement));
                        if (pRasterLayer != NULL)
                        {
                           pSignaturePlot->setRasterLayer(pRasterLayer);
                        }
                     }
                  }
               }

               mPlots.push_back(pSignaturePlot);
            }
         }

         pPlot->attach(SIGNAL_NAME(Subject, Deleted), Slot(this, &SignatureWindow::plotWidgetDeleted));
      }
   }
}

void SignatureWindow::plotWidgetDeleted(Subject& subject, const string& signal, const boost::any& value)
{
   PlotWidget* pPlotWidget = dynamic_cast<PlotWidget*> (&subject);
   if (pPlotWidget != NULL)
   {
      vector<SignaturePlotObject*>::iterator iter = mPlots.begin();
      while (iter != mPlots.end())
      {
         SignaturePlotObject* pSignaturePlot = *iter;
         if (pSignaturePlot != NULL)
         {
            PlotWidget* pCurrentPlot = pSignaturePlot->getPlotWidget();
            if (pCurrentPlot == pPlotWidget)
            {
               mPlots.erase(iter);
               delete pSignaturePlot;
               break;
            }
         }

         ++iter;
      }
   }
}

void SignatureWindow::sessionRestored(Subject& subject, const string& signal, const boost::any& value)
{
   Service<DesktopServices> pDesktop;

   vector<Window*> windows;
   pDesktop->getWindows(SPATIAL_DATA_WINDOW, windows);

   for (vector<Window*>::iterator iter = windows.begin(); iter != windows.end(); ++iter)
   {
      SpatialDataWindow* pWindow = static_cast<SpatialDataWindow*>(*iter);
      if (pWindow != NULL)
      {
         addPixelSignatureMode(pWindow);
      }
   }

   vector<SignaturePlotObjectInitializer>::iterator iter;
   for (iter = mSessionPlots.begin(); iter != mSessionPlots.end(); ++iter)
   {
      SignaturePlotObjectInitializer initializer = *iter;

      MouseMode* pMouseMode = NULL;
      if (initializer.mpPlotWidget != NULL)
      {
         PlotView* pPlotView = initializer.mpPlotWidget->getPlot();
         if (pPlotView != NULL)
         {
            pMouseMode = pPlotView->getCurrentMouseMode();
         }
      }

      SignaturePlotObject* pSignaturePlot = new SignaturePlotObject(initializer.mpPlotWidget, mpProgress);
      if (pSignaturePlot != NULL)
      {
         // Signatures
         pSignaturePlot->initializeFromPlot(initializer.mSignatures);

         // Raster layer
         pSignaturePlot->setRasterLayer(initializer.mpRasterLayer);

         // Plot
         pSignaturePlot->setWavelengthUnits(initializer.mWavelengthUnits);
         pSignaturePlot->displayBandNumbers(initializer.mBandsDisplayed);
         pSignaturePlot->setClearOnAdd(initializer.mClearOnAdd);

         // Regions
         pSignaturePlot->displayRegions(initializer.mRegionsDisplayed);
         pSignaturePlot->setRegionColor(initializer.mRegionColor);
         pSignaturePlot->setRegionOpacity(initializer.mRegionOpacity);

         // Mouse mode
         if (pMouseMode != NULL)
         {
            // Set the mouse mode in the plot view because the initialization of the
            // plot object could reset the value that was restored in the session
            VERIFYNRV(initializer.mpPlotWidget != NULL);

            PlotView* pPlotView = initializer.mpPlotWidget->getPlot();
            VERIFYNRV(pPlotView != NULL);

            pPlotView->setMouseMode(pMouseMode);
         }

         mPlots.push_back(pSignaturePlot);
      }
   }
}

void SignatureWindow::addPixelSignatureMode(SpatialDataWindow* pWindow)
{
   if (pWindow == NULL)
   {
      return;
   }

   SpatialDataView* pView = pWindow->getSpatialDataView();
   if (pView == NULL)
   {
      return;
   }

   pView->attach(SIGNAL_NAME(SpatialDataView, LayerActivated), Slot(this, &SignatureWindow::layerActivated));

   QWidget* pViewWidget = pView->getWidget();
   if (pViewWidget != NULL)
   {
      pViewWidget->installEventFilter(this);
   }

   // Create the pixel spectrum mouse mode
   if (mpPixelSignatureMode == NULL)
   {
      Service<DesktopServices> pDesktop;
      mpPixelSignatureMode = pDesktop->createMouseMode("PlugInPixelSignatureMode", NULL, NULL, -1, -1,
         mpPixelSignatureAction);
   }

   // Add the mode to the view
   if (mpPixelSignatureMode != NULL)
   {
      pView->addMouseMode(mpPixelSignatureMode);
   }
}

void SignatureWindow::removePixelSignatureMode(SpatialDataWindow* pWindow)
{
   if (pWindow == NULL)
   {
      return;
   }

   SpatialDataView* pView = pWindow->getSpatialDataView();
   if (pView == NULL)
   {
      return;
   }

   pView->detach(SIGNAL_NAME(SpatialDataView, LayerActivated), Slot(this, &SignatureWindow::layerActivated));

   QWidget* pViewWidget = pView->getWidget();
   if (pViewWidget != NULL)
   {
      pViewWidget->removeEventFilter(this);
   }

   if (mpPixelSignatureMode != NULL)
   {
      pView->removeMouseMode(mpPixelSignatureMode);
   }
}

void SignatureWindow::enableActions()
{
   Service<DesktopServices> pDesktop;

   bool bActiveWindow = false;
   bool bAoiMode = false;

   SpatialDataWindow* pWindow = dynamic_cast<SpatialDataWindow*>(pDesktop->getCurrentWorkspaceWindow());
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

   if (mpPixelSignatureAction != NULL)
   {
      mpPixelSignatureAction->setEnabled(bActiveWindow);
   }

   if (mpAoiSignaturesAction != NULL)
   {
      mpAoiSignaturesAction->setEnabled(bAoiMode);
   }
}

SignaturePlotObject* SignatureWindow::getSignaturePlot(const PlotWidget* pPlot) const
{
   if (pPlot == NULL)
   {
      return NULL;
   }

   vector<SignaturePlotObject*>::const_iterator iter = mPlots.begin();
   while (iter != mPlots.end())
   {
      SignaturePlotObject* pSignaturePlot = *iter;
      if (pSignaturePlot != NULL)
      {
         PlotWidget* pCurrentPlot = pSignaturePlot->getPlotWidget();
         if (pCurrentPlot == pPlot)
         {
            return pSignaturePlot;
         }
      }

      ++iter;
   }

   return NULL;
}

SignaturePlotObject* SignatureWindow::getSignaturePlot(SpatialDataView* pView, const string& plotName)
{
   if ((pView == NULL) || (plotName.empty() == true))
   {
      return NULL;
   }

   Service<DesktopServices> pDesktop;

   // Get a pointer to the signature window
   PlotWindow* pPlotWindow = static_cast<PlotWindow*> (pDesktop->getWindow("Signature Window", PLOT_WINDOW));
   if (pPlotWindow == NULL)
   {
      return NULL;
   }

   // Show the window to ensure it is visible
   pPlotWindow->show();

   // Get the plot set name from the view
   string plotSetName;
   LayerList* pLayerList = pView->getLayerList();
   if (pLayerList != NULL)
   {
      RasterElement* pRaster = pLayerList->getPrimaryRasterElement();
      if (pRaster != NULL)
      {
         plotSetName = pRaster->getName();
      }
   }

   // Get or create the plot set
   PlotSet* pPlotSet = NULL;
   if (plotSetName.empty() == false)
   {
      pPlotSet = pPlotWindow->getPlotSet(plotSetName);
      if (pPlotSet == NULL)
      {
         pPlotSet = pPlotWindow->createPlotSet(plotSetName);
         if (pPlotSet != NULL)
         {
            pPlotSet->setAssociatedView(pView);
         }
      }
      else
      {
         pPlotWindow->setCurrentPlotSet(pPlotSet);
      }
   }
   else
   {
      pPlotSet = pPlotWindow->getCurrentPlotSet();
   }

   if (pPlotSet == NULL)
   {
      return NULL;
   }

   // Get or create the plot
   PlotWidget* pPlot = pPlotSet->getPlot(plotName);
   if (pPlot == NULL)
   {
      pPlot = pPlotSet->createPlot(plotName, SIGNATURE_PLOT);
   }

   if (pPlot == NULL)
   {
      return NULL;
   }

   // Set the plot as the active plot
   pPlotWindow->setCurrentPlot(pPlot);

   return getSignaturePlot(pPlot);
}

void SignatureWindow::displaySignatureWindow(bool bDisplay)
{
   Service<DesktopServices> pDesktop;

   DockWindow* pWindow = static_cast<DockWindow*>(pDesktop->getWindow("Signature Window", PLOT_WINDOW));
   if (pWindow != NULL)
   {
      if (bDisplay == true)
      {
         pWindow->show();
      }
      else
      {
         pWindow->hide();
      }
   }
}

void SignatureWindow::addDefaultPlot()
{
   Service<DesktopServices> pDesktop;

   PlotWindow* pWindow = static_cast<PlotWindow*> (pDesktop->getWindow("Signature Window", PLOT_WINDOW));
   if (pWindow == NULL)
   {
      return;
   }

   // Create a default plot name
   QString strPlotName = "Plot " + QString::number(pWindow->getNumPlots() + 1);

   // Add the plot
   PlotSet* pPlotSet = pWindow->getCurrentPlotSet();
   if (pPlotSet == NULL)
   {
      pPlotSet = pWindow->createPlotSet("Custom Plots");
   }

   if (pPlotSet != NULL)
   {
      pPlotSet->createPlot(strPlotName.toStdString(), SIGNATURE_PLOT);
   }
}

void SignatureWindow::renameCurrentPlot()
{
   Service<DesktopServices> pDesktop;

   PlotWindow* pWindow = static_cast<PlotWindow*> (pDesktop->getWindow("Signature Window", PLOT_WINDOW));
   if (pWindow == NULL)
   {
      return;
   }

   PlotSet* pPlotSet = pWindow->getCurrentPlotSet();
   if (pPlotSet != NULL)
   {
      PlotWidget* pPlot = pPlotSet->getCurrentPlot();
      if (pPlot != NULL)
      {
         pPlotSet->renamePlot(pPlot);
      }
   }
}

void SignatureWindow::deleteCurrentPlot()
{
   Service<DesktopServices> pDesktop;

   PlotWindow* pWindow = static_cast<PlotWindow*> (pDesktop->getWindow("Signature Window", PLOT_WINDOW));
   if (pWindow == NULL)
   {
      return;
   }

   PlotSet* pPlotSet = pWindow->getCurrentPlotSet();
   if (pPlotSet != NULL)
   {
      PlotWidget* pPlot = pPlotSet->getCurrentPlot();
      if (pPlot != NULL)
      {
         pPlotSet->deletePlot(pPlot);
      }
   }
}

void SignatureWindow::displayAoiSignatures()
{
   // Get the current spatial data view
   Service<DesktopServices> pDesktop;

   SpatialDataView* pView = dynamic_cast<SpatialDataView*>(pDesktop->getCurrentWorkspaceWindowView());
   if (pView == NULL)
   {
      return;
   }

   // Get the current AOI
   AoiElement* pAoi = NULL;

   AoiLayer* pAoiLayer = dynamic_cast<AoiLayer*>(pView->getActiveLayer());
   if (pAoiLayer != NULL)
   {
      pAoi = static_cast<AoiElement*>(pAoiLayer->getDataElement());
   }

   if (pAoi == NULL)
   {
      return;
   }

   // Get the sensor data from which to get the pixel signatures
   RasterElement* pRaster= NULL;

   LayerList* pLayerList = pView->getLayerList();
   if (pLayerList != NULL)
   {
      pRaster = pLayerList->getPrimaryRasterElement();
   }

   if (pRaster == NULL)
   {
      return;
   }

   // Get or create the signature plot
   string aoiName = pAoi->getName();

   SignaturePlotObject* pSignaturePlot = getSignaturePlot(pView, aoiName);
   if (pSignaturePlot == NULL)
   {
      return;
   }

   pSignaturePlot->clearSignatures();

   // Get the bounding box of the selected points in the AOI
   const BitMask* pBitMask = pAoi->getSelectedPoints();
   if (pBitMask == NULL)
   {
      return;
   }

   int iStartRow = 0;
   int iEndRow = 0;
   int iStartColumn = 0;
   int iEndColumn = 0;

   pBitMask->getBoundingBox(iStartColumn, iStartRow, iEndColumn, iEndRow);

   // Get the number of selected pixels within the bounding box
   int iTotalCount = 0;
   iTotalCount = pBitMask->getCount();

   // Add the number of selected pixels outside the bounding box
   bool bOutside = pBitMask->getPixel(-1, -1);
   if (bOutside == true)
   {
      const RasterDataDescriptor* pDescriptor =
         dynamic_cast<const RasterDataDescriptor*>(pRaster->getDataDescriptor());
      if (pDescriptor == NULL)
      {
         return;
      }

      unsigned int numRows = pDescriptor->getRowCount();
      unsigned int numColumns = pDescriptor->getColumnCount();

      iTotalCount += (numRows * numColumns) - (iEndRow - iStartRow + 1) * (iEndColumn - iStartColumn + 1);

      iStartRow = 0;
      iEndRow = numRows - 1;
      iStartColumn = 0;
      iEndColumn = numColumns - 1;
   }

   int iCount = 0;

   // Get each selected pixel signature
   if (mpProgress != NULL)
   {
      mpProgress->updateProgress("Adding AOI pixel signatures to the plot...", 0, NORMAL);
   }

   vector<Signature*> aoiSignatures;

   Opticks::PixelLocation pixelLocation;
   for (int i = iStartColumn; i <= iEndColumn; i++)
   {
      for (int j = iStartRow; j <= iEndRow; j++)
      {
         if (isAborted() == true)
         {
            pSignaturePlot->clearSignatures();
            if (mpProgress != NULL)
            {
               mpProgress->updateProgress("Display AOI signatures aborted!", 0, ABORT);
            }

            return;
         }

         bool bSelected = pBitMask->getPixel(i, j);
         if (bSelected == true)
         {
            pixelLocation.mX = static_cast<double>(i);
            pixelLocation.mY = static_cast<double>(j);

            Signature* pSignature = SpectralUtilities::getPixelSignature(pRaster, pixelLocation);
            if (pSignature != NULL)
            {
               aoiSignatures.push_back(pSignature);
            }

            iCount++;

            if (mpProgress != NULL)
            {
               int iPercent = static_cast<int>(static_cast<double>(iCount) /
                  static_cast<double>(iTotalCount) * 100.0);      // Prevent integer overflow
               mpProgress->updateProgress("Adding AOI pixel signatures to the plot...", iPercent, NORMAL);
            }
         }
      }
   }

   // Add the signatures to the plot
   if (aoiSignatures.empty() == false)
   {
      pSignaturePlot->addSignatures(aoiSignatures);
   }

   // Add the averaged signature to the plot
   Service<ModelServices> pModel;

   Signature* pAveragedSignature = static_cast<Signature*>(pModel->getElement(aoiName,
      TypeConverter::toString<Signature>(), pRaster));
   if (pAveragedSignature == NULL)
   {
      pAveragedSignature = static_cast<Signature*>(pModel->createElement(aoiName,
         TypeConverter::toString<Signature>(), pRaster));
   }

   if (pAveragedSignature != NULL)
   {
      if (SpectralUtilities::convertAoiToSignature(pAoi, pAveragedSignature, pRaster) == true)
      {
         pSignaturePlot->addSignature(pAveragedSignature);
         pSignaturePlot->setSignatureColor(pAveragedSignature, Qt::red, true);
      }
   }

   if (mpProgress != NULL)
   {
      mpProgress->updateProgress("Display AOI signatures complete!", 100, NORMAL);
   }
}

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
#include "ColorType.h"
#include "ContextMenu.h"
#include "ContextMenuActions.h"
#include "LayerList.h"
#include "MenuBar.h"
#include "ModelServices.h"
#include "MouseMode.h"
#include "PlotSet.h"
#include "PlotView.h"
#include "PlotWidget.h"
#include "PlotWindow.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "SessionItemDeserializer.h"
#include "SessionItemSerializer.h"
#include "SessionManager.h"
#include "SessionResource.h"
#include "Signature.h"
#include "SignaturePlotObject.h"
#include "SignatureSet.h"
#include "SignatureWindow.h"
#include "SignatureWindowIcons.h"
#include "SignatureWindowOptions.h"
#include "Slot.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "SpectralContextMenuActions.h"
#include "SpectralUtilities.h"
#include "SpectralVersion.h"
#include "ToolBar.h"
#include "TypeConverter.h"
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
   mSignatureWindowName("Signature Window"),
   mDefaultPlotSetName("Custom Plots"),
   mpWindowAction(NULL),
   mpPinSigPlotAction(NULL),
   mpPixelSignatureMode(NULL),
   mpPixelSignatureAction(NULL),
   mpAoiSignaturesAction(NULL),
   mpAoiAverageSigAction(NULL),
   mbNotifySigPlotObjectsOfAbort(true),
   mbFirstTime(true)
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
   // Remove the window action
   if (mpWindowAction != NULL)
   {
      ToolBar* pToolBar = static_cast<ToolBar*>(mpDesktop->getWindow("Spectral", TOOLBAR));
      if (pToolBar != NULL)
      {
         MenuBar* pMenuBar = pToolBar->getMenuBar();
         if (pMenuBar != NULL)
         {
            pMenuBar->removeMenuItem(mpWindowAction);
         }

         pToolBar->removeItem(mpWindowAction);
      }

      if (mpDesktop->getMainWidget() != NULL)
      {
         delete mpWindowAction;
      }
   }

   // Delete the signature window
   PlotWindow* pWindow = static_cast<PlotWindow*>(mpDesktop->getWindow(mSignatureWindowName, PLOT_WINDOW));
   if (pWindow != NULL)
   {
      pWindow->detach(SIGNAL_NAME(DockWindow, Shown), Slot(this, &SignatureWindow::plotWindowShown));
      pWindow->detach(SIGNAL_NAME(DockWindow, Hidden), Slot(this, &SignatureWindow::plotWindowHidden));
      pWindow->detach(SIGNAL_NAME(DockWindow, AboutToShowContextMenu),
         Slot(this, &SignatureWindow::updateContextMenu));
      mpDesktop->deleteWindow(pWindow);
   }

   // Remove the toolbar buttons
   ToolBar* pToolBar = static_cast<ToolBar*>(mpDesktop->getWindow("Spectral", TOOLBAR));
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
      if (mpAoiAverageSigAction != NULL)
      {
         disconnect(mpAoiAverageSigAction, SIGNAL(activated()), this, SLOT(displayAoiAverageSig()));
         pToolBar->removeItem(mpAoiAverageSigAction);
         delete mpAoiAverageSigAction;
      }
   }

   // Detach from the session manager
   Service<SessionManager> pSessionManager;
   pSessionManager->detach(SIGNAL_NAME(SessionManager, SessionRestored),
      Slot(this, &SignatureWindow::sessionRestored));

   // Detach from desktop services
   mpDesktop->detach(SIGNAL_NAME(DesktopServices, WindowAdded), Slot(this, &SignatureWindow::windowAdded));
   mpDesktop->detach(SIGNAL_NAME(DesktopServices, WindowActivated), Slot(this, &SignatureWindow::windowActivated));
   mpDesktop->detach(SIGNAL_NAME(DesktopServices, WindowRemoved), Slot(this, &SignatureWindow::windowRemoved));

   // Remove the mouse mode from the views
   vector<Window*> windows;
   mpDesktop->getWindows(SPATIAL_DATA_WINDOW, windows);

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
      mpDesktop->deleteMouseMode(mpPixelSignatureMode);
   }
}

bool SignatureWindow::setBatch()
{
   AlgorithmShell::setBatch();
   return false;
}

bool SignatureWindow::getInputSpecification(PlugInArgList*& pArgList)
{
   if (mbFirstTime)
   {
      pArgList = NULL;
   }
   else
   {
      pArgList = Service<PlugInManagerServices>()->getPlugInArgList();
      VERIFY(pArgList != NULL);
      bool addPlot(false);
      VERIFY(pArgList->addArg<bool>("Add Plot", &addPlot));
      VERIFY(pArgList->addArg<RasterElement>(Executable::DataElementArg(), NULL));
      VERIFY(pArgList->addArg<Signature>("Signature to add", NULL));
      ColorType defaultColor(0, 0, 0);
      VERIFY(pArgList->addArg<ColorType>("Curve color", &defaultColor));
      VERIFY(pArgList->addArg<bool>("Clear before adding", &addPlot));
   }

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

   // check for first time executed
   if (mbFirstTime)
   {
      mbFirstTime = false;
      QPixmap pixSignatureWindow(SignatureWindowIcons::SignatureWindowIcon);
      pixSignatureWindow.setMask(QPixmap(SignatureWindowIcons::SignatureWindowMask));
      QIcon signatureWindowIcon(pixSignatureWindow);

      // Add a menu command and toolbar button to invoke the window
      ToolBar* pToolBar = static_cast<ToolBar*>(mpDesktop->getWindow("Spectral", TOOLBAR));
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
      Service<PlugInManagerServices> pMgr;
      mpProgress = pMgr->getProgress(this);
      if (mpProgress != NULL)
      {
         mpDesktop->createProgressDialog(getName(), mpProgress);
      }

      // Create the window
      PlotWindow* pWindow = static_cast<PlotWindow*>(mpDesktop->getWindow(mSignatureWindowName, PLOT_WINDOW));
      if (pWindow == NULL)
      {
         pWindow = static_cast<PlotWindow*>(mpDesktop->createWindow(mSignatureWindowName, PLOT_WINDOW));
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

      mpAoiSignaturesAction = new QAction(QIcon(pixAoiSignatures), "&Display AOI Signatures and Average "
         "Signature", this);
      mpAoiSignaturesAction->setAutoRepeat(false);
      mpAoiSignaturesAction->setStatusTip("Displays the pixel signatures and average signature of "
         "the selected pixels in the current AOI layer.");
      VERIFYNR(connect(mpAoiSignaturesAction, SIGNAL(triggered()), this, SLOT(displayAoiSignatures())));

      // Create the pin signature plot action
      QPixmap pixPlotPinned(SignatureWindowIcons::PinIcon);
      QPixmap pixPlotUnpinned(SignatureWindowIcons::UnPinIcon);
      QIcon pinIcon;
      pinIcon.addPixmap(pixPlotUnpinned);
      pinIcon.addPixmap(pixPlotPinned, QIcon::Normal, QIcon::On);
      mpPinSigPlotAction = new QAction(pinIcon, "Pin/Unpin the Signature Window", this);
      mpPinSigPlotAction->setAutoRepeat(false);
      mpPinSigPlotAction->setStatusTip("Pins or unpins the Signature Window such that signatures are added to a custom plot");
      mpPinSigPlotAction->setCheckable(true);
      mpPinSigPlotAction->setChecked(SignatureWindowOptions::getSettingPinSignaturePlot());
      VERIFYNR(connect(mpPinSigPlotAction, SIGNAL(toggled(bool)), this, SLOT(pinSignatureWindow(bool))));

      // Create the AOI average signature action
      QPixmap pixAoiAverageSig(SignatureWindowIcons::AoiAverageSignatureIcon);
      QBitmap bmpAoiAverageSigMask(SignatureWindowIcons::AoiAverageSignatureMask);
      pixAoiAverageSig.setMask(bmpAoiAverageSigMask);

      mpAoiAverageSigAction = new QAction(QIcon(pixAoiAverageSig), "&Display AOI Average Signature", this);
      mpAoiAverageSigAction->setAutoRepeat(false);
      mpAoiAverageSigAction->setStatusTip("Displays the average signature of the selected pixels in the "
         "active AOI layer");
      VERIFYNR(connect(mpAoiAverageSigAction, SIGNAL(triggered()), this, SLOT(displayAoiAverageSig())));

      // Add buttons to the toolbar
      if (pToolBar != NULL)
      {
         pToolBar->addButton(mpPinSigPlotAction);
         pToolBar->addButton(mpPixelSignatureAction);
         pToolBar->addButton(mpAoiSignaturesAction);
         pToolBar->addButton(mpAoiAverageSigAction);
      }

      // Add a default plot to the window
      Service<SessionManager> pSessionManager;
      if (pSessionManager->isSessionLoading() == false)
      {
         addDefaultPlot();
      }

      // Attach to the session manager
      pSessionManager->attach(SIGNAL_NAME(SessionManager, SessionRestored),
         Slot(this, &SignatureWindow::sessionRestored));

      // Initialization
      enableActions();
      pWindow->setIcon(signatureWindowIcon);
      pWindow->enableSessionItemDrops(this);
      pWindow->hide();

      // Connections
      mpDesktop->attach(SIGNAL_NAME(DesktopServices, WindowAdded), Slot(this, &SignatureWindow::windowAdded));
      mpDesktop->attach(SIGNAL_NAME(DesktopServices, WindowActivated), Slot(this, &SignatureWindow::windowActivated));
      mpDesktop->attach(SIGNAL_NAME(DesktopServices, WindowRemoved), Slot(this, &SignatureWindow::windowRemoved));

      Service<SessionExplorer> pExplorer;
      mpExplorer.reset(pExplorer.get());

      return true;
   }

   // add plot interface
   if (pInArgList != NULL)
   {
      bool* pAddPlot = pInArgList->getPlugInArgValue<bool>("Add Plot");
      VERIFY(pAddPlot != NULL);
      if (*pAddPlot)
      {
         RasterElement* pRaster = pInArgList->getPlugInArgValue<RasterElement>(Executable::DataElementArg());
         Signature* pSignature = pInArgList->getPlugInArgValue<Signature>("Signature to add");
         ColorType color(0, 0, 0);  //default color
         pInArgList->getPlugInArgValue<ColorType>("Curve color", color);
         bool* pClear = pInArgList->getPlugInArgValue<bool>("Clear before adding");
         VERIFY(pRaster != NULL && pSignature != NULL && color.isValid() && pClear != NULL);
         if (pSignature->getType() == TypeConverter::toString<SignatureSet>())
         {
            SignatureSet* pSet = dynamic_cast<SignatureSet*>(pSignature);
            VERIFY(pSet != NULL);
            std::vector<Signature*> signatures = pSet->getSignatures();
            for (std::vector<Signature*>::iterator it = signatures.begin(); it != signatures.end(); ++it)
            {
               addPlot(pRaster, *it, color, *pClear);
            }
         }
         else
         {
            addPlot(pRaster, pSignature, color, *pClear);
         }
         return true;
      }
   }

   return false;
}

bool SignatureWindow::abort()
{
   if (mbNotifySigPlotObjectsOfAbort)
   {
      // get the active plot and call abort
      PlotWindow* pPlotWindow = static_cast<PlotWindow*>(mpDesktop->getWindow(mSignatureWindowName, PLOT_WINDOW));
      VERIFY(pPlotWindow != NULL);
      PlotWidget* pPlotWidget = pPlotWindow->getCurrentPlot();
      SignaturePlotObject* pSigPlot = getSignaturePlot(pPlotWidget);
      if (pSigPlot != NULL)
      {
         pSigPlot->abort();
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
      writer.addAttr("pinSignaturePlot", mpPinSigPlotAction->isChecked());

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
            writer.addAttr("rescaleOnAdd", pPlot->getRescaleOnAdd());

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

   bool pinSigPlot(false);
   bool success = execute(NULL, NULL);
   if ((success == true) && (mpWindowAction != NULL))
   {
      // Initialize the menu action
      XmlReader reader(NULL, false);
      DOMElement* pRootElement = deserializer.deserialize(reader, "SignatureWindow");
      if (pRootElement)
      {
         bool shown = StringUtilities::fromXmlString<bool>(A(pRootElement->getAttribute(X("shown"))));
         mpWindowAction->setChecked(shown);

         // have to save value till after session restore finishes
         pinSigPlot = StringUtilities::fromXmlString<bool>(A(pRootElement->getAttribute(X("pinSignaturePlot"))));
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
            initializer.mWavelengthUnits = StringUtilities::fromXmlString<WavelengthUnitsType>(
               A(pElement->getAttribute(X("wavelengthUnits"))));
            initializer.mBandsDisplayed = StringUtilities::fromXmlString<bool>(
               A(pElement->getAttribute(X("bandsDisplayed"))));
            initializer.mClearOnAdd = StringUtilities::fromXmlString<bool>(A(pElement->getAttribute(X("clearOnAdd"))));
            initializer.mRescaleOnAdd = StringUtilities::fromXmlString<bool>(A(pElement->getAttribute(X("rescaleOnAdd"))));

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
   mpPinSigPlotAction->setChecked(pinSigPlot);

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

                              SignaturePlotObject* pSignaturePlot = getSignaturePlot(sensorName);
                              if (pSignaturePlot != NULL)
                              {
                                 // get color for the pixel signature
                                 ColorType color = SignatureWindowOptions::getSettingPixelSignaturesColor();
                                 if (!color.isValid())
                                 {
                                    color = ColorType(0, 0, 0);  //default to black
                                 }

                                 pSignaturePlot->addSignature(pSignature, color);
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
      // Lock Session Save while adding the dropped sig
      SessionSaveLock lock;

      PlotWindow* pPlotWindow = static_cast<PlotWindow*>(mpDesktop->getWindow(mSignatureWindowName, PLOT_WINDOW));
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

   PlotWindow* pWindow = static_cast<PlotWindow*>(mpDesktop->getWindow(mSignatureWindowName, PLOT_WINDOW));
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
   setCurrentPlotSet(getPlotSetName());
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
   vector<Window*> windows;
   mpDesktop->getWindows(SPATIAL_DATA_WINDOW, windows);

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
         pSignaturePlot->setRescaleOnAdd(initializer.mRescaleOnAdd);

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
      mpPixelSignatureMode = mpDesktop->createMouseMode("PlugInPixelSignatureMode", NULL, NULL, -1, -1,
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

   if (mpPinSigPlotAction != NULL)
   {
      mpPinSigPlotAction->setEnabled(bActiveWindow);
   }

   if (mpPixelSignatureAction != NULL)
   {
      mpPixelSignatureAction->setEnabled(bActiveWindow);
   }

   if (mpAoiSignaturesAction != NULL)
   {
      mpAoiSignaturesAction->setEnabled(bAoiMode);
   }

   if (mpAoiAverageSigAction != NULL)
   {
      mpAoiAverageSigAction->setEnabled(bAoiMode);
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

SignaturePlotObject* SignatureWindow::getSignaturePlot(const string& plotName)
{
   SpatialDataView* pView = dynamic_cast<SpatialDataView*>(mpDesktop->getCurrentWorkspaceWindowView());

   if (pView == NULL || plotName.empty())
   {
      return NULL;
   }

   // Get a pointer to the signature window
   PlotWindow* pPlotWindow = static_cast<PlotWindow*>(mpDesktop->getWindow(mSignatureWindowName, PLOT_WINDOW));
   if (pPlotWindow == NULL)
   {
      return NULL;
   }

   // Show the window to ensure it is visible
   pPlotWindow->show();

   // Get or create the plot set
   string plotSetName = getPlotSetName();
   if (plotSetName.empty())
   {
      return NULL;
   }
   PlotSet* pPlotSet = pPlotWindow->getPlotSet(plotSetName);
   if (pPlotSet == NULL)
   {
      if (mpPinSigPlotAction->isChecked())
      {
         addDefaultPlot();
         pPlotSet = pPlotWindow->getPlotSet(mDefaultPlotSetName);
      }
      else
      {
         pPlotSet = pPlotWindow->createPlotSet(plotSetName);
         if (pPlotSet != NULL)
         {
            pPlotSet->setAssociatedView(pView);
         }
      }
      if (pPlotSet == NULL)
      {
         return NULL;
      }
   }

   pPlotWindow->setCurrentPlotSet(pPlotSet);

   // Get or create the plot
   PlotWidget* pPlot(NULL);
   if (mpPinSigPlotAction->isChecked())
   {
      pPlot = pPlotSet->getCurrentPlot();
      if (pPlot == NULL)
      {
         addDefaultPlot();  // adds a plot to current plotset
         pPlot = pPlotSet->getCurrentPlot();
      }
   }
   else
   {
      pPlot = pPlotSet->getPlot(plotName);
      if (pPlot == NULL)
      {
         pPlot = pPlotSet->createPlot(plotName, SIGNATURE_PLOT);
      }
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
   PlotWindow* pWindow = static_cast<PlotWindow*>(mpDesktop->getWindow(mSignatureWindowName, PLOT_WINDOW));
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
   PlotWindow* pWindow = static_cast<PlotWindow*>(mpDesktop->getWindow(mSignatureWindowName, PLOT_WINDOW));
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
      pPlotSet = pWindow->createPlotSet(mDefaultPlotSetName);
   }

   if (pPlotSet != NULL)
   {
      pPlotSet->createPlot(strPlotName.toStdString(), SIGNATURE_PLOT);
   }
}

SignaturePlotObject* SignatureWindow::getSignaturePlotForAverage() const
{
   SpatialDataView* pView = dynamic_cast<SpatialDataView*>(mpDesktop->getCurrentWorkspaceWindowView());
   if (pView == NULL)
   {
      return NULL;
   }

   PlotWindow* pWindow = static_cast<PlotWindow*>(mpDesktop->getWindow(mSignatureWindowName, PLOT_WINDOW));
   if (pWindow == NULL)
   {
      return NULL;
   }

   // Show the window to ensure it is visible
   pWindow->show();

   // Get the plot set
   string plotSetName = getPlotSetName();
   if (plotSetName.empty())
   {
      return NULL;
   }

   PlotSet* pPlotSet = pWindow->getPlotSet(plotSetName);
   if (pPlotSet == NULL)
   {
      return NULL;
   }

   // set as current plotset in case user has changed the currently displayed plotset 
   pWindow->setCurrentPlotSet(pPlotSet);

   PlotWidget* pPlot = pPlotSet->getCurrentPlot();
   if (pPlot == NULL)
   {
      return NULL;
   }

   return getSignaturePlot(pPlot);
}

void SignatureWindow::renameCurrentPlot()
{
   PlotWindow* pWindow = static_cast<PlotWindow*>(mpDesktop->getWindow(mSignatureWindowName, PLOT_WINDOW));
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
   PlotWindow* pWindow = static_cast<PlotWindow*>(mpDesktop->getWindow(mSignatureWindowName, PLOT_WINDOW));
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
   // Lock Session Save while generating and displaying the AOI signatures
   SessionSaveLock lock;

   // reset abort flag - may have been set when a signature search was canceled
   mAborted = false;

   // Get the current spatial data view
   SpatialDataView* pView = dynamic_cast<SpatialDataView*>(mpDesktop->getCurrentWorkspaceWindowView());
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
   SignaturePlotObject* pSignaturePlot = getSignaturePlot(pAoi->getName());
   if (pSignaturePlot == NULL)
   {
      return;
   }

   updateProgress("Generating AOI pixel signatures...", 0, NORMAL);

   mbNotifySigPlotObjectsOfAbort = false;  // turn off while in call to SpectralUtilities
   vector<Signature*> aoiSignatures = SpectralUtilities::getAoiSignatures(pAoi, pRaster, mpProgress, &mAborted);
   mbNotifySigPlotObjectsOfAbort = true;
   if (isAborted())
   {
      updateProgress("Display of AOI pixel signatures aborted", 0, ABORT);
      mAborted = false;
      return;
   }

   if (aoiSignatures.empty())
   {
      updateProgress("Unable to generate AOI pixel signatures", 0, ERRORS);
      return;
   }

   // get color for the AOI signatures
   ColorType color;
   if (SignatureWindowOptions::getSettingUseAoiColorForAoiSignatures())
   {
      color = pAoiLayer->getColor();
   }
   else
   {
      color = SignatureWindowOptions::getSettingAoiSignaturesColor();
   }
   if (!color.isValid())
   {
      color = ColorType(0, 0, 0);  //default to black
   }

   // Add the signatures to the plot
   bool saveClearOnAdd = pSignaturePlot->isClearOnAdd();
   pSignaturePlot->setClearOnAdd(true); // always clear plot before adding aoi sigs since aoi may have changed
   pSignaturePlot->addSignatures(aoiSignatures, color);
   pSignaturePlot->setClearOnAdd(saveClearOnAdd); // restore original setting
   if (isAborted())
   {
      // clear plot and reset local abort flag
      pSignaturePlot->clearSignatures();
      mAborted = false;
      return;
   }

   updateProgress("Display AOI signatures complete!", 100, NORMAL);

   // now add average signature
   displayAoiAverageSig();
}

void SignatureWindow::displayAoiAverageSig()
{
   // Lock Session Save while generating and displaying the AOI avg sig
   SessionSaveLock lock;

   // reset abort flag - may have been set when a signature search was canceled
   mAborted = false;

   // Get the current spatial data view
   SpatialDataView* pView = dynamic_cast<SpatialDataView*>(mpDesktop->getCurrentWorkspaceWindowView());
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

   // Get the raster element
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

   // Get the current plot for the plotset or create the signature plot for the AOI if there is no current plot
   SignaturePlotObject* pSignaturePlot = getSignaturePlotForAverage();
   if (pSignaturePlot == NULL)
   {
      pSignaturePlot = getSignaturePlot(pAoi->getName());  // this method will create the plot if it doesn't exist
      if (pSignaturePlot == NULL)
      {
         updateProgress("Unable to retrieve or create the plot for the AOI", 0, ERRORS);
         return;
      }
   }

   // Add the averaged signature to the plot
   Service<ModelServices> pModel;
   string avgSigName = pAoi->getName() + " Average Signature";
   Signature* pAveragedSignature = static_cast<Signature*>(pModel->getElement(avgSigName,
      TypeConverter::toString<Signature>(), pRaster));
   if (pAveragedSignature == NULL)
   {
      pAveragedSignature = static_cast<Signature*>(pModel->createElement(avgSigName,
         TypeConverter::toString<Signature>(), pRaster));
   }

   updateProgress("Computing average AOI signature...", 0, NORMAL);

   if (pAveragedSignature != NULL)
   {
      mbNotifySigPlotObjectsOfAbort = false;
      bool success = SpectralUtilities::convertAoiToSignature(pAoi, pAveragedSignature, pRaster,
         mpProgress, &mAborted);
      mbNotifySigPlotObjectsOfAbort = true;
      if (isAborted())
      {
         updateProgress("Compute AOI average signature aborted", 0, ABORT);
         mAborted = false;
         return;
      }
      if (success)
      {
         // get color for the AOI average signature
         ColorType color;
         if (SignatureWindowOptions::getSettingUseAoiColorForAverage())
         {
            color = pAoiLayer->getColor();
         }
         else
         {
            color = SignatureWindowOptions::getSettingAoiAverageColor();
         }
         if (!color.isValid())
         {
            color = ColorType(255, 0, 0);  //default to red
         }

         pSignaturePlot->addSignature(pAveragedSignature, color);
         updateProgress("Display average AOI signature complete!", 100, NORMAL);
      }
      else
      {
         updateProgress("Unable to compute the average AOI signature!", 0, ERRORS);
      }
   }
   else
   {
      updateProgress("Unable to create average AOI signature!", 0, ERRORS);
   }
}

bool SignatureWindow::setCurrentPlotSet(const string& plotsetName)
{
   PlotWindow* pSigWindow = static_cast<PlotWindow*>(mpDesktop->getWindow(mSignatureWindowName, PLOT_WINDOW));
   VERIFY(pSigWindow != NULL);  // signature window should always exist - no means for user to close or delete it.

   PlotSet* pPlotSet = pSigWindow->getPlotSet(plotsetName);
   if (pPlotSet == NULL)
   {
      return false;
   }

   return pSigWindow->setCurrentPlotSet(pPlotSet);
}

void SignatureWindow::pinSignatureWindow(bool enable)
{
   if (enable)
   {
      setCurrentPlotSet(mDefaultPlotSetName);
   }
   else
   {
      setCurrentPlotSet(getPlotSetName());
   }
}

string SignatureWindow::getPlotSetName() const
{
   if (mpPinSigPlotAction->isChecked())
   {
      return mDefaultPlotSetName;
   }

   SpatialDataView* pSpatialDataView =
      dynamic_cast<SpatialDataView*>(mpDesktop->getCurrentWorkspaceWindowView());
   if (pSpatialDataView == NULL)
   {
      return mDefaultPlotSetName;
   }

   LayerList* pLayerList = pSpatialDataView->getLayerList();
   VERIFYRV(pLayerList != NULL, mDefaultPlotSetName);

   RasterElement* pRaster = pLayerList->getPrimaryRasterElement();
   if (pRaster == NULL)
   {
      return mDefaultPlotSetName;
   }

   return pRaster->getName();
}

void SignatureWindow::updateProgress(const std::string& msg, int percent, ReportingLevel level)
{
   if (mpProgress != NULL)
   {
      mpProgress->updateProgress(msg, percent, level);
   }
}

void SignatureWindow::addPlot(const RasterElement* pRaster, Signature* pSignature, const ColorType& color, bool clearBeforeAdd)
{
   if (pRaster != NULL && pSignature != NULL && color.isValid())
   {
      string sensorName = pRaster->getName();

      SignaturePlotObject* pSignaturePlot = getSignaturePlot(sensorName);
      if (pSignaturePlot != NULL)
      {
         if (clearBeforeAdd)
         {
            pSignaturePlot->clearSignatures();
         }
         pSignaturePlot->displayBandNumbers(false);
         pSignaturePlot->addSignature(pSignature, color);
      }
   }
}
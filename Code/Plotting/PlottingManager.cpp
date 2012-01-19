/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "DesktopServices.h"
#include "DockWindow.h"
#include "LayerList.h"
#include "MouseMode.h"
#include "PlotSetGroup.h"
#include "PlottingManager.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "Progress.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "SessionItemDeserializer.h"
#include "SessionItemSerializer.h"
#include "SessionManager.h"
#include "SessionResource.h"
#include "Slot.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "SpectralVersion.h"
#include "ToolBar.h"
#include "xmlreader.h"
#include "xmlwriter.h"

#include <QtGui/QAction>
#include <QtGui/QMouseEvent>
#include <QtGui/QSpinBox>

using XERCES_CPP_NAMESPACE_QUALIFIER DOMNode;
using XERCES_CPP_NAMESPACE_QUALIFIER DOMElement;
using XERCES_CPP_NAMESPACE_QUALIFIER XMLString;

REGISTER_PLUGIN_BASIC(SpectralPlotting, PlottingManager);

PlottingManager::PlottingManager() :
         mHorizontal(true),
         mVertical(false),
         mDesktopAttachment(Service<DesktopServices>().get()),
         mpProfileMouseMode(NULL),
         mpProfileAction(NULL)
{
   PlugInShell::setName("Plotting Manager");
   setDescription("Singleton plug-in to manage the plotting data types and views.");
   setType("Manager");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setDescriptorId("{f61c1ab5-2667-48fd-acd7-374129dec3b6}");
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   allowMultipleInstances(false);
   executeOnStartup(true);
   destroyAfterExecute(false);
   setWizardSupported(false);

   Q_INIT_RESOURCE(Plotting);
}

PlottingManager::~PlottingManager()
{
   // Remove the actions
   ToolBar* pToolBar = static_cast<ToolBar*>(Service<DesktopServices>()->getWindow("Spectral", TOOLBAR));
   if (pToolBar != NULL)
   {
      for (std::map<DockWindow*, QAction*>::iterator toggleAction = mToggleActions.begin();
              toggleAction != mToggleActions.end(); ++toggleAction)
      {
         pToolBar->removeItem(toggleAction->second);
         toggleAction->first->detach(SIGNAL_NAME(DockWindow, Shown),
            Slot(this, &PlottingManager::dockWindowShown));
         toggleAction->first->detach(SIGNAL_NAME(DockWindow, Hidden),
            Slot(this, &PlottingManager::dockWindowHidden));
         Service<DesktopServices>()->deleteWindow(toggleAction->first);
         delete toggleAction->second;
      }
      pToolBar->removeItem(mpProfileAction);
   }

   delete mpProfileAction;

   std::vector<Window*> windows;
   Service<DesktopServices>()->getWindows(SPATIAL_DATA_WINDOW, windows);
   for (std::vector<Window*>::iterator window = windows.begin(); window != windows.end(); ++window)
   {
      SpatialDataWindow* pWindow = static_cast<SpatialDataWindow*>(*window);
      if (pWindow != NULL)
      {
         SpatialDataView* pView = pWindow->getSpatialDataView();
         pView->removeMouseMode(mpProfileMouseMode);
         pView->getWidget()->removeEventFilter(this);
      }
   }

   Service<DesktopServices>()->deleteMouseMode(mpProfileMouseMode);

   Q_CLEANUP_RESOURCE(Plotting);
}

bool PlottingManager::setBatch()
{
   ExecutableShell::setBatch();
   return false;
}

bool PlottingManager::getInputSpecification(PlugInArgList*& pInArgList)
{
   pInArgList = NULL;
   return true;
}

bool PlottingManager::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   pOutArgList = NULL;
   return true;
}

bool PlottingManager::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   bool sesLoad = Service<SessionManager>()->isSessionLoading();

   // Setup the plot windows
   DockWindow* pHorizontalDockWindow = static_cast<DockWindow*>(
      sesLoad ? Service<DesktopServices>()->getWindow("Horizontal Profiles", DOCK_WINDOW)
              : Service<DesktopServices>()->createWindow("Horizontal Profiles", DOCK_WINDOW));
   if (pHorizontalDockWindow == NULL)
   {
      return false;
   }
   pHorizontalDockWindow->attach(SIGNAL_NAME(DockWindow, Shown), Slot(this, &PlottingManager::dockWindowShown));
   pHorizontalDockWindow->attach(SIGNAL_NAME(DockWindow, Hidden), Slot(this, &PlottingManager::dockWindowHidden));

   PlotSetGroup* pHorizontalWidget = Service<DesktopServices>()->createPlotSetGroup();
   if (pHorizontalWidget == NULL)
   {
      return false;
   }
   pHorizontalDockWindow->setWidget(pHorizontalWidget->getWidget());

   PlotSet* pHorizontal = pHorizontalWidget->createPlotSet("Horizontal Profiles");
   if (pHorizontal == NULL)
   {
      return false;
   }
   mHorizontal.setPlotSet(pHorizontal, pHorizontalDockWindow);
   pHorizontalDockWindow->hide();

   DockWindow* pVerticalDockWindow = static_cast<DockWindow*>(
      sesLoad ? Service<DesktopServices>()->getWindow("Vertical Profiles", DOCK_WINDOW)
              : Service<DesktopServices>()->createWindow("Vertical Profiles", DOCK_WINDOW));
   if (pVerticalDockWindow == NULL)
   {
      return false;
   }
   pVerticalDockWindow->attach(SIGNAL_NAME(DockWindow, Shown), Slot(this, &PlottingManager::dockWindowShown));
   pVerticalDockWindow->attach(SIGNAL_NAME(DockWindow, Hidden), Slot(this, &PlottingManager::dockWindowHidden));

   PlotSetGroup* pVerticalWidget = Service<DesktopServices>()->createPlotSetGroup();
   if (pVerticalWidget == NULL)
   {
      return false;
   }
   pVerticalDockWindow->setWidget(pVerticalWidget->getWidget());

   PlotSet* pVertical = pVerticalWidget->createPlotSet("Vertical Profiles");
   if (pVertical == NULL)
   {
      return false;
   }
   mVertical.setPlotSet(pVertical, pVerticalDockWindow);
   pVerticalDockWindow->hide();

   // Create mouse modes
   mpProfileAction = new QAction(QIcon(":/Spectral/icons/ProfileMouseMode"), "Profile Plot", this);
   mpProfileAction->setAutoRepeat(false);
   mpProfileAction->setCheckable(true);
   mpProfileAction->setStatusTip("Plot horizontal and vertical profiles for a selected pixel.");
   mpProfileMouseMode = Service<DesktopServices>()->createMouseMode("PlugInProfileMouseMode", NULL, NULL, -1, -1,
         mpProfileAction);

   // Setup the toolbar
   ToolBar* pToolBar = static_cast<ToolBar*>(Service<DesktopServices>()->getWindow("Spectral", TOOLBAR));
   if (pToolBar == NULL)
   {
      return false;
   }

   pToolBar->addSeparator();

   QAction* pHorizontalToggleAction = new QAction(QIcon(":/Spectral/icons/HorizontalProfilePlot"),
      "Horizontal Profiles", this);
   pHorizontalToggleAction->setAutoRepeat(false);
   pHorizontalToggleAction->setCheckable(true);
   pHorizontalToggleAction->setChecked(false);
   pHorizontalToggleAction->setStatusTip("Toggle the display of the Horizontal Profiles");
   VERIFYNR(connect(pHorizontalToggleAction, SIGNAL(triggered(bool)), this, SLOT(dockWindowActionToggled(bool))));
   pHorizontalToggleAction->setData(qVariantFromValue(reinterpret_cast<void*>(pHorizontalDockWindow)));
   mToggleActions[pHorizontalDockWindow] = pHorizontalToggleAction;
   pToolBar->addButton(pHorizontalToggleAction);

   QAction* pVerticalToggleAction = new QAction(QIcon(":/Spectral/icons/VerticalProfilePlot"),
      "Vertical Profiles", this);
   pVerticalToggleAction->setAutoRepeat(false);
   pVerticalToggleAction->setCheckable(true);
   pVerticalToggleAction->setChecked(false);
   pVerticalToggleAction->setStatusTip("Toggle the display of the Band Vertical Profiles");
   VERIFYNR(connect(pVerticalToggleAction, SIGNAL(triggered(bool)), this, SLOT(dockWindowActionToggled(bool))));
   pVerticalToggleAction->setData(qVariantFromValue(reinterpret_cast<void*>(pVerticalDockWindow)));
   mToggleActions[pVerticalDockWindow] = pVerticalToggleAction;
   pToolBar->addButton(pVerticalToggleAction);

   pToolBar->addButton(mpProfileAction);

   pToolBar->addSeparator();

   enableAction();

   // misc attachments
   mDesktopAttachment.addSignal(SIGNAL_NAME(DesktopServices, WindowAdded),
      Slot(this, &PlottingManager::windowAdded));
   mDesktopAttachment.addSignal(SIGNAL_NAME(DesktopServices, WindowRemoved),
      Slot(this, &PlottingManager::windowRemoved));
   mDesktopAttachment.addSignal(SIGNAL_NAME(DesktopServices, WindowActivated),
      Slot(this, &PlottingManager::windowActivated));

   return true;
}

ProfilePlotUtilities& PlottingManager::getHorizontal()
{
   return mHorizontal;
}

ProfilePlotUtilities& PlottingManager::getVertical()
{
   return mVertical;
}

bool PlottingManager::serialize(SessionItemSerializer& serializer) const
{
   XMLWriter xml("PlottingManager");
   xml.addAttr("mouseModeActive", mpProfileAction->isChecked());
   for (std::map<DockWindow*, QAction*>::const_iterator toggle = mToggleActions.begin();
         toggle != mToggleActions.end(); ++toggle)
   {
      xml.pushAddPoint(xml.addElement("ToggleAction"));
      xml.addAttr("shown", toggle->second->isChecked());
      xml.addText(toggle->first->getId());
      xml.popAddPoint();
   }

   xml.pushAddPoint(xml.addElement("Horizontal"));
   if (!mHorizontal.toXml(&xml))
   {
      return false;
   }
   xml.popAddPoint();
   xml.pushAddPoint(xml.addElement("Vertical"));
   if (!mVertical.toXml(&xml))
   {
      return false;
   }
   xml.popAddPoint();
   return serializer.serialize(xml);
}

bool PlottingManager::deserialize(SessionItemDeserializer& deserializer)
{
   if (!execute(NULL, NULL) || mpProfileAction == NULL)
   {
      return false;
   }
   XmlReader xml(NULL, false);
   DOMElement * pDoc = deserializer.deserialize(xml, "PlottingManager");
   if (pDoc == NULL)
   {
      return false;
   }
   mpProfileAction->setChecked(
      StringUtilities::fromXmlString<bool>(A(pDoc->getAttribute(X("mouseModeActive")))));
   for (DOMNode* pChild = pDoc->getFirstChild(); pChild != NULL; pChild = pChild->getNextSibling())
   {
      if (XMLString::equals(pChild->getNodeName(), X("ToggleAction")))
      {
         DOMElement* pElement = static_cast<DOMElement*>(pChild);
         bool shown = StringUtilities::fromXmlString<bool>(A(pElement->getAttribute(X("shown"))));
         std::string sessId = A(pElement->getTextContent());
         DockWindow* pWindow = dynamic_cast<DockWindow*>(
            Service<SessionManager>()->getSessionItem(A(pElement->getTextContent())));
         std::map<DockWindow*, QAction*>::iterator it = mToggleActions.find(pWindow);
         if (it == mToggleActions.end())
         {
            return false;
         }
         it->second->setChecked(shown);
      }
      else if (XMLString::equals(pChild->getNodeName(), X("Horizontal")))
      {
         if (!mHorizontal.fromXml(pChild, 0))
         {
            return false;
         }
      }
      else if (XMLString::equals(pChild->getNodeName(), X("Vertical")))
      {
         if (!mVertical.fromXml(pChild, 0))
         {
            return false;
         }
      }
   }
   return true;
}

void PlottingManager::windowAdded(Subject& subject, const std::string& signal, const boost::any& value)
{
   if (dynamic_cast<DesktopServices*>(&subject) != NULL)
   {
      SpatialDataWindow* pWindow = dynamic_cast<SpatialDataWindow*>(boost::any_cast<Window*>(value));
      SpatialDataView* pView = pWindow == NULL ? NULL : pWindow->getSpatialDataView();
      if (pView != NULL)
      {
         pView->addMouseMode(mpProfileMouseMode);
         pView->getWidget()->installEventFilter(this);
      }
   }
}

void PlottingManager::windowRemoved(Subject& subject, const std::string& signal, const boost::any& value)
{
   if (dynamic_cast<DesktopServices*>(&subject) != NULL)
   {
      SpatialDataWindow* pWindow = dynamic_cast<SpatialDataWindow*>(boost::any_cast<Window*>(value));
      SpatialDataView* pView = pWindow == NULL ? NULL : pWindow->getSpatialDataView();
      if (pView != NULL)
      {
         pView->removeMouseMode(mpProfileMouseMode);
         pView->getWidget()->removeEventFilter(this);
      }
   }
}

void PlottingManager::windowActivated(Subject& subject, const std::string& signal, const boost::any& value)
{
   enableAction();
}

void PlottingManager::enableAction()
{
   if (mpProfileAction != NULL)
   {
      SpatialDataWindow* pWindow =
         dynamic_cast<SpatialDataWindow*>(Service<DesktopServices>()->getCurrentWorkspaceWindow());
      SpatialDataView* pView = (pWindow == NULL) ? NULL : pWindow->getSpatialDataView();
      mpProfileAction->setEnabled(pView != NULL);
   }
}

void PlottingManager::dockWindowShown(Subject& subject, const std::string& signal, const boost::any& value)
{
   DockWindow* pDockWindow = dynamic_cast<DockWindow*>(&subject);
   QAction* pAction = mToggleActions[pDockWindow];
   if (pAction != NULL)
   {
      pAction->setChecked(true);
   }
}

void PlottingManager::dockWindowHidden(Subject& subject, const std::string& signal, const boost::any& value)
{
   DockWindow* pDockWindow = dynamic_cast<DockWindow*>(&subject);
   QAction* pAction = mToggleActions[pDockWindow];
   if (pAction != NULL)
   {
      pAction->setChecked(false);
   }
}

void PlottingManager::dockWindowActionToggled(bool shown)
{
   QAction* pSender = qobject_cast<QAction*>(sender());
   if (pSender == NULL)
   {
      return;
   }
   DockWindow* pDockWindow = reinterpret_cast<DockWindow*>(qvariant_cast<void*>(pSender->data()));
   if (pDockWindow == NULL)
   {
      return;
   }
   if (shown)
   {
      pDockWindow->show();
   }
   else
   {
      pDockWindow->hide();
   }
}

bool PlottingManager::eventFilter(QObject* pObject, QEvent* pEvent)
{
   if (pObject != NULL && pEvent != NULL && pEvent->type() == QEvent::MouseButtonPress)
   {
      QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(pEvent);
      if (pMouseEvent->button() == Qt::LeftButton)
      {
         // Lock Session Save while generating and displaying the pixel signature
         SessionSaveLock lock;

         SpatialDataView* pSpatialDataView =
            dynamic_cast<SpatialDataView*>(Service<DesktopServices>()->getCurrentWorkspaceWindowView());
         QWidget* pViewWidget = pSpatialDataView == NULL ? NULL : pSpatialDataView->getWidget();
         if (pViewWidget == pObject)
         {
            MouseMode* pMouseMode = pSpatialDataView->getCurrentMouseMode();
            if (pMouseMode != NULL)
            {
               std::string mouseMode;
               pMouseMode->getName(mouseMode);
               if (mouseMode == "PlugInProfileMouseMode")
               {
                  QPoint ptMouse = pMouseEvent->pos();
                  ptMouse.setY(pViewWidget->height() - pMouseEvent->pos().y());

                  LocationType pixelCoord;

                  // find the appropriate layer
                  RasterLayer* pCurHoriz = mHorizontal.getActivePlotLayer();
                  RasterLayer* pCurVert = mVertical.getActivePlotLayer();
                  RasterLayer* pLayer = NULL;

                  // are either of these in the current view? if so, plot to them giving preference to horizontal
                  // otherwise use the primary
                  LayerList* pLayerList = pSpatialDataView->getLayerList();
                  VERIFY(pLayerList != NULL);
                  if (pLayerList->containsLayer(pCurHoriz))
                  {
                     pLayer = pCurHoriz;
                  }
                  else if (pLayerList->containsLayer(pCurVert))
                  {
                     pLayer = pCurVert;
                  }
                  else
                  {
                     pLayer = static_cast<RasterLayer*>(
                        pLayerList->getLayer(RASTER, pLayerList->getPrimaryRasterElement()));
                  }
                  if (pLayer == NULL)
                  {
                     return false;
                  }
                  pLayer->translateScreenToData(ptMouse.x(), ptMouse.y(), pixelCoord.mX, pixelCoord.mY);
                  RasterDataDescriptor* pDesc =
                     static_cast<RasterDataDescriptor*>(pLayer->getDataElement()->getDataDescriptor());
                  VERIFY(pDesc);
                  if (pixelCoord.mX < 0 || pixelCoord.mX >= pDesc->getColumnCount() ||
                      pixelCoord.mY < 0 || pixelCoord.mY >= pDesc->getRowCount())
                  {
                     return false;
                  }

                  // finally, plot the data to the appropriate plots
                  mHorizontal.clearPlot(pLayer);
                  mVertical.clearPlot(pLayer);
                  if (!mHorizontal.plot(pLayer, pixelCoord.mY, pixelCoord.mX))
                  {
                     mHorizontal.clearPlot(pLayer);
                     return false;
                  }
                  if (!mVertical.plot(pLayer, pixelCoord.mX, pixelCoord.mY))
                  {
                     mVertical.clearPlot(pLayer);
                     return false;
                  }
               }
            }
         }
      }
   }

   return QObject::eventFilter(pObject, pEvent);
}

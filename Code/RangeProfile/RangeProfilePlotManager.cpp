/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "Axis.h"
#include "Classification.h"
#include "ColorType.h"
#include "ContextMenu.h"
#include "ContextMenuActions.h"
#include "DataDescriptor.h"
#include "DataVariant.h"
#include "DesktopServices.h"
#include "DynamicObject.h"
#include "MenuBar.h"
#include "MouseMode.h"
#include "ObjectResource.h"
#include "PlotSet.h"
#include "PlotView.h"
#include "PlotWidget.h"
#include "PlugInRegistration.h"
#include "Point.h"
#include "PointSet.h"
#include "RangeProfilePlotManager.h"
#include "Signature.h"
#include "SessionItemDeserializer.h"
#include "SessionItemSerializer.h"
#include "SessionManager.h"
#include "Slot.h"
#include "SpectralVersion.h"
#include "Units.h"
#include "XercesIncludes.h"
#include "xmlreader.h"
#include "xmlwriter.h"

#include <QtGui/QAction>
#include <QtGui/QKeyEvent>
#include <QtGui/QMenu>
#include <QtGui/QMessageBox>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QWidget>

XERCES_CPP_NAMESPACE_USE

#define WINDOW_NAME "Range Profiles"

namespace
{
   static const char*  double_arrow_vert[] = {
      "7 19 2 1",
      "#	c #FFFFFF",
      ".	c #000000",
      "###.###",
      "##...##",
      "#.....#",
      ".......",
      "#######",
      "###.###",
      "###.###",
      "###.###",
      "###.###",
      "###.###",
      "###.###",
      "###.###",
      "###.###",
      "###.###",
      "#######",
      ".......",
      "#.....#",
      "##...##",
      "###.###"};
}

REGISTER_PLUGIN_BASIC(SpectralRangeProfile, RangeProfilePlotManager);

RangeProfilePlotManager::RangeProfilePlotManager() : mpPlot(NULL), mpView(NULL), mpMode(NULL)
{
   setName("Range Profile Plot Manager");
   setVersion(SPECTRAL_VERSION_NUMBER);
   setCopyright(SPECTRAL_COPYRIGHT);
   setDescription("Manage range profile plots.");
   setDescriptorId("{2dc48270-fd6f-42b7-acf5-604125f64ffb}");
   setSubtype("Plot");
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

RangeProfilePlotManager::~RangeProfilePlotManager()
{
   Window* pWindow = getDockWindow();
   if (pWindow != NULL)
   {
      pWindow->detach(SIGNAL_NAME(Window, SessionItemDropped), Slot(this, &RangeProfilePlotManager::dropSessionItem));
   }
   for (std::map<Signature*, std::string>::iterator sig = mSigPointSets.begin(); sig != mSigPointSets.end(); ++sig)
   {
      sig->first->detach(SIGNAL_NAME(Subject, Deleted), Slot(this, &RangeProfilePlotManager::signatureDeleted));
      sig->first->getDataDescriptor()->detach(SIGNAL_NAME(DataDescriptor, Renamed),
         Slot(this, &RangeProfilePlotManager::signatureRenamed));
   }
   QAction* pWindowAction = getAction();
   if (pWindowAction != NULL)
   {
      MenuBar* pMenuBar = Service<DesktopServices>()->getMainMenuBar();
      if (pMenuBar != NULL)
      {
         pMenuBar->removeMenuItem(pWindowAction);
      }
      if (Service<DesktopServices>()->getMainWidget() != NULL)
      {
         delete pWindowAction;
      }
   }
}

bool RangeProfilePlotManager::accept(SessionItem* pItem) const
{
   return dynamic_cast<Signature*>(pItem) != NULL;
}

bool RangeProfilePlotManager::plotProfile(Signature* pSignature)
{
   VERIFY(mpView && pSignature);
   std::string plotName = pSignature->getDisplayName();
   if (plotName.empty())
   {
      plotName = pSignature->getName();
   }
   if (plotName == "Difference")
   {
      QMessageBox::warning(Service<DesktopServices>()->getMainWidget(),
         "Invalid signature", "Signatures can not be named 'Difference' as this is a reserved "
                              "name for this plot. Please rename your signature and try again.");
      return false;
   }
   const Units* pXUnits = NULL;
   const Units* pYUnits = NULL;
   std::vector<double> xData;
   std::vector<double> yData;
   std::set<std::string> dataNames = pSignature->getDataNames();
   for (std::set<std::string>::const_iterator dataName = dataNames.begin();
      dataName != dataNames.end() && (pXUnits == NULL || pYUnits == NULL);
      ++dataName)
   {
      const Units* pUnits = pSignature->getUnits(*dataName);
      if (pUnits == NULL)
      {
         continue;
      }
      if (pUnits->getUnitType() == DISTANCE)
      {
         if (pXUnits == NULL)
         {
            pXUnits = pUnits;
            xData = dv_cast<std::vector<double> >(pSignature->getData(*dataName), std::vector<double>());
         }
      }
      else if (pYUnits == NULL)
      {
         pYUnits = pUnits;
         yData = dv_cast<std::vector<double> >(pSignature->getData(*dataName), std::vector<double>());
      }
   }
   if (xData.empty() || xData.size() != yData.size())
   {
      QMessageBox::warning(Service<DesktopServices>()->getMainWidget(),
         "Invalid signature", QString("Signatures must have a distance axis. '%1' does not and will not be plotted.")
                                 .arg(QString::fromStdString(pSignature->getName())));
      return false;
   }
   std::map<Signature*, std::string>::iterator oldPointSet = mSigPointSets.find(pSignature);
   PointSet* pSet = getPointSet(pSignature);
   if (pSet != NULL)
   {
      pSet->clear(true);
   }
   std::list<PlotObject*> curObjects;
   mpView->getObjects(POINT_SET, curObjects);
   if (pSet == NULL)
   {
      std::vector<ColorType> excluded;
      excluded.push_back(ColorType(255, 255, 255)); // background
      excluded.push_back(ColorType(200, 0, 0)); // color for the difference plot
      for (std::list<PlotObject*>::const_iterator cur = curObjects.begin(); cur != curObjects.end(); ++cur)
      {
         excluded.push_back(static_cast<PointSet*>(*cur)->getLineColor());
      }
      pSet = static_cast<PointSet*>(mpView->addObject(POINT_SET, true));
      mSigPointSets[pSignature] = plotName;
      pSignature->attach(SIGNAL_NAME(Subject, Deleted), Slot(this, &RangeProfilePlotManager::signatureDeleted));
      pSignature->getDataDescriptor()->attach(SIGNAL_NAME(DataDescriptor, Renamed),
         Slot(this, &RangeProfilePlotManager::signatureRenamed));
      std::vector<ColorType> colors;
      ColorType::getUniqueColors(1, colors, excluded);
      if (!colors.empty())
      {
         pSet->setLineColor(colors.front());
      }
   }
   pSet->setObjectName(plotName);
   for (size_t idx = 0; idx < xData.size(); ++idx)
   {
      pSet->addPoint(xData[idx], yData[idx]);
   }

   VERIFY(mpPlot);
   Axis* pBottom = mpPlot->getAxis(AXIS_BOTTOM);
   Axis* pLeft = mpPlot->getAxis(AXIS_LEFT);
   VERIFYRV(pBottom && pLeft, NULL);
   if (pBottom->getTitle().empty())
   {
      pBottom->setTitle(pXUnits->getUnitName());
   }
   if (pLeft->getTitle().empty())
   {
      pLeft->setTitle(pYUnits->getUnitName());
   }
   else if (pLeft->getTitle() != pYUnits->getUnitName())
   {
      Axis* pRight = mpPlot->getAxis(AXIS_RIGHT);
      VERIFYRV(pRight, NULL);
      if (pRight->getTitle().empty())
      {
         pRight->setTitle(pYUnits->getUnitName());
      }
   }

   std::string classificationText = dv_cast<std::string>(pSignature->getMetadata()->getAttribute("Raw Classification"),
      mpPlot->getClassificationText());
   if (classificationText.empty() == false)
   {
      FactoryResource<Classification> pClassification;
      if (pClassification->setClassification(classificationText) == true)
      {
         mpPlot->setClassification(pClassification.get());
      }
      else
      {
         QMessageBox::warning(Service<DesktopServices>()->getMainWidget(), QString::fromStdString(getName()),
            "The plot could not be updated with the signature classification.  Please ensure that the plot "
            "has the proper classification.");
      }
   }

   getDockWindow()->show();
   mpView->zoomExtents();
   mpView->refresh();

   return true;
}

QWidget* RangeProfilePlotManager::createWidget()
{
   DockWindow* pWindow = getDockWindow();
   VERIFYRV(pWindow, NULL);
   pWindow->attach(SIGNAL_NAME(Window, SessionItemDropped), Slot(this, &RangeProfilePlotManager::dropSessionItem));
   pWindow->enableSessionItemDrops(this);
   if (!Service<SessionManager>()->isSessionLoading())
   {
      mpPlot = Service<DesktopServices>()->createPlotWidget(getName(), CARTESIAN_PLOT);
   }
   if (mpPlot == NULL)
   {
      return NULL;
   }
   mpView = mpPlot->getPlot();
#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : If a SHALLOW_SELECTION selection mode is added " \
                                              "to plot view (OPTICKS-528), mpView should use it (tclarke)")
   mpMode = (mpView == NULL) ? NULL : mpView->getMouseMode("SelectionMode");
   if (mpMode == NULL)
   {
      return NULL;
   }
   mpView->getWidget()->installEventFilter(this);
   mpPlot->attach(SIGNAL_NAME(PlotWidget, AboutToShowContextMenu),
      Slot(this, &RangeProfilePlotManager::updateContextMenu));
   mpView->enableMouseMode(mpMode, true);
   mpView->setMouseMode(mpMode);

   return mpPlot->getWidget();
}

QAction* RangeProfilePlotManager::createAction()
{
   QAction* pWindowAction = NULL;
   // Add a menu command to invoke the window
   MenuBar* pMenuBar = Service<DesktopServices>()->getMainMenuBar();
   if (pMenuBar != NULL)
   {
      QAction* pBeforeAction = NULL;
      QAction* pToolsAction = pMenuBar->getMenuItem("&Tools");
      if (pToolsAction != NULL)
      {
         QMenu* pMenu = pToolsAction->menu();
         if (pMenu != NULL)
         {
            QList<QAction*> actions = pMenu->actions();
            for (int i = 0; i < actions.count(); ++i)
            {
               QAction* pAction = actions[i];
               if (pAction != NULL)
               {
                  if ((pAction->text() == "S&cripting Window") && (pAction != actions.back()))
                  {
                     pBeforeAction = actions[i + 1];
                     break;
                  }
               }
            }
         }
      }

      pWindowAction = pMenuBar->addCommand("&Tools/&" + getName(), getName(), pBeforeAction);
      if (pWindowAction != NULL)
      {
         pWindowAction->setAutoRepeat(false);
         pWindowAction->setCheckable(true);
         pWindowAction->setToolTip(QString::fromStdString(getName()));
         pWindowAction->setStatusTip("Toggles the display of the " + QString::fromStdString(getName()));
      }
   }
   return pWindowAction;
}

void RangeProfilePlotManager::dropSessionItem(Subject& subject, const std::string& signal, const boost::any& value)
{
   Signature* pSignature = dynamic_cast<Signature*>(boost::any_cast<SessionItem*>(value));
   if (pSignature != NULL)
   {
      plotProfile(pSignature);
   }
}

void RangeProfilePlotManager::updateContextMenu(Subject& subject, const std::string& signal, const boost::any& value)
{
   ContextMenu* pMenu = boost::any_cast<ContextMenu*>(value);
   if (pMenu == NULL)
   {
      return;
   }
   QAction* pDiffAction = new QAction("Calculate Difference", pMenu->getActionParent());
   VERIFYNRV(mpView);
   int numSelectedObjects = mpView->getNumSelectedObjects(true);

   if (numSelectedObjects != 2)
   {
      pDiffAction->setEnabled(false);
   }
   else
   {
      std::list<PlotObject*> objects;
      mpView->getSelectedObjects(objects, true);
      for (std::list<PlotObject*>::const_iterator obj = objects.begin(); obj != objects.end(); ++obj)
      {
         std::string name;
         (*obj)->getObjectName(name);
         if (name == "Difference")
         {
            pDiffAction->setEnabled(false);
            break;
         }
      }
   }

   VERIFYNR(connect(pDiffAction, SIGNAL(triggered()), this, SLOT(calculateDifferences())));
   pMenu->addActionBefore(pDiffAction, "SPECTRAL_RANGEPROFILEPLOT_DIFFERENCE_ACTION", APP_PLOTWIDGET_PRINT_ACTION);

   QAction* pDelAction = new QAction(
      (numSelectedObjects > 1) ? "Delete Plots" : "Delete Plot", pMenu->getActionParent());
   if (mpView != NULL && numSelectedObjects == 0)
   {
      pDelAction->setEnabled(false);
   }
   VERIFYNR(connect(pDelAction, SIGNAL(triggered()), this, SLOT(deleteSelectedPlots())));
   pMenu->addActionAfter(pDelAction, "SPECTRAL_RANGEPROFILEPLOT_DELETE_ACTION",
      "SPECTRAL_RANGEPROFILEPLOT_DIFFERENCE_ACTION");
}

void RangeProfilePlotManager::signatureDeleted(Subject& subject, const std::string& signal, const boost::any& value)
{
   Signature* pSig = dynamic_cast<Signature*>(&subject);
   PointSet* pSet = getPointSet(pSig);
   if (pSet != NULL)
   {
      VERIFYNRV(mpView != NULL);
      mpView->deleteObject(pSet);
      mSigPointSets.erase(pSig);
      mpView->refresh();
   }
}

void RangeProfilePlotManager::signatureRenamed(Subject& subject, const std::string& signal, const boost::any& value)
{
   std::string newName = boost::any_cast<std::string>(value);
   DataDescriptor& desc = dynamic_cast<DataDescriptor&>(subject);
   for (std::map<Signature*, std::string>::iterator sig = mSigPointSets.begin(); sig != mSigPointSets.end(); ++sig)
   {
      if (sig->first->getDataDescriptor() == &desc)
      {
         PointSet* pSet = getPointSet(sig->first);
         if (pSet != NULL)
         {
            pSet->setObjectName(newName);
            mSigPointSets[sig->first] = newName;
            return;
         }
      }
   }
}

bool RangeProfilePlotManager::eventFilter(QObject* pObj, QEvent* pEvent)
{
   PlotView* pView = dynamic_cast<PlotView*>(pObj);
   if (pView == NULL || pView->getCurrentMouseMode() != mpMode)
   {
      return QObject::eventFilter(pObj, pEvent);
   }
   switch(pEvent->type())
   {
   case QEvent::MouseButtonPress:
      return mousePressEvent(pView, static_cast<QMouseEvent*>(pEvent));
   case QEvent::MouseButtonRelease:
      return mouseReleaseEvent(pView, static_cast<QMouseEvent*>(pEvent));
   case QEvent::MouseMove:
      return mouseMoveEvent(pView, static_cast<QMouseEvent*>(pEvent));
   case QEvent::Wheel:
      return wheelEvent(pView, static_cast<QWheelEvent*>(pEvent));
   case QEvent::KeyPress:
      return keyPressEvent(pView, static_cast<QKeyEvent*>(pEvent));
   default:
      ; // do nothing
   }
   return QObject::eventFilter(pObj, pEvent);
}

bool RangeProfilePlotManager::mousePressEvent(PlotView* pView, QMouseEvent* pEvent)
{
   if (pEvent->button() == Qt::LeftButton && pView->getNumSelectedObjects(true) > 0)
   {
      mMouseStart = pEvent->pos();
   }
   return false;
}

bool RangeProfilePlotManager::mouseReleaseEvent(PlotView* pView, QMouseEvent* pEvent)
{
   if (pEvent->button() == Qt::LeftButton)
   {
      mMouseStart = QPoint();
   }
   return false;
}

bool RangeProfilePlotManager::mouseMoveEvent(PlotView* pView, QMouseEvent* pEvent)
{
   bool rval = false;
   if (!mMouseStart.isNull())
   {
      double dataX, startY, curY;
      pView->translateScreenToData(0.0, mMouseStart.y(), dataX, startY);
      pView->translateScreenToData(0.0, pEvent->y(), dataX, curY);
      double shift = curY - startY;

      std::list<PlotObject*> selected;
      pView->getSelectedObjects(selected, true);
      for (std::list<PlotObject*>::iterator obj = selected.begin(); obj != selected.end(); ++obj)
      {
         PointSet* pSet = dynamic_cast<PointSet*>(*obj);
         if (pSet != NULL)
         {
            rval = true;
            std::vector<Point*> points = pSet->getPoints();
            for (std::vector<Point*>::iterator point = points.begin(); point != points.end(); ++point)
            {
               LocationType loc = (*point)->getLocation();
               loc.mY -= shift;
               (*point)->setLocation(loc);
            }
         }
      }
      mMouseStart = pEvent->pos();
      pView->refresh();
   }
   return rval;
}

bool RangeProfilePlotManager::wheelEvent(PlotView* pView, QWheelEvent* pEvent)
{
   double scaleAdjust = pEvent->delta() / 1440.0; // yup, a magic number, started with one full rotation to double
                                                  // then experimented until I got something that looked good
   scaleAdjust = (scaleAdjust < 0.0) ? (-1.0 - scaleAdjust) : (1.0 - scaleAdjust);
   std::list<PlotObject*> selected;
   pView->getSelectedObjects(selected, true);
   for (std::list<PlotObject*>::iterator obj = selected.begin(); obj != selected.end(); ++obj)
   {
      PointSet* pSet = dynamic_cast<PointSet*>(*obj);
      if (pSet != NULL)
      {
         double minX, minY, maxX, maxY;
         pSet->getExtents(minX, minY, maxX, maxY);
         double shift = minY;
         std::vector<Point*> points = pSet->getPoints();
         for (std::vector<Point*>::iterator point = points.begin(); point != points.end(); ++point)
         {
            LocationType loc = (*point)->getLocation();
            loc.mY -= shift;
            if (scaleAdjust < 0.0)
            {
               loc.mY *= fabs(scaleAdjust);
            }
            else
            {
               loc.mY /= scaleAdjust;
            }
            loc.mY += shift;
            (*point)->setLocation(loc);
         }
      }
   }
   pView->refresh();
   return false;
}

bool RangeProfilePlotManager::keyPressEvent(PlotView* pView, QKeyEvent* pEvent)
{
   if (pEvent->matches(QKeySequence::Delete) && pView == mpView)
   {
      return deleteSelectedPlots();
   }
   return false;
}

void RangeProfilePlotManager::calculateDifferences()
{
   // ensure we have only two objects selected
   VERIFYNRV(mpView);
   std::list<PlotObject*> selected;
   mpView->getSelectedObjects(selected, true);
   std::list<PlotObject*>::iterator selIt = selected.begin();
   PointSet* pFirst = (selIt == selected.end()) ? NULL : dynamic_cast<PointSet*>(*(selIt++));
   PointSet* pSecond = (selIt == selected.end()) ? NULL : dynamic_cast<PointSet*>(*(selIt++));
   if (pFirst == NULL || pSecond == NULL)
   {
      return;
   }

   // locate the Difference point set
   std::list<PlotObject*> allObjects;
   mpView->getObjects(POINT_SET, allObjects);
   PointSet* pDiffSet = NULL;
   for (std::list<PlotObject*>::iterator obj = allObjects.begin(); obj != allObjects.end(); ++obj)
   {
      PointSet* pSet = static_cast<PointSet*>(*obj);
      std::string name;
      pSet->getObjectName(name);
      if (name == "Difference")
      {
         pDiffSet = pSet;
         pDiffSet->clear(true);
         break;
      }
   }
   if (pDiffSet == NULL)
   {
      pDiffSet = static_cast<PointSet*>(mpView->addObject(POINT_SET, true));
      pDiffSet->setObjectName("Difference");
   }

   // calculate the differences and errors
   std::vector<Point*> aPoints = pFirst->getPoints();
   std::vector<Point*> bPoints = pSecond->getPoints();
   if (aPoints.size() < 2 || bPoints.size() < 2)
   {
      return;
   }
   double mae = 0.0;
   double mse1 = 0.0;
   double mse2 = 0.0;

   for (size_t aIdx = 0; aIdx < aPoints.size(); ++aIdx)
   {
      Point* pA = aPoints[aIdx];
      VERIFYNRV(pA);
      LocationType aVal = pA->getLocation();
      LocationType newVal;

      // locate the associated spot in b
      for (size_t bIdx = 0; bIdx < bPoints.size(); ++bIdx)
      {
         Point* pB = bPoints[bIdx];
         VERIFYNRV(pB);
         LocationType bVal = pB->getLocation();
         double diff = aVal.mX - bVal.mX;
         if (fabs(diff) < 0.0000001) // a == b   use the exact value
         {
            newVal.mX = aVal.mX;
            newVal.mY = bVal.mY - aVal.mY;
            break;
         }
         else if (diff < 0.0) // a < b   found the upper point, interpolate
         {
            newVal.mX = aVal.mX;
            LocationType secondBVal;
            if (bIdx == 0) // we are at the start so continue the segment from the right
            {
               Point* pSecondB = bPoints[1];
               VERIFYNRV(pSecondB);
               secondBVal = pSecondB->getLocation();
            }
            else // grab the previous point for interpolation
            {
               Point* pSecondB = bPoints[bIdx-1];
               VERIFYNRV(pSecondB);
               secondBVal = pSecondB->getLocation();
            }

            // calculate slope-intercept
            double m = (bVal.mY - secondBVal.mY) / (bVal.mX - secondBVal.mX);
            double b = bVal.mY - m * bVal.mX;

            // find the y corresponding to the interpolated x
            newVal.mY = m * newVal.mX + b;
            newVal.mY -= aVal.mY;
            break;
         }
      }
      mae += fabs(newVal.mY);
      mse1 += newVal.mY * newVal.mY;
      mse2 += (newVal.mY * newVal.mY) / (aVal.mY * aVal.mY);
      pDiffSet->addPoint(newVal.mX, newVal.mY);
   }
   pDiffSet->setLineColor(ColorType(200, 0, 0));
   mpView->refresh();
   mae /= aPoints.size();
   mse1 /= aPoints.size();
   mse2 /= aPoints.size();
   QMessageBox::information(mpView->getWidget(), "Comparison metrics",
      QString("Mean squared error (method 1): %1\n"
              "Mean squared error (method 2): %2\n"
              "Mean absolute error:           %3").arg(mse1).arg(mse2).arg(mae), QMessageBox::Close);
}

bool RangeProfilePlotManager::deleteSelectedPlots()
{
   VERIFY(mpView);
   std::list<PlotObject*> selected;
   mpView->getSelectedObjects(selected, false);
   std::list<std::string> selectedNames;
   for (std::list<PlotObject*>::iterator selectedIt = selected.begin(); selectedIt != selected.end(); ++selectedIt)
   {
      std::string selectedName;
      (*selectedIt)->getObjectName(selectedName);
      selectedNames.push_back(selectedName);
   }
   for (std::map<Signature*, std::string>::iterator spsIt = mSigPointSets.begin(); spsIt != mSigPointSets.end(); )
   {
      if (std::find(selectedNames.begin(), selectedNames.end(), spsIt->second) != selectedNames.end())
      {
         std::map<Signature*, std::string>::iterator tmpIt = spsIt;
         ++spsIt;
         mSigPointSets.erase(tmpIt);
      }
      else
      {
         ++spsIt;
      }
   }
   mpView->deleteSelectedObjects(true);
   mpView->refresh();
   return true;
}

bool RangeProfilePlotManager::serialize(SessionItemSerializer& serializer) const
{
   XMLWriter writer("RangeProfilePlotManager");
   writer.addAttr("viewId", mpView->getId());

#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : We should be able to save the plot's session id and restore " \
"using that but the PlotWidget is not really part of the session. The following code is a work around and should be changed " \
"when OPTICKS-542 is implemented. (tclarke)")

   writer.pushAddPoint(writer.addElement("plot"));
   Serializable* pPlotSer = dynamic_cast<Serializable*>(mpPlot); // The imp side is serializable
   VERIFY(pPlotSer);
   if (!pPlotSer->toXml(&writer))
   {
      return false;
   }
   writer.popAddPoint();
   writer.pushAddPoint(writer.addElement("plotView"));
   Serializable* pPlotViewSer = dynamic_cast<Serializable*>(mpView); // The imp side is serializable
   VERIFY(pPlotViewSer);
   if (!pPlotViewSer->toXml(&writer))
   {
      return false;
   }
   writer.popAddPoint();

   for (std::map<Signature*, std::string>::const_iterator sig = mSigPointSets.begin();
      sig != mSigPointSets.end(); ++sig)
   {
      writer.pushAddPoint(writer.addElement("signature"));
      writer.addAttr("sigId", sig->first->getId());
      writer.addAttr("pointSetName", sig->second);
      writer.popAddPoint();
   }

   if (!serializer.serialize(writer))
   {
      return false;
   }
   serializer.endBlock();
   return DockWindowShell::serialize(serializer);
}

bool RangeProfilePlotManager::deserialize(SessionItemDeserializer& deserializer)
{
   XmlReader reader(NULL, false);
   DOMElement* pRootElement = deserializer.deserialize(reader, "RangeProfilePlotManager");
   if (pRootElement)
   {
      std::string viewId = A(pRootElement->getAttribute(X("viewId")));
      mpView = dynamic_cast<PlotView*>(Service<SessionManager>()->getSessionItem(viewId));

      mpPlot = Service<DesktopServices>()->createPlotWidget(getName(), CARTESIAN_PLOT);
      VERIFY(mpPlot);
      Serializable* pPlotSer = dynamic_cast<Serializable*>(mpPlot); // The imp side is serializable
      VERIFY(pPlotSer);
      if (!pPlotSer->fromXml(findChildNode(pRootElement, "plot"), reader.VERSION))
      {
         return false;
      }

      mpView = mpPlot->getPlot();
      VERIFY(mpView);
      Serializable* pPlotViewSer = dynamic_cast<Serializable*>(mpView); // The imp side is serializable
      VERIFY(pPlotViewSer);
      if (!pPlotViewSer->fromXml(findChildNode(pRootElement, "plotView"), reader.VERSION))
      {
         return false;
      }

      std::list<PlotObject*> objects;
      mpView->getObjects(POINT_SET, objects);
      FOR_EACH_DOMNODE(pRootElement, pChild)
      {
         if (XMLString::equals(pChild->getNodeName(), X("signature")))
         {
            DOMElement* pChldElmnt = static_cast<DOMElement*>(pChild);
            std::string sigId = A(pChldElmnt->getAttribute(X("sigId")));
            std::string pointSetName = A(pChldElmnt->getAttribute(X("pointSetName")));
            Signature* pSignature = static_cast<Signature*>(Service<SessionManager>()->getSessionItem(sigId));
            if (pSignature == NULL)
            {
               return false;
            }
            mSigPointSets[pSignature] = pointSetName;
            pSignature->attach(SIGNAL_NAME(Subject, Deleted), Slot(this, &RangeProfilePlotManager::signatureDeleted));
            pSignature->getDataDescriptor()->attach(SIGNAL_NAME(DataDescriptor, Renamed),
               Slot(this, &RangeProfilePlotManager::signatureRenamed));
         }
      }
      deserializer.nextBlock();
      return DockWindowShell::deserialize(deserializer);
   }
   return false;
}

PointSet* RangeProfilePlotManager::getPointSet(Signature* pSig)
{
   std::map<Signature*, std::string>::const_iterator nmIt = mSigPointSets.find(pSig);
   if (nmIt == mSigPointSets.end())
   {
      return NULL;
   }
   std::string pointSetName = nmIt->second;
   std::list<PlotObject*> objects;
   mpView->getObjects(objects);
   for (std::list<PlotObject*>::iterator object = objects.begin(); object != objects.end(); ++object)
   {
      std::string candidateName;
      (*object)->getObjectName(candidateName);
      if (candidateName == pointSetName)
      {
         return dynamic_cast<PointSet*>(*object);
      }
   }
   return NULL;
}

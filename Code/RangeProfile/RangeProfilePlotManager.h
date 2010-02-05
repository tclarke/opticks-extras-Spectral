/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RANGEPROFILEPLOTMANAGER_H
#define RANGEPROFILEPLOTMANAGER_H

#include "DockWindowShell.h"
#include "Window.h"
#include <boost/any.hpp>
#include <QtCore/QObject>
#include <QtCore/QPoint>

class MouseMode;
class PlotView;
class PlotWidget;
class PointSet;
class QAction;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class Signature;

class RangeProfilePlotManager : public DockWindowShell, public Window::SessionItemDropFilter
{
   Q_OBJECT

public:
   RangeProfilePlotManager();
   virtual ~RangeProfilePlotManager();

   virtual bool accept(SessionItem* pItem) const;
   bool plotProfile(Signature* pSignature);

   bool serialize(SessionItemSerializer& serializer) const;
   bool deserialize(SessionItemDeserializer& deserializer);

protected:
   virtual QWidget* createWidget();
   virtual QAction* createAction();
   void dropSessionItem(Subject& subject, const std::string& signal, const boost::any& value);
   void updateContextMenu(Subject& subject, const std::string& signal, const boost::any& value);
   void signatureDeleted(Subject& subject, const std::string& signal, const boost::any& value);
   void signatureRenamed(Subject& subject, const std::string& signal, const boost::any& value);
   bool eventFilter(QObject* pObj, QEvent* pEvent);
   bool mousePressEvent(PlotView* pView, QMouseEvent* pEvent);
   bool mouseReleaseEvent(PlotView* pView, QMouseEvent* pEvent);
   bool mouseMoveEvent(PlotView* pView, QMouseEvent* pEvent);
   bool wheelEvent(PlotView* pView, QWheelEvent* pEvent);
   bool keyPressEvent(PlotView* pView, QKeyEvent* pEvent);

protected slots:
   void calculateDifferences();
   bool deleteSelectedPlots();

private:
   PointSet* getPointSet(Signature* pSig);

   PlotWidget* mpPlot;
   PlotView* mpView;
   MouseMode* mpMode;
   QPoint mMouseStart;
   std::map<Signature*, std::string> mSigPointSets;
};

#endif

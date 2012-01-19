/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef PLOTTINGMANAGER_H__
#define PLOTTINGMANAGER_H__

#include "AttachmentPtr.h"
#include "DesktopServices.h"
#include "ExecutableShell.h"
#include "ProfilePlotUtilities.h"
#include <boost/any.hpp>
#include <QtCore/QObject>

class DockWindow;
class QAction;
class QSpinBox;

class PlottingManager : public QObject, public ExecutableShell
{
   Q_OBJECT

public:
   PlottingManager();
   virtual ~PlottingManager();

   virtual bool setBatch();
   virtual bool getInputSpecification(PlugInArgList*& pInArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pOutArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

   ProfilePlotUtilities& getHorizontal();
   ProfilePlotUtilities& getVertical();

   virtual bool serialize(SessionItemSerializer& serializer) const;
   virtual bool deserialize(SessionItemDeserializer& deserializer);

private slots:
   void dockWindowActionToggled(bool shown);

private:
   void windowAdded(Subject& subject, const std::string& signal, const boost::any& value);
   void windowRemoved(Subject& subject, const std::string& signal, const boost::any& value);
   void windowActivated(Subject& subject, const std::string& signal, const boost::any& value);
   void enableAction();
   void dockWindowShown(Subject& subject, const std::string& signal, const boost::any& value);
   void dockWindowHidden(Subject& subject, const std::string& signal, const boost::any& value);
   virtual bool eventFilter(QObject* pObject, QEvent* pEvent);

   ProfilePlotUtilities mHorizontal;
   ProfilePlotUtilities mVertical;

   AttachmentPtr<DesktopServices> mDesktopAttachment;

   MouseMode* mpProfileMouseMode;
   QAction* mpProfileAction;
   std::map<DockWindow*, QAction*> mToggleActions;
};

#endif

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef ELMINTERACTIVE_H
#define ELMINTERACTIVE_H

#include "MessageLogResource.h"
#include "ElmCore.h"
#include "ViewerShell.h"

class ElmDlg;
class SpatialDataView;
class Step;

class ElmInteractive : public ViewerShell, public ElmCore
{
public:
   ElmInteractive();
   virtual ~ElmInteractive();

   virtual bool setBatch();
   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInputArgList, PlugInArgList* pOutputArgList);

   Step* getLogStep();

protected:
   bool extractInputArgs(PlugInArgList* pInputArgList);
   virtual QWidget* getWidget() const;

private:
   SpatialDataView* mpView;
   ElmDlg* mpDialog;
   StepResource mpStep;
};

#endif

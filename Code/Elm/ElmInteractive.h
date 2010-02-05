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

#include "AlgorithmShell.h"
#include "MessageLogResource.h"
#include "ElmCore.h"

class SpatialDataView;

class ElmInteractive : public AlgorithmShell, public ElmCore
{
public:
   ElmInteractive();
   virtual ~ElmInteractive();

   bool setBatch();
   bool getInputSpecification(PlugInArgList*& pArgList);
   bool getOutputSpecification(PlugInArgList*& pArgList);
   bool execute(PlugInArgList* pInputArgList, PlugInArgList* pOutputArgList);

protected:
   bool extractInputArgs(PlugInArgList* pInputArgList);

private:
   SpatialDataView* mpView;
   StepResource mpStep;
};

#endif

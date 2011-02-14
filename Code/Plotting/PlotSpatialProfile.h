/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef PLOTSPATIALPROFILE_H__
#define PLOTSPATIALPROFILE_H__

#include "WizardShell.h"

class PlotSpatialProfile : public WizardShell
{
public:
   PlotSpatialProfile();
   virtual ~PlotSpatialProfile();

   virtual bool getInputSpecification(PlugInArgList*& pInArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
};

#endif
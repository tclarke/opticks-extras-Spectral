/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef WAVELENGTHEDITOR_H
#define WAVELENGTHEDITOR_H

#include "ExecutableShell.h"

class WavelengthEditor : public ExecutableShell
{
public:
   WavelengthEditor();
   ~WavelengthEditor();
   SETTING(WavelengthEditorHelp, SpectralContextSensitiveHelp, std::string, "");

   bool getInputSpecification(PlugInArgList*& pArgList);
   bool getOutputSpecification(PlugInArgList*& pArgList);
   bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
};

#endif

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef LIBRARYBUILDER_H
#define LIBRARYBUILDER_H

#include "ConfigurationSettings.h"
#include "ExecutableShell.h"

class SpectralLibraryDlg;

class LibraryBuilder : public ExecutableShell
{
public:
   LibraryBuilder();
   virtual ~LibraryBuilder();

   SETTING(SpectralLibraryBuilderHelp, SpectralContextSensitiveHelp, std::string, "");

   virtual bool setBatch();
   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
   virtual bool abort();

private:
   SpectralLibraryDlg* mpLibraryDlg;
};

#endif

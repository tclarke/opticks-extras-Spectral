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
   ~LibraryBuilder();
   SETTING(SpectralLibraryHelp, SpectralContextSensitiveHelp, std::string, "");

   bool setBatch();
   bool getInputSpecification(PlugInArgList*& pArgList);
   bool getOutputSpecification(PlugInArgList*& pArgList);
   bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
   bool abort();

private:
   SpectralLibraryDlg* mpLibraryDlg;
};

#endif

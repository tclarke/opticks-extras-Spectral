/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef WAVELENGTHIMPORTER_H
#define WAVELENGTHIMPORTER_H

#include "ExecutableShell.h"
#include "Wavelengths.h"

#include <string>

class WavelengthImporter : public ExecutableShell
{
public:
   WavelengthImporter();
   ~WavelengthImporter();

   bool getInputSpecification(PlugInArgList*& pArgList);
   bool getOutputSpecification(PlugInArgList*& pArgList);
   bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

protected:
   virtual bool loadWavelengths(Wavelengths& wavelengths, std::string& errorMessage) const = 0;
   const std::string& getFilename() const;

private:
   std::string mFilename;
};

#endif

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef WAVELENGTHEXPORTER_H
#define WAVELENGTHEXPORTER_H

#include "ExecutableShell.h"
#include "Wavelengths.h"

#include <string>

class WavelengthExporter : public ExecutableShell
{
public:
   WavelengthExporter();
   ~WavelengthExporter();

   bool getInputSpecification(PlugInArgList*& pArgList);
   bool getOutputSpecification(PlugInArgList*& pArgList);
   bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

protected:
   virtual bool saveWavelengths(const Wavelengths& wavelengths) const = 0;
   const std::string& getFilename() const;

private:
   std::string mFilename;
};

#endif

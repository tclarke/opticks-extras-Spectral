/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef WAVELENGTHTEXTIMPORTER_H
#define WAVELENGTHTEXTIMPORTER_H

#include "WavelengthImporter.h"
#include "Wavelengths.h"

class WavelengthTextImporter : public WavelengthImporter
{
public:
   WavelengthTextImporter();
   ~WavelengthTextImporter();

   bool getInputSpecification(PlugInArgList*& pArgList);
   bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

protected:
   bool loadWavelengths(Wavelengths& wavelengths, std::string& errorMessage) const;

private:
   Wavelengths::WavelengthUnitsType mUnits;
};

#endif

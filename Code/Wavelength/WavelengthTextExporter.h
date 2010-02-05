/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef WAVELENGTHTEXTEXPORTER_H
#define WAVELENGTHTEXTEXPORTER_H

#include "WavelengthExporter.h"

class WavelengthTextExporter : public WavelengthExporter
{
public:
   WavelengthTextExporter();
   ~WavelengthTextExporter();

protected:
   bool saveWavelengths(const Wavelengths& wavelengths) const;
};

#endif

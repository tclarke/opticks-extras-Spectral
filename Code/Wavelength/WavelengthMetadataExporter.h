/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef WAVELENGTHMETADATAEXPORTER_H
#define WAVELENGTHMETADATAEXPORTER_H

#include "WavelengthExporter.h"

class WavelengthMetadataExporter : public WavelengthExporter
{
public:
   WavelengthMetadataExporter();
   ~WavelengthMetadataExporter();

protected:
   bool saveWavelengths(const Wavelengths& wavelengths) const;
};

#endif

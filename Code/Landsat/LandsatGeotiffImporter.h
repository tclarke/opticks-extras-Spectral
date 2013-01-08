/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef LANDSATGEOTIFFIMPORTER_H
#define LANDSATGEOTIFFIMPORTER_H

#include "LandsatUtilities.h"
#include "RasterElementImporterShell.h"

#include <string>
#include <vector>

class DataDescriptor;
class ImportDescriptor;
class RasterElement;

class LandsatGeotiffImporter : public RasterElementImporterShell
{
public:
   LandsatGeotiffImporter();
   virtual ~LandsatGeotiffImporter();

   bool validate(const DataDescriptor* pDescriptor, const std::vector<const DataDescriptor*>& importedDescriptors,
      std::string& errorMessage) const;
   unsigned char getFileAffinity(const std::string& filename);
   std::vector<ImportDescriptor*> getImportDescriptors(const std::string& filename);
   bool createRasterPager(RasterElement* pRaster) const;
   int getValidationTest(const DataDescriptor* pDescriptor) const;

private:
   std::vector<ImportDescriptor*> createImportDescriptors(const std::string& filename,
      const DynamicObject* pMetadata,
      Landsat::LandsatImageType type);
   std::vector<std::string> mWarnings;
   std::vector<std::string> mErrors;
};

#endif

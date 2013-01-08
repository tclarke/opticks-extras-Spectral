/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef DGIMPORTER_H
#define DGIMPORTER_H

#include "RasterElementImporterShell.h"

#include <string>
#include <vector>

class RasterDataDescriptor;

class DgImporter : public RasterElementImporterShell
{
public:
   DgImporter();
   virtual ~DgImporter();

   virtual bool validate(const DataDescriptor* pDescriptor,
      const std::vector<const DataDescriptor*>& importedDescriptors, std::string& errorMessage) const;
   virtual unsigned char getFileAffinity(const std::string& filename);
   virtual std::vector<ImportDescriptor*> getImportDescriptors(const std::string& filename);

   virtual bool createRasterPager(RasterElement* pRaster) const;
   virtual int getValidationTest(const DataDescriptor* pDescriptor) const;

private:
   std::vector<std::string> mWarnings;
   std::vector<std::string> mErrors;
};

#endif

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RANGEPROFILEIMPORTER_H
#define RANGEPROFILEIMPORTER_H

#include "ImporterShell.h"

class RangeProfileImporter : public ImporterShell
{
public:
   RangeProfileImporter();
   virtual ~RangeProfileImporter();

   virtual std::vector<ImportDescriptor*> getImportDescriptors(const std::string& filename);
   virtual unsigned char getFileAffinity(const std::string& filename);
   virtual bool getInputSpecification(PlugInArgList*& pInArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pOutArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
};


#endif
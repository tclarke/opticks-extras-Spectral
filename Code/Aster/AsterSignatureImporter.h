/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef ASTERSIGNATUREIMPORTER_H
#define ASTERSIGNATUREIMPORTER_H

#include "ImporterShell.h"
#include "Testable.h"
#include <string>
#include <vector>

class AsterSignatureImporter : public ImporterShell, public Testable
{
public:
   AsterSignatureImporter();
   ~AsterSignatureImporter();

   virtual unsigned char getFileAffinity(const std::string& filename);
   virtual std::vector<ImportDescriptor*> getImportDescriptors(const std::string& filename);
   virtual bool getInputSpecification(PlugInArgList*& pInArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pOutArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

   virtual bool runOperationalTests(Progress* pProgress, std::ostream& failure)
   {
      return true;
   }
   virtual bool runAllTests(Progress* pProgress, std::ostream& failure);
};

#endif

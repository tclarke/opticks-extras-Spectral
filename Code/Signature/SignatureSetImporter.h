/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SIGNATURESETIMPORTER_H
#define SIGNATURESETIMPORTER_H

#include "ImporterShell.h"
#include "xmlreader.h"
#include <map>
#include <string>
#include <vector>

class SignatureSetImporter : public ImporterShell
{
public:
   SignatureSetImporter();
   ~SignatureSetImporter();

   unsigned char getFileAffinity(const std::string& filename);
   std::vector<ImportDescriptor*> getImportDescriptors(const std::string& filename);
   bool getInputSpecification(PlugInArgList*& pInArgList);
   bool getOutputSpecification(PlugInArgList*& pOutArgList);
   bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

private:
   std::vector<ImportDescriptor*> createImportDescriptors(XERCES_CPP_NAMESPACE_QUALIFIER DOMTreeWalker* pTree, std::vector<std::string>& datasetPath);
   void loadDoc(const std::string& filename);

   unsigned int mDatasetNumber;
   std::map<std::string, XmlReader*> mXml;
   std::map<std::string, XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument*> mDoc;
   std::string mFilename;
};

#endif
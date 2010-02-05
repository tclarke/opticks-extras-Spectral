/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SIGNATUREEXPORTER_H
#define SIGNATUREEXPORTER_H

#include "ExporterShell.h"

class LargeFileResource;

class SignatureExporter : public ExporterShell
{
public:
   SignatureExporter();
   ~SignatureExporter();

   bool getInputSpecification(PlugInArgList*& pInArgList);
   bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

private:
   template<typename T>
   void writeMetadataEntry(LargeFileResource& file, const std::string& key, const T& val);
};

#endif
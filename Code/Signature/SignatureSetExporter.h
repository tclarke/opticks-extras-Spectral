/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SIGNATURESETEXPORTER_H
#define SIGNATURESETEXPORTER_H

#include "ExporterShell.h"
#include "ProgressTracker.h"

class LabeledSection;
class Progress;
class QCheckBox;
class QComboBox;
class SignatureSet;
class XMLWriter;

class SignatureSetExporter : public ExporterShell
{
public:
   static std::string sDefaultSignatureExporter;
   static std::string SignatureExporterArg();
   static std::string FreezeSignatureSetArg();

   SignatureSetExporter();
   ~SignatureSetExporter();

   QWidget* getExportOptionsWidget(const PlugInArgList* pInArgList);
   bool getInputSpecification(PlugInArgList*& pInArgList);
   bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

private:
   bool writeSignatureSet(XMLWriter& xml, SignatureSet* pSignatureSet, std::string outputDirectory);

   LabeledSection* mpOptionsWidget;
   QComboBox* mpSignatureExporterSelector;
   QCheckBox* mpFreezeCheck;
   std::string mSignatureExporter;
   bool mFreeze;
   Progress* mpProgress;
   ProgressTracker mProgress;
};

#endif
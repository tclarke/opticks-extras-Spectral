/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "DynamicObject.h"
#include "FileDescriptor.h"
#include "FileResource.h"
#include "LabeledSection.h"
#include "PlugInArgList.h"
#include "PlugInDescriptor.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "RasterUtilities.h"
#include "SignatureSet.h"
#include "SignatureSetExporter.h"
#include "SpectralVersion.h"
#include "StringUtilities.h"
#include "TypeConverter.h"
#include "xmlwriter.h"

#include <QtCore/QDir>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QGridLayout>

using namespace std;

REGISTER_PLUGIN_BASIC(SpectralSignature, SignatureSetExporter);

string SignatureSetExporter::sDefaultSignatureExporter = "Spectral Signature Exporter";

string SignatureSetExporter::SignatureExporterArg()
{
   return "Signature Exporter";
}

string SignatureSetExporter::FreezeSignatureSetArg()
{
   return "Freeze";
}

SignatureSetExporter::SignatureSetExporter() : mpOptionsWidget(NULL), mpSignatureExporterSelector(NULL), mpFreezeCheck(NULL)
{
   setDescriptorId("{41848F89-A1FF-4f7c-98F2-6A4111F29AA9}");
   setName("Spectral Signature Library Exporter");
   setCreator("Ball Aerospace & Technologies Corp.");
   setShortDescription("Export spectral signature libraries.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setExtensions("Spectral Signature Library Files (*.slb)");
   setSubtype(TypeConverter::toString<SignatureSet>());
}

SignatureSetExporter::~SignatureSetExporter()
{
   delete mpOptionsWidget;
}

QWidget* SignatureSetExporter::getExportOptionsWidget(const PlugInArgList* pInArgList)
{
   if (mpOptionsWidget == NULL)
   {
      mpOptionsWidget = new LabeledSection(QString::fromStdString(getName()));
      QWidget* pWidget = new QWidget(mpOptionsWidget);
      mpOptionsWidget->setSectionWidget(pWidget);
      mpSignatureExporterSelector = new QComboBox(pWidget);
      mpSignatureExporterSelector->setDuplicatesEnabled(false);
      mpSignatureExporterSelector->setEditable(false);
      mpSignatureExporterSelector->setInsertPolicy(QComboBox::InsertAlphabetically);
      mpSignatureExporterSelector->setToolTip("Plug-in which is used to export signatures.");
      mpSignatureExporterSelector->setWhatsThis("This signature exporter will be used to save the signatures in the "
                                                "signature set if they are not already exported or the signature set is frozen.");
      mpFreezeCheck = new QCheckBox("Freeze Signature Set", pWidget);
      mpFreezeCheck->setChecked(false);
      mpFreezeCheck->setToolTip("Prepare a signature set for transfer to another computer.");
      mpFreezeCheck->setWhatsThis("Freezing a signature set will export all signatures in the signature set to the "
                                  "same directory as the signature set file. In addition, references to those signature files "
                                  "will be relative to the signature set file. The resultant files can be copied to another machine "
                                  "or another directory.");
      QGridLayout* pTopLevel = new QGridLayout(pWidget);
      pTopLevel->setMargin(0);
      pTopLevel->setSpacing(10);
      pTopLevel->addWidget(mpSignatureExporterSelector, 0, 0);
      pTopLevel->addWidget(mpFreezeCheck, 1, 0);
      pTopLevel->setRowStretch(2, 5);
      pTopLevel->setColumnStretch(1, 5);
   }
   mpSignatureExporterSelector->clear();
   vector<PlugInDescriptor*> plugins = Service<PlugInManagerServices>()->getPlugInDescriptors(getType());
   for (vector<PlugInDescriptor*>::const_iterator plugin = plugins.begin(); plugin != plugins.end(); ++plugin)
   {
      PlugInDescriptor* pPlugin = *plugin;
      if ((pPlugin != NULL) && (pPlugin->getSubtype() == TypeConverter::toString<Signature>()))
      {
         mpSignatureExporterSelector->addItem(QString::fromStdString(pPlugin->getName()));
      }
   }

   string signatureExporter = sDefaultSignatureExporter;
   if (pInArgList != NULL)
   {
      pInArgList->getPlugInArgValue<string>(SignatureExporterArg(), signatureExporter);
   }

   mpSignatureExporterSelector->setCurrentIndex(
      mpSignatureExporterSelector->findText(QString::fromStdString(signatureExporter)));

   return mpOptionsWidget;
}

bool SignatureSetExporter::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY((pInArgList = Service<PlugInManagerServices>()->getPlugInArgList()) != NULL);
   VERIFY(pInArgList->addArg<Progress>(Executable::ProgressArg(), NULL, Executable::ProgressArgDescription()));
   VERIFY(pInArgList->addArg<SignatureSet>(Exporter::ExportItemArg(), NULL, "Spectral library to be exported."));
   VERIFY(pInArgList->addArg<FileDescriptor>(Exporter::ExportDescriptorArg(), NULL, "File descriptor for the "
      "output file."));
   VERIFY(pInArgList->addArg<string>(SignatureExporterArg(), sDefaultSignatureExporter, "Signature exporter "
      "to be used."));
   VERIFY(pInArgList->addArg<bool>(FreezeSignatureSetArg(), false, "Flag for whether the exported spectral library "
      "should be frozen. Freezing a spectral library will export all signatures in the library to the "
      "same directory as the spectral library. In addition, references to those signature files "
      "will be relative to the spectral library."));
   return true;
}

bool SignatureSetExporter::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   VERIFY(pInArgList != NULL);
   mpProgress = pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg());
   mProgress = ProgressTracker(mpProgress, "Exporting spectral signature library", "spectral", "F2250874-1772-45EB-801A-1DAE99106B95");

   SignatureSet* pSignatureSet = pInArgList->getPlugInArgValue<SignatureSet>(Exporter::ExportItemArg());
   VERIFY(pSignatureSet != NULL);
   FileDescriptor* pFileDescriptor = pInArgList->getPlugInArgValue<FileDescriptor>(Exporter::ExportDescriptorArg());
   VERIFY(pFileDescriptor != NULL);

   VERIFY(pInArgList->getPlugInArgValue<string>(SignatureExporterArg(), mSignatureExporter));
   if (!isBatch() && mpSignatureExporterSelector != NULL)
   {
      mSignatureExporter = mpSignatureExporterSelector->currentText().toStdString();
      VERIFY(mSignatureExporter.empty() == false);
   }
   VERIFY(pInArgList->getPlugInArgValue<bool>(FreezeSignatureSetArg(), mFreeze));
   if (!isBatch() && mpFreezeCheck != NULL)
   {
      mFreeze = mpFreezeCheck->isChecked();
   }

   if (pSignatureSet->getNumSignatures() == 0)
   {
      mProgress.report("No signatures to export.", 0, ERRORS, true);
      return false;
   }

   // Create the signature library document
   XMLWriter xml("signature_set", NULL, false);
   if (!writeSignatureSet(xml, pSignatureSet, pFileDescriptor->getFilename().getPath()))
   {
      return false;
   }
   FileResource outFile(pFileDescriptor->getFilename().getFullPathAndName().c_str(), "wt");
   if (outFile.get() == NULL)
   {
      mProgress.report("Enable to open spectral library file.", 0, ERRORS);
      return false;
   }
   xml.writeToFile(outFile);

   mProgress.report("Exported spectral library.", 100, NORMAL);
   mProgress.upALevel();
   return true;
}

bool SignatureSetExporter::writeSignatureSet(XMLWriter& xml, SignatureSet* pSignatureSet, string outputDirectory)
{
   bool hasName = false;
   const DynamicObject* pMetadata = pSignatureSet->getMetadata();
   if (pMetadata != NULL)
   {
      vector<string> names;
      pMetadata->getAttributeNames(names);
      for (vector<string>::iterator name = names.begin(); name != names.end(); ++name)
      {
         if (*name == "Name")
         {
            hasName = true;
         }
         xml.pushAddPoint(xml.addElement("metadata"));
         xml.addAttr("name", *name);
         xml.addAttr("value", pMetadata->getAttribute(*name).toXmlString());
         xml.popAddPoint();
      }
   }
   if (!hasName)
   {
      xml.pushAddPoint(xml.addElement("metadata"));
      xml.addAttr("name", "Name");
      xml.addAttr("value", pSignatureSet->getName());
      xml.popAddPoint();
   }
   vector<Signature*> signatures = pSignatureSet->getSignatures();
   int totalSigs = signatures.size();
   for (int sigNum = 0; sigNum < totalSigs; sigNum++)
   {
      mProgress.report("Exporting signatures", static_cast<int>(100.0 * sigNum / totalSigs), NORMAL);
      Signature* pSig = signatures[sigNum];
      SignatureSet* pSubSet = dynamic_cast<SignatureSet*>(pSig);
      if (pSubSet != NULL)
      {
         xml.pushAddPoint(xml.addElement("signature_set"));
         if (!writeSignatureSet(xml, pSubSet, outputDirectory))
         {
            return false;
         }
         xml.popAddPoint();
      }
      else if (pSig != NULL)
      {
         xml.pushAddPoint(xml.addElement("signature"));
         string filename = pSig->getFilename();
         if (mFreeze || filename.empty())
         {
            // export the signature
            ExporterResource signatureExporter(mSignatureExporter, mpProgress);
            string extension = signatureExporter->getDefaultExtensions();
            string::size_type start = extension.find('(');
            string::size_type end = extension.find(')', start);
            if (start != string::npos)
            {
               extension = extension.substr(start + 1, end - start - 1);
               start = extension.find('.');
               end = extension.find(' ', start);
               if (start != string::npos)
               {
                  extension = extension.substr(start + 1, end - start - 1);
               }
            }
            FactoryResource<Filename> pSigFilename;
            pSigFilename->setFullPathAndName(pSig->getName());
            pSigFilename->setFullPathAndName(outputDirectory + SLASH + pSigFilename->getTitle() + "." + extension);
            FileDescriptor* pSigFile = RasterUtilities::generateFileDescriptorForExport(
               pSig->getDataDescriptor(), pSigFilename->getFullPathAndName());
            signatureExporter->setItem(pSig);
            signatureExporter->setFileDescriptor(pSigFile);
            if (!signatureExporter->execute())
            {
               mProgress.report("Unable to export signature " + pSig->getName(), 0, ERRORS, true);
               return false;
            }
            filename = pSigFilename->getFileName();
         }
         if (mFreeze)
         {
            QDir d(QString::fromStdString(outputDirectory));
            filename = d.relativeFilePath(QString::fromStdString(filename)).toStdString();
         }
         xml.addAttr("filename", filename);
         xml.popAddPoint();
      }
   }
   return true;
}

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "AsterSignatureImporter.h"
#include "DataDescriptor.h"
#include "DataVariant.h"
#include "DesktopServices.h"
#include "DynamicObject.h"
#include "FileDescriptor.h"
#include "ImportDescriptor.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "ProgressTracker.h"
#include "Signature.h"
#include "SpectralVersion.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegExp>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtGui/QFileDialog>

using namespace std;

REGISTER_PLUGIN_BASIC(SpectralAster, AsterSignatureImporter);

AsterSignatureImporter::AsterSignatureImporter()
{
   setDescriptorId("{10A20306-E843-4377-BB59-2B01904186B1}");
   setName("ASTER Spectral Signature Importer");
   setSubtype("Signature");
   setCreator("Ball Aerospace & Technologies Corp.");
   setShortDescription("Import ASTER Spectral Library signatures.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setExtensions("ASTER Spectral Signature Files (*.spectrum.txt)");
   setAbortSupported(true);
   allowMultipleInstances(true);
}

AsterSignatureImporter::~AsterSignatureImporter()
{}

unsigned char AsterSignatureImporter::getFileAffinity(const string& filename)
{
   return getImportDescriptors(filename).empty() ? CAN_NOT_LOAD : CAN_LOAD;
}

vector<ImportDescriptor*> AsterSignatureImporter::getImportDescriptors(const string& filename)
{
   vector<ImportDescriptor*> descriptors;
   if (filename.empty())
   {
      return descriptors;
   }

   QFile sigFile(QString::fromStdString(filename));
   if (!sigFile.open(QIODevice::ReadOnly | QIODevice::Text))
   {
      return descriptors;
   }

   //make sure the file starts with "Name:" to ensure it's a valid ASTER sig
   char fileStart[6];
   if (sigFile.read(fileStart, 5) != 5)
   {
      return descriptors;
   }
   if (string(fileStart, 5) != "Name:")
   {
      return descriptors;
   }
   if (!sigFile.seek(0))
   {
      return descriptors;
   }

   QTextStream sigStream(&sigFile);
   FactoryResource<DynamicObject> pMetadata;
   VERIFYRV(pMetadata.get() != NULL, descriptors);

   QString line;
   string lastKeyParsed = "";
   bool foundSigValues = false;
   unsigned int numSigFloats = 0;
   bool foundEmptyLine = true;
   unsigned int lineCount = 0;
   while (!sigStream.atEnd() && !foundSigValues && lineCount <= 26)
   {
      line = sigStream.readLine();
      if (line.indexOf(":") == -1)
      {
         if (!line.isEmpty())
         {
            if (lineCount >= 25)
            {
               //is it the start of signature values or a value for a key that spans lines.
               bool containOnlyDoubles = true;
               QRegExp whitespace("\\s+");
               QStringList parts = line.split(whitespace, QString::SkipEmptyParts);
               for (QStringList::iterator iter = parts.begin(); iter != parts.end(); ++iter)
               {
                  bool valid = false;
                  iter->toDouble(&valid);
                  if (!valid)
                  {
                     containOnlyDoubles = false;
                     break;
                  }
               }
               if (foundEmptyLine && containOnlyDoubles && !parts.isEmpty())
               {
                  //only count lines consisting only of doubles as the start of the sig
                  //section if it was preceded by an empty line.
                  foundSigValues = true;
                  numSigFloats = parts.size();
               }
            }
            if (!foundSigValues && !lastKeyParsed.empty())
            {
               //non empty line that isn't whitespace separated doubles (e.g. start of sig values)
               //so must be continuation of value for a key
               string* pValue = dv_cast<string>(&pMetadata->getAttribute(lastKeyParsed));
               if (pValue != NULL)
               {
                  string existingVal = *pValue;
                  existingVal += " " + line.trimmed().toStdString();
                  pMetadata->setAttribute(lastKeyParsed, existingVal);
               }
            }
         }
      }
      else
      {
         QStringList parts = line.split(":");
         QString key = parts.takeFirst().trimmed();
         lastKeyParsed = key.toStdString();
         QString value = parts.join(":").trimmed();
         pMetadata->setAttribute(key.toStdString(), value.toStdString());
      }
      foundEmptyLine = line.trimmed().isEmpty();
      ++lineCount;
   }

   if (!foundSigValues || numSigFloats != 2)
   {
      //no sigs or invalid amount of numbers, so don't return descriptor
      return descriptors;
   }

   QString yUnits = QString::fromStdString(dv_cast<string>(pMetadata->getAttribute("Y Units"), "")).toLower();
   QString xUnits = QString::fromStdString(dv_cast<string>(pMetadata->getAttribute("X Units"), "")).toLower();
   string firstColumn = dv_cast<string>(pMetadata->getAttribute("First Column"), "");
   string secondColumn = dv_cast<string>(pMetadata->getAttribute("Second Column"), "");

   if (firstColumn != "X" || secondColumn != "Y")
   {
      return descriptors;
   }

   if (xUnits.indexOf("wavelength") == -1)
   {
      return descriptors;
   }

   if ((yUnits.indexOf("reflec") == -1) && (yUnits.indexOf("trans") == -1))
   {
      return descriptors;
   }

   ImportDescriptorResource pImportDescriptor(filename, "Signature");
   VERIFYRV(pImportDescriptor.get() != NULL, descriptors);
   DataDescriptor* pDataDescriptor = pImportDescriptor->getDataDescriptor();
   VERIFYRV(pDataDescriptor != NULL, descriptors);

   FactoryResource<FileDescriptor> pFileDescriptor;
   VERIFYRV(pFileDescriptor.get() != NULL, descriptors);
   pFileDescriptor->setFilename(filename);

   pDataDescriptor->setFileDescriptor(pFileDescriptor.get());
   DynamicObject* pDataMetadata = pDataDescriptor->getMetadata();
   if (pDataMetadata != NULL)
   {
      pDataMetadata->setAttribute("ASTER Signature", *(pMetadata.get()));
   }
   descriptors.push_back(pImportDescriptor.release());
   return descriptors;
}

bool AsterSignatureImporter::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY((pInArgList = Service<PlugInManagerServices>()->getPlugInArgList()) != NULL);
   VERIFY(pInArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   VERIFY(pInArgList->addArg<Signature>(Importer::ImportElementArg()));
   return true;
}

bool AsterSignatureImporter::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   pOutArgList = NULL;
   return true;
}

bool AsterSignatureImporter::execute(PlugInArgList* pInArgList, PlugInArgList* OutArgList)
{
   VERIFY(pInArgList != NULL);
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg()),
      "Loading ASTER spectral signature", "spectral", "3A2C85C5-F541-40B6-855C-43A0E8B288CD");

   Signature* pSignature = pInArgList->getPlugInArgValue<Signature>(Importer::ImportElementArg());
   VERIFY(pSignature != NULL);
   DataDescriptor* pDataDescriptor = pSignature->getDataDescriptor();
   VERIFY(pDataDescriptor != NULL);
   DynamicObject* pMetadata = pDataDescriptor->getMetadata();
   VERIFY(pMetadata != NULL);
   FileDescriptor* pFileDescriptor = pDataDescriptor->getFileDescriptor();
   VERIFY(pFileDescriptor != NULL);

   progress.getCurrentStep()->addProperty("filename", pFileDescriptor->getFilename().getFullPathAndName());

   // Read the signature data
   vector<double> wavelengthData;
   vector<double> yData;

   QFile sigFile(QString::fromStdString(pFileDescriptor->getFilename().getFullPathAndName()));
   if (!sigFile.open(QIODevice::ReadOnly | QIODevice::Text))
   {
      progress.report("Error opening signature file", 0, ERRORS, true);
      return false;
   }
   QTextStream sigStream(&sigFile);

   unsigned int lineCount = 0;
   bool validWave = false;
   bool validY = false;
   bool validDouble = false;
   bool foundEmptyLine = true;
   bool foundSigs = false;
   double wavelength;
   double yValue;
   qint64 fileSize = sigFile.size();
   QString line;
   while (!sigStream.atEnd())
   {
      if (isAborted())
      {
         progress.report("Importer aborted", 0, ABORT, true);
         return false;
      }

      qint64 fileLocation = sigFile.pos();
      progress.report("Loading signature data", static_cast<int>(fileLocation * 100.0 / fileSize), NORMAL);

      line = sigStream.readLine();
      if (lineCount >= 25 && !foundSigs && !line.isEmpty() && line.indexOf(":") == -1 && foundEmptyLine)
      {
         QRegExp whitespace("\\s+");
         QStringList parts = line.split(whitespace, QString::SkipEmptyParts);
         if (parts.size() == 2)
         {
            validDouble = false;
            parts[0].toDouble(&validDouble);
            if (validDouble)
            {
               validDouble = false;
               parts[1].toDouble(&validDouble);
               foundSigs = true;
            }
         }
      }
      if (foundSigs)
      {
         QRegExp whitespace("\\s+");
         QStringList parts = line.split(whitespace, QString::SkipEmptyParts);
         if (parts.size() != 2)
         {
            continue;
         }

         validWave = false;
         validY = false;
         wavelength = parts[0].toDouble(&validWave);
         yValue = parts[1].toDouble(&validY);
         if (validWave && validY)
         {
            wavelengthData.push_back(wavelength);
            yData.push_back(yValue / 100.0);
         }
      }
      foundEmptyLine = line.trimmed().isEmpty();
      lineCount++;
   }

   if (wavelengthData.empty() || yData.empty())
   {
      progress.report("Error parsing signature data", 0, ERRORS, true);
      return false;
   }

   FactoryResource<Units> pReflectanceUnits;
   VERIFY(pReflectanceUnits.get() != NULL);
   string yUnits = dv_cast<string>(pMetadata->getAttributeByPath("ASTER Signature/Y Units"), "");
   if (yUnits.find("Reflec") != string::npos)
   {
      pReflectanceUnits->setUnitType(REFLECTANCE);
      pReflectanceUnits->setUnitName("Reflectance");
      pReflectanceUnits->setScaleFromStandard(1.0);
   }
   else
   {
      pReflectanceUnits->setUnitType(TRANSMITTANCE);
      pReflectanceUnits->setUnitName("Transmittance");
      pReflectanceUnits->setScaleFromStandard(1.0);
   }
   pSignature->setUnits("Reflectance", pReflectanceUnits.get());
   pSignature->setData("Wavelength", wavelengthData);
   pSignature->setData("Reflectance", yData);
   progress.report("Aster signature loaded", 100, NORMAL);
   progress.upALevel();
   return true;
}

bool AsterSignatureImporter::runAllTests(Progress* pProgress, std::ostream& failure)
{
   if (Service<ApplicationServices>()->isBatch())
   {
      return false;
   }
   QStringList filters;
   filters << "*.spectrum.txt";
   Service<DesktopServices> pDesktop;
   QString sigDir = QFileDialog::getExistingDirectory(pDesktop->getMainWidget(), "ASTER Library Directory");
   QDir dir(sigDir);
   QStringList files = dir.entryList(filters, QDir::Files);
   vector<DataElement*> elements;
   for (int i = 0; i < files.size(); ++i)
   {
      ImporterResource res("ASTER Spectral Signature Importer");
      if (pProgress != NULL)
      {
         pProgress->updateProgress("Loading ASTER sigs", (i * 100) / (files.size()), NORMAL);
      }
      string filepath = dir.absoluteFilePath(files[i]).toStdString();
      res->setFilename(filepath);
      bool imported = res->execute();
      if (!imported)
      {
         if (pProgress != NULL)
         {
            pProgress->updateProgress("File did not load " + filepath, 1, ERRORS);
         }
         continue;
      }
      elements = res->getImportedElements();
      if (elements.size() != 1)
      {
         if (pProgress != NULL)
         {
            pProgress->updateProgress("File did not load " + filepath, 1, ERRORS);
         }
         continue;
      }
      Signature* pSig = dynamic_cast<Signature*>(elements[0]);
      if (pSig == NULL)
      {
         if (pProgress != NULL)
         {
            pProgress->updateProgress("File did not load " + filepath, 1, ERRORS);
         }
         continue;
      }
      const vector<double>* pWaves = dv_cast<vector<double> >(&pSig->getData("Wavelength"));
      const vector<double>* pYValues = dv_cast<vector<double> >(&pSig->getData("Reflectance"));
      if (pWaves == NULL || pWaves->empty() || pYValues == NULL || pYValues->empty())
      {
         if (pProgress != NULL)
         {
            pProgress->updateProgress("File did not load " + filepath, 1, ERRORS);
         }
         continue;
      }
   }
   if (pProgress != NULL)
   {
      pProgress->updateProgress("Done Importing", 100, WARNING);
   }
   return true;
}

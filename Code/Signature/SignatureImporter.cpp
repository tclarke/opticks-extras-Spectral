/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "DataDescriptor.h"
#include "DataVariant.h"
#include "DynamicObject.h"
#include "FileDescriptor.h"
#include "FileResource.h"
#include "ImportDescriptor.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "ProgressTracker.h"
#include "Signature.h"
#include "SignatureImporter.h"
#include "SpectralVersion.h"
#include "StringUtilities.h"
#include "Wavelengths.h"

#include <boost/algorithm/string.hpp>

using namespace boost;
using namespace boost::algorithm;
using namespace std;

REGISTER_PLUGIN_BASIC(SpectralSignature, SignatureImporter);

SignatureImporter::SignatureImporter()
{
   setDescriptorId("{B9A94AE2-97D2-44d8-9BC9-511C06D050CF}");
   setName("Spectral Signature Importer");
   setSubtype("Signature");
   setCreator("Ball Aerospace & Technologies Corp.");
   setShortDescription("Import spectral signatures.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setExtensions("Spectral Signature Files (*.sig *.elm *.txt)");
   setAbortSupported(true);
}

SignatureImporter::~SignatureImporter()
{
}

unsigned char SignatureImporter::getFileAffinity(const string& filename)
{
   return getImportDescriptors(filename).empty() ? CAN_NOT_LOAD : CAN_LOAD;
}

vector<ImportDescriptor*> SignatureImporter::getImportDescriptors(const string& filename)
{
   vector<ImportDescriptor*> descriptors;
   if (filename.empty())
   {
      return descriptors;
   }

   LargeFileResource pSigFile;
   if (!pSigFile.open(filename, O_RDONLY | O_BINARY, S_IREAD))
   {
      return descriptors;
   }

   // load the data
   FactoryResource<DynamicObject> pMetadata;
   VERIFYRV(pMetadata.get() != NULL, descriptors);

   bool readError = false;
   string line;

   // parse the metadata
   for (line = pSigFile.readLine(&readError);
      (readError == false) && (line.find('=') != string::npos);
      line = pSigFile.readLine(&readError))
   {
      vector<string> metadataEntry;

      trim(line);
      split(metadataEntry, line, is_any_of("="));
      if (metadataEntry.size() == 2)
      {
         string key = metadataEntry[0];
         string value = metadataEntry[1];
         trim(key);
         trim(value);
         if (ends_with(key, "Bands") || key == "Pixels")
         {
            pMetadata->setAttribute(key, StringUtilities::fromXmlString<unsigned long>(value));
         }
         else if (key == "UnitType")
         {
            pMetadata->setAttribute(key, StringUtilities::fromXmlString<UnitType>(value));
         }
         else if (key == "UnitScale")
         {
            pMetadata->setAttribute(key, StringUtilities::fromXmlString<float>(value));
         }
         else
         {
            pMetadata->setAttribute(key, value);
         }
      }
   }
   if ((readError == true) && (pSigFile.eof() != 1))
   {
      return descriptors;
   }
   // Verify that the next line contains float float pairs
   vector<string> dataEntry;

   trim(line);
   split(dataEntry, line, is_space());
   if (dataEntry.size() != 2)
   {
      return descriptors;
   }
   bool error = false;
   StringUtilities::fromXmlString<float>(dataEntry[0], &error);
   !error && StringUtilities::fromXmlString<float>(dataEntry[1], &error);
   if (error)
   {
      return descriptors;
   }

   string datasetName = dv_cast<string>(pMetadata->getAttribute("Name"), filename);

   ImportDescriptorResource pImportDescriptor(datasetName, "Signature");
   VERIFYRV(pImportDescriptor.get() != NULL, descriptors);
   DataDescriptor* pDataDescriptor = pImportDescriptor->getDataDescriptor();
   VERIFYRV(pDataDescriptor != NULL, descriptors);

   FactoryResource<FileDescriptor> pFileDescriptor;
   VERIFYRV(pFileDescriptor.get() != NULL, descriptors);
   pFileDescriptor->setFilename(filename);

   pDataDescriptor->setFileDescriptor(pFileDescriptor.get());
   pDataDescriptor->setMetadata(pMetadata.get());
   descriptors.push_back(pImportDescriptor.release());
   return descriptors;
}

bool SignatureImporter::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY((pInArgList = Service<PlugInManagerServices>()->getPlugInArgList()) != NULL);
   VERIFY(pInArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   VERIFY(pInArgList->addArg<Signature>(Importer::ImportElementArg()));
   return true;
}

bool SignatureImporter::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   pOutArgList = NULL;
   return true;
}

bool SignatureImporter::execute(PlugInArgList* pInArgList, PlugInArgList* OutArgList)
{
   VERIFY(pInArgList != NULL);
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg()),
      "Loading spectral signature", "spectral", "5A9F8379-7D7D-4575-B78B-305AE0DFC66D");

   Signature* pSignature = pInArgList->getPlugInArgValue<Signature>(Importer::ImportElementArg());
   VERIFY(pSignature != NULL);
   DataDescriptor* pDataDescriptor = pSignature->getDataDescriptor();
   VERIFY(pDataDescriptor != NULL);
   FileDescriptor* pFileDescriptor = pDataDescriptor->getFileDescriptor();
   VERIFY(pFileDescriptor != NULL);

   progress.getCurrentStep()->addProperty("filename", pFileDescriptor->getFilename().getFullPathAndName());
   
   DynamicObject* pMetadata = pSignature->getMetadata();
   VERIFY(pMetadata != NULL);
   UnitType units = dv_cast<UnitType>(pMetadata->getAttribute("UnitType"), REFLECTANCE);
   float unitScale = dv_cast<float>(pMetadata->getAttribute("UnitScale"), 1.0);

   LargeFileResource pSigFile;
   VERIFY(pSigFile.open(pFileDescriptor->getFilename().getFullPathAndName(), O_RDONLY | O_BINARY, S_IREAD));

   // Read the signature data
   vector<double> wavelengthData, reflectanceData;

   int64_t fileSize = pSigFile.fileLength();
   bool readError = false;
   for (string line = pSigFile.readLine(&readError); readError == false; line = pSigFile.readLine(&readError))
   {
      if (isAborted())
      {
         progress.report("Importer aborted", 0, ABORT, true);
         return false;
      }

      int64_t fileLocation = pSigFile.tell();
      progress.report("Loading signature data", static_cast<int>(fileLocation * 100.0 / fileSize), NORMAL);

      trim(line);
      if (line.empty())
      {
         continue;
      }
      if (line.find('=') == string::npos)
      {
         double wavelength = 0.0, reflectance = 0.0;
         vector<string> dataEntry;
         split(dataEntry, line, is_space());
         bool error = true;
         if (dataEntry.size() >= 1)
         {
            wavelength = StringUtilities::fromXmlString<double>(dataEntry[0], &error);
            if (wavelength > 50.0)
            {
               // Assume wavelength values are in nanometers and convert to microns
               wavelength = Wavelengths::convertValue(wavelength, Wavelengths::NANOMETERS, Wavelengths::MICRONS);
            }
         }
         if (!error && dataEntry.size() == 2)
         {
            reflectance = StringUtilities::fromXmlString<double>(dataEntry[1], &error);
            if (units == REFLECTANCE && unitScale == 1.0 && reflectance > 2.0) // scale reflectance values to (0,1)
                                                                               // Values assumed to be scaled 0 to 10000
            {
               reflectance *= 0.0001;
            }
         }
         if (error)
         {
            progress.report("Error parsing signature data", 0, ERRORS, true);
         }

         if (reflectance != 0.0)
         {
            if (reflectance < 0.0)
            {
               // zero out negative reflectances, black holes not expected
               reflectance = 0.0;
            }

            wavelengthData.push_back(wavelength);
            reflectanceData.push_back(reflectance);
         }
      }
   }

   if ((readError == true) && (pSigFile.eof() != 1))
   {
      progress.report("Unable to read signature file", 0, ERRORS, true);
      return false;
   }
   FactoryResource<Units> pReflectanceUnits;
   VERIFY(pReflectanceUnits.get() != NULL);
   string unitName = dv_cast<string>(pMetadata->getAttribute("UnitName"), StringUtilities::toDisplayString(units));
   pReflectanceUnits->setUnitType(units);
   pReflectanceUnits->setUnitName(unitName);
   if (unitScale != 0.0)
   {
      pReflectanceUnits->setScaleFromStandard(1.0 / unitScale);
   }
   pSignature->setUnits("Reflectance", pReflectanceUnits.get());
   pSignature->setData("Wavelength", wavelengthData);
   pSignature->setData("Reflectance", reflectanceData);
   progress.report("Spectral signature loaded", 100, NORMAL);
   progress.upALevel();
   return true;
}

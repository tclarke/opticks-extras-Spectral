/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "CommonPlugInArgs.h"
#include "DynamicObject.h"
#include "FileDescriptor.h"
#include "FileResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "ProgressTracker.h"
#include "Signature.h"
#include "SignatureExporter.h"
#include "SpectralVersion.h"
#include "StringUtilities.h"
#include "TypeConverter.h"

#include <stdexcept>
using namespace std;

REGISTER_PLUGIN_BASIC(SpectralSignature, SignatureExporter);

SignatureExporter::SignatureExporter()
{
   setDescriptorId("{8E7CCDA6-B777-47f7-8731-C6D2E1AB88AB}");
   setName("Spectral Signature Exporter");
   setCreator("Ball Aerospace & Technologies Corp.");
   setShortDescription("Export spectral signatures.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setExtensions("Spectral Signature Files (*.sig *.sign *.txt)");
   setSubtype(TypeConverter::toString<Signature>());
}

SignatureExporter::~SignatureExporter()
{
}

bool SignatureExporter::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY((pInArgList = Service<PlugInManagerServices>()->getPlugInArgList()) != NULL);
   VERIFY(pInArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   VERIFY(pInArgList->addArg<Signature>(Exporter::ExportItemArg()));
   VERIFY(pInArgList->addArg<FileDescriptor>(Exporter::ExportDescriptorArg()));
   VERIFY(pInArgList->addArg<bool>(SpectralCommon::ExportMetadataArg(), true));
   return true;
}

bool SignatureExporter::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   VERIFY(pInArgList != NULL);
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg()),
      "Exporting spectral signature", "spectral", "C6BC621B-1AAA-4B5F-A6CB-389CB554CAB3");

   Signature* pSignature = pInArgList->getPlugInArgValue<Signature>(Exporter::ExportItemArg());
   VERIFY(pSignature != NULL);
   FileDescriptor* pFileDescriptor = pInArgList->getPlugInArgValue<FileDescriptor>(Exporter::ExportDescriptorArg());
   VERIFY(pFileDescriptor != NULL);
   bool exportMetadata;
   pInArgList->getPlugInArgValue<bool>(SpectralCommon::ExportMetadataArg(), exportMetadata);

   vector<double> wavelengthData;
   vector<double> reflectanceData;
   try
   {
      wavelengthData = dv_cast<vector<double> >(pSignature->getData("Wavelength"));
      reflectanceData = dv_cast<vector<double> >(pSignature->getData("Reflectance"));
   }
   catch(bad_cast&)
   {
      progress.report("Signature data format is unknown. Must contain \"Wavelength\" and \"Reflectance\" data as vector<double>.", 0, ERRORS, true);
      return false;
   }
   if (wavelengthData.size() != reflectanceData.size())
   {
      progress.report("Wavelength and reflectance contain different amounts of data", 0, ERRORS, true);
      return false;
   }
   if (wavelengthData.empty())
   {
      progress.report("Signature is empty", 0, ERRORS, true);
      return false;
   }
   if (pFileDescriptor->getFilename().getFileName().empty())
   {
      progress.report("Invalid export file name.", 0, ERRORS, true);
      return false;
   }
   LargeFileResource signatureFile;
#if defined(UNIX_API)
   int newFileMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
#else
   int newFileMode = S_IREAD | S_IWRITE;
#endif
   if (!signatureFile.open(pFileDescriptor->getFilename().getFullPathAndName(),
      O_WRONLY | O_CREAT | O_BINARY, newFileMode))
   {
      progress.report("Unable to open file for export.", 0, ERRORS, true);
      return false;
   }

   try
   {
      progress.report("Export metadata", 1, NORMAL);
      DynamicObject* pMetadata = pSignature->getMetadata();
      VERIFY(pMetadata != NULL);
      writeMetadataEntry(signatureFile, "Version", dv_cast<unsigned int>(pMetadata->getAttribute("Version"), 3));
      writeMetadataEntry(signatureFile, "Name", pSignature->getName());
      const Units* pReflectanceUnits = pSignature->getUnits("Reflectance");
      if (pReflectanceUnits != NULL)
      {
         writeMetadataEntry(signatureFile, "UnitName", pReflectanceUnits->getUnitName());
         writeMetadataEntry(signatureFile, "UnitType", pReflectanceUnits->getUnitType());
         double unitScale = pReflectanceUnits->getScaleFromStandard();
         if (unitScale != 0.0)
         {
            unitScale = 1.0 / unitScale;
         }
         writeMetadataEntry(signatureFile, "UnitScale", unitScale);
      }

      if (exportMetadata)
      {
         vector<string> attributeNames;
         pMetadata->getAttributeNames(attributeNames);
         for (vector<string>::size_type attributeNum = 0; attributeNum < attributeNames.size(); attributeNum++)
         {
            if (isAborted())
            {
               progress.report("Exporter aborted", 0, ABORT, true);
               return false;
            }
            progress.report("Export metadata", static_cast<int>(
               static_cast<float>(attributeNum) / attributeNames.size() * 50.0), NORMAL);
            string attributeName = attributeNames[attributeNum];
            DataVariant val = pMetadata->getAttribute(attributeName);
            if (val.getTypeName() != TypeConverter::toString<DynamicObject>() &&
               attributeName != "Name" &&
               attributeName != "Version" &&
               attributeName != "UnitName" &&
               attributeName != "UnitType" &&
               attributeName != "UnitScale")
            {
               writeMetadataEntry(signatureFile, attributeName, val.toXmlString());
            }
         }
      }
   }
   catch (runtime_error &err)
   {
      progress.report(err.what(), 0, ERRORS, true);
      return false;
   }

   for (vector<double>::size_type i = 0; i < wavelengthData.size(); ++i)
   {
      if (isAborted())
      {
         progress.report("Exporter aborted", 0, ABORT, true);
         return false;
      }
      progress.report("Export signature", static_cast<int>(
         static_cast<float>(i) / wavelengthData.size() * 50.0 + 50.0), NORMAL);
      string output = StringUtilities::toXmlString<double>(wavelengthData[i]) + " " + StringUtilities::toXmlString<double>(reflectanceData[i]) + "\n";
      if (signatureFile.write(output.c_str(), output.size()) != output.size())
      {
         progress.report("Unable to write signature entry to file.", 0, ERRORS);
         return false;
      }
   }

   progress.report("Spectral signature export complete", 100, NORMAL);
   progress.upALevel();
   return true;
}

template<typename T>
void SignatureExporter::writeMetadataEntry(LargeFileResource& file, const string& key, const T& val)
{
   bool error = false;
   string valString = StringUtilities::toXmlString(val, &error);
   if (!error)
   {
      string output = key + " = " + valString + "\n";
      error = (file.write(output.c_str(), output.size()) != output.size());
   }
   if (error)
   {
      throw runtime_error("Unable to write metadata entry to file.");
   }
}

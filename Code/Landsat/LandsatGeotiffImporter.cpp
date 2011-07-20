/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "CachedPager.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DataVariant.h"
#include "DateTime.h"
#include "DynamicObject.h"
#include "FileFinder.h"
#include "FileResource.h"
#include "ImportDescriptor.h"
#include "LandsatGeotiffImporter.h"
#include "LandsatUtilities.h"
#include "MessageLogResource.h"
#include "ObjectResource.h"
#include "OptionsLandsatImport.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterPager.h"
#include "RasterUtilities.h"
#include "SpecialMetadata.h"
#include "SpectralVersion.h"
#include "StringUtilities.h"
#include "Units.h"
#include "Wavelengths.h"
#include "XercesIncludes.h"
#include "xmlreader.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <limits>

XERCES_CPP_NAMESPACE_USE 

REGISTER_PLUGIN_BASIC(SpectralLandsat, LandsatGeotiffImporter);

LandsatGeotiffImporter::LandsatGeotiffImporter()
{
   setDescriptorId("{A8E57295-9A8-44FD-96D8-26A4FA13298F}");
   setName("Landsat GeoTIFF Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setShortDescription("Importer for Landsat data in GeoTIFF format.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setExtensions("Landsat 5 Files (*_MTL.txt *_mtl.txt *.tif *.TIF);;"
      "Landsat 7 Files (*_MTL.txt *_mtl.txt *.tif *.TIF)");
}

LandsatGeotiffImporter::~LandsatGeotiffImporter()
{}

bool LandsatGeotiffImporter::validate(const DataDescriptor* pDescriptor, std::string& errorMessage) const
{
   errorMessage = "";
   if (!mErrors.empty())
   {
      for (std::vector<std::string>::const_iterator iter = mErrors.begin(); iter != mErrors.end(); ++iter)
      {
         errorMessage += *iter + "\n";
      }
      return false;
   }
   std::string baseErrorMessage;
   bool bValidate = RasterElementImporterShell::validate(pDescriptor, baseErrorMessage);
   if (!mWarnings.empty())
   {
      if (!baseErrorMessage.empty())
      {
         errorMessage += baseErrorMessage + "\n";
      }
      for (std::vector<std::string>::const_iterator iter = mWarnings.begin(); iter != mWarnings.end(); ++iter)
      {
         errorMessage += *iter + "\n";
      }
   }
   else
   {
      errorMessage = baseErrorMessage;
   }
   return bValidate;
}

namespace
{
   std::string determineMetadataFile(const std::string& filename, bool* originallyTiff = NULL)
   {
      if (originallyTiff != NULL)
      {
         *originallyTiff = false;
      }
      QString filenameStr = QString::fromStdString(filename);
      if (filenameStr.endsWith(".tif", Qt::CaseInsensitive))
      {
         QFileInfo fileInfo(filenameStr);
         QString name = fileInfo.baseName();
         if ((name.startsWith("L5", Qt::CaseInsensitive) ||
              name.startsWith("L7", Qt::CaseInsensitive)) &&
             name.length() >= 5)
         {
            name = name.mid(0, name.length() - 3) + "MTL.txt";
            QString newName = fileInfo.path() + QString::fromStdString(SLASH) + name;
            if (QFile::exists(newName))
            {
               if (originallyTiff != NULL)
               {
                  *originallyTiff = true;
               }
               filenameStr = newName;
            }
         }
      }
      return filenameStr.toStdString();
   }
}

unsigned char LandsatGeotiffImporter::getFileAffinity(const std::string& filename)
{
   if (filename.empty())
   {
      return CAN_NOT_LOAD;
   }
   bool originallyTiff = false;
   std::string metadataFile = determineMetadataFile(filename, &originallyTiff);
   bool readMetadata = false;
   FactoryResource<DynamicObject> pImageMetadata = Landsat::parseMtlFile(metadataFile, readMetadata);
   if (!readMetadata)
   {
      return CAN_NOT_LOAD;
   }
   std::string spacecraft = dv_cast<std::string>(
      pImageMetadata->getAttributeByPath("LANDSAT_MTL/L1_METADATA_FILE/PRODUCT_METADATA/SPACECRAFT_ID"), "");
   if (spacecraft != "Landsat5" && spacecraft != "Landsat7")
   {
      return CAN_NOT_LOAD;
   }
   if (originallyTiff)
   {
      return CAN_LOAD + 5; //override normal tiff importers
   }
   return CAN_LOAD;
}

std::vector<ImportDescriptor*> LandsatGeotiffImporter::getImportDescriptors(const std::string& filename)
{
   mErrors.clear();
   mWarnings.clear();

   std::vector<ImportDescriptor*> descriptors;
   if (filename.empty())
   {
      return descriptors;
   }

   std::string metadataFile = determineMetadataFile(filename);

   bool readMetadata = false;
   FactoryResource<DynamicObject> pImageMetadata = Landsat::parseMtlFile(metadataFile, readMetadata);
   if (!readMetadata)
   {
      return descriptors;
   }
   std::string spacecraft = dv_cast<std::string>(
      pImageMetadata->getAttributeByPath("LANDSAT_MTL/L1_METADATA_FILE/PRODUCT_METADATA/SPACECRAFT_ID"), "");
   if (spacecraft != "Landsat5" && spacecraft != "Landsat7")
   {
      return descriptors;
   }
   std::vector<ImportDescriptor*> vnirDescriptors = createImportDescriptors(metadataFile, pImageMetadata.get(),
      Landsat::LANDSAT_VNIR);
   std::vector<ImportDescriptor*> panDescriptors = createImportDescriptors(metadataFile, pImageMetadata.get(),
      Landsat::LANDSAT_PAN);
   std::vector<ImportDescriptor*> tirDescriptors = createImportDescriptors(metadataFile, pImageMetadata.get(),
      Landsat::LANDSAT_TIR);
   std::copy(vnirDescriptors.begin(), vnirDescriptors.end(), std::back_inserter(descriptors));
   std::copy(panDescriptors.begin(), panDescriptors.end(), std::back_inserter(descriptors));
   std::copy(tirDescriptors.begin(), tirDescriptors.end(), std::back_inserter(descriptors));

   return descriptors;
}

std::vector<ImportDescriptor*> LandsatGeotiffImporter::createImportDescriptors(const std::string& filename,
   const DynamicObject* pImageMetadata,
   Landsat::LandsatImageType type)
{
   std::string suffix;
   if (type == Landsat::LANDSAT_VNIR)
   {
      suffix = "vnir";
   }
   else if (type == Landsat::LANDSAT_PAN)
   {
      suffix = "pan";
   }
   else if (type == Landsat::LANDSAT_TIR)
   {
      suffix = "tir";
   }
   std::vector<ImportDescriptor*> descriptors;
   std::string spacecraft = dv_cast<std::string>(
      pImageMetadata->getAttributeByPath("LANDSAT_MTL/L1_METADATA_FILE/PRODUCT_METADATA/SPACECRAFT_ID"), "");
   std::vector<std::string> bandNames = Landsat::getSensorBandNames(spacecraft, type);
   if (bandNames.empty())
   {
      //this spacecraft and iamge type
      //isn't meant to have any bands, so terminate early
      //e.g. spacecraft == "Landsat5" && type == Landsat::LANDSAT_PAN
      return descriptors;
   }
   std::vector<unsigned int> validBands;
   std::vector<std::string> bandFiles = Landsat::getGeotiffBandFilenames(
      pImageMetadata, filename, type, validBands);
   if (bandFiles.empty())
   {
      mWarnings.push_back("Unable to locate band files for " + suffix + " product."); 
      return descriptors;
   }
   ImportDescriptorResource pImportDescriptor(filename + "-" + suffix,
      TypeConverter::toString<RasterElement>(), NULL, false);
   if (pImportDescriptor.get() == NULL)
   {
      return descriptors;
   }
   RasterDataDescriptor* pDescriptor = dynamic_cast<RasterDataDescriptor*>(pImportDescriptor->getDataDescriptor());
   if (pDescriptor == NULL)
   {
      return descriptors;
   }
   pDescriptor->setProcessingLocation(ON_DISK);
   DynamicObject* pMetadata = pDescriptor->getMetadata();
   pMetadata->merge(pImageMetadata);
   FactoryResource<RasterFileDescriptor> pFileDescriptorRes;
   pDescriptor->setFileDescriptor(pFileDescriptorRes.get());
   RasterFileDescriptor* pFileDescriptor = dynamic_cast<RasterFileDescriptor*>(pDescriptor->getFileDescriptor());
   pFileDescriptor->setFilename(filename);

   std::string tiffFile = bandFiles[0];
   if (!Landsat::parseBasicsFromTiff(tiffFile, pDescriptor))
   {
      mWarnings.push_back("Unable to parse basic information about image from tiff file for " + suffix + " product.");
      return descriptors;
   }
   if (pDescriptor->getBandCount() != 1 || pDescriptor->getDataType() != INT1UBYTE)
   {
      mWarnings.push_back("Improperly formatted tiff file for " + suffix + " product.");
      return descriptors;
   }
   pDescriptor->setInterleaveFormat(BSQ); //one tiff file per band.
   pFileDescriptor->setInterleaveFormat(BSQ);
   std::vector<DimensionDescriptor> bands = RasterUtilities::generateDimensionVector(
      bandFiles.size(), true, false, true);
   pDescriptor->setBands(bands);
   pFileDescriptor->setBands(bands);
   pDescriptor->setBadValues(std::vector<int>(1, 0));
   pFileDescriptor->setDatasetLocation(suffix);

   //special metadata here
   Landsat::fixMtlMetadata(pMetadata, type, validBands);

   std::vector<std::string> defaultImport = OptionsLandsatImport::getSettingDefaultImport();
   bool fallbackToDn = false;
   descriptors.push_back(pImportDescriptor.release());

   if (type == Landsat::LANDSAT_VNIR)
   {
      //attempt to display true-color
      DimensionDescriptor redBand = RasterUtilities::findBandWavelengthMatch(0.630, 0.690, pDescriptor);
      DimensionDescriptor greenBand = RasterUtilities::findBandWavelengthMatch(0.510, 0.590, pDescriptor);
      DimensionDescriptor blueBand = RasterUtilities::findBandWavelengthMatch(0.410, 0.490, pDescriptor);
      if (redBand.isValid() && greenBand.isValid() && blueBand.isValid())
      {
         pDescriptor->setDisplayMode(RGB_MODE);
         pDescriptor->setDisplayBand(RED, redBand);
         pDescriptor->setDisplayBand(GREEN, greenBand);
         pDescriptor->setDisplayBand(BLUE, blueBand);
      }
   }

   std::vector<std::pair<double, double> > radianceFactors = Landsat::determineRadianceConversionFactors(
      pMetadata, type, validBands);
   bool shouldDefaultImportRadiance =
      std::find(defaultImport.begin(), defaultImport.end(), suffix + "-Radiance") != defaultImport.end();
   if (radianceFactors.size() == bandFiles.size())
   {
      //we have enough to create radiance import descriptor
      RasterDataDescriptor* pRadianceDescriptor = dynamic_cast<RasterDataDescriptor*>(
         pDescriptor->copy(filename + "-" + suffix + "-radiance", NULL));
      if (pRadianceDescriptor != NULL)
      {
         pRadianceDescriptor->setDataType(FLT4BYTES);
         pRadianceDescriptor->setValidDataTypes(std::vector<EncodingType>(1, pRadianceDescriptor->getDataType()));
         pRadianceDescriptor->setBadValues(std::vector<int>(1, -100));
         FactoryResource<Units> pUnits;
         pUnits->setUnitType(RADIANCE);
         pUnits->setUnitName("w/(m^2*sr*um)");
         pUnits->setScaleFromStandard(1.0);
         pRadianceDescriptor->setUnits(pUnits.get());
         FileDescriptor* pRadianceFileDescriptor = pRadianceDescriptor->getFileDescriptor();
         if (pRadianceFileDescriptor != NULL)
         {
            pRadianceFileDescriptor->setDatasetLocation(suffix + "-radiance");
            ImportDescriptorResource pRadianceImportDescriptor(pRadianceDescriptor,
               shouldDefaultImportRadiance);
            descriptors.push_back(pRadianceImportDescriptor.release());
         }
      }
   }
   else if (shouldDefaultImportRadiance)
   {
      fallbackToDn = true;
   }

   std::vector<double> reflectanceFactors = Landsat::determineReflectanceConversionFactors(
      pMetadata, type, validBands);
   bool shouldDefaultImportReflectance =
      std::find(defaultImport.begin(), defaultImport.end(), suffix + "-Reflectance") != defaultImport.end();
   if (radianceFactors.size() == bandFiles.size() && reflectanceFactors.size() == bandFiles.size())
   {
      //we have enough to create reflectance import descriptor
      RasterDataDescriptor* pReflectanceDescriptor = dynamic_cast<RasterDataDescriptor*>(
         pDescriptor->copy(filename + "-" + suffix + "-reflectance", NULL));
      if (pReflectanceDescriptor != NULL)
      {
         pReflectanceDescriptor->setDataType(INT2SBYTES);
         pReflectanceDescriptor->setValidDataTypes(
            std::vector<EncodingType>(1, pReflectanceDescriptor->getDataType()));
         pReflectanceDescriptor->setBadValues(std::vector<int>(1, std::numeric_limits<short>::max()));
         FactoryResource<Units> pUnits;
         pUnits->setUnitType(REFLECTANCE);
         pUnits->setUnitName("Reflectance");
         pUnits->setScaleFromStandard(1/10000.0);
         pReflectanceDescriptor->setUnits(pUnits.get());
         FileDescriptor* pReflectanceFileDescriptor = pReflectanceDescriptor->getFileDescriptor();
         if (pReflectanceFileDescriptor != NULL)
         {
            pReflectanceFileDescriptor->setDatasetLocation(suffix + "-reflectance");
            ImportDescriptorResource pReflectanceImportDescriptor(pReflectanceDescriptor,
               shouldDefaultImportReflectance);
            descriptors.push_back(pReflectanceImportDescriptor.release());
         }
      }
   }
   else if (shouldDefaultImportReflectance)
   {
      fallbackToDn = true;
   }

   double K1 = 0.0;
   double K2 = 0.0;
   bool haveTemperatureFactors = Landsat::getTemperatureConstants(pMetadata, type,
      K1, K2);
   bool shouldDefaultImportTemperature =
      std::find(defaultImport.begin(), defaultImport.end(), suffix + "-Temperature") != defaultImport.end();
   if (radianceFactors.size() == bandFiles.size() && haveTemperatureFactors)
   {
      //we have enough to create temperature import descriptor
      RasterDataDescriptor* pTemperatureDescriptor = dynamic_cast<RasterDataDescriptor*>(
         pDescriptor->copy(filename + "-" + suffix + "-temperature", NULL));
      if (pTemperatureDescriptor != NULL)
      {
         pTemperatureDescriptor->setDataType(FLT4BYTES);
         pTemperatureDescriptor->setValidDataTypes(
            std::vector<EncodingType>(1, pTemperatureDescriptor->getDataType()));
         pTemperatureDescriptor->setBadValues(std::vector<int>(1, -1));
         FactoryResource<Units> pUnits;
         pUnits->setUnitType(EMISSIVITY);
         pUnits->setUnitName("K");
         pUnits->setScaleFromStandard(1.0);
         pTemperatureDescriptor->setUnits(pUnits.get());
         FileDescriptor* pTemperatureFileDescriptor = pTemperatureDescriptor->getFileDescriptor();
         if (pTemperatureFileDescriptor != NULL)
         {
            pTemperatureFileDescriptor->setDatasetLocation(suffix + "-temperature");
            ImportDescriptorResource pTemperatureImportDescriptor(pTemperatureDescriptor,
               shouldDefaultImportTemperature);
            descriptors.push_back(pTemperatureImportDescriptor.release());
         }
      }
   }
   else if (shouldDefaultImportTemperature)
   {
      fallbackToDn = true;
   }

   if (fallbackToDn ||
      std::find(defaultImport.begin(), defaultImport.end(), suffix + "-DN") != defaultImport.end())
   {
      pImportDescriptor->setImported(true);
   }

   return descriptors;
}

bool LandsatGeotiffImporter::createRasterPager(RasterElement* pRaster) const
{
   VERIFY(pRaster != NULL);
   DataDescriptor* pDescriptor = pRaster->getDataDescriptor();
   VERIFY(pDescriptor != NULL);
   FileDescriptor* pFileDescriptor = pDescriptor->getFileDescriptor();
   VERIFY(pFileDescriptor != NULL);

   std::string filename = pRaster->getFilename();
   std::string datasetName = pFileDescriptor->getDatasetLocation();
   Progress* pProgress = getProgress();

   FactoryResource<Filename> pFilename;
   pFilename->setFullPathAndName(filename);

   ExecutableResource pagerPlugIn("Landsat GeoTIFF Raster Pager", std::string(), pProgress);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedElementArg(), pRaster);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedFilenameArg(), pFilename.get());

   bool success = pagerPlugIn->execute();

   RasterPager* pPager = dynamic_cast<RasterPager*>(pagerPlugIn->getPlugIn());
   if (!success || pPager == NULL)
   {
      std::string message = "Execution of Landsat Geotiff Raster Pager failed!";
      if (pProgress != NULL) pProgress->updateProgress(message, 0, ERRORS);
      return false;
   }

   pRaster->setPager(pPager);
   pagerPlugIn->releasePlugIn();

   return true;
}

int LandsatGeotiffImporter::getValidationTest(const DataDescriptor* pDescriptor) const
{
   int validationTest = RasterElementImporterShell::getValidationTest(pDescriptor);
   const RasterDataDescriptor* pRasterDesc = dynamic_cast<const RasterDataDescriptor*>(pDescriptor);
   if (pRasterDesc == NULL)
   {
      return validationTest;
   }
   const RasterFileDescriptor* pRasterFileDesc =
      dynamic_cast<const RasterFileDescriptor*>(pRasterDesc->getFileDescriptor());
   if (pRasterFileDesc == NULL)
   {
      return validationTest;
   }
   if (pRasterDesc->getProcessingLocation() == ON_DISK_READ_ONLY)
   {
      // Disabling these checks since the importer supports band subsets with BSQ for on-disk read-only
      validationTest &= ~(NO_BAND_SUBSETS);
   }
   return validationTest;
}

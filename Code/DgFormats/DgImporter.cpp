/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "CachedPager.h"
#include "DataRequest.h"
#include "DataVariant.h"
#include "DateTime.h"
#include "DgFileTile.h"
#include "DgImporter.h"
#include "DgUtilities.h"
#include "DynamicObject.h"
#include "FileResource.h"
#include "ImportDescriptor.h"
#include "MessageLogResource.h"
#include "ObjectResource.h"
#include "OptionsDgImport.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "SpectralVersion.h"
#include "Units.h"
#include "XercesIncludes.h"
#include "xmlreader.h"

#include <limits>

XERCES_CPP_NAMESPACE_USE 

REGISTER_PLUGIN_BASIC(SpectralDgFormats, DgImporter);

DgImporter::DgImporter()
{
   setDescriptorId("{1237B5EC-2B51-4601-894A-3BC3577F2F3E}");
   setName("DgImporter");
   setCreator("Ball Aerospace & Technologies Corp.");
   setShortDescription("Importer for QuickBird-2, WorldView-1 and WorldView-2 data");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setExtensions("QuickBird-2 Files (*.xml *.XML);;WorldView-1 Files (*.xml *.XML);;WorldView-2 Files (*.xml *.XML)");
}

DgImporter::~DgImporter()
{}

bool DgImporter::validate(const DataDescriptor* pDescriptor, std::string& errorMessage) const
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

unsigned char DgImporter::getFileAffinity(const std::string& filename)
{
   if (filename.empty())
   {
      return CAN_NOT_LOAD;
   }

   XmlReader xml(Service<MessageLogMgr>()->getLog(), false);
   DOMDocument* pDoc = xml.parse(filename);
   if (pDoc == NULL)
   {
      return CAN_NOT_LOAD;
   }
   DOMElement* pRoot = pDoc->getDocumentElement();
   if (pRoot == NULL)
   {
      return CAN_NOT_LOAD;
   }
   if (XMLString::equals(pRoot->getNodeName(), X("isd")))
   {
      return CAN_LOAD;
   }
   if (XMLString::equals(pRoot->getNodeName(), X("README")))
   {
      //check for the overview file and accept it so 
      //that no other importer tries to load it.

      DOMNodeList* pElements = pRoot->getElementsByTagName(X("DGORDERNO"));
      if (pElements->getLength() != 1)
      {
         return CAN_NOT_LOAD;
      }
      pElements = pRoot->getElementsByTagName(X("DGORDERITEMNO"));
      if (pElements->getLength() != 1)
      {
         return CAN_NOT_LOAD;
      }
      return CAN_LOAD;
   }
   return CAN_NOT_LOAD;
}

std::vector<ImportDescriptor*> DgImporter::getImportDescriptors(const std::string& filename)
{
   mErrors.clear();
   mWarnings.clear();

   std::vector<ImportDescriptor*> descriptors;
   if (filename.empty())
   {
      return descriptors;
   }

   ImportDescriptorResource pErrorDescriptor(filename, "RasterElement");
   { //scope the pTempFileDescriptor resource
      DataDescriptor* pErrorDataDescriptor = pErrorDescriptor->getDataDescriptor();
      FactoryResource<RasterFileDescriptor> pTempFileDescriptor;
      pTempFileDescriptor->setFilename(filename);
      pErrorDataDescriptor->setFileDescriptor(pTempFileDescriptor.get());
   }

   XmlReader xml(Service<MessageLogMgr>()->getLog(), false);
   DOMDocument* pDoc = xml.parse(filename);
   if (pDoc == NULL)
   {
      return descriptors;
   }
   DOMElement* pRoot = pDoc->getDocumentElement();
   if (pRoot == NULL)
   {
      return descriptors;
   }

   if (XMLString::equals(pRoot->getNodeName(), X("README")))
   {
      DOMNodeList* pElements = pRoot->getElementsByTagName(X("DGORDERNO"));
      DOMNodeList* pElements2 = pRoot->getElementsByTagName(X("DGORDERITEMNO"));
      if (pElements->getLength() == 1 && pElements->getLength() == 1)
      {
         mErrors.push_back("Overview files are not supported, please load the *.xml "
            "file that corresponds directly to an image.");
         descriptors.push_back(pErrorDescriptor.release());
         return descriptors;
      }
   }

   FactoryResource<DynamicObject> pImageMetadata = DgUtilities::parseMetadata(pDoc);
   if (pImageMetadata.get() == NULL || pImageMetadata->getNumAttributes() == 0)
   {
      mErrors.push_back("Unable to parse the file.");
      descriptors.push_back(pErrorDescriptor.release());
      return descriptors;
   }

   unsigned int height = 0;
   unsigned int width = 0;
   std::vector<DgFileTile> tiles = DgFileTile::getTiles(pDoc, filename, height, width);
   if (tiles.empty() || height == 0 || width == 0)
   {
      mErrors.push_back("Image tiles are missing from this file.");
      descriptors.push_back(pErrorDescriptor.release());
      return descriptors;
   }
   ImportDescriptorResource pImportDescriptor(filename, TypeConverter::toString<RasterElement>(), NULL, false);
   if (pImportDescriptor.get() == NULL)
   {
      return descriptors;
   }
   RasterDataDescriptor* pDescriptor = dynamic_cast<RasterDataDescriptor*>(pImportDescriptor->getDataDescriptor());
   if (pDescriptor == NULL)
   {
      return descriptors;
   }
   DynamicObject* pMetadata = pDescriptor->getMetadata();
   pMetadata->adoptiveMerge(pImageMetadata.get());
   {
      FactoryResource<RasterFileDescriptor> pTempFileDescriptorRes;
      pDescriptor->setFileDescriptor(pTempFileDescriptorRes.get());
   }
   RasterFileDescriptor* pFileDescriptor = dynamic_cast<RasterFileDescriptor*>(pDescriptor->getFileDescriptor());
   pFileDescriptor->setFilename(filename);

   std::string tileErrorMsg;
   if (!DgUtilities::verifyTiles(pMetadata, tiles, tileErrorMsg))
   {
      mErrors.push_back(tileErrorMsg);
      descriptors.push_back(pErrorDescriptor.release());
      return descriptors;
   }

   std::string tiffFile = tiles[0].mTilFilename;
   if (!DgUtilities::parseBasicsFromTiff(tiffFile, pDescriptor))
   {
      mErrors.push_back("Unable to parse basic information about image from tile file.");
      descriptors.push_back(pErrorDescriptor.release());
      return descriptors;
   }
   if (pDescriptor->getDataType() != INT1UBYTE && pDescriptor->getDataType() != INT2UBYTES)
   {
      mErrors.push_back("Improperly formatted tiff file.");
      descriptors.push_back(pErrorDescriptor.release());
      return descriptors;
   }

   int bitsPerPixel = StringUtilities::fromXmlString<int>(dv_cast<std::string>(pMetadata->getAttributeByPath(
      "DIGITALGLOBE_ISD/IMD/BITSPERPIXEL"), ""));
   if (bitsPerPixel != pFileDescriptor->getBitsPerElement())
   {
      mErrors.push_back("Reported bits per element and detected bits per element differ which forbids import.");
      descriptors.push_back(pErrorDescriptor.release());
      return descriptors;
   }

   std::string sensor = dv_cast<std::string>(pMetadata->getAttributeByPath("DIGITALGLOBE_ISD/IMD/IMAGE/SATID"), "");
   std::string product = dv_cast<std::string>(pMetadata->getAttributeByPath("DIGITALGLOBE_ISD/IMD/BANDID"), "");
   if (sensor != "QB02" && sensor != "WV01" && sensor != "WV02")
   {
      mWarnings.push_back("Unrecognized sensor \"" + sensor + 
         "\". Only the imagery can be loaded, no additional features of this importer will be supported.");
   }
   if (product != "P" && product != "Multi")
   {
      mWarnings.push_back("Unrecognized product \"" + product + 
         "\". Only the imagery can be loaded, no additional features of this importer will be supported.");
   }

   std::vector<DimensionDescriptor> rows = RasterUtilities::generateDimensionVector(height + 1, true, false, true);
   pDescriptor->setRows(rows);
   pFileDescriptor->setRows(rows);
   std::vector<DimensionDescriptor> columns = RasterUtilities::generateDimensionVector(width + 1, true, false, true);
   pDescriptor->setColumns(columns);
   pFileDescriptor->setColumns(columns);
   std::vector<int> badValues;
   badValues.push_back(0);
   pDescriptor->setBadValues(badValues);
   pDescriptor->setProcessingLocation(ON_DISK);

   //set any special metadata - including wavelengths
   DgUtilities::handleSpecialMetadata(pMetadata, pDescriptor->getBandCount());

   if (product == "Multi")
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

   //set corner coordinates
   std::list<GcpPoint> gcps = DgUtilities::parseGcps(tiles);
   if (gcps.empty())
   {
      mWarnings.push_back("Could not locate corner coordinates for image.");
   }
   pFileDescriptor->setGcps(gcps);

   //set rpcs if found
   DgUtilities::parseRpcs(pMetadata);
   std::vector<std::string> defaultImport;
   if (sensor == "QB02")
   {
      defaultImport = OptionsQB2Import::getSettingDefaultQB2Import();
   }
   else if (sensor == "WV02")
   {
      defaultImport = OptionsWV2Import::getSettingDefaultWV2Import();
   }
   else if (sensor == "WV01")
   {
      defaultImport.push_back("DN");
   }
   bool fallbackToDn = false;


   descriptors.push_back(pImportDescriptor.release()); 
   std::vector<double> radianceFactors = DgUtilities::determineConversionFactors(pMetadata,
      DgUtilities::DG_RADIANCE_DATA);
   bool validRadianceFactors = (!radianceFactors.empty());
   for (std::vector<double>::iterator radIter = radianceFactors.begin();
      radIter != radianceFactors.end();
      ++radIter)
   {
      if (*radIter < 0.0)
      {
         validRadianceFactors = false;
         break;
      }
   }
   bool shouldDefaultImportRadiance =
      std::find(defaultImport.begin(), defaultImport.end(), "Radiance") != defaultImport.end();
   if (validRadianceFactors)
   {
      //we have enough to create radiance import descriptor
      RasterDataDescriptor* pRadianceDescriptor = dynamic_cast<RasterDataDescriptor*>(
         pDescriptor->copy(pDescriptor->getName() + "-radiance", NULL));
      if (pRadianceDescriptor != NULL)
      {
         pRadianceDescriptor->setDataType(FLT4BYTES);
         pRadianceDescriptor->setValidDataTypes(std::vector<EncodingType>(1, pRadianceDescriptor->getDataType()));
         pRadianceDescriptor->setBadValues(std::vector<int>(1, -1.0));
         FactoryResource<Units> pUnits;
         pUnits->setUnitType(RADIANCE);
         pUnits->setUnitName("w/(m^2*sr*um)");
         pUnits->setScaleFromStandard(1.0);
         pRadianceDescriptor->setUnits(pUnits.get());
         FileDescriptor* pRadianceFileDescriptor = pRadianceDescriptor->getFileDescriptor();
         if (pRadianceFileDescriptor != NULL)
         {
            pRadianceFileDescriptor->setDatasetLocation("radiance");
            ImportDescriptorResource pRadianceImportDescriptor(pRadianceDescriptor, shouldDefaultImportRadiance);
            descriptors.push_back(pRadianceImportDescriptor.release());
         }
      }
   }
   else if (shouldDefaultImportRadiance)
   {
      fallbackToDn = true;
   }


   std::vector<double> reflectanceFactors = DgUtilities::determineConversionFactors(pMetadata,
      DgUtilities::DG_REFLECTANCE_DATA);
   bool validRelectanceFactors = (!reflectanceFactors.empty());
   for (std::vector<double>::iterator reflIter = reflectanceFactors.begin();
      reflIter != reflectanceFactors.end();
      ++reflIter)
   {
      if (*reflIter < 0.0)
      {
         validRelectanceFactors = false;
         break;
      }
   }
   bool shouldDefaultImportReflectance =
      std::find(defaultImport.begin(), defaultImport.end(), "Reflectance") != defaultImport.end();
   if (validRadianceFactors && validRelectanceFactors)
   {
      //we have enough to create reflectance import descriptor
      RasterDataDescriptor* pReflectanceDescriptor = dynamic_cast<RasterDataDescriptor*>(
         pDescriptor->copy(pDescriptor->getName() + "-reflectance", NULL));
      if (pReflectanceDescriptor != NULL)
      {
         pReflectanceDescriptor->setDataType(INT2UBYTES);
         pReflectanceDescriptor->setValidDataTypes(
            std::vector<EncodingType>(1, pReflectanceDescriptor->getDataType()));
         pReflectanceDescriptor->setBadValues(std::vector<int>(1, std::numeric_limits<unsigned short>::max()));
         FactoryResource<Units> pUnits;
         pUnits->setUnitType(REFLECTANCE);
         pUnits->setUnitName("Reflectance");
         pUnits->setScaleFromStandard(1/10000.0);
         pReflectanceDescriptor->setUnits(pUnits.get());
         FileDescriptor* pReflectanceFileDescriptor = pReflectanceDescriptor->getFileDescriptor();
         if (pReflectanceFileDescriptor != NULL)
         {
            pReflectanceFileDescriptor->setDatasetLocation("reflectance");
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

   if (fallbackToDn ||
      std::find(defaultImport.begin(), defaultImport.end(), "DN") != defaultImport.end())
   {
      pImportDescriptor->setImported(true);
   }

   return descriptors;
}

bool DgImporter::createRasterPager(RasterElement* pRaster) const
{
   VERIFY(pRaster != NULL);
   DataDescriptor *pDescriptor = pRaster->getDataDescriptor();
   VERIFY(pDescriptor != NULL);
   FileDescriptor *pFileDescriptor = pDescriptor->getFileDescriptor();
   VERIFY(pFileDescriptor != NULL);

   std::string filename = pRaster->getFilename();
   std::string datasetName = pFileDescriptor->getDatasetLocation();
   Progress* pProgress = getProgress();

   FactoryResource<Filename> pFilename;
   pFilename->setFullPathAndName(filename);

   ExecutableResource pagerPlugIn("DgFormats Raster Pager", std::string(), pProgress);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedElementArg(), pRaster);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedFilenameArg(), pFilename.get());

   bool success = pagerPlugIn->execute();

   RasterPager* pPager = dynamic_cast<RasterPager*>(pagerPlugIn->getPlugIn());
   if (!success || pPager == NULL)
   {
      std::string message = "Execution of DgFormats Raster Pager failed!";
      if (pProgress != NULL)
      {
         pProgress->updateProgress(message, 0, ERRORS);
      }
      return false;
   }

   pRaster->setPager(pPager);
   pagerPlugIn->releasePlugIn();

   return true;
}

int DgImporter::getValidationTest(const DataDescriptor* pDescriptor) const
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
   if (pRasterDesc->getProcessingLocation() == ON_DISK_READ_ONLY && pRasterFileDesc->getInterleaveFormat() == BSQ)
   {
      // Disabling these checks since the importer supports band subsets with BSQ for on-disk read-only
      validationTest &= ~(NO_BAND_SUBSETS);
   }
   return validationTest;
}

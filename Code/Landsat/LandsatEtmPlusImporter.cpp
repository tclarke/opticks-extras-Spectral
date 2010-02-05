/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "DateTime.h"
#include "DynamicObject.h"
#include "FileFinder.h"
#include "FileResource.h"
#include "ImportDescriptor.h"
#include "LandsatEtmPlusImporter.h"
#include "LandsatUtilities.h"
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

#include <boost/assign/std/list.hpp>
#include <boost/assign/std/vector.hpp>
#include <boost/format.hpp>

using namespace boost::assign;
using namespace std;

#define BAND6 5

REGISTER_PLUGIN_BASIC(SpectralLandsat, LandsatEtmPlusImporter);

LandsatEtmPlusImporter::LandsatEtmPlusImporter() : mNumRows(0), mNumCols(0), mB6Rows(0), mB6Cols(0), mB8Rows(0), mB8Cols(0)
{
   setDescriptorId("{60A6BDFD-14AE-47f2-80F9-17759087ED35}");
   setName("Landsat ETM+ Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setShortDescription("Landsat Enhanced Thematic Mapper");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setExtensions("Landsat ETM+ Header Files (*.hdr *htm.fst *HTM.FST *hrf.fst *HRF.FST *hpn.fst *HPN.FST)");

   initFieldLengths();
}

LandsatEtmPlusImporter::~LandsatEtmPlusImporter()
{
}

unsigned char LandsatEtmPlusImporter::getFileAffinity(const string& filename)
{
   return readHeader(filename) ? CAN_LOAD : CAN_NOT_LOAD;
}

vector<ImportDescriptor*> LandsatEtmPlusImporter::getImportDescriptors(const string& filename)
{
   vector<ImportDescriptor*> descriptors;
   if (filename.empty())
   {
      return descriptors;
   }
   if (!readHeader(filename))
   {
      return descriptors;
   }

   string lowGainDatasetName = filename + " Low Gain";
   string highGainDatasetName = filename + " High Gain";
   string panDatasetName = filename + " Panchromatic";

   if (!mFieldHRF.empty() && !mFieldHTM.empty())
   {
      // low gain
      RasterDataDescriptor* pDescriptor = RasterUtilities::generateRasterDataDescriptor(lowGainDatasetName, NULL,
         mNumRows, mNumCols, 7, BSQ, INT1UBYTE, IN_MEMORY);
      VERIFYRV(pDescriptor != NULL, descriptors);
      ImportDescriptorResource pLowGainImportDescriptor(pDescriptor);
      VERIFYRV(pLowGainImportDescriptor.get() != NULL, descriptors);

      RasterFileDescriptor* pFileDescriptor = static_cast<RasterFileDescriptor*>(
         RasterUtilities::generateAndSetFileDescriptor(pDescriptor, filename, "L", LITTLE_ENDIAN_ORDER));
      VERIFYRV(pFileDescriptor != NULL, descriptors);
      vector<int> badValues;
      badValues.push_back(0);
      pDescriptor->setBadValues(badValues);
      pFileDescriptor->setBandFiles(getBandFilenames(filename, LOW_GAIN));

      DynamicObject* pMetadata = pDescriptor->getMetadata();
      populateMetaData(pMetadata, pFileDescriptor, LOW_GAIN);

      pDescriptor->setDisplayMode(RGB_MODE);
      pDescriptor->setDisplayBand(GRAY, pDescriptor->getOriginalBand(0));
      pDescriptor->setDisplayBand(RED, pDescriptor->getOriginalBand(3));
      pDescriptor->setDisplayBand(GREEN, pDescriptor->getOriginalBand(2));
      pDescriptor->setDisplayBand(BLUE, pDescriptor->getOriginalBand(1));
      pDescriptor->getUnits()->setUnitType(DIGITAL_NO);

      descriptors.push_back(pLowGainImportDescriptor.release());

      // high gain
      pDescriptor = static_cast<RasterDataDescriptor*>(pDescriptor->copy(highGainDatasetName, NULL));
      VERIFYRV(pDescriptor != NULL, descriptors);
      ImportDescriptorResource pHighGainImportDescriptor(pDescriptor);
      VERIFYRV(pHighGainImportDescriptor.get() != NULL, descriptors);
      pHighGainImportDescriptor->setImported(false);
      pFileDescriptor = static_cast<RasterFileDescriptor*>(
         RasterUtilities::generateAndSetFileDescriptor(pDescriptor, filename, "H", LITTLE_ENDIAN_ORDER));
      VERIFYRV(pFileDescriptor != NULL, descriptors);
      pFileDescriptor->setBandFiles(getBandFilenames(filename, HIGH_GAIN));
      pMetadata = pDescriptor->getMetadata();
      populateMetaData(pMetadata, pFileDescriptor, HIGH_GAIN);

      descriptors.push_back(pHighGainImportDescriptor.release());
   }

   if (!mFieldHPN.empty())
   {
      // panchromatic
      RasterDataDescriptor* pDescriptor = RasterUtilities::generateRasterDataDescriptor(panDatasetName, NULL,
         mB8Rows, mB8Cols, 1, BSQ, INT1UBYTE, IN_MEMORY);
      VERIFYRV(pDescriptor != NULL, descriptors);
      ImportDescriptorResource pPanImportDescriptor(pDescriptor);
      VERIFYRV(pPanImportDescriptor.get() != NULL, descriptors);
      pPanImportDescriptor->setImported(false);

      RasterFileDescriptor* pFileDescriptor = static_cast<RasterFileDescriptor*>(
         RasterUtilities::generateAndSetFileDescriptor(pDescriptor, filename, "Pan", LITTLE_ENDIAN_ORDER));
      VERIFYRV(pFileDescriptor != NULL, descriptors);
      vector<int> badValues;
      badValues.push_back(0);
      pDescriptor->setBadValues(badValues);
      pFileDescriptor->setBandFiles(getBandFilenames(filename, PANCHROMATIC));
      DynamicObject* pMetadata = pDescriptor->getMetadata();
      populateMetaData(pMetadata, pFileDescriptor, PANCHROMATIC);

      pDescriptor->setDisplayMode(GRAYSCALE_MODE);
      pDescriptor->setDisplayBand(GRAY, pDescriptor->getOriginalBand(0));
      pDescriptor->getUnits()->setUnitType(DIGITAL_NO);

      descriptors.push_back(pPanImportDescriptor.release());
   }

   return descriptors;
}

bool LandsatEtmPlusImporter::createRasterPager(RasterElement* pRaster) const
{
   string srcFile = pRaster->getFilename();
   if (srcFile.empty())
   {
      return false;
   }
   {
      //scoping to ensure the file is closed before creating the pager
      LargeFileResource srcFileRes;
      if (!srcFileRes.open(srcFile, O_RDONLY | O_BINARY, S_IREAD))
      {
         return false;
      }
   }
   // create the pager
   ExecutableResource pPager("BandResamplePager");
   VERIFY(pPager->getPlugIn() != NULL);
   bool isWritable = false;
   bool useDataDescriptor = false;
   FactoryResource<Filename> pFilename;
   VERIFY(pFilename.get() != NULL);
   pFilename->setFullPathAndName(pRaster->getFilename());

   pPager->getInArgList().setPlugInArgValue("Raster Element", pRaster);
   pPager->getInArgList().setPlugInArgValue("Filename", pFilename.get());
   pPager->getInArgList().setPlugInArgValue("isWritable", &isWritable);
   pPager->getInArgList().setPlugInArgValue("Use Data Descriptor", &useDataDescriptor);
   RasterDataDescriptor* pDescriptor = static_cast<RasterDataDescriptor*>(pRaster->getDataDescriptor());
   VERIFY(pDescriptor != NULL);
   RasterFileDescriptor* pFileDescriptor = static_cast<RasterFileDescriptor*>(pDescriptor->getFileDescriptor());
   VERIFY(pFileDescriptor != NULL);
   unsigned int band = 5, rows = mB6Rows, cols = mB6Cols;
   pPager->getInArgList().setPlugInArgValue("Band", &band);
   pPager->getInArgList().setPlugInArgValue("Rows", &rows);
   pPager->getInArgList().setPlugInArgValue("Columns", &cols);
   if (!pPager->execute())
   {
      return false;
   }
   RasterPager* pRasterPager = dynamic_cast<RasterPager*>(pPager->getPlugIn());
   if (pRasterPager != NULL)
   {
      pPager->releasePlugIn();
   }
   else
   {
      return false;
   }

   pRaster->setPager(pRasterPager);
   return true;
}

void LandsatEtmPlusImporter::populateMetaData(DynamicObject* pMetadata, RasterFileDescriptor* pFileDescriptor, BandSetType bandSet)
{
   vector<string> &field = (bandSet == PANCHROMATIC) ? mFieldHPN : mFieldHRF;

   if (bandSet != PANCHROMATIC)
   {
      vector<double> startWavelengths;
      startWavelengths += 0.45, 0.525, 0.63, 0.75, 1.55, 10.4, 2.09;
      pMetadata->setAttributeByPath(START_WAVELENGTHS_METADATA_PATH, startWavelengths);

      vector<double> centerWavelengths;
      centerWavelengths += 0.483, 0.565, 0.66, 0.825, 1.65, 11.45, 2.22;
      pMetadata->setAttributeByPath(CENTER_WAVELENGTHS_METADATA_PATH, centerWavelengths);

      vector<double> endWavelengths;
      endWavelengths += 0.515, 0.605, 0.69, 0.9, 1.75, 12.5, 2.35;
      pMetadata->setAttributeByPath(END_WAVELENGTHS_METADATA_PATH, endWavelengths);
   }

   pMetadata->setAttribute("Product", field[PRODUCT]);
   pMetadata->setAttribute("LOC", field[LOC]);
   pMetadata->setAttribute("Satellite", field[SATELLITE]);
   pMetadata->setAttribute("Sensor", field[INSTRUMENT]);
   pMetadata->setAttribute("Sensor Mode", field[SENSOR_MODE]);
   pMetadata->setAttribute("Orientation Angle", StringUtilities::fromDisplayString<double>(field[LOOK_ANGLE]));
   pMetadata->setAttribute("Location", field[LOCATION]);
   pMetadata->setAttribute("Product Type", field[PRODUCT_TYPE]);
   pMetadata->setAttribute("Product Size", field[PRODUCT_SIZE]);
   pMetadata->setAttribute("Geodetic Processing", field[TYPE_OF_PROCESSING]);
   pMetadata->setAttribute("Resampling", field[RESAMPLING]);
   pMetadata->setAttribute("Volume Number/Number in Set", field[VOLUME_NUMBER]);
   pMetadata->setAttribute("Start Line", StringUtilities::fromDisplayString<unsigned int>(field[START_LINE]));
   pMetadata->setAttribute("Blocking Factor", StringUtilities::fromDisplayString<unsigned int>(field[BLOCKING_FACTOR]));
   pMetadata->setAttribute("Record Length", StringUtilities::fromDisplayString<unsigned int>(field[REC_SIZE]));
   pMetadata->setAttribute("Output Bits per Pixel", StringUtilities::fromDisplayString<unsigned int>(field[OUTPUT_BITS_PER_PIXEL]));
   pMetadata->setAttribute("Acquired Bits per Pixel", StringUtilities::fromDisplayString<unsigned int>(field[ACQUIRED_BITS_PER_PIXEL]));
   vector<double> gain;
   vector<double> bias;
   gain += StringUtilities::fromDisplayString<double>(field[GAIN_1]);
   bias += StringUtilities::fromDisplayString<double>(field[BIAS_1]);
   if (bandSet != PANCHROMATIC)
   {
      gain += StringUtilities::fromDisplayString<double>(field[GAIN_2]),
              StringUtilities::fromDisplayString<double>(field[GAIN_3]),
              StringUtilities::fromDisplayString<double>(field[GAIN_4]),
              StringUtilities::fromDisplayString<double>(field[GAIN_5]),
              StringUtilities::fromDisplayString<double>(mFieldHTM[(bandSet == LOW_GAIN) ? GAIN_1 : GAIN_2]),
              StringUtilities::fromDisplayString<double>(field[GAIN_6]);
      bias += StringUtilities::fromDisplayString<double>(field[BIAS_2]),
              StringUtilities::fromDisplayString<double>(field[BIAS_3]),
              StringUtilities::fromDisplayString<double>(field[BIAS_4]),
              StringUtilities::fromDisplayString<double>(field[BIAS_5]),
              StringUtilities::fromDisplayString<double>(mFieldHTM[(bandSet == LOW_GAIN) ? BIAS_1 : BIAS_2]),
              StringUtilities::fromDisplayString<double>(field[BIAS_6]);
   }
   pMetadata->setAttributeByPath("Radiance Adjust/Gain", gain);
   pMetadata->setAttributeByPath("Radiance Adjust/Bias", bias);
   pMetadata->setAttribute("Geometric Data Map Projection", field[GEOMETRIC_MAP_PROJECTION]);
   pMetadata->setAttribute("Earth Ellipsoid", field[ELLIPSOID]);
   pMetadata->setAttribute("Datum", field[DATUM]);
   vector<double> projParam;
   projParam += StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_1]),
                StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_2]),
                StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_3]),
                StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_4]),
                StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_5]),
                StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_6]),
                StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_7]),
                StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_8]),
                StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_9]),
                StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_10]),
                StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_11]),
                StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_12]),
                StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_13]),
                StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_14]),
                StringUtilities::fromDisplayString<double>(field[USGS_PROJECTION_PARAMETER_15]);
   pMetadata->setAttribute("USGS Projection Parameters", projParam);
   pMetadata->setAttribute("USGS Map Zone", StringUtilities::fromDisplayString<int>(field[USGS_MAP_ZONE]));
   pMetadata->setAttribute("UL Easting", StringUtilities::fromDisplayString<double>(field[UL_EASTING]));
   pMetadata->setAttribute("UL Northing", StringUtilities::fromDisplayString<double>(field[UL_NORTHING]));
   pMetadata->setAttribute("UR Easting", StringUtilities::fromDisplayString<double>(field[UR_EASTING]));
   pMetadata->setAttribute("UR Northing", StringUtilities::fromDisplayString<double>(field[UR_NORTHING]));
   pMetadata->setAttribute("LR Easting", StringUtilities::fromDisplayString<double>(field[LR_EASTING]));
   pMetadata->setAttribute("LR Northing", StringUtilities::fromDisplayString<double>(field[LR_NORTHING]));
   pMetadata->setAttribute("LL Easting", StringUtilities::fromDisplayString<double>(field[LL_EASTING]));
   pMetadata->setAttribute("LL Northing", StringUtilities::fromDisplayString<double>(field[LL_NORTHING]));
   pMetadata->setAttribute("Center Easting", StringUtilities::fromDisplayString<double>(field[CENTER_EASTING]));
   pMetadata->setAttribute("Center Northing", StringUtilities::fromDisplayString<double>(field[CENTER_NORTHING]));
   pMetadata->setAttribute("Scene Center Pixel Number", StringUtilities::fromDisplayString<int>(field[SCENE_CENTER_PIXEL_NUMBER]));
   pMetadata->setAttribute("Scene Center Line Number", StringUtilities::fromDisplayString<int>(field[SCENE_CENTER_LINE_NUMBER]));
   pMetadata->setAttribute("Offset", StringUtilities::fromDisplayString<int>(field[OFFSET]));
   pMetadata->setAttribute("Orientation Angle", StringUtilities::fromDisplayString<double>(field[ORIENTATION]));
   if (field[INSTRUMENT].substr(0, 2) == "TM")
   {
      pMetadata->setAttribute("Sensor Name", string("LandSat ETM+"));
      pMetadata->setAttribute("Change Detection Format", string("ETM"));
   }
   int yyyy = StringUtilities::fromDisplayString<int>(field[ACQUISITION_DATE].substr(0, 4));
   int mm = StringUtilities::fromDisplayString<int>(field[ACQUISITION_DATE].substr(4, 2));
   int dd = StringUtilities::fromDisplayString<int>(field[ACQUISITION_DATE].substr(6));
   FactoryResource<DateTime> collectionDate;
   VERIFYNRV(collectionDate.get() != NULL);
   collectionDate->set(yyyy, mm, dd);
   pMetadata->setAttributeByPath(COLLECTION_DATE_TIME_METADATA_PATH, *collectionDate.get());
   pMetadata->setAttribute("Sun Elevation", StringUtilities::fromDisplayString<double>(field[SUN_ELEVATION]));
   pMetadata->setAttribute("Sun Azimuth", StringUtilities::fromDisplayString<double>(field[SUN_AZIMUTH]));

   GcpPoint ul;
   ul.mPixel = LocationType(0,0);
   ul.mCoordinate = LocationType(Landsat::LatLongConvert(field[UL_LATITUDE]), Landsat::LatLongConvert(field[UL_LONGITUDE]));
   GcpPoint ur;
   ur.mPixel = LocationType(mNumCols-1,0);
   ur.mCoordinate = LocationType(Landsat::LatLongConvert(field[UR_LATITUDE]), Landsat::LatLongConvert(field[UR_LONGITUDE]));
   GcpPoint lr;
   lr.mPixel = LocationType(mNumCols-1,mNumRows-1);
   lr.mCoordinate = LocationType(Landsat::LatLongConvert(field[LR_LATITUDE]), Landsat::LatLongConvert(field[LR_LONGITUDE]));
   GcpPoint ll;
   ll.mPixel = LocationType(0,mNumRows-1);
   ll.mCoordinate = LocationType(Landsat::LatLongConvert(field[LL_LATITUDE]), Landsat::LatLongConvert(field[LL_LONGITUDE]));
   GcpPoint center;
   center.mPixel = LocationType(mNumCols/2,mNumRows/2);
   center.mCoordinate = LocationType(Landsat::LatLongConvert(field[CENTER_LATITUDE]), Landsat::LatLongConvert(field[CENTER_LONGITUDE]));
   list<GcpPoint> gcps;
   gcps += ul,ur,lr,ll,center;
   pFileDescriptor->setGcps(gcps);
}

void LandsatEtmPlusImporter::initFieldLengths()
{ 
   mFieldLen.push_back(8);    //0 - "REQ ID ="
   mFieldLen.push_back(20);   //1 - product data
   mFieldLen.push_back(6);    //2 - " LOC ='
   mFieldLen.push_back(17);   //3 - 
   mFieldLen.push_back(19);   //4 - " ACQUISITION DATE ="
   mFieldLen.push_back(8);    //5 - date in 'yyyymmdd'format
   mFieldLen.push_back(11);   //6 - "SATELLITE ="
   mFieldLen.push_back(10);   //7 - satellite number: 'LANDSAT7  '
   mFieldLen.push_back(9);    //8 - " SENSOR ="
   mFieldLen.push_back(10);   //9 - instrument type: 'ETM+      ' 
   mFieldLen.push_back(14);   //10 - " SENSOR MODE ="
   mFieldLen.push_back(6);    //11 - sensor mode: 'NORMAL'
   mFieldLen.push_back(13);   //12 - " LOOK ANGLE ="
   mFieldLen.push_back(5);    //13 - look angle
   mFieldLen.push_back(24);   //14 - spaces
   mFieldLen.push_back(10);   //15 - "LOCATION ="
   mFieldLen.push_back(17);   //16 - 
   mFieldLen.push_back(19);   //17 - " ACQUISITION DATE ="
   mFieldLen.push_back(8);    //18 - date in 'yyyymmdd'format
   mFieldLen.push_back(11);   //19 - "SATELLITE ="
   mFieldLen.push_back(10);   //20 -  
   mFieldLen.push_back(9);    //21 - " SENSOR ="
   mFieldLen.push_back(10);   //22 - 
   mFieldLen.push_back(14);   //23 - " SENSOR MODE ="
   mFieldLen.push_back(6);    //24 - 
   mFieldLen.push_back(13);   //25 - " LOOK ANGLE ="
   mFieldLen.push_back(5);    //26
   mFieldLen.push_back(24);   //27 - spaces
   mFieldLen.push_back(10);   //28 - "LOCATION ="
   mFieldLen.push_back(17);   //29 - 
   mFieldLen.push_back(19);   //30 - " ACQUISITION DATE ="
   mFieldLen.push_back(8);    //31
   mFieldLen.push_back(11);   //32 - "SATELLITE ="
   mFieldLen.push_back(10);   //33 -  
   mFieldLen.push_back(9);    //34 - " SENSOR ="
   mFieldLen.push_back(10);   //35 - 
   mFieldLen.push_back(14);   //36 - " SENSOR MODE ="
   mFieldLen.push_back(6);    //37 - 
   mFieldLen.push_back(13);   //38 - " LOOK ANGLE ="
   mFieldLen.push_back(5);    //39 - 
   mFieldLen.push_back(24);   //40 - spaces
   mFieldLen.push_back(10);   //41 - "LOCATION ="
   mFieldLen.push_back(17);   //42 - 
   mFieldLen.push_back(19);   //43 - " ACQUISITION DATE ="
   mFieldLen.push_back(8);    //44 - 
   mFieldLen.push_back(11);   //45 - "SATELLITE ="
   mFieldLen.push_back(10);   //46 - 
   mFieldLen.push_back(9);    //47 - " SENSOR ="
   mFieldLen.push_back(10);   //48 - 
   mFieldLen.push_back(14);   //49 - " SENSOR MODE ="
   mFieldLen.push_back(6);    //50 - 
   mFieldLen.push_back(13);   //51 - " LOOK ANGLE ="
   mFieldLen.push_back(5);    //52 - 
   mFieldLen.push_back(14);   //53 - "PRODUCT TYPE ="
   mFieldLen.push_back(18);   //54 - "MAP ORIENTED      "
   mFieldLen.push_back(15);   //55 - " PRODUCT SIZE ="
   mFieldLen.push_back(31);   //56 - "FULL SCENE"
   mFieldLen.push_back(20);   //57 - "TYPE OF PROCESSING ="
   mFieldLen.push_back(11);   //58 - "SYSTEMATIC "
   mFieldLen.push_back(13);   //59 - " RESAMPLING ="
   mFieldLen.push_back(2);    //60 - "NN"
   mFieldLen.push_back(33);   //61 - blank
   mFieldLen.push_back(19);   //62 - "VOLUME #/# IN SET ="
   mFieldLen.push_back(5);    //63 - "01/01"
   mFieldLen.push_back(18);   //64 - " PIXELS PER LINE ="
   mFieldLen.push_back(5);    //65 - pixels per row
   mFieldLen.push_back(17);   //66 - " LINES PER BAND ="
   mFieldLen.push_back(5);    //67 - number of lines
   mFieldLen.push_back(14);   //68 - "START LINE # ="
   mFieldLen.push_back(5);    //69 - 
   mFieldLen.push_back(18);   //70 - " BLOCKING FACTOR ="
   mFieldLen.push_back(2);    //71 - 
   mFieldLen.push_back(12);   //72 - " REC SIZE  ="
   mFieldLen.push_back(8);    //73 - 8 ch int number
   mFieldLen.push_back(14);   //74 - "  PIXEL SIZE ="
   mFieldLen.push_back(5);    //75 - 
   mFieldLen.push_back(23);   //76 - "OUTPUT BITS PER PIXEL ="
   mFieldLen.push_back(2);    //77 - 
   mFieldLen.push_back(26);   //78 - " ACQUIRED BITS PER PIXEL ="
   mFieldLen.push_back(2);    //79 - 
   mFieldLen.push_back(20);   //80 - spaces
   mFieldLen.push_back(15);   //81 - "BANDS PRESENT ="
   mFieldLen.push_back(6);    //82 = 123457 - if HRF; band 6 is in the htm header
   mFieldLen.push_back(50);   //83 - spaces
   mFieldLen.push_back(10);   //84 - "FILENAME ="
   mFieldLen.push_back(29);   //85 - 
   mFieldLen.push_back(10);   //86 - "FILENAME ="
   mFieldLen.push_back(29);   //87 - 
   mFieldLen.push_back(10);   //88 - "FILENAME ="
   mFieldLen.push_back(29);   //89 - 
   mFieldLen.push_back(10);   //90 - "FILENAME ="
   mFieldLen.push_back(29);   //91 - 
   mFieldLen.push_back(10);   //92 - "FILENAME ="
   mFieldLen.push_back(29);   //93 - 
   mFieldLen.push_back(10);   //94 - "FILENAME ="
   mFieldLen.push_back(29);   //95 - 
   mFieldLen.push_back(80);   //96 - spaces
   mFieldLen.push_back(80);   //97 - spaces
   mFieldLen.push_back(12);   //98 - REV         "
   mFieldLen.push_back(3);    //99 - rev number
   mFieldLen.push_back(75);   //100 - "GAINS AND BIASES IN ASCENDING BAND NUMBER ORDER"
   mFieldLen.push_back(18);   //101 - gains
   mFieldLen.push_back(6);    //102 - spaces
   mFieldLen.push_back(18);   //103 - biases
   mFieldLen.push_back(35);   //104 - spaces
   mFieldLen.push_back(18);   //105 - gains
   mFieldLen.push_back(6);    //106 - spaces
   mFieldLen.push_back(18);   //107 - biases
   mFieldLen.push_back(35);   //108 - spaces
   mFieldLen.push_back(18);   //109 - gains
   mFieldLen.push_back(6);    //110 - spaces
   mFieldLen.push_back(18);   //111 - biases
   mFieldLen.push_back(35);   //112 - spaces
   mFieldLen.push_back(18);   //113 - gains
   mFieldLen.push_back(6);    //114 - spaces
   mFieldLen.push_back(18);   //115 - biases
   mFieldLen.push_back(35);   //116 - spaces
   mFieldLen.push_back(18);   //117 - gains
   mFieldLen.push_back(6);    //118 - spaces
   mFieldLen.push_back(18);   //119 - biases
   mFieldLen.push_back(35);   //120 - spaces
   mFieldLen.push_back(18);   //121 - gains
   mFieldLen.push_back(6);    //122 - spaces
   mFieldLen.push_back(18);   //123 - biases
   mFieldLen.push_back(35);   //124 - spaces
   mFieldLen.push_back(80);   //125 - spaces
   mFieldLen.push_back(80);   //126 - spaces
   mFieldLen.push_back(80);   //127 - spaces
   mFieldLen.push_back(80);   //128 - spaces
   mFieldLen.push_back(80);   //129 - spaces
   mFieldLen.push_back(80);   //130 - spaces
   mFieldLen.push_back(80);   //131 - spaces
   mFieldLen.push_back(80);   //132 - spaces
   mFieldLen.push_back(80);   //133 - spaces
   mFieldLen.push_back(80);   //134 - spaces
   mFieldLen.push_back(80);   //135 - spaces
   mFieldLen.push_back(80);   //136 - spaces
   mFieldLen.push_back(16);   //137 - spaces
   mFieldLen.push_back(31);   //138 - "GEOMETRIC DATA MAP PROJECTION ="
   mFieldLen.push_back(4);    //139 - UTM
   mFieldLen.push_back(12);   //140 - " ELLIPSOID ="
   mFieldLen.push_back(18);   //141 - "WGS84             "
   mFieldLen.push_back(8);    //142 - " DATUM ="
   mFieldLen.push_back(6);    //143 - "WGS84 "
   mFieldLen.push_back(29);   //144 - "USGS PROJECTION PARAMETERS = "
   mFieldLen.push_back(17);   //145 - "0.000000000000000"   param1
   mFieldLen.push_back(8);    //146 - spaces"       "
   mFieldLen.push_back(17);   //147 - "0.000000000000000"   param2
   mFieldLen.push_back(17);   //148 - "0.000000000000000"   param3
   mFieldLen.push_back(8);    //149 - spaces
   mFieldLen.push_back(17);   //150 - "0.000000000000000"   param4
   mFieldLen.push_back(8);    //151 - spaces"       "
   mFieldLen.push_back(17);   //152 - "0.000000000000000"   param5
   mFieldLen.push_back(17);   //153 - "0.000000000000000"   param6
   mFieldLen.push_back(8);    //154 - spaces
   mFieldLen.push_back(17);   //155 - "0.000000000000000"   param7
   mFieldLen.push_back(8);    //156 - spaces"       "
   mFieldLen.push_back(17);   //157 - "0.000000000000000"   param8
   mFieldLen.push_back(17);   //158 - "0.000000000000000"   param9
   mFieldLen.push_back(8);    //159 - spaces
   mFieldLen.push_back(17);   //160 - "0.000000000000000"   param10
   mFieldLen.push_back(8);    //161 - spaces"       "
   mFieldLen.push_back(17);   //162 - "0.000000000000000"   param11
   mFieldLen.push_back(17);   //163 - "0.000000000000000"   param12
   mFieldLen.push_back(8);    //164 - spaces
   mFieldLen.push_back(17);   //165 - "0.000000000000000"   param13
   mFieldLen.push_back(8);    //166 - spaces"       "
   mFieldLen.push_back(17);   //167 - "0.000000000000000"   param14
   mFieldLen.push_back(17);   //168 - "0.000000000000000"   param15
   mFieldLen.push_back(7);    //169 - spaces
   mFieldLen.push_back(16);   //170 - " USGS MAP ZONE ="
   mFieldLen.push_back(2);    //171 - 16
   mFieldLen.push_back(37);   //172 - spaces
   mFieldLen.push_back(5);    //173 - "UL = "
   mFieldLen.push_back(13);   //174 - geodetic longitude of upper left corner of image.
   mFieldLen.push_back(1);    //175 - blank
   mFieldLen.push_back(12);   //176 - geodetic latitude of upper left corner of image.
   mFieldLen.push_back(1);    //177 - blank
   mFieldLen.push_back(13);   //178 - easting of upper left corner of image in meters X.
   mFieldLen.push_back(1);    //179 - blank
   mFieldLen.push_back(13);   //180 - northing of upper left corner of image in meters Y.
   mFieldLen.push_back(20);   //181 - spaces
   mFieldLen.push_back(5);    //182 - UR = "
   mFieldLen.push_back(13);   //183 - geodetic longitude of upper right corner of image.
   mFieldLen.push_back(1);    //184 - blank
   mFieldLen.push_back(12);   //185 - geodetic latitude of upper right corner of image.
   mFieldLen.push_back(1);    //186 - blank
   mFieldLen.push_back(13);   //187 - easting of upper right corner of image in meters X.
   mFieldLen.push_back(1);    //188 - blank
   mFieldLen.push_back(13);   //189 - northing of upper right corner of image in meters Y.
   mFieldLen.push_back(20);   //190 - spaces
   mFieldLen.push_back(5);    //191 - "LR = "
   mFieldLen.push_back(13);   //192 - geodetic longitude of lower right corner of image.
   mFieldLen.push_back(1);    //193 - blank
   mFieldLen.push_back(12);   //194 - geodetic latitude of lower right corner of image.
   mFieldLen.push_back(1);    //195 - blank
   mFieldLen.push_back(13);   //196 - easting of lower right corner of image in meters X.
   mFieldLen.push_back(1);    //197 - blank
   mFieldLen.push_back(13);   //198 - northing of lower right corner of image in meters Y.
   mFieldLen.push_back(20);   //199 - spaces
   mFieldLen.push_back(5);    //200 - "LL = "
   mFieldLen.push_back(13);   //201 - geodetic longitude of lower left corner of image.
   mFieldLen.push_back(1);    //202 - blank
   mFieldLen.push_back(12);   //203 - geodetic latitude of lower left corner of image.
   mFieldLen.push_back(1);    //204 - blank
   mFieldLen.push_back(13);   //205 - easting of lower left corner of image in meters X.
   mFieldLen.push_back(1);    //206 - blank
   mFieldLen.push_back(13);   //207 - northing of lower left corner of image in meters Y.
   mFieldLen.push_back(20);   //208 - spaces
   mFieldLen.push_back(9);    //209 - "CENTER = "
   mFieldLen.push_back(13);   //210 - longitude 
   mFieldLen.push_back(1);    //211 - blank
   mFieldLen.push_back(12);   //212 - latitude
   mFieldLen.push_back(1);    //213 - blank
   mFieldLen.push_back(13);   //214 - easting of lower left corner of image in meters X.
   mFieldLen.push_back(1);    //215 - blank
   mFieldLen.push_back(13);   //216 - northing of lower left corner of image in meters Y.
   mFieldLen.push_back(1);    //217 - spaces
   mFieldLen.push_back(5);    //218 - easting of lower left corner of image in meters X.
   mFieldLen.push_back(1);    //219 - blank
   mFieldLen.push_back(5);    //220 - northing of lower left corner of image in meters Y.
   mFieldLen.push_back(4);    //221 - spaces
   mFieldLen.push_back(8);    //222 - "OFFSET ="
   mFieldLen.push_back(6);    //223 - offset
   mFieldLen.push_back(20);   //224 - " ORIENTATION ANGLE ="
   mFieldLen.push_back(5);    //225 - orientation angle
   mFieldLen.push_back(38);   //226 - spaces
   mFieldLen.push_back(21);   //227 - "SUN ELEVATION ANGLE ="
   mFieldLen.push_back(4);    //228 - sun elevation angle
   mFieldLen.push_back(20);   //229 - " SUN AZIMUTH ANGLE ="
   mFieldLen.push_back(5);    //230 - sun azimuth angle
}

bool LandsatEtmPlusImporter::readHeader(const string& strInFstHeaderFileName)
{
   if (strInFstHeaderFileName.empty())
   {
      return false;
   }

   //The HTM header file contains information for Band 6 Channel 1 and 2. 
   //This set extracts the info needed to process HTM(band 6) header
   mFieldHTM.clear();
   string baseFileName = strInFstHeaderFileName.substr(0, strInFstHeaderFileName.length() - 7);
   FactoryResource<Filename> filename;
   filename->setFullPathAndName(baseFileName);
   FactoryResource<FileFinder> fileFinder;
   string htmFileName = filename->getFileName() + "HTM.FST";
   if (fileFinder->findFile(filename->getPath(), htmFileName))
   {
      fileFinder->findNextFile();
      fileFinder->getFullPath(htmFileName);
   }

   LargeFileResource htmFile;
   if (htmFile.open(htmFileName, O_RDONLY, S_IREAD))
   {
      //process HTM header
      vector<char> buf(5120);
      size_t count = static_cast<size_t>(htmFile.read(&buf.front(), 5120));
      VERIFY(htmFile.eof());
      string htmHeaderFull(&buf.front(), count);
      vector<string> htmHeaderLines = StringUtilities::split(htmHeaderFull, '\n');

      if (!parseHeader(htmHeaderLines, mFieldHTM)) //parse band 6 header
      {
         mFieldHTM.clear();
      }
   }
   htmFile.close();

   // The HRF Header file contains the parameters for Bands 1-5 and 7.  
   mFieldHRF.clear();
   string hrfFileName = filename->getFileName() + "HRF.FST";
   if (fileFinder->findFile(filename->getPath(), hrfFileName))
   {
      fileFinder->findNextFile();
      fileFinder->getFullPath(hrfFileName);
   }

   LargeFileResource hrfFile;
   if (hrfFile.open(hrfFileName, O_RDONLY, S_IREAD))
   {
      //process HRF header
      vector<char> buf(5120);
      size_t count = static_cast<size_t>(hrfFile.read(&buf.front(), 5120));
      VERIFY(hrfFile.eof());
      string hrfHeaderFull(&buf.front(), count);
      vector<string> hrfHeaderLines = StringUtilities::split(hrfHeaderFull, '\n');

      if (!parseHeader(hrfHeaderLines, mFieldHRF)) //parse bands 1-5 and 7 header
      {
         mFieldHRF.clear();
      }
   }
   hrfFile.close();

   // The HPN Header file contains the parameters for Band 8.
   mFieldHPN.clear();
   string hpnFileName = filename->getFileName() + "HPN.FST";
   if (fileFinder->findFile(filename->getPath(), hpnFileName))
   {
      fileFinder->findNextFile();
      fileFinder->getFullPath(hpnFileName);
   }

   LargeFileResource hpnFile;
   if (hpnFile.open(hpnFileName, O_RDONLY, S_IREAD))
   {
      //process HPN header
      vector<char> buf(5120);
      size_t count = static_cast<size_t>(hpnFile.read(&buf.front(), 5120));
      VERIFY(hpnFile.eof());
      string hpnHeaderFull(&buf.front(), count);
      vector<string> hpnHeaderLines = StringUtilities::split(hpnHeaderFull, '\n');

      if (!parseHeader(hpnHeaderLines, mFieldHPN)) //parse band 8 header
      {
         mFieldHPN.clear();
      }
   }
   hpnFile.close();

   return (!mFieldHRF.empty() || !mFieldHTM.empty() || !mFieldHPN.empty());
}

bool LandsatEtmPlusImporter::parseHeader(const vector<string>& header, vector<string>& field)
{ 
   int field_ctr = 0;
   vector<string>::const_iterator headerIndex = header.begin();
   size_t lineIdx = 0;
   size_t iFieldLength = 0;
   int iNumFields = 0;
   string headerLine = *(headerIndex++);
   
   iNumFields = mFieldLen.size();

   for (field_ctr = 0; headerIndex != header.end() && field_ctr < iNumFields; field_ctr++)
   {
      iFieldLength = mFieldLen[field_ctr];
      if (lineIdx + iFieldLength > headerLine.size())
      {
         headerLine = *(headerIndex++);
         lineIdx = 0;
      }

      field.push_back(StringUtilities::stripWhitespace(headerLine.substr(lineIdx, iFieldLength)));
      if ((field_ctr + 1) < iNumFields)
      {
         int tempLen = mFieldLen[field_ctr + 1];
         lineIdx = lineIdx + mFieldLen[field_ctr];
      }
   }

   //Set the extracted rows and cols in the main class.
   bool error;
   string bandsUsed = field[BANDS_PRESENT];
   if (bandsUsed == "LH") // thermal
   {
      mB6Rows = StringUtilities::fromDisplayString<unsigned int>(field[LINES_PER_BAND], &error);
      mB6Cols = error ? 0 : StringUtilities::fromDisplayString<unsigned int>(field[PIXELS_PER_LINE], &error);
   }
   else if (bandsUsed == "123457") // vis
   {
      mNumRows = StringUtilities::fromDisplayString<unsigned int>(field[LINES_PER_BAND], &error);
      mNumCols = error ? 0 : StringUtilities::fromDisplayString<unsigned int>(field[PIXELS_PER_LINE], &error);
   }
   else if (bandsUsed == "8") // pan
   {
      mB8Rows = StringUtilities::fromDisplayString<unsigned int>(field[LINES_PER_BAND], &error);
      mB8Cols = error ? 0 : StringUtilities::fromDisplayString<unsigned int>(field[PIXELS_PER_LINE], &error);
   }
   else // unsupported band set or ordering
   {
      return false;
   }

   return true; 
}

vector<string> LandsatEtmPlusImporter::getBandFilenames(std::string strInHeaderFileName, LandsatEtmPlusImporter::BandSetType bandSet) const
{
   vector<string> bandFilenames;
   if (strInHeaderFileName.empty())
   {
      return bandFilenames;
   }
   FactoryResource<Filename> headerFileName;
   VERIFYRV(headerFileName.get() != NULL, bandFilenames);
   headerFileName->setFullPathAndName(strInHeaderFileName);
   string bandFilePath = headerFileName->getPath();
   
   // get the requested band filenames
   FactoryResource<FileFinder> fileFinder;
   if (bandSet == PANCHROMATIC)
   {
      if (!fileFinder->findFile(bandFilePath, mFieldHPN[FILENAME_1]))
      {
         return vector<string>();
      }
      fileFinder->findNextFile();
      string path;
      fileFinder->getFullPath(path);
      bandFilenames += path;
   }
   else
   {
      vector<string> fileNames;
      fileNames += mFieldHRF[FILENAME_1],
                   mFieldHRF[FILENAME_2],
                   mFieldHRF[FILENAME_3],
                   mFieldHRF[FILENAME_4],
                   mFieldHRF[FILENAME_5],
                   ((bandSet == LOW_GAIN) ? mFieldHTM[FILENAME_1] : mFieldHTM[FILENAME_2]),
                   mFieldHRF[FILENAME_6];
      for (vector<string>::const_iterator fileName = fileNames.begin(); fileName != fileNames.end(); ++fileName)
      {
         if (!fileFinder->findFile(bandFilePath, *fileName))
         {
            return vector<string>();
         }
         fileFinder->findNextFile();
         string path;
         fileFinder->getFullPath(path);
         bandFilenames += path;
      }
   }
   return bandFilenames;
}

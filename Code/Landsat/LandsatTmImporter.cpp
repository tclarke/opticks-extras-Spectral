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
#include "LandsatTmImporter.h"
#include "LandsatUtilities.h"
#include "PlugInRegistration.h"
#include "RasterDataDescriptor.h"
#include "RasterFileDescriptor.h"
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

REGISTER_PLUGIN_BASIC(SpectralLandsat, LandsatTmImporter);

LandsatTmImporter::LandsatTmImporter() : mNumRows(0), mNumCols(0), mNumBands(0)
{
   setDescriptorId("{F7F3197D-1D1F-40b9-92F4-D4E5214248BD}");
   setName("Landsat TM Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setShortDescription("Landsat Thematic Mapper");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setExtensions("Landsat TM Header Files (*.hdr *.HDR H*.DAT h*.dat)");

   initFieldLengths();
}

LandsatTmImporter::~LandsatTmImporter()
{
}

unsigned char LandsatTmImporter::getFileAffinity(const string& filename)
{
   return readHeader(filename) ? CAN_LOAD : CAN_NOT_LOAD;
}

vector<ImportDescriptor*> LandsatTmImporter::getImportDescriptors(const string& filename)
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

   RasterDataDescriptor* pDescriptor = RasterUtilities::generateRasterDataDescriptor(filename, NULL,
      mNumRows, mNumCols, mNumBands, BSQ, INT1UBYTE, IN_MEMORY);
   VERIFYRV(pDescriptor != NULL, descriptors);
   pDescriptor->setValidDataTypes(vector<EncodingType>(1, INT1UBYTE));
   ImportDescriptorResource pImportDescriptor(pDescriptor);
   VERIFYRV(pImportDescriptor.get() != NULL, descriptors);
   RasterFileDescriptor* pFileDescriptor = static_cast<RasterFileDescriptor*>(
      RasterUtilities::generateAndSetFileDescriptor(pDescriptor, filename, string(), LITTLE_ENDIAN_ORDER));
   VERIFYRV(pFileDescriptor != NULL, descriptors);

   vector<int> badValues;
   badValues.push_back(0);
   pDescriptor->setBadValues(badValues);
   vector<string> bandFilenames = getBandFilenames(filename);
   if (bandFilenames.size() != mNumBands)
   {
      return descriptors;
   }
   pFileDescriptor->setBandFiles(getBandFilenames(filename));

   DynamicObject* pMetadata = pDescriptor->getMetadata();

   vector<double> startWavelengths;
   startWavelengths += 0.45, 0.52, 0.63, 0.76, 1.55, 10.4, 2.08;
   pMetadata->setAttributeByPath(START_WAVELENGTHS_METADATA_PATH, startWavelengths);

   vector<double> centerWavelengths;
   centerWavelengths += 0.485, 0.56, 0.66, 0.83, 1.65, 11.45, 2.215;
   pMetadata->setAttributeByPath(CENTER_WAVELENGTHS_METADATA_PATH, centerWavelengths);

   vector<double> endWavelengths;
   endWavelengths += 0.52, 0.6, 0.69, 0.9, 1.75, 12.5, 2.35;
   pMetadata->setAttributeByPath(END_WAVELENGTHS_METADATA_PATH, endWavelengths);

   pDescriptor->setDisplayMode(RGB_MODE);
   pDescriptor->setDisplayBand(GRAY, pDescriptor->getOriginalBand(0));
   pDescriptor->setDisplayBand(RED, pDescriptor->getOriginalBand(3));
   pDescriptor->setDisplayBand(GREEN, pDescriptor->getOriginalBand(2));
   pDescriptor->setDisplayBand(BLUE, pDescriptor->getOriginalBand(1));
   pDescriptor->getUnits()->setUnitType(DIGITAL_NO);

   pMetadata->setAttribute("Product", mField[PRODUCT]);
   pMetadata->setAttribute("WRS", mField[WRS]);
   pMetadata->setAttribute("Satellite", mField[SATELLITE]);
   pMetadata->setAttribute("Instrument", mField[INSTRUMENT]);
   pMetadata->setAttribute("Product Type", mField[PRODUCT_TYPE]);
   pMetadata->setAttribute("Product Size", mField[PRODUCT_SIZE]);
   pMetadata->setAttribute("Geodetic Processing", mField[GEODETIC_PROCESSING]);
   pMetadata->setAttribute("Resampling", mField[RESAMPLING]);
   vector<double> gain;
   vector<double> bias;
   bool success =       parseGainBias(GAIN_BIAS_1, gain, bias);
   success = success && parseGainBias(GAIN_BIAS_2, gain, bias);
   success = success && parseGainBias(GAIN_BIAS_3, gain, bias);
   success = success && parseGainBias(GAIN_BIAS_4, gain, bias);
   success = success && parseGainBias(GAIN_BIAS_5, gain, bias);
   success = success && parseGainBias(GAIN_BIAS_6, gain, bias);
   success = success && parseGainBias(GAIN_BIAS_7, gain, bias);
   if (success)
   {
      pMetadata->setAttributeByPath("Radiance Adjust/Gain", gain);
      pMetadata->setAttributeByPath("Radiance Adjust/Bias", bias);
   }
   else
   {
      vector<string> gainBias;
      gainBias += mField[GAIN_BIAS_1],
                  mField[GAIN_BIAS_2],
                  mField[GAIN_BIAS_3],
                  mField[GAIN_BIAS_4],
                  mField[GAIN_BIAS_5],
                  mField[GAIN_BIAS_6],
                  mField[GAIN_BIAS_7];
      pMetadata->setAttribute("Radiance Adjust", gainBias);
   }
   pMetadata->setAttribute("Volume Number/Number in Set", mField[VOLUME_NUMBER]);
   pMetadata->setAttribute("Start Line", StringUtilities::fromDisplayString<unsigned int>(mField[START_LINE]));
   pMetadata->setAttribute("Lines per Volume", StringUtilities::fromDisplayString<unsigned int>(mField[LINES_PER_VOLUME]));
   pMetadata->setAttribute("Orientation Angle", StringUtilities::fromDisplayString<double>(mField[ORIENTATION_ANGLE]));
   pMetadata->setAttribute("Projection", mField[PROJECTION]);
   pMetadata->setAttribute("USGS Projection Number", StringUtilities::fromDisplayString<int>(mField[USGS_PROJECTION_NUMBER]));
   pMetadata->setAttribute("USGS Map Zone", StringUtilities::fromDisplayString<int>(mField[USGS_MAP_ZONE]));
   vector<string> projParamStr = StringUtilities::split(mField[USGS_PROJECTION_PARAMETERS], ' ');
   vector<double> projParam;
   for (vector<string>::const_iterator ppsit = projParamStr.begin(); ppsit != projParamStr.end(); ++ppsit)
   {
      projParam.push_back(StringUtilities::fromDisplayString<double>(*ppsit));
   }
   pMetadata->setAttribute("USGS Projection Parameters", projParam);
   pMetadata->setAttribute("Earth Ellipsoid", mField[EARTH_ELLIPSOID]);
   pMetadata->setAttribute("Semi-Major Axis", StringUtilities::fromDisplayString<double>(mField[SEMIMAJOR_AXIS]));
   pMetadata->setAttribute("Semi-Minor Axis", StringUtilities::fromDisplayString<double>(mField[SEMIMINOR_AXIS]));
   pMetadata->setAttribute("UL Easting", StringUtilities::fromDisplayString<double>(mField[UL_EASTING]));
   pMetadata->setAttribute("UL Northing", StringUtilities::fromDisplayString<double>(mField[UL_NORTHING]));
   pMetadata->setAttribute("UR Easting", StringUtilities::fromDisplayString<double>(mField[UR_EASTING]));
   pMetadata->setAttribute("UR Northing", StringUtilities::fromDisplayString<double>(mField[UR_NORTHING]));
   pMetadata->setAttribute("LR Easting", StringUtilities::fromDisplayString<double>(mField[LR_EASTING]));
   pMetadata->setAttribute("LR Northing", StringUtilities::fromDisplayString<double>(mField[LR_NORTHING]));
   pMetadata->setAttribute("LL Easting", StringUtilities::fromDisplayString<double>(mField[LL_EASTING]));
   pMetadata->setAttribute("LL Northing", StringUtilities::fromDisplayString<double>(mField[LL_NORTHING]));
   pMetadata->setAttribute("Blocking Factor", StringUtilities::fromDisplayString<unsigned int>(mField[BLOCKING_FACTOR]));
   pMetadata->setAttribute("Record Length", StringUtilities::fromDisplayString<unsigned int>(mField[RECORD_LENGTH]));
   pMetadata->setAttribute("Center Easting", StringUtilities::fromDisplayString<double>(mField[CENTER_EASTING]));
   pMetadata->setAttribute("Center Northing", StringUtilities::fromDisplayString<double>(mField[CENTER_NORTHING]));
   pMetadata->setAttribute("Scene Center Pixel Number", StringUtilities::fromDisplayString<int>(mField[SCENE_CENTER_PIXEL_NUMBER]));
   pMetadata->setAttribute("Scene Center Line Number", StringUtilities::fromDisplayString<int>(mField[SCENE_CENTER_LINE_NUMBER]));
   pMetadata->setAttribute("Offset", StringUtilities::fromDisplayString<int>(mField[OFFSET]));

   if (mField[INSTRUMENT].substr(0, 2) == "TM")
   {
      pMetadata->setAttribute("Sensor Name", string("LandSat TM"));
      pMetadata->setAttribute("Change Detection Format", string("TM"));
   }
   int yyyy = StringUtilities::fromDisplayString<int>(mField[ACQUISITION_DATE].substr(0, 4));
   int mm = StringUtilities::fromDisplayString<int>(mField[ACQUISITION_DATE].substr(4, 2));
   int dd = StringUtilities::fromDisplayString<int>(mField[ACQUISITION_DATE].substr(6));
   FactoryResource<DateTime> collectionDate;
   VERIFYRV(collectionDate.get() != NULL, descriptors);
   collectionDate->set(yyyy, mm, dd);
   pMetadata->setAttributeByPath(COLLECTION_DATE_TIME_METADATA_PATH, *collectionDate.get());
   pMetadata->setAttribute("Sun Elevation", StringUtilities::fromDisplayString<double>(mField[SUN_ELEVATION]));
   pMetadata->setAttribute("Sun Azimuth", StringUtilities::fromDisplayString<double>(mField[SUN_AZIMUTH]));

   GcpPoint ul;
   ul.mPixel = LocationType(0,0);
   ul.mCoordinate = LocationType(Landsat::LatLongConvert(mField[UL_LATITUDE]), Landsat::LatLongConvert(mField[UL_LONGITUDE]));
   GcpPoint ur;
   ur.mPixel = LocationType(mNumCols-1,0);
   ur.mCoordinate = LocationType(Landsat::LatLongConvert(mField[UR_LATITUDE]), Landsat::LatLongConvert(mField[UR_LONGITUDE]));
   GcpPoint lr;
   lr.mPixel = LocationType(mNumCols-1,mNumRows-1);
   lr.mCoordinate = LocationType(Landsat::LatLongConvert(mField[LR_LATITUDE]), Landsat::LatLongConvert(mField[LR_LONGITUDE]));
   GcpPoint ll;
   ll.mPixel = LocationType(0,mNumRows-1);
   ll.mCoordinate = LocationType(Landsat::LatLongConvert(mField[LL_LATITUDE]), Landsat::LatLongConvert(mField[LL_LONGITUDE]));
   GcpPoint center;
   center.mPixel = LocationType(mNumCols/2,mNumRows/2);
   center.mCoordinate = LocationType(Landsat::LatLongConvert(mField[CENTER_LATITUDE]), Landsat::LatLongConvert(mField[CENTER_LONGITUDE]));
   list<GcpPoint> gcps;
   gcps += ul,ur,lr,ll,center;
   pFileDescriptor->setGcps(gcps);

   descriptors.push_back(pImportDescriptor.release());
   return descriptors;
}

bool LandsatTmImporter::parseGainBias(size_t headerIndex, vector<double>& gain, vector<double>& bias)
{
   if (mField[headerIndex].empty())
   {
      gain += 0.0;
      bias += 0.0;
   }
   else
   {
      vector<string> gb = StringUtilities::split(mField[headerIndex], '/');
      if (gb.size() != 2)
      {
         return false;
      }
      gain += StringUtilities::fromDisplayString<double>(gb[0]);
      bias += StringUtilities::fromDisplayString<double>(gb[1]);
   }
   return true;
}

void LandsatTmImporter::initFieldLengths()
{ 
   mFieldLen.push_back(9);   //0 - "PRODUCT ="
   mFieldLen.push_back(11);  //1 - product data
   mFieldLen.push_back(6);   //2 - " WRS ='
   mFieldLen.push_back(9);   //3 - wrs data format 'pp/rrrff'
   mFieldLen.push_back(19);  //4 - " ACQUISITION DATE ="
   mFieldLen.push_back(8);   //5 - date in 'yyyymmdd'format
   mFieldLen.push_back(12);  //6 - " SATELLITE ="
   mFieldLen.push_back(2);   //7 - satellite number: 'L5'
   mFieldLen.push_back(13);  //8 - " INSTRUMENT ="
   mFieldLen.push_back(4);   //9 - instrument ytpe: 'TMmn' 
   mFieldLen.push_back(15);  //10 - " PRODUCT TYPE ="
   mFieldLen.push_back(14);  //11 - product type: 'MAP ORIENTED ', 'ORBIT ORIENTED'
   mFieldLen.push_back(15);  //12 - " PRODUCT SIZE ="
   mFieldLen.push_back(10);  //13 - product size: 'FULL SCENE', 'SUBSCENE ', 'MAP SHEET'
   mFieldLen.push_back(78);  //14 - map sheet name
   mFieldLen.push_back(30);  //15 - " TYPE OF GEODETIC PROCESSING ="
   mFieldLen.push_back(10);  //16 - 'SYSTEMATIC', 'PRECISION ', 'TERRAIN  '
   mFieldLen.push_back(13);  //17 - " RESAMPLING ="
   mFieldLen.push_back(2);   //18 - resampling algorighm used: 'CC', 'BL', 'NN'
   mFieldLen.push_back(20);  //19 - " RAD GAINS/BIASES = "
   mFieldLen.push_back(16);  //20 - Max and Min detectable radiance values for the first 
   //     band on the tape in 'mm.mmmmm/n.nnnnn' format
   mFieldLen.push_back(1);   //21 - blank
   mFieldLen.push_back(16);  //22 - Max and Min detectable radiance values for the second 
   //     band on the tape in 'mm.mmmmm/n.nnnnn' format
   mFieldLen.push_back(1);   //23 - blank
   mFieldLen.push_back(16);  //24 - Max and Min detectable radiance values for the third 
   //     band on the tape in 'mm.mmmmm/n.nnnnn' format
   mFieldLen.push_back(1);   //25 - blank
   mFieldLen.push_back(16);  //26 - Max and Min detectable radiance values for the forth 
   //     band on the tape in 'mm.mmmmm/n.nnnnn' format
   mFieldLen.push_back(1);   //27 - blank
   mFieldLen.push_back(16);  //28 - Max and Min detectable radiance values for the fifth 
   //     band on the tape in 'mm.mmmmm/n.nnnnn' format
   mFieldLen.push_back(1);   //29 - blank
   mFieldLen.push_back(16);  //30 - Max and Min detectable radiance values for the sixth 
   //     band on the tape in 'mm.mmmmm/n.nnnnn' format
   mFieldLen.push_back(1);   //31 - blank
   mFieldLen.push_back(16);  //32 - Max and Min detectable radiance values for the seventh 
   //     band on the tape in 'mm.mmmmm/n.nnnnn' format
   mFieldLen.push_back(20);  //33 - " VOLUME #/# IN SET ="
   mFieldLen.push_back(3);   //34 - Tape volume number and number of volumes in tape set in 
   //     'n/m ' format
   mFieldLen.push_back(14);  //35 - " START LINE #="
   mFieldLen.push_back(5);   //36 - First image line number on this volume
   mFieldLen.push_back(15);  //37 - Number of image lines on this volume
   mFieldLen.push_back(5);   //38 - unknown
   mFieldLen.push_back(14);  //39 - " ORIENTATION ="
   mFieldLen.push_back(6);   //40 - Orientation angle on degrees
   mFieldLen.push_back(13);  //41 - " PROJECTION ="
   mFieldLen.push_back(4);   //42 - map projection name
   mFieldLen.push_back(20);  //43 - " USGS PROJECTION # ="
   mFieldLen.push_back(6);   //44 - USGS projection number
   mFieldLen.push_back(16);  //45 - " USGS MAP ZONE ="
   mFieldLen.push_back(6);   //46 - USGS map zone
   mFieldLen.push_back(29);  //47 - " USGS PROJECTION PARAMETERS ="
   mFieldLen.push_back(360); //48 - usgs PROJECTION PARAMETERS IN STANDARD usgs order
   mFieldLen.push_back(18);  //49 - " EARTH ELLIPSOID ='
   mFieldLen.push_back(20);  //50 - ellipsoid used
   mFieldLen.push_back(18);  //51 - " SEMI-MAJOR AXIS ='
   mFieldLen.push_back(11);  //52 - semi-major axis of earth ellipsoid in meters
   mFieldLen.push_back(18);  //53 - " SEMI-MINOR AXIS ="
   mFieldLen.push_back(11);  //54 - semi-minor axis of earth ellipsoid in meters
   mFieldLen.push_back(13);  //55 - " PIXEL SIZE ="
   mFieldLen.push_back(5);   //56 - pixel size in meters
   mFieldLen.push_back(17);  //57 - " PIXELS PER LINE="
   mFieldLen.push_back(5);   //58 - number of pixels per image line
   mFieldLen.push_back(17);  //59 - " LINES PER IMAGE="
   mFieldLen.push_back(5);   //60 - total number of lines in the output image
   mFieldLen.push_back(4);   //61 - " UL "
   mFieldLen.push_back(13);  //62 - geodetic longitude of upper left corner of image.
   mFieldLen.push_back(1);   //63 - blank
   mFieldLen.push_back(12);  //64 - geodetic latitude of upper left corner of image.
   mFieldLen.push_back(1);   //65 - blank
   mFieldLen.push_back(13);  //66 - easting of upper left corner of image in meters X.
   mFieldLen.push_back(1);   //67 - blank
   mFieldLen.push_back(13);  //68 - northing of upper left corner of image in meters Y.
   mFieldLen.push_back(4);   //69 - " UR "
   mFieldLen.push_back(13);  //70 - geodetic longitude of upper right corner of image.
   mFieldLen.push_back(1);   //71 - blank
   mFieldLen.push_back(12);  //72 - geodetic latitude of upper right corner of image.
   mFieldLen.push_back(1);   //73 - blank
   mFieldLen.push_back(13);  //74 - easting of upper right corner of image in meters X.
   mFieldLen.push_back(1);   //75 - blank
   mFieldLen.push_back(13);  //76 - northing of upper right corner of image in meters Y.
   mFieldLen.push_back(4);   //77 - " LR "
   mFieldLen.push_back(13);  //78 - geodetic longitude of lower right corner of image.
   mFieldLen.push_back(1);   //79 - blank
   mFieldLen.push_back(12);  //80 - geodetic latitude of lower right corner of image.
   mFieldLen.push_back(1);   //81 - blank
   mFieldLen.push_back(13);  //82 - easting of lower right corner of image in meters X.
   mFieldLen.push_back(1);   //83 - blank
   mFieldLen.push_back(13);  //84 - northing of lower right corner of image in meters Y.
   mFieldLen.push_back(4);   //85 - " LL "
   mFieldLen.push_back(13);  //86 - geodetic longitude of lower left corner of image.
   mFieldLen.push_back(1);   //87 - blank
   mFieldLen.push_back(12);  //88 - geodetic latitude of lower left corner of image.
   mFieldLen.push_back(1);   //89 - blank
   mFieldLen.push_back(13);  //90 - easting of lower left corner of image in meters X.
   mFieldLen.push_back(1);   //91 - blank
   mFieldLen.push_back(13);  //92 - northing of lower left corner of image in meters Y.
   mFieldLen.push_back(16);  //93 - " BANDS PRESENT ="
   mFieldLen.push_back(7);   //94 - bands present on this volume
   mFieldLen.push_back(18);  //95 - " BLOCKING FACTOR ="
   mFieldLen.push_back(4);   //96 - tape blocking factor
   mFieldLen.push_back(16);  //97 - " RECORD LENGTH ="
   mFieldLen.push_back(5);   //98 - Length of physical tape record
   mFieldLen.push_back(16);  //99 - " SUN ELEVATION ="
   mFieldLen.push_back(2);   //100 - sun elevation angle in degrees at scene center
   mFieldLen.push_back(14);  //101 - " SUN AZIMUTH ="
   mFieldLen.push_back(3);   //102 - sun azimuth in degrees at scene center
   mFieldLen.push_back(8);   //103 - " CENTER "
   mFieldLen.push_back(13);  //104 - Scene center geodetic longitude expressed in degrees, minutes, seconds as above.
   mFieldLen.push_back(1);   //105 - blank
   mFieldLen.push_back(12);  //106 - Scene center geodetic latitude expressed in degrees, minutes, seconds as above.
   mFieldLen.push_back(1);   //107 - lank
   mFieldLen.push_back(13);  //108 - Scene center geodetic easting expressed in meters X.
   mFieldLen.push_back(1);   //109 - blank
   mFieldLen.push_back(13);  //110 - Scene center geodetic northing expressed in meters Y.
   mFieldLen.push_back(6);   //111 - scene center pixel number measured from the product upper left corner, 
   //      rounded to nearest whole pixel( may be negative).
   mFieldLen.push_back(6);   //112 - scene center line number measured from the product upper left corner,
   //      rounded to nearest whole line(may be negative).
   mFieldLen.push_back(8);   //113 - " OFFSET="   
   mFieldLen.push_back(4);   //114 - horizontal offset of the true scene center from the nominal WRS scene
   //      center in units of whole pixels
   mFieldLen.push_back(4);   //115 - " REV"
   mFieldLen.push_back(1);   //116 - format version code (A-Z).  This plugIn uses version B.
}

bool LandsatTmImporter::readHeader(const string& strInHeaderFile)
{
   if (strInHeaderFile.empty())
   {
      return false;
   }

   LargeFileResource pHeaderFile;
   if (!pHeaderFile.open(strInHeaderFile, O_RDONLY, S_IREAD))
   {
      return false;
   }

   vector<char> buf(1537);
   if (pHeaderFile.read(&buf.front(), 1537) != 1536)
   {
      return false;
   }

   mField.clear();
   int headerIdx = 0;
   for (vector<int>::size_type field_ctr = 0; field_ctr < mFieldLen.size(); field_ctr++)
   {
      int fieldLength = mFieldLen[field_ctr];
      string fieldTmp(&buf[headerIdx], fieldLength);
      if (fieldTmp.empty())
      {
         return false;
      }
      mField.push_back(StringUtilities::stripWhitespace(fieldTmp));
      headerIdx += mFieldLen[field_ctr];
   }

   //Extract the dimensions of the cube.
   bool error;
   mNumRows = StringUtilities::fromDisplayString<unsigned int>(mField[ROW_COUNT], &error);
   mNumCols = error ? 0 : StringUtilities::fromDisplayString<unsigned int>(mField[COLUMN_COUNT], &error);
   mNumBands = mField[BAND_COUNT].size();
   if (error)
   {
      return false;
   }

   if ((mField[PRODUCT_MAGIC] != "PRODUCT =") || (mField[FORMAT_VERSION] != "B"))
   {
      return false;
   }

   return true;
}

vector<string> LandsatTmImporter::getBandFilenames(std::string strInHeaderFileName) const
{
   vector<string> bandFilenames;
   if (strInHeaderFileName.empty())
   {
      return bandFilenames;
   }
   FactoryResource<Filename> headerFileName;
   VERIFYRV(headerFileName.get() != NULL, bandFilenames);
   headerFileName->setFullPathAndName(strInHeaderFileName);
   string headerPath = headerFileName->getPath();
   string filebaseName = headerFileName->getTitle();

   FactoryResource<FileFinder> fileFinder;

   // The following can contain %1% to substitute the base name for the header file
   // or %2% to put the band number
   vector<string> tryThese;
   tryThese += "%1%%2%.raw", "band%2%.dat", "%1%.i%2%";
   for (vector<string>::iterator tryThis = tryThese.begin(); tryThis != tryThese.end(); ++tryThis)
   {
      boost::format formatter(*tryThis);
      formatter.exceptions(boost::io::all_error_bits ^ (boost::io::too_many_args_bit | boost::io::too_few_args_bit));
      string glob = (formatter % filebaseName % "[1-9]*").str();
      if (fileFinder->findFile(headerPath, glob))
      {
         while (fileFinder->findNextFile())
         {
            string path;
            fileFinder->getFullPath(path);
            bandFilenames += path;
         }
      }
   }
   return bandFilenames;
}

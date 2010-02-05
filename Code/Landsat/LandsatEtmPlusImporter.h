/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef LANDSATETMPLUSIMPORTER_H
#define LANDSATETMPLUSIMPORTER_H

#include "EnumWrapper.h"
#include "RasterElementImporterShell.h"

#include <string>
#include <vector>

class RasterFileDescriptor;

class LandsatEtmPlusImporter : public RasterElementImporterShell
{
public:
   LandsatEtmPlusImporter();
   ~LandsatEtmPlusImporter();

   unsigned char getFileAffinity(const std::string& filename);
   std::vector<ImportDescriptor*> getImportDescriptors(const std::string& filename);
   bool createRasterPager(RasterElement* pRaster) const;

protected:
   enum FieldIndexEnum
   {
      PRODUCT = 1,
      LOC = 3,
      ACQUISITION_DATE = 5,
      SATELLITE = 7,
      INSTRUMENT = 9,
      SENSOR_MODE = 11,
      LOOK_ANGLE = 13,
      LOCATION = 16,
      PRODUCT_TYPE = 54,
      PRODUCT_SIZE = 56,
      TYPE_OF_PROCESSING = 58,
      RESAMPLING = 60,
      VOLUME_NUMBER = 63,
      PIXELS_PER_LINE = 65,
      LINES_PER_BAND = 67,
      START_LINE = 69,
      BLOCKING_FACTOR = 71,
      REC_SIZE = 73,
      PIXEL_SIZE = 75,
      OUTPUT_BITS_PER_PIXEL = 77,
      ACQUIRED_BITS_PER_PIXEL = 79,
      BANDS_PRESENT = 82,
      FILENAME_1 = 85,
      FILENAME_2 = 87,
      FILENAME_3 = 89,
      FILENAME_4 = 91,
      FILENAME_5 = 93,
      FILENAME_6 = 95,
      REV = 99,
      GAIN_1 = 101,
      BIAS_1 = 103,
      GAIN_2 = 105,
      BIAS_2 = 107,
      GAIN_3 = 109,
      BIAS_3 = 111,
      GAIN_4 = 113,
      BIAS_4 = 115,
      GAIN_5 = 117,
      BIAS_5 = 119,
      GAIN_6 = 121,
      BIAS_6 = 123,
      GEOMETRIC_MAP_PROJECTION = 139,
      ELLIPSOID = 141,
      DATUM = 143,
      USGS_PROJECTION_PARAMETER_1 = 145,
      USGS_PROJECTION_PARAMETER_2 = 147,
      USGS_PROJECTION_PARAMETER_3 = 148,
      USGS_PROJECTION_PARAMETER_4 = 150,
      USGS_PROJECTION_PARAMETER_5 = 152,
      USGS_PROJECTION_PARAMETER_6 = 153,
      USGS_PROJECTION_PARAMETER_7 = 155,
      USGS_PROJECTION_PARAMETER_8 = 157,
      USGS_PROJECTION_PARAMETER_9 = 158,
      USGS_PROJECTION_PARAMETER_10 = 160,
      USGS_PROJECTION_PARAMETER_11 = 162,
      USGS_PROJECTION_PARAMETER_12 = 163,
      USGS_PROJECTION_PARAMETER_13 = 165,
      USGS_PROJECTION_PARAMETER_14 = 167,
      USGS_PROJECTION_PARAMETER_15 = 168,
      USGS_MAP_ZONE = 171,
      UL_LONGITUDE = 174,
      UL_LATITUDE = 176,
      UL_EASTING = 178,
      UL_NORTHING = 180,
      UR_LONGITUDE = 183,
      UR_LATITUDE = 185,
      UR_EASTING = 187,
      UR_NORTHING = 189,
      LR_LONGITUDE = 192,
      LR_LATITUDE = 194,
      LR_EASTING = 196,
      LR_NORTHING = 198,
      LL_LONGITUDE = 201,
      LL_LATITUDE = 203,
      LL_EASTING = 205,
      LL_NORTHING = 207,
      CENTER_LONGITUDE = 210,
      CENTER_LATITUDE = 212,
      CENTER_EASTING = 214,
      CENTER_NORTHING = 216,
      SCENE_CENTER_PIXEL_NUMBER = 218,
      SCENE_CENTER_LINE_NUMBER = 220,
      OFFSET = 223,
      ORIENTATION = 225,
      SUN_ELEVATION = 228,
      SUN_AZIMUTH = 230
   };
   typedef EnumWrapper<FieldIndexEnum> FieldIndex;
   enum BandSetTypeEnum
   {
      LOW_GAIN,
      HIGH_GAIN,
      PANCHROMATIC
   };
   typedef EnumWrapper<BandSetTypeEnum> BandSetType;
   void populateMetaData(DynamicObject* pMetadata, RasterFileDescriptor* pFileDescriptor, BandSetType bandSet);
   void initFieldLengths();
   bool readHeader(const std::string& strInFstHeaderFileName);
   bool parseHeader(const std::vector<std::string>& header, std::vector<std::string>& field);
   std::vector<std::string> getBandFilenames(std::string headerFilename, BandSetType bandSet) const;

private:
   std::vector<int> mFieldLen;
   std::vector<std::string> mFieldHRF;
   std::vector<std::string> mFieldHTM;
   std::vector<std::string> mFieldHPN;

   unsigned int mNumRows;
   unsigned int mNumCols;
   unsigned int mB6Rows;
   unsigned int mB6Cols;
   unsigned int mB8Rows;
   unsigned int mB8Cols;
};

#endif

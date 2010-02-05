/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef LANDSATTMIMPORTER_H
#define LANDSATTMIMPORTER_H

#include "EnumWrapper.h"
#include "RasterElementImporterShell.h"
#include <string>
#include <vector>

class LandsatTmImporter : public RasterElementImporterShell
{
public:
   LandsatTmImporter();
   ~LandsatTmImporter();

   unsigned char getFileAffinity(const std::string& filename);
   std::vector<ImportDescriptor*> getImportDescriptors(const std::string& filename);

protected:
   enum FieldIndexEnum
   {
      PRODUCT_MAGIC = 0,
      PRODUCT = 1,
      WRS = 3,
      ACQUISITION_DATE = 5,
      SATELLITE = 7,
      INSTRUMENT = 9,
      PRODUCT_TYPE = 11,
      PRODUCT_SIZE = 13,
      MAP_SHEET_NAME = 14,
      GEODETIC_PROCESSING = 16,
      RESAMPLING = 18,
      GAIN_BIAS_1 = 20,
      GAIN_BIAS_2 = 22,
      GAIN_BIAS_3 = 24,
      GAIN_BIAS_4 = 26,
      GAIN_BIAS_5 = 28,
      GAIN_BIAS_6 = 30,
      GAIN_BIAS_7 = 32,
      VOLUME_NUMBER = 34,
      START_LINE = 36,
      LINES_PER_VOLUME = 38,
      ORIENTATION_ANGLE = 40,
      PROJECTION = 42,
      USGS_PROJECTION_NUMBER = 44,
      USGS_MAP_ZONE = 46,
      USGS_PROJECTION_PARAMETERS = 48,
      EARTH_ELLIPSOID = 50,
      SEMIMAJOR_AXIS = 52,
      SEMIMINOR_AXIS = 54,
      PIXEL_SIZE = 56,
      COLUMN_COUNT = 58,
      ROW_COUNT = 60,
      UL_LONGITUDE = 62,
      UL_LATITUDE = 64,
      UL_EASTING = 66,
      UL_NORTHING = 68,
      UR_LONGITUDE = 70,
      UR_LATITUDE = 72,
      UR_EASTING = 74,
      UR_NORTHING = 76,
      LR_LONGITUDE = 78,
      LR_LATITUDE = 80,
      LR_EASTING = 82,
      LR_NORTHING = 84,
      LL_LONGITUDE = 86,
      LL_LATITUDE = 88,
      LL_EASTING = 90,
      LL_NORTHING = 92,
      BAND_COUNT = 94,
      BLOCKING_FACTOR = 96,
      RECORD_LENGTH = 98,
      SUN_ELEVATION = 100,
      SUN_AZIMUTH = 102,
      CENTER_LONGITUDE = 104,
      CENTER_LATITUDE = 106,
      CENTER_EASTING = 108,
      CENTER_NORTHING = 110,
      SCENE_CENTER_PIXEL_NUMBER = 111,
      SCENE_CENTER_LINE_NUMBER = 112,
      OFFSET = 114,
      FORMAT_VERSION = 116
   };
   typedef EnumWrapper<FieldIndexEnum> FieldIndex;
   void initFieldLengths();
   bool readHeader(const std::string& filename);
   std::vector<std::string> getBandFilenames(std::string headerFilename) const;

private:
   bool parseGainBias(size_t headerIndex, std::vector<double>& gain, std::vector<double>& bias);

   std::vector<int> mFieldLen;
   std::vector<std::string> mField;

   unsigned int mNumRows;
   unsigned int mNumCols;
   unsigned int mNumBands;
};

#endif

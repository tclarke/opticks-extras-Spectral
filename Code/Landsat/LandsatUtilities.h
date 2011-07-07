/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef LANDSATUTILITIES_H
#define LANDSATUTILITIES_H

#include "DynamicObject.h"
#include "EnumWrapper.h"
#include "ObjectResource.h"
#include "StringUtilities.h"

#include <string>
#include <vector>

class DateTime;
class GcpPoint;
class RasterDataDescriptor;

namespace Landsat
{
   enum LandsatImageTypeEnum
   {
      LANDSAT_VNIR,
      LANDSAT_PAN,
      LANDSAT_TIR
   };
   typedef EnumWrapper<LandsatImageTypeEnum> LandsatImageType;

   enum LandsatDataTypeEnum
   {
      LANDSAT_RAW_DATA,
      LANDSAT_RADIANCE_DATA,
      LANDSAT_REFLECTANCE_DATA,
      LANDSAT_TEMPERATURE_DATA
   };
   typedef EnumWrapper<LandsatDataTypeEnum> LandsatDataType;

   enum L5BandsTypeEnum
   {
      L5_TM1,
      L5_TM2,
      L5_TM3,
      L5_TM4,
      L5_TM5,
      L5_TM6,
      L5_TM7
   };
   typedef EnumWrapper<L5BandsTypeEnum> L5BandsType;

   enum L7BandsTypeEnum
   {
      L7_ETM1,
      L7_ETM2,
      L7_ETM3,
      L7_ETM4,
      L7_ETM5,
      L7_ETM61,
      L7_ETM62,
      L7_ETM7,
      L7_PAN
   };
   typedef EnumWrapper<L7BandsTypeEnum> L7BandsType;

   double LatLongConvert(std::string strInputLatLongData);

   FactoryResource<DynamicObject> parseMtlFile(const std::string& filename, bool& success);

   std::vector<std::string> getGeotiffBandFilenames(const DynamicObject* pMetadata,
      std::string filename, LandsatImageType type, std::vector<unsigned int>& validBands);

   bool parseGcpFromGeotiff(std::string filename, double geotiffPixelX, double geotiffPixelY, GcpPoint& gcp);

   bool parseBasicsFromTiff(std::string filename, RasterDataDescriptor* pDescriptor);

   void fixMtlMetadata(DynamicObject* pMetadata, LandsatImageType type,
      const std::vector<unsigned int>& validBands);

   std::vector<std::string> getSensorBandNames(std::string spacecraft, LandsatImageType type);

   std::vector<std::pair<double, double> > determineRadianceConversionFactors(const DynamicObject* pMetadata,
      LandsatImageType type, const std::vector<unsigned int>& validBands);

   std::vector<double> determineReflectanceConversionFactors(const DynamicObject* pMetadata,
      LandsatImageType type, const std::vector<unsigned int>& validBands);

   double getL5SolarIrradiance(L5BandsType band, bool& error);

   double determineL5ReflectanceConversionFactor(double solarElevationAngleInDegrees,
      L5BandsType band, const DateTime* pDate);

   double getL7SolarIrradiance(L7BandsType band, bool& error);

   double determineL7ReflectanceConversionFactor(double solarElevationAngleInDegrees,
      L7BandsType band, const DateTime* pDate);

   void getL5TemperatureConstants(double& K1, double& K2);

   void getL7TemperatureConstants(double& K1, double& K2);

   bool getTemperatureConstants(const DynamicObject* pMetadata, LandsatImageType imageType,
      double& K1, double& K2);

   //KIP - move to SpectralUtilities
   double determineReflectanceConversionFactor(double solarElevationAngleInDegrees,
      double solarIrradiance, const DateTime* pDate);

   //KIP - move to SpectralUtilities
   double determineJulianDay(const DateTime* pDate);

   //KIP - move to SpectralUtilities
   double determineEarthSunDistance(const DateTime* pDate);

   template<typename T>
   std::vector<T> getSensorBandValues(const DynamicObject* pMetadata,
      std::vector<std::string> sensorBandNames, std::string bandKey, std::string bandKeySuffix)
   {
      std::vector<T> bandValues;
      bool error = false;
      for (std::vector<std::string>::iterator bandNameIter = sensorBandNames.begin();
            bandNameIter != sensorBandNames.end();
            ++bandNameIter)
      {
         T value = StringUtilities::fromXmlString<T>(dv_cast<std::string>(pMetadata->getAttributeByPath(
            "LANDSAT_MTL/L1_METADATA_FILE/" + bandKey + *bandNameIter + bandKeySuffix), ""), &error);
         if (error)
         {
            break;
         }
         bandValues.push_back(value);
      }
      if (error)
      {
         bandValues.clear();
      }
      return bandValues;
   }
};

#endif

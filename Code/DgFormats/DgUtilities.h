/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef DGUTILITIES_H__
#define DGUTILITIES_H__

#include "DgFileTile.h"
#include "DynamicObject.h"
#include "GcpList.h"
#include "ObjectResource.h"
#include "StringUtilities.h"

#include "XercesIncludes.h"

#include <QtCore/QStringList>

#include <string>
#include <vector>

class DateTime;
class RasterDataDescriptor;

namespace DgUtilities
{
   enum DgDataTypeEnum
   {
      DG_RAW_DATA,
      DG_RADIANCE_DATA,
      DG_REFLECTANCE_DATA
   };

   typedef EnumWrapper<DgDataTypeEnum> DgDataType;

   enum Wv2BandsTypeEnum
   {
      WV2_PAN,
      WV2_COASTAL,
      WV2_BLUE,
      WV2_GREEN,
      WV2_YELLOW,
      WV2_RED,
      WV2_REDEDGE,
      WV2_NIR1,
      WV2_NIR2
   };
   typedef EnumWrapper<Wv2BandsTypeEnum> Wv2BandsType;

   enum Qb2BandsEnumType
   {
      QB2_PAN,
      QB2_BLUE,
      QB2_GREEN,
      QB2_RED,
      QB2_NIR
   };
   typedef EnumWrapper<Qb2BandsEnumType> Qb2BandsType;

   std::vector<std::string> getSensorBandNames(std::string product, std::string sensor);

   FactoryResource<DynamicObject> parseMetadata(XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* pDoc);

   void handleSpecialMetadata(DynamicObject* pMetadata, unsigned int bandCount);

   double getWv2SolarIrradiance(Wv2BandsType band, bool& error);

   double getQb2RevisedKfactors(Qb2BandsType band, bool is16bit, int tdiLevel, bool& error);

   double getQb2SolarIrradiance(Qb2BandsType band, bool& error);

   double determineJulianDay(const DateTime* pDate);

   double determineEarthSunDistance(const DateTime* pDate);

   double determineWv2RadianceConversionFactor(double absCalBandFactor, double effectiveBandwidth);

   double determineWv2ReflectanceConversionFactor(double absCalBandFactor, double effectiveBandwidth,
      double solarElevationAngleInDegrees, Wv2BandsType band, const DateTime* pDate);

   double determineQb2RadianceConversionFactor(double absCalBandFactor, double effectiveBandwidth,
      bool before20030606, int tdiLevel, Qb2BandsType band, bool is16bit);

   double determineQb2ReflectanceConversionFactor(double absCalBandFactor, double effectiveBandwidth,
      int tdiLevel, double solarElevationAngleInDegrees,
      Qb2BandsType band, bool is16bit, const DateTime* pDate);

   std::vector<double> determineConversionFactors(const DynamicObject* pMetadata, DgDataType type);

   bool verifyTiles(const DynamicObject* pMetadata, const std::vector<DgFileTile>& tiles, std::string& errorMsg);

   bool parseGcpFromGeotiff(std::string filename, double pixelX, double pixelY,
      double geotiffPixelX, double geotiffPixelY, GcpPoint& gcp);

   std::list<GcpPoint> parseGcps(const std::vector<DgFileTile>& tiles);

   void parseRpcs(DynamicObject* pMetadata);

   bool parseBasicsFromTiff(std::string filename, RasterDataDescriptor* pDescriptor);

   template<typename T>
   std::vector<T> getSensorBandValues(const DynamicObject* pMetadata,
      std::vector<std::string> sensorBandNames, std::string bandKey)
   {
      std::vector<T> bandValues;
      bool error = false;
      for (std::vector<std::string>::iterator bandNameIter = sensorBandNames.begin();
            bandNameIter != sensorBandNames.end();
            ++bandNameIter)
      {
         T value = StringUtilities::fromXmlString<T>(dv_cast<std::string>(pMetadata->getAttributeByPath(
            "DIGITALGLOBE_ISD/IMD/" + *bandNameIter + "/" + bandKey), ""), &error);
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

   template<class T>
   std::vector<T> parseTextIntoVector(std::string text)
   {
      bool error = false;
      std::vector<T> parsedValues;
      QStringList list = QString::fromStdString(text).split(QRegExp("\\s+"));
      for (QStringList::iterator listIter = list.begin();
         listIter != list.end();
         ++listIter)
      {
         T value = StringUtilities::fromXmlString<T>(listIter->toStdString(), &error);
         if (error)
         {
            break;
         }
         parsedValues.push_back(value);
      }
      if (error)
      {
         parsedValues.clear();
      }
      return parsedValues;
   }

};

#endif
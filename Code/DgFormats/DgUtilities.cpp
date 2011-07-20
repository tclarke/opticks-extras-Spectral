/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "DateTime.h"
#include "DgUtilities.h"
#include "Endian.h"
#include "FileResource.h"
#include "RasterDataDescriptor.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "SpecialMetadata.h"
#include "SpectralUtilities.h"
#include "Wavelengths.h"
#include "xmlreader.h"

#include <QtCore/QDate>
#include <QtCore/QFile>

#include <geotiff.h>
#include <geovalues.h>
#include <geo_normalize.h>
#include <xtiffio.h>

#include <algorithm>
#include <list>

XERCES_CPP_NAMESPACE_USE 

namespace
{
   template<class T>
   class FindPairName
   {
   public:
      FindPairName(std::string matchValue) : mMatchValue(matchValue) {}
      bool operator()(const T& curValue)
      {
         return curValue.first == mMatchValue;
      }
   private:
      std::string mMatchValue;
   };
};

std::vector<std::string> DgUtilities::getSensorBandNames(std::string product, std::string sensor)
{
   std::vector<std::string> bands;
   if (product == "P" && (sensor == "QB02" || sensor == "WV01" || sensor == "WV02")) //just pan band
   {
      bands.push_back("BAND_P");
   }
   else if (product == "Multi") //all multi-spectral bands
   {
      if (sensor == "QB02")
      {
         bands.push_back("BAND_B");
         bands.push_back("BAND_G");
         bands.push_back("BAND_R");
         bands.push_back("BAND_N");
      }
      if (sensor == "WV02")
      {
         bands.push_back("BAND_C");
         bands.push_back("BAND_B");
         bands.push_back("BAND_G");
         bands.push_back("BAND_Y");
         bands.push_back("BAND_R");
         bands.push_back("BAND_RE");
         bands.push_back("BAND_N");
         bands.push_back("BAND_N2");
      }
   }
   return bands;
}

FactoryResource<DynamicObject> DgUtilities::parseMetadata(XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* pDoc)
{
   FactoryResource<DynamicObject> pImageMetadata;
   DOMElement* pRoot = pDoc->getDocumentElement();
   if (pRoot == NULL || !XMLString::equals(pRoot->getNodeName(), X("isd")))
   {
      return pImageMetadata;
   }

   //canonicalize document to ease parsing
   DOMConfiguration* pConf = pDoc->getDOMConfig();
   if (pConf != NULL)
   {
      try
      {
         pConf->setParameter(X("canonical-from"), true);
      }
      catch (DOMException) {}
   }
   pDoc->normalizeDocument();
   std::list<std::pair<std::string, DOMElement*> > remainingElements;
   remainingElements.push_back(std::make_pair(std::string("/") + A(pRoot->getNodeName()), pRoot));
   pImageMetadata->setAttribute(A(pRoot->getNodeName()), *(FactoryResource<DynamicObject>().get()));
   while (!remainingElements.empty())
   {
      std::pair<std::string, DOMElement*> element = remainingElements.front();
      remainingElements.pop_front();
      std::string curName = element.first;
      DOMElement* pElement = element.second;
      if (pElement == NULL)
      {
         continue;
      }
      DynamicObject* pParentObj = dv_cast<DynamicObject>(&(pImageMetadata->getAttributeByPath(curName)));
      if (pParentObj == NULL)
      {
         continue;
      }
      unsigned int childElementCount = pElement->getChildElementCount();
      if (childElementCount == 0)
      {
         //text for element
         std::string value = A(pElement->getTextContent());
         pImageMetadata->setAttributeByPath(curName, value);
      }
      std::list<std::pair<std::string, unsigned int> > duplicates;
      DOMElement* pElementNode = NULL;
      unsigned int i = 0;
      for (i = 0, pElementNode = pElement->getFirstElementChild();
            i < childElementCount && pElementNode != NULL;
            ++i, pElementNode = pElementNode->getNextElementSibling())
      {
         if (pElementNode == NULL)
         {
            continue;
         }
         std::string nodeName = A(pElementNode->getNodeName());
         std::string nameToUse = curName + "/" + nodeName;
         std::list<std::pair<std::string, unsigned int> >::iterator foundDup =
            std::find_if(duplicates.begin(), duplicates.end(),
            FindPairName<std::pair<std::string, unsigned int> >(nodeName));
         if ((foundDup != duplicates.end()) || pParentObj->getAttribute(nodeName).isValid())
         {
            //should we log duplicates to message log?
            //duplicate detected
            if (foundDup == duplicates.end())
            {
               //first time, so rename existing element
               duplicates.push_back(make_pair(nodeName, 2));
               foundDup = std::find_if(duplicates.begin(), duplicates.end(),
                  FindPairName<std::pair<std::string, unsigned int> >(nodeName));

               DataVariant curValue = std::string("");
               pParentObj->adoptAttribute(nodeName, curValue); //curValue now contains previous contents
               if (curValue.isValid())
               {
                  pParentObj->removeAttribute(nodeName);
                  pParentObj->adoptAttribute(nodeName + "_1", curValue); //move into renamed element
               }

               std::list<std::pair<std::string, DOMElement*> >::iterator elementIter;
               do
               {
                  elementIter = std::find_if(remainingElements.begin(), remainingElements.end(),
                     FindPairName<std::pair<std::string, DOMElement*> >(nameToUse));
                  if (elementIter != remainingElements.end())
                  {
                     elementIter->first = nameToUse + "_1";
                  }
               } while ( elementIter != remainingElements.end() );
            }
            nameToUse = curName + "/" + nodeName + "_" + StringUtilities::toDisplayString(foundDup->second);
            foundDup->second++;
         }
         remainingElements.push_back(std::make_pair(nameToUse, static_cast<DOMElement*>(pElementNode)));
         pImageMetadata->setAttributeByPath(nameToUse, *(FactoryResource<DynamicObject>().release()));
      }
   }
   //rename "isd" attribute to "DIGITALGLOBE_ISD"
   DataVariant temp(std::string(""));
   pImageMetadata->adoptAttribute("isd", temp); //temp now contains contents of "isd"
   pImageMetadata->removeAttribute("isd");
   pImageMetadata->adoptAttribute("DIGITALGLOBE_ISD", temp);
   return pImageMetadata;
}

void DgUtilities::handleSpecialMetadata(DynamicObject* pMetadata, unsigned int bandCount)
{
   if (pMetadata == NULL)
   {
      return;
   }
   std::string product = dv_cast<std::string>(pMetadata->getAttributeByPath("DIGITALGLOBE_ISD/IMD/BANDID"), "");
   std::string sensor = dv_cast<std::string>(pMetadata->getAttributeByPath("DIGITALGLOBE_ISD/IMD/IMAGE/SATID"), "");
   std::vector<std::string> bands = DgUtilities::getSensorBandNames(product, sensor);
   if (bands.size() == bandCount)
   {
      //must match number of bands that will be in resulting image.
      std::vector<double> absCalFactor = DgUtilities::getSensorBandValues<double>(pMetadata, bands, "ABSCALFACTOR");
      if (absCalFactor.size() == bands.size())
      {
         pMetadata->setAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/DgAbsScaleFactor",
            absCalFactor);
      }
      std::vector<double> effectiveBandwidth = DgUtilities::getSensorBandValues<double>(pMetadata, bands,
         "EFFECTIVEBANDWIDTH");
      if (effectiveBandwidth.size() == bands.size())
      {
         pMetadata->setAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/DgEffectiveBandwidth",
            effectiveBandwidth);
      }
      std::vector<int> tdiLevels;
      std::string tdiLevelStr = dv_cast<std::string>(pMetadata->getAttributeByPath(
         "DIGITALGLOBE_ISD/IMD/IMAGE/TDILEVEL"), "");
      if (!tdiLevelStr.empty())
      {
         bool parseError = false;
         int tdiLevel = StringUtilities::fromXmlString<int>(tdiLevelStr, &parseError);
         if (!parseError)
         {
            tdiLevels.push_back(tdiLevel);
         }
      }
      if (tdiLevels.size() == bands.size())
      {
         pMetadata->setAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/DgTdiLevel", tdiLevels);
      }
   }

   std::string dateTimeText = dv_cast<std::string>(pMetadata->getAttributeByPath(
      "DIGITALGLOBE_ISD/IMD/IMAGE/FIRSTLINETIME"), "");
   FactoryResource<DateTime> pDateTime;
   if (!dateTimeText.empty())
   {
      if (pDateTime->set(dateTimeText))
      {
         pMetadata->setAttributeByPath(COLLECTION_DATE_TIME_METADATA_PATH, *pDateTime.get());
      }
   }

   std::vector<std::string> bandNames;
   std::vector<double> startWaves;
   std::vector<double> centerWaves;
   std::vector<double> endWaves;
   if (product == "P") //just pan band
   {
      bandNames.push_back("PAN");
      if (sensor == "QB02")
      {
         startWaves.push_back(0.405);
         centerWaves.push_back(0.729);
         endWaves.push_back(1.053);
      }
      else if (sensor == "WV01")
      {
         startWaves.push_back(0.397);
         centerWaves.push_back(0.651);
         endWaves.push_back(0.905);
      }
      else if (sensor == "WV02")
      {
         startWaves.push_back(0.447);
         centerWaves.push_back(0.627);
         endWaves.push_back(0.808);
      }
   }
   else if (product == "Multi") //all multi-spectral bands
   {
      if (sensor == "QB02")
      {
         bandNames.push_back("Blue");
         bandNames.push_back("Green");
         bandNames.push_back("Red");
         bandNames.push_back("NIR");
         startWaves.push_back(0.43);
         startWaves.push_back(0.466);
         startWaves.push_back(0.59);
         startWaves.push_back(0.715);
         centerWaves.push_back(0.488);
         centerWaves.push_back(0.543);
         centerWaves.push_back(0.650);
         centerWaves.push_back(0.817);
         endWaves.push_back(0.545);
         endWaves.push_back(0.62);
         endWaves.push_back(0.71);
         endWaves.push_back(0.918);
      }
      if (sensor == "WV02")
      {
         bandNames.push_back("Coastal Blue");
         bandNames.push_back("Blue");
         bandNames.push_back("Green");
         bandNames.push_back("Yellow");
         bandNames.push_back("Red");
         bandNames.push_back("Red Edge");
         bandNames.push_back("NIR");
         bandNames.push_back("NIR2");
         startWaves.push_back(0.396);
         startWaves.push_back(0.442);
         startWaves.push_back(0.506);
         startWaves.push_back(0.584);
         startWaves.push_back(0.624);
         startWaves.push_back(0.699);
         startWaves.push_back(0.765);
         startWaves.push_back(0.856);
         centerWaves.push_back(0.427);
         centerWaves.push_back(0.478);
         centerWaves.push_back(0.546);
         centerWaves.push_back(0.608);
         centerWaves.push_back(0.659);
         centerWaves.push_back(0.724);
         centerWaves.push_back(0.833);
         centerWaves.push_back(0.949);
         endWaves.push_back(0.458);
         endWaves.push_back(0.515);
         endWaves.push_back(0.586);
         endWaves.push_back(0.632);
         endWaves.push_back(0.694);
         endWaves.push_back(0.749);
         endWaves.push_back(0.901);
         endWaves.push_back(1.043);
      }
   }

   if (startWaves.size() == bandCount)
   {
      FactoryResource<Wavelengths> pWaves;
      pWaves->setStartValues(startWaves, MICRONS);
      pWaves->setCenterValues(centerWaves, MICRONS);
      pWaves->setEndValues(endWaves, MICRONS);
      pWaves->applyToDynamicObject(pMetadata);
      pMetadata->setAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/" + NAMES_METADATA_NAME,
         bandNames);
   }
}

double DgUtilities::getWv2SolarIrradiance(Wv2BandsType band, bool& error)
{
   error = false;
   switch(band)
   {
   case WV2_PAN:
      return 1580.8140;
   case WV2_COASTAL:
      return 1758.2229;
   case WV2_BLUE:
      return 1974.2416;
   case WV2_GREEN:
      return 1856.4104;
   case WV2_YELLOW:
      return 1738.4791;
   case WV2_RED:
      return 1559.4555;
   case WV2_REDEDGE:
      return 1342.0695;
   case WV2_NIR1:
      return 1069.7302;
   case WV2_NIR2:
      return 861.2866;
   default:
      error = true;
      return 1.0;
   };
}

double DgUtilities::getQb2RevisedKfactors(Qb2BandsType band, bool is16bit, int tdiLevel, bool& error)
{
   error = false;
   double kfactor = 1.0;
   if (is16bit)
   {
      if (band == QB2_PAN)
      {
         switch(tdiLevel)
         {
         case 10:
            return 8.381880E-2;
         case 13:
            return 6.447600E-2;
         case 18:
            return 4.656600E-2;
         case 24:
            return 3.494440E-2;
         case 32:
            return 2.618840E-2;
         default:
            break;
         };
      }
      else
      {
         switch(band)
         {
         case QB2_BLUE:
            return 1.604120E-2;
         case QB2_GREEN:
            return 1.438470E-2;
         case QB2_RED:
            return 1.267350E-2;
         case QB2_NIR:
            return 1.542420E-2;
         default:
            break;
         }
      }
   }
   else
   {
      if (band == QB2_PAN)
      {
         switch(tdiLevel)
         {
         case 10:
            return 1.02681367;
         case 13:
            return 1.02848939;
         case 18:
            return 1.02794702;
         case 24:
            return 1.02989685;
         case 32:
            return 1.02739898;
         default:
            break;
         };
      }
      else
      {
         switch(band)
         {
         case QB2_BLUE:
            return 1.12097834;
         case QB2_GREEN:
            return 1.37652632;
         case QB2_RED:
            return 1.30924587;
         case QB2_NIR:
            return 0.98368622;
         default:
            break;
         }
      }
   }
   error = true;
   return 1.0;
}

double DgUtilities::getQb2SolarIrradiance(Qb2BandsType band, bool& error)
{
   error = false;
   switch(band)
   {
   case QB2_PAN:
      return 1381.79;
   case QB2_BLUE:
      return 1924.59;
   case QB2_GREEN:
      return 1843.08;
   case QB2_RED:
      return 1574.77;
   case QB2_NIR:
      return 1113.71;
   default:
      error = true;
      return 1.0;
   };
}

double DgUtilities::determineWv2RadianceConversionFactor(double absCalBandFactor, double effectiveBandwidth)
{
   if (fabs(effectiveBandwidth) == 0.0)
   {
      return 1.0;
   }
   return absCalBandFactor / effectiveBandwidth;
}

double DgUtilities::determineWv2ReflectanceConversionFactor(double absCalBandFactor, double effectiveBandwidth,
   double solarElevationAngleInDegrees, Wv2BandsType band, const DateTime* pDate)
{
   bool error = false;
   double solarIrradiance = getWv2SolarIrradiance(band, error);
   if (error || pDate == NULL)
   {
      return 1.0;
   }
   double radianceFactor = determineWv2RadianceConversionFactor(absCalBandFactor, effectiveBandwidth);
   double reflectanceFactor = SpectralUtilities::determineReflectanceConversionFactor(solarElevationAngleInDegrees,
      solarIrradiance, *pDate);
   return radianceFactor * reflectanceFactor;
}

double DgUtilities::determineQb2RadianceConversionFactor(double absCalBandFactor, double effectiveBandwidth,
   bool before20030606, int tdiLevel, Qb2BandsType band, bool is16bit)
{
   if (fabs(effectiveBandwidth) == 0.0)
   {
      return 1.0;
   }
   bool error = false;
   double revisedCalFactor = absCalBandFactor;
   if (before20030606)
   {
      if (is16bit)
      {
         revisedCalFactor = getQb2RevisedKfactors(band, is16bit, tdiLevel, error);
      }
      else
      {
         //8bit
         revisedCalFactor = absCalBandFactor * getQb2RevisedKfactors(band, is16bit, tdiLevel, error);
      }
   }
   if (error)
   {
      return 1.0;
   }
   return revisedCalFactor / effectiveBandwidth;
}

double DgUtilities::determineQb2ReflectanceConversionFactor(double absCalBandFactor, double effectiveBandwidth,
   int tdiLevel, double solarElevationAngleInDegrees,
   Qb2BandsType band, bool is16bit, const DateTime* pDate)
{
   bool error = false;
   double solarIrradiance = getQb2SolarIrradiance(band, error);
   if (error || pDate == NULL)
   {
      return 1.0;
   }
   FactoryResource<DateTime> pDate20030606;
   pDate20030606->set(2003, 06, 06);
   bool before20030606 = pDate->getSecondsSince(*pDate20030606.get()) < 0.0;
   double radianceFactor = determineQb2RadianceConversionFactor(absCalBandFactor, effectiveBandwidth,
      before20030606, tdiLevel, band, is16bit);
   double reflectanceFactor = SpectralUtilities::determineReflectanceConversionFactor(solarElevationAngleInDegrees,
      solarIrradiance, *pDate);
   return radianceFactor * reflectanceFactor; 
}

std::vector<double> DgUtilities::determineConversionFactors(const DynamicObject* pMetadata, DgDataType type)
{
   std::vector<double> factors;
   std::string sensor = dv_cast<std::string>(pMetadata->getAttributeByPath("DIGITALGLOBE_ISD/IMD/IMAGE/SATID"), "");
   std::string product = dv_cast<std::string>(pMetadata->getAttributeByPath("DIGITALGLOBE_ISD/IMD/BANDID"), "");
   const DateTime* pDateTime = dv_cast<DateTime>(&pMetadata->getAttributeByPath(COLLECTION_DATE_TIME_METADATA_PATH));
   bool parseSunElevError = false;
   double solarElevationAngleInDegrees = StringUtilities::fromXmlString<double>(
      dv_cast<std::string>(pMetadata->getAttributeByPath("DIGITALGLOBE_ISD/IMD/IMAGE/MEANSUNEL"), ""),
      &parseSunElevError);
   bool haveSunElev = !parseSunElevError;
   std::vector<double> absCalFactors = dv_cast<std::vector<double> >(
      pMetadata->getAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/DgAbsScaleFactor"),
      std::vector<double>());
   std::vector<double> effectiveBandwidth = dv_cast<std::vector<double> >(
      pMetadata->getAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/DgEffectiveBandwidth"),
      std::vector<double>());
   std::vector<int> tdiLevel = dv_cast<std::vector<int> >(
      pMetadata->getAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/DgTdiLevel"),
      std::vector<int>());
   FactoryResource<DateTime> pDate20030606;
   pDate20030606->set(2003, 06, 06);
   bool before20030606 = pDateTime->getSecondsSince(*pDate20030606.get()) < 0.0;

   if (sensor == "WV02")
   {
      if (type == DG_REFLECTANCE_DATA && haveSunElev && pDateTime != NULL)
      {
         if (product == "P" && absCalFactors.size() == 1 && effectiveBandwidth.size() == 1)
         {
            factors.push_back(determineWv2ReflectanceConversionFactor(absCalFactors[0],
               effectiveBandwidth[0], solarElevationAngleInDegrees, WV2_PAN, pDateTime));
         }
         if (product == "Multi" && absCalFactors.size() == 8 && effectiveBandwidth.size() == 8)
         {
            factors.push_back(determineWv2ReflectanceConversionFactor(absCalFactors[0],
               effectiveBandwidth[0], solarElevationAngleInDegrees, WV2_COASTAL, pDateTime));
            factors.push_back(determineWv2ReflectanceConversionFactor(absCalFactors[1],
               effectiveBandwidth[1], solarElevationAngleInDegrees, WV2_BLUE, pDateTime));
            factors.push_back(determineWv2ReflectanceConversionFactor(absCalFactors[2],
               effectiveBandwidth[2], solarElevationAngleInDegrees, WV2_GREEN, pDateTime));
            factors.push_back(determineWv2ReflectanceConversionFactor(absCalFactors[3],
               effectiveBandwidth[3], solarElevationAngleInDegrees, WV2_YELLOW, pDateTime));
            factors.push_back(determineWv2ReflectanceConversionFactor(absCalFactors[4],
               effectiveBandwidth[4], solarElevationAngleInDegrees, WV2_RED, pDateTime));
            factors.push_back(determineWv2ReflectanceConversionFactor(absCalFactors[5],
               effectiveBandwidth[5], solarElevationAngleInDegrees, WV2_REDEDGE, pDateTime));
            factors.push_back(determineWv2ReflectanceConversionFactor(absCalFactors[6],
               effectiveBandwidth[6], solarElevationAngleInDegrees, WV2_NIR1, pDateTime));
            factors.push_back(determineWv2ReflectanceConversionFactor(absCalFactors[7],
               effectiveBandwidth[7], solarElevationAngleInDegrees, WV2_NIR2, pDateTime));
         }
      }
      else if (type == DG_RADIANCE_DATA)
      {
         if (product == "P" && absCalFactors.size() == 1 && effectiveBandwidth.size() == 1)
         {
            factors.push_back(determineWv2RadianceConversionFactor(absCalFactors[0],
               effectiveBandwidth[0]));
         }
         if (product == "Multi" && absCalFactors.size() == 8 && effectiveBandwidth.size() == 8)
         {
            factors.push_back(determineWv2RadianceConversionFactor(absCalFactors[0],
               effectiveBandwidth[0]));
            factors.push_back(determineWv2RadianceConversionFactor(absCalFactors[1],
               effectiveBandwidth[1]));
            factors.push_back(determineWv2RadianceConversionFactor(absCalFactors[2],
               effectiveBandwidth[2]));
            factors.push_back(determineWv2RadianceConversionFactor(absCalFactors[3],
               effectiveBandwidth[3]));
            factors.push_back(determineWv2RadianceConversionFactor(absCalFactors[4],
               effectiveBandwidth[4]));
            factors.push_back(determineWv2RadianceConversionFactor(absCalFactors[5],
               effectiveBandwidth[5]));
            factors.push_back(determineWv2RadianceConversionFactor(absCalFactors[6],
               effectiveBandwidth[6]));
            factors.push_back(determineWv2RadianceConversionFactor(absCalFactors[7],
               effectiveBandwidth[7]));
         }
      }
   }
   else if (sensor == "QB02")
   {
      std::string bitsPerPixel = dv_cast<std::string>(pMetadata->getAttributeByPath(
         "DIGITALGLOBE_ISD/IMD/BITSPERPIXEL"), "");
      bool is16bit = (bitsPerPixel == "16");
      if (type == DG_REFLECTANCE_DATA && haveSunElev && pDateTime != NULL)
      {
         if (product == "P" && absCalFactors.size() == 1 && effectiveBandwidth.size() == 1 &&
               tdiLevel.size() == 1)
         {
            factors.push_back(determineQb2ReflectanceConversionFactor(absCalFactors[0],
               effectiveBandwidth[0], tdiLevel[0], solarElevationAngleInDegrees,
               QB2_PAN, is16bit, pDateTime));
         }
         if (product == "Multi" && absCalFactors.size() == 4 && effectiveBandwidth.size() == 4 &&
               tdiLevel.size() == 4)
         {
            factors.push_back(determineQb2ReflectanceConversionFactor(absCalFactors[0],
               effectiveBandwidth[0], tdiLevel[0], solarElevationAngleInDegrees,
               QB2_BLUE, is16bit, pDateTime));
            factors.push_back(determineQb2ReflectanceConversionFactor(absCalFactors[1],
               effectiveBandwidth[1], tdiLevel[1], solarElevationAngleInDegrees,
               QB2_GREEN, is16bit, pDateTime));
            factors.push_back(determineQb2ReflectanceConversionFactor(absCalFactors[2],
               effectiveBandwidth[2], tdiLevel[2], solarElevationAngleInDegrees,
               QB2_RED, is16bit, pDateTime));
            factors.push_back(determineQb2ReflectanceConversionFactor(absCalFactors[3],
               effectiveBandwidth[3], tdiLevel[3], solarElevationAngleInDegrees,
               QB2_NIR, is16bit, pDateTime));
         }
      }
      else if (type == DG_RADIANCE_DATA)
      {
         if (product == "P" && absCalFactors.size() == 1 && effectiveBandwidth.size() == 1 &&
               tdiLevel.size() == 1)
         {
            factors.push_back(determineQb2RadianceConversionFactor(absCalFactors[0],
               effectiveBandwidth[0], before20030606, tdiLevel[0], QB2_PAN, is16bit));
         }
         if (product == "Multi" && absCalFactors.size() == 4 && effectiveBandwidth.size() == 4 &&
               tdiLevel.size() == 4)
         {
            factors.push_back(determineQb2RadianceConversionFactor(absCalFactors[0],
               effectiveBandwidth[0], before20030606, tdiLevel[0], QB2_BLUE, is16bit));
            factors.push_back(determineQb2RadianceConversionFactor(absCalFactors[0],
               effectiveBandwidth[0], before20030606, tdiLevel[1], QB2_GREEN, is16bit));
            factors.push_back(determineQb2RadianceConversionFactor(absCalFactors[0],
               effectiveBandwidth[0], before20030606, tdiLevel[2], QB2_RED, is16bit));
            factors.push_back(determineQb2RadianceConversionFactor(absCalFactors[0],
               effectiveBandwidth[0], before20030606, tdiLevel[3], QB2_NIR, is16bit));
         }
      }
   }

   return factors;
}

bool DgUtilities::verifyTiles(const DynamicObject* pMetadata, const std::vector<DgFileTile>& tiles,
   std::string& errorMsg)
{
   std::string missingFiles = "";
   if (tiles.empty())
   {
      errorMsg = " No data can be loaded, because no tiles could be located in this file.";
   }
   else
   {
      std::string tileUnits = dv_cast<std::string>(pMetadata->getAttributeByPath("DIGITALGLOBE_ISD/TIL/TILEUNITS"),
         "missing");
      if (tileUnits != "Pixels")
      {
         errorMsg = " Unsupported tile units of " + tileUnits + ". Only tile units of Pixels are supported.";
      }
   }
   for (unsigned int i = 0; i < tiles.size(); ++i)
   {
      QString tileFile = QString::fromStdString(tiles[i].mTilFilename);
      if (!QFile::exists(tileFile))
      {
         errorMsg += " Tile file " + tileFile.toStdString() + " does not exist.\n";
      }
   }
   return errorMsg.empty();
}

bool DgUtilities::parseGcpFromGeotiff(std::string filename, double pixelX, double pixelY,
 double geotiffPixelX, double geotiffPixelY, GcpPoint& gcp)
{
   TIFF* pTiffFile = XTIFFOpen(filename.c_str(), "r");
   if (pTiffFile == NULL)
   {
      return false;
   }
   // Latitude/longitude GCPs
   GTIF* pGeoTiff = GTIFNew(pTiffFile);

   GTIFDefn defn;
   GTIFGetDefn(pGeoTiff, &defn);

   char* pProj4Defn = GTIFGetProj4Defn(&defn);
   if (pProj4Defn == NULL)
   {
      XTIFFClose(pTiffFile);
      return false;
   }

   if (GTIFImageToPCS(pGeoTiff, &geotiffPixelX, &geotiffPixelY))
   {
      if (defn.Model != ModelTypeGeographic)
      {
         GTIFProj4ToLatLong(&defn, 1, &geotiffPixelX, &geotiffPixelY);
      }

      gcp.mPixel.mX = pixelX;
      gcp.mPixel.mY = pixelY;
      gcp.mCoordinate.mX = geotiffPixelY;
      gcp.mCoordinate.mY = geotiffPixelX;

      XTIFFClose(pTiffFile);
      return true;
   }

   XTIFFClose(pTiffFile);
   return false;
}

std::list<GcpPoint> DgUtilities::parseGcps(const std::vector<DgFileTile>& tiles)
{
   bool matchingTile = false;
   double tilePixelX = 0.0;
   double tilePixelY = 0.0;
   double pixelX = 0.0;
   double pixelY = 0.0;
   std::list<GcpPoint> gcps;
   GcpPoint tempGcp;
   DgFileTile llTile;
   DgFileTile ulTile;
   DgFileTile lrTile;
   DgFileTile urTile;
   for (unsigned int i = 0; i < tiles.size(); ++i)
   {
      DgFileTile tile = tiles[i];
      if (tile.mStartCol == 0 && tile.mStartRow == 0)
      {
         ulTile = tile;
      }
      if (tile.mStartCol == 0 && tile.mEndRow > llTile.mEndRow)
      {
         llTile = tile;
      }
      if (tile.mStartRow == 0 && tile.mEndCol > urTile.mEndCol)
      {
         urTile = tile;
      }
      if (tile.mEndCol > lrTile.mEndCol && tile.mEndRow > lrTile.mEndRow)
      {
         lrTile = tile;
      }
   }

   if (!ulTile.mTilFilename.empty() && parseGcpFromGeotiff(ulTile.mTilFilename, 0.0, 0.0,
      0.0, 0.0, tempGcp))
   {
      gcps.push_back(tempGcp);
   }
   if (!llTile.mTilFilename.empty() && parseGcpFromGeotiff(llTile.mTilFilename, 0.0, llTile.mEndRow,
      0.0, llTile.mEndRow - llTile.mStartRow, tempGcp))
   {
      gcps.push_back(tempGcp);
   }
   if (!urTile.mTilFilename.empty() && parseGcpFromGeotiff(urTile.mTilFilename, urTile.mEndCol, 0.0,
      urTile.mEndCol - urTile.mStartCol, 0.0, tempGcp))
   {
      gcps.push_back(tempGcp);
   }
   if (!lrTile.mTilFilename.empty() && parseGcpFromGeotiff(lrTile.mTilFilename, lrTile.mEndCol, lrTile.mEndRow,
      lrTile.mEndCol - lrTile.mStartCol, lrTile.mEndRow - lrTile.mStartRow, tempGcp))
   {
      gcps.push_back(tempGcp);
   }
   return gcps;
}

void DgUtilities::parseRpcs(DynamicObject* pMetadata)
{
   std::string specId = dv_cast<std::string>(pMetadata->getAttributeByPath("DIGITALGLOBE_ISD/RPB/SPECID"), "");
   if (specId != "RPC00B")
   {
      return;
   }
   DynamicObject* pRpbObject = dv_cast<DynamicObject>(&pMetadata->getAttributeByPath("DIGITALGLOBE_ISD/RPB/IMAGE"));
   if (pRpbObject == NULL)
   {
      return;
   }
   bool error = false; 
   double errBias = StringUtilities::fromXmlString<double>(dv_cast<std::string>(
      pRpbObject->getAttribute("ERRBIAS"), ""), &error);
   if (error)
   {
      return;
   }
   double errRand = StringUtilities::fromXmlString<double>(dv_cast<std::string>(
      pRpbObject->getAttribute("ERRRAND"), ""), &error);
   if (error)
   {
      return;
   }
   unsigned int lineOffset = StringUtilities::fromXmlString<unsigned int>(dv_cast<std::string>(
      pRpbObject->getAttribute("LINEOFFSET"), ""), &error);
   if (error)
   {
      return;
   }
   unsigned int sampOffset = StringUtilities::fromXmlString<unsigned int>(dv_cast<std::string>(
      pRpbObject->getAttribute("SAMPOFFSET"), ""), &error);
   if (error)
   {
      return;
   }
   double latOffset = StringUtilities::fromXmlString<double>(dv_cast<std::string>(
      pRpbObject->getAttribute("LATOFFSET"), ""), &error);
   if (error)
   {
      return;
   }
   double lonOffset = StringUtilities::fromXmlString<double>(dv_cast<std::string>(
      pRpbObject->getAttribute("LONGOFFSET"), ""), &error);
   if (error)
   {
      return;
   }
   int heightOffset = StringUtilities::fromXmlString<int>(dv_cast<std::string>(
      pRpbObject->getAttribute("HEIGHTOFFSET"), ""), &error);
   if (error)
   {
      return;
   }
   unsigned int lineScale = StringUtilities::fromXmlString<unsigned int>(dv_cast<std::string>(
      pRpbObject->getAttribute("LINESCALE"), ""), &error);
   if (error)
   {
      return;
   }
   unsigned int sampScale = StringUtilities::fromXmlString<unsigned int>(dv_cast<std::string>(
      pRpbObject->getAttribute("SAMPSCALE"), ""), &error);
   if (error)
   {
      return;
   }
   double latScale = StringUtilities::fromXmlString<double>(dv_cast<std::string>(
      pRpbObject->getAttribute("LATSCALE"), ""), &error);
   if (error)
   {
      return;
   }
   double lonScale = StringUtilities::fromXmlString<double>(dv_cast<std::string>(
      pRpbObject->getAttribute("LONGSCALE"), ""), &error);
   if (error)
   {
      return;
   }
   int heightScale = StringUtilities::fromXmlString<int>(dv_cast<std::string>(
      pRpbObject->getAttribute("HEIGHTSCALE"), ""), &error);
   if (error)
   {
      return;
   }
   std::vector<double> lineNumCoef = parseTextIntoVector<double>(dv_cast<std::string>(
      pRpbObject->getAttributeByPath("LINENUMCOEFList/LINENUMCOEF"), ""));
   if (lineNumCoef.empty())
   {
      return;
   }
   std::vector<double> lineDenCoef = parseTextIntoVector<double>(dv_cast<std::string>(
      pRpbObject->getAttributeByPath("LINEDENCOEFList/LINEDENCOEF"), ""));
   if (lineDenCoef.size() != lineNumCoef.size())
   {
      return;
   }
   std::vector<double> sampNumCoef = parseTextIntoVector<double>(dv_cast<std::string>(
      pRpbObject->getAttributeByPath("SAMPNUMCOEFList/SAMPNUMCOEF"), ""));
   if (sampNumCoef.size() != lineNumCoef.size())
   {
      return;
   }
   std::vector<double> sampDefCoef = parseTextIntoVector<double>(dv_cast<std::string>(
      pRpbObject->getAttributeByPath("SAMPDENCOEFList/SAMPDENCOEF"), ""));
   if (sampDefCoef.size() != lineNumCoef.size())
   {
      return;
   }

   //parse succeeded at this point, update metadata now
   pMetadata->setAttributeByPath("NITF/TRE/RPC00B/0/SUCCESS", true);
   FactoryResource<DynamicObject> image;
   pMetadata->setAttributeByPath("NITF/Image Subheader", *image);
   pMetadata->setAttributeByPath("NITF/TRE/RPC00B/0/ERR_BIAS", errBias);
   pMetadata->setAttributeByPath("NITF/TRE/RPC00B/0/ERR_RAND", errRand);
   pMetadata->setAttributeByPath("NITF/TRE/RPC00B/0/LINE_OFF", lineOffset);
   pMetadata->setAttributeByPath("NITF/TRE/RPC00B/0/SAMP_OFF", sampOffset);
   pMetadata->setAttributeByPath("NITF/TRE/RPC00B/0/LAT_OFF", latOffset);
   pMetadata->setAttributeByPath("NITF/TRE/RPC00B/0/LONG_OFF", lonOffset);
   pMetadata->setAttributeByPath("NITF/TRE/RPC00B/0/HEIGHT_OFF", heightOffset);
   pMetadata->setAttributeByPath("NITF/TRE/RPC00B/0/LINE_SCALE", lineScale);
   pMetadata->setAttributeByPath("NITF/TRE/RPC00B/0/SAMP_SCALE", sampScale);
   pMetadata->setAttributeByPath("NITF/TRE/RPC00B/0/LAT_SCALE", latScale);
   pMetadata->setAttributeByPath("NITF/TRE/RPC00B/0/LONG_SCALE", lonScale);
   pMetadata->setAttributeByPath("NITF/TRE/RPC00B/0/HEIGHT_SCALE", heightScale);
   for (unsigned int index = 0; index < lineNumCoef.size(); ++index)
   {
      pMetadata->setAttributeByPath(QString("NITF/TRE/RPC00B/0/LNNUMCOEF%1")
         .arg(index + 1, 2, 10, QChar('0')).toStdString(), lineNumCoef[index]);
   }
   for (unsigned int index = 0; index < lineDenCoef.size(); ++index)
   {
      pMetadata->setAttributeByPath(QString("NITF/TRE/RPC00B/0/LNDENCOEF%1")
         .arg(index + 1, 2, 10, QChar('0')).toStdString(), lineDenCoef[index]);
   }
   for (unsigned int index = 0; index < sampNumCoef.size(); ++index)
   {
      pMetadata->setAttributeByPath(QString("NITF/TRE/RPC00B/0/SMPNUMCOEF%1")
         .arg(index + 1, 2, 10, QChar('0')).toStdString(), sampNumCoef[index]);
   }
   for (unsigned int index = 0; index < sampDefCoef.size(); ++index)
   {
      pMetadata->setAttributeByPath(QString("NITF/TRE/RPC00B/0/SMPDENCOEF%1")
         .arg(index + 1, 2, 10, QChar('0')).toStdString(), sampDefCoef[index]);
   }
}

bool DgUtilities::parseBasicsFromTiff(std::string filename, RasterDataDescriptor* pDescriptor)
{
   if (pDescriptor == NULL)
   {
      return false;
   }

   RasterFileDescriptor* pFileDescriptor = dynamic_cast<RasterFileDescriptor*>
      (pDescriptor->getFileDescriptor());
   if (pFileDescriptor == NULL)
   {
      return false;
   }

   {
      // Check the first four bytes for TIFF magic number

      //force file to be closed when scope block ends
      FileResource pFile(filename.c_str(), "r");
      if (pFile.get() != NULL)
      {
         const unsigned short tiffBigEndianMagicNumber = 0x4d4d;
         const unsigned short tiffLittleEndianMagicNumber = 0x4949;
         const unsigned short tiffVersionMagicNumber = 42;

         unsigned short fileEndian;
         fread(&fileEndian, sizeof(fileEndian), 1, pFile);

         if ( (fileEndian == tiffBigEndianMagicNumber) || (fileEndian == tiffLittleEndianMagicNumber) )
         {
            unsigned short tiffVersion;
            fread(&tiffVersion, sizeof(tiffVersion), 1, pFile);

            EndianType fileEndianType = 
               (fileEndian == tiffBigEndianMagicNumber ? BIG_ENDIAN_ORDER : LITTLE_ENDIAN_ORDER);
            Endian swapper(fileEndianType);
            swapper.swapBuffer(&tiffVersion, 1);

            if (tiffVersion != tiffVersionMagicNumber)
            {
               return false;
            }
            pFileDescriptor->setEndian(fileEndianType);
         }
         else
         {
            return false;
         }
      }
   }

   TIFF* pTiffFile = XTIFFOpen(filename.c_str(), "r");
   if (pTiffFile == NULL)
   {
      return false;
   }

   // Check for unsupported palette data
   unsigned short photometric = 0;
   TIFFGetField(pTiffFile, TIFFTAG_PHOTOMETRIC, &photometric);

   if (photometric == PHOTOMETRIC_PALETTE)
   {
      XTIFFClose(pTiffFile);
      return false;
   }

   // Bands
   unsigned short numBands = 1;
   TIFFGetField(pTiffFile, TIFFTAG_SAMPLESPERPIXEL, &numBands);

   std::vector<DimensionDescriptor> bands = RasterUtilities::generateDimensionVector(numBands, true, false, true);

   pDescriptor->setBands(bands);
   pFileDescriptor->setBands(bands);

   // Bits per pixel
   unsigned short bitsPerElement = 0;
   TIFFGetField(pTiffFile, TIFFTAG_BITSPERSAMPLE, &bitsPerElement);

   pFileDescriptor->setBitsPerElement(bitsPerElement);

   // Data type
   unsigned short sampleFormat = SAMPLEFORMAT_VOID;
   TIFFGetField(pTiffFile, TIFFTAG_SAMPLEFORMAT, &sampleFormat);

   EncodingType dataType = INT1UBYTE;

   unsigned int bytesPerElement = bitsPerElement / 8;
   switch (bytesPerElement)
   {
      case 1:
         if (sampleFormat == SAMPLEFORMAT_INT)
         {
            dataType = INT1SBYTE;
         }
         else
         {
            dataType = INT1UBYTE;
         }
         break;

      case 2:
         if (sampleFormat == SAMPLEFORMAT_INT)
         {
            dataType = INT2SBYTES;
         }
         else
         {
            dataType = INT2UBYTES;
         }
         break;

      case 4:
         if (sampleFormat == SAMPLEFORMAT_INT)
         {
            dataType = INT4SBYTES;
         }
         else if (sampleFormat == SAMPLEFORMAT_IEEEFP)
         {
            dataType = FLT4BYTES;
         }
         else
         {
            dataType = INT4UBYTES;
         }
         break;

      case 8:
         dataType = FLT8BYTES;
         break;

      default:
         break;
   }

   pDescriptor->setDataType(dataType);
   pDescriptor->setValidDataTypes(std::vector<EncodingType>(1, pDescriptor->getDataType()));

   // Interleave format
   unsigned short planarConfig = 0;
   TIFFGetField(pTiffFile, TIFFTAG_PLANARCONFIG, &planarConfig);

   if (planarConfig == PLANARCONFIG_SEPARATE)
   {
      pFileDescriptor->setInterleaveFormat(BSQ);
   }
   else if (planarConfig == PLANARCONFIG_CONTIG)
   {
      pFileDescriptor->setInterleaveFormat(BIP);
   }

   pDescriptor->setInterleaveFormat(pFileDescriptor->getInterleaveFormat());

   // Close the TIFF file
   XTIFFClose(pTiffFile);
   return true;
}
/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "DateTime.h"
#include "DimensionDescriptor.h"
#include "DynamicObject.h"
#include "Endian.h"
#include "FileResource.h"
#include "GcpList.h"
#include "LandsatUtilities.h"
#include "LocationType.h"
#include "RasterDataDescriptor.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "SpecialMetadata.h"
#include "SpectralUtilities.h"
#include "Wavelengths.h"

#include <QtCore/QDate>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <geotiff.h>
#include <geovalues.h>
#include <geo_normalize.h>
#include <xtiffio.h>

#include <stdlib.h>

using namespace std;

namespace Landsat
{
   double LatLongConvert(string strInputLatLongData)
   {
      int iStringLength = 0;	//len
      double dblTemperature = 0.0;		//temp
      double dblDegrees = 0.0;			//deg
      double dblMinutes = 0.0;			//min
      double dblSeconds = 0.0;			//sec
      double dblValue = 0.0;			//value
      string strHemisphereCode = "";
      string strLatLongData = "";

      iStringLength = strInputLatLongData.length();							  //get length total length of the string
      strHemisphereCode = strInputLatLongData.at(iStringLength - 1);			  //pull off the directional character   
      strLatLongData.append( strInputLatLongData.c_str(), iStringLength - 1 );   //get the actual lat/long data

      dblTemperature = atof( strLatLongData.c_str() );      //convert the string to a double

      dblDegrees = (int)( dblTemperature / 10000 );
      dblMinutes = (int)(( dblTemperature - 10000 * dblDegrees ) / 100 );
      dblSeconds = ( dblTemperature - 10000 * dblDegrees ) - ( 100 * dblMinutes );

      dblValue = dblDegrees + (dblMinutes / 60) + (dblSeconds / 3600);

      //This checks to see if the data is in South or West Hemisphere and applies a negative value to the coordinate
      if (strHemisphereCode == "s" || strHemisphereCode == "S" || strHemisphereCode == "w" || strHemisphereCode == "W")
      {
         dblValue = (dblValue * -1.00);
      }

      return dblValue;
   }

   FactoryResource<DynamicObject> parseMtlFile(const std::string& filename, bool& success)
   {
      success = false;
      FactoryResource<DynamicObject> pObject;
      LargeFileResource landsatFile;
      if (!landsatFile.open(filename, O_RDONLY | O_BINARY, S_IREAD))
      {
         return pObject;
      }
      std::vector<char> buffer(200, 0); //read first 200 bytes
      landsatFile.read(&(buffer.front()), buffer.size());
      QString strReadIn(&(buffer.front()));
      if (!strReadIn.startsWith("GROUP = L1_METADATA_FILE"))
      {
         return pObject;
      }
      landsatFile.seek(0, SEEK_SET);
      bool failedRead = true;
      QString curPath;
      QStringList curParents;
      while (!landsatFile.eof())
      {
         failedRead = true;
         QString line = QString::fromStdString(landsatFile.readLine(&failedRead));
         if (failedRead)
         {
            break;
         }
         line.replace("\"", "");
         QStringList contents = line.split("=");
         QString group;
         QString value;
         if (!contents.empty())
         {
            group = contents.takeFirst();
            group = group.toUpper().trimmed();
         }
         if (!contents.empty())
         {
            value = contents.join("=").trimmed();
         }
         if (group == "END;" || group == "END")
         {
            break;
         }
         if (group == "BEGIN_GROUP" || group == "GROUP")
         {
            curParents.append(value);
            curPath = curParents.join("/");
            continue;
         }
         if (group == "END_GROUP")
         {
            curParents.takeLast();
            curPath = curParents.join("/");
            continue;
         }
         std::string attributePath = QString(curPath + "/" + group).toStdString();
         pObject->setAttributeByPath(attributePath, value.toStdString());
      }
      //rename L1_METADATA_FILE to LANDSAT_MTL
      FactoryResource<DynamicObject> pMetadata;
      pMetadata->setAttribute("LANDSAT_MTL", *pObject.get());
      success = !failedRead;
      return pMetadata;
   }

   bool parseGcpFromGeotiff(std::string filename, double geotiffPixelX, double geotiffPixelY, GcpPoint& gcp)
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

      double pixelX = geotiffPixelX;
      double pixelY = geotiffPixelY;
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

   bool parseBasicsFromTiff(std::string filename, RasterDataDescriptor* pDescriptor)
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

            if ((fileEndian == tiffBigEndianMagicNumber) || (fileEndian == tiffLittleEndianMagicNumber))
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

      // Rows
      unsigned int numRows = 0;
      TIFFGetField(pTiffFile, TIFFTAG_IMAGELENGTH, &numRows);

      vector<DimensionDescriptor> rows = RasterUtilities::generateDimensionVector(numRows, true, false, true);
      pDescriptor->setRows(rows);
      pFileDescriptor->setRows(rows);

      // Columns
      unsigned int numColumns = 0;
      TIFFGetField(pTiffFile, TIFFTAG_IMAGEWIDTH, &numColumns);

      vector<DimensionDescriptor> columns = RasterUtilities::generateDimensionVector(numColumns, true, false, true);

      pDescriptor->setColumns(columns);
      pFileDescriptor->setColumns(columns);

      // Bands
      unsigned short numBands = 1;
      TIFFGetField(pTiffFile, TIFFTAG_SAMPLESPERPIXEL, &numBands);

      vector<DimensionDescriptor> bands = RasterUtilities::generateDimensionVector(numBands, true, false, true);

      pDescriptor->setBands(bands);
      pFileDescriptor->setBands(bands);

      // Corner Coordinates
      std::list<GcpPoint> gcps;
      GcpPoint temp;
      if (parseGcpFromGeotiff(filename, 0, 0, temp))
      {
         //UL
         gcps.push_back(temp);
      }
      if (parseGcpFromGeotiff(filename, numColumns - 1, 0, temp))
      {
         //UR
         gcps.push_back(temp);
      }
      if (parseGcpFromGeotiff(filename, 0, numRows - 1, temp))
      {
         //LL
         gcps.push_back(temp);
      }
      if (parseGcpFromGeotiff(filename, numColumns - 1, numRows - 1, temp))
      {
         //LR
         gcps.push_back(temp);
      }
      if (parseGcpFromGeotiff(filename, (numColumns - 1) / 2.0, (numRows - 1) / 2.0, temp))
      {
         //Center
         gcps.push_back(temp);
      }
      pFileDescriptor->setGcps(gcps);

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
      pDescriptor->setValidDataTypes(vector<EncodingType>(1, dataType));

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

   std::vector<double> calculate_bias(const std::vector<double>& lMins,
      const std::vector<double>& qcalMins,
      const std::vector<double>& gains)
   {
      std::vector<double> biases;
      double bias;
      if (lMins.size() != qcalMins.size() ||
          lMins.size() != gains.size())
      {
         return biases;
      }
      for (unsigned int index = 0; index < lMins.size(); ++index)
      {
         bias = lMins[index] - (gains[index] * qcalMins[index]);
         biases.push_back(bias);
      }
      return biases;
   }

   std::vector<double> calculate_gain(const std::vector<double>& lMins,
      const std::vector<double>& lMaxs, const std::vector<double>& qcalMins,
      const std::vector<double>& qcalMaxs)
   {
      std::vector<double> gains;
      double numerator;
      double denominator;
      double gain;
      if (lMins.size() != lMaxs.size() || lMins.size() != qcalMins.size() ||
          lMins.size() != qcalMaxs.size())
      {
         return gains;
      }
      for (unsigned int index = 0; index < lMins.size(); ++index)
      {
         numerator = lMaxs[index] - lMins[index];
         denominator = qcalMaxs[index] - qcalMins[index];
         if (fabs(denominator) == 0.0)
         {
            gains.clear();
            break;
         }
         gain = numerator / denominator;
         gains.push_back(gain);
      }
      return gains;
   }

   void fixMtlMetadata(DynamicObject* pMetadata, LandsatImageType type,
      const std::vector<unsigned int>& validBands)
   {
      std::string spacecraft = dv_cast<std::string>(
         pMetadata->getAttributeByPath("LANDSAT_MTL/L1_METADATA_FILE/PRODUCT_METADATA/SPACECRAFT_ID"), "");
      std::vector<std::string> tempBandNames = getSensorBandNames(spacecraft, type);
      std::vector<std::string> bandNames;
      for (std::vector<unsigned int>::const_iterator validBandIter = validBands.begin();
         validBandIter != validBands.end();
         ++validBandIter)
      {
         if (*validBandIter < tempBandNames.size())
         {
            bandNames.push_back(tempBandNames[*validBandIter]);
         }
      }
      std::vector<double> lMins;
      lMins = getSensorBandValues<double>(pMetadata, bandNames, "MIN_MAX_RADIANCE/LMIN_BAND", "");
      if (lMins.size() == bandNames.size())
      {
         pMetadata->setAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/LandsatMin", lMins);
      }
      std::vector<double> lMaxs;
      lMaxs = getSensorBandValues<double>(pMetadata, bandNames, "MIN_MAX_RADIANCE/LMAX_BAND", "");
      if (lMaxs.size() == bandNames.size())
      {
         pMetadata->setAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/LandsatMax", lMaxs);
      }
      std::vector<double> qcalMins;
      qcalMins = getSensorBandValues<double>(pMetadata, bandNames, "MIN_MAX_PIXEL_VALUE/QCALMIN_BAND", "");
      if (qcalMins.size() == bandNames.size())
      {
         pMetadata->setAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/LandsatCalMin", qcalMins);
      }
      std::vector<double> qcalMaxs;
      qcalMaxs = getSensorBandValues<double>(pMetadata, bandNames, "MIN_MAX_PIXEL_VALUE/QCALMAX_BAND", "");
      if (qcalMaxs.size() == bandNames.size())
      {
         pMetadata->setAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/LandsatCalMax", qcalMaxs);
      }
      std::vector<double> gains = calculate_gain(lMins, lMaxs, qcalMins, qcalMaxs);
      if (gains.size() == bandNames.size())
      {
         pMetadata->setAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/LandsatScale", gains);
      }
      std::vector<double> biases = calculate_bias(lMins, qcalMins, gains);
      if (biases.size() == bandNames.size())
      {
         pMetadata->setAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/LandsatBias", biases);
      }

      std::vector<std::string> correctionMethod;
      correctionMethod = getSensorBandValues<std::string>(pMetadata, bandNames,
         "PRODUCT_PARAMETERS/CORRECTION_METHOD_GAIN_BAND", "");
      if (correctionMethod.size() == bandNames.size())
      {
         pMetadata->setAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/LandsatCorrectionMethod",
            correctionMethod);
      }

      std::string dateText = dv_cast<std::string>(pMetadata->getAttributeByPath(
         "LANDSAT_MTL/L1_METADATA_FILE/PRODUCT_METADATA/ACQUISITION_DATE"), "");
      std::string timeText = dv_cast<std::string>(pMetadata->getAttributeByPath(
         "LANDSAT_MTL/L1_METADATA_FILE/PRODUCT_METADATA/SCENE_CENTER_SCAN_TIME"), "");
      if (!dateText.empty() && !timeText.empty())
      {
         FactoryResource<DateTime> pDateTime;
         std::string dateTimeText = dateText + "T" + timeText;
         if (pDateTime->set(dateTimeText))
         {
            pMetadata->setAttributeByPath(COLLECTION_DATE_TIME_METADATA_PATH, *pDateTime.get());
         }
      }

      //wavelengths and band names
      std::vector<std::string> bandTextNames;
      std::vector<double> startWaves;
      std::vector<double> centerWaves;
      std::vector<double> endWaves;
      if (type == LANDSAT_VNIR)
      {
         if (spacecraft == "Landsat5")
         {
            bandTextNames.push_back("TM1");
            bandTextNames.push_back("TM2");
            bandTextNames.push_back("TM3");
            bandTextNames.push_back("TM4");
            bandTextNames.push_back("TM5");
            bandTextNames.push_back("TM7");
            startWaves.push_back(0.45);
            startWaves.push_back(0.52);
            startWaves.push_back(0.63);
            startWaves.push_back(0.76);
            startWaves.push_back(1.55);
            startWaves.push_back(2.08);
            centerWaves.push_back(0.485);
            centerWaves.push_back(0.56);
            centerWaves.push_back(0.66);
            centerWaves.push_back(0.83);
            centerWaves.push_back(1.65);
            centerWaves.push_back(2.215);
            endWaves.push_back(0.52);
            endWaves.push_back(0.6);
            endWaves.push_back(0.69);
            endWaves.push_back(0.9);
            endWaves.push_back(1.75);
            endWaves.push_back(2.35);
         }
         else if (spacecraft == "Landsat7")
         {
            bandTextNames.push_back("ETM1");
            bandTextNames.push_back("ETM2");
            bandTextNames.push_back("ETM3");
            bandTextNames.push_back("ETM4");
            bandTextNames.push_back("ETM5");
            bandTextNames.push_back("ETM7");
            startWaves.push_back(0.45);
            startWaves.push_back(0.525);
            startWaves.push_back(0.63);
            startWaves.push_back(0.75);
            startWaves.push_back(1.55);
            startWaves.push_back(2.09);
            centerWaves.push_back(0.483);
            centerWaves.push_back(0.565);
            centerWaves.push_back(0.66);
            centerWaves.push_back(0.825);
            centerWaves.push_back(1.65);
            centerWaves.push_back(2.22);
            endWaves.push_back(0.515);
            endWaves.push_back(0.605);
            endWaves.push_back(0.69);
            endWaves.push_back(0.9);
            endWaves.push_back(1.75);
            endWaves.push_back(2.35);
         }
      }
      else if (type == LANDSAT_PAN)
      {
         if (spacecraft == "Landsat7")
         {
            bandTextNames.push_back("ETM-PAN");
            startWaves.push_back(0.52);
            centerWaves.push_back(0.71);
            endWaves.push_back(0.90);
         }
      }
      else if (type == LANDSAT_TIR)
      {
         if (spacecraft == "Landsat5")
         {
            bandTextNames.push_back("TM6");
            startWaves.push_back(10.4);
            centerWaves.push_back(11.45);
            endWaves.push_back(12.5);
         }
         else if (spacecraft == "Landsat7")
         {
            bandTextNames.push_back("ETM61");
            bandTextNames.push_back("ETM62");
            startWaves.push_back(10.4);
            startWaves.push_back(10.4);
            centerWaves.push_back(11.45);
            centerWaves.push_back(11.45);
            endWaves.push_back(12.5);
            endWaves.push_back(12.5);
         }
      }
      std::vector<std::string> finalBandTextNames;
      std::vector<double> finalStartWaves;
      std::vector<double> finalCenterWaves;
      std::vector<double> finalEndWaves;
      for (std::vector<unsigned int>::const_iterator validBandIter = validBands.begin();
         validBandIter != validBands.end();
         ++validBandIter)
      {
         if (*validBandIter < bandTextNames.size())
         {
            finalBandTextNames.push_back(bandTextNames[*validBandIter]);
         }
         if (*validBandIter < startWaves.size())
         {
            finalStartWaves.push_back(startWaves[*validBandIter]);
         }
         if (*validBandIter < centerWaves.size())
         {
            finalCenterWaves.push_back(centerWaves[*validBandIter]);
         }
         if (*validBandIter < endWaves.size())
         {
            finalEndWaves.push_back(endWaves[*validBandIter]);
         }
      }

      if (!finalStartWaves.empty())
      {
         FactoryResource<Wavelengths> pWaves;
         pWaves->setStartValues(finalStartWaves, MICRONS);
         pWaves->setCenterValues(finalCenterWaves, MICRONS);
         pWaves->setEndValues(finalEndWaves, MICRONS);
         pWaves->applyToDynamicObject(pMetadata);
         pMetadata->setAttributeByPath(SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/" + NAMES_METADATA_NAME,
            finalBandTextNames);
      }
   }

   std::vector<std::string> getGeotiffBandFilenames(const DynamicObject* pMetadata,
      std::string filename, LandsatImageType type, std::vector<unsigned int>& validBands)
   {
      validBands.clear();
      QFileInfo fileInfo(QString::fromStdString(filename));
      QDir fileDir = fileInfo.dir();
      std::vector<std::string> filenames;
      std::string spacecraft = dv_cast<std::string>(
         pMetadata->getAttributeByPath("LANDSAT_MTL/L1_METADATA_FILE/PRODUCT_METADATA/SPACECRAFT_ID"), "");
      std::vector<std::string> bandNames = getSensorBandNames(spacecraft, type);
      if (bandNames.empty())
      {
         return filenames;
      }
      std::vector<std::string> tempFilenames = 
         getSensorBandValues<std::string>(pMetadata, bandNames, "PRODUCT_METADATA/BAND", "_FILE_NAME");
      for (unsigned int i = 0; i < tempFilenames.size(); ++i)
      {
         if (tempFilenames[i].empty())
         {
            continue;
         }
         QString filePath = fileDir.filePath(QString::fromStdString(tempFilenames[i]));
         if (!QFile::exists(filePath))
         {
            continue;
         }
         filenames.push_back(filePath.toStdString());
         validBands.push_back(i);
      }
      return filenames;
   }

   std::vector<std::string> getSensorBandNames(std::string spacecraft, LandsatImageType type)
   {
      std::vector<std::string> bandNames;
      if (type == LANDSAT_VNIR)
      {
         if (spacecraft == "Landsat5" || spacecraft == "Landsat7")
         {
            bandNames.push_back("1");
            bandNames.push_back("2");
            bandNames.push_back("3");
            bandNames.push_back("4");
            bandNames.push_back("5");
            bandNames.push_back("7");
         }
      }
      else if (type == LANDSAT_PAN)
      {
         if (spacecraft == "Landsat7")
         {
            bandNames.push_back("8");
         }
      }
      else if (type == LANDSAT_TIR)
      {
         if (spacecraft == "Landsat5")
         {
            bandNames.push_back("6");
         }
         else if (spacecraft == "Landsat7")
         {
            bandNames.push_back("61");
            bandNames.push_back("62");
         }
      }
      return bandNames;
   }

   std::vector<std::pair<double, double> > determineRadianceConversionFactors(const DynamicObject* pMetadata,
      LandsatImageType type, const std::vector<unsigned int>& validBands)
   {
      std::vector<std::pair<double, double> > factors;
      std::vector<double> biases = dv_cast<std::vector<double> >(pMetadata->getAttributeByPath(
         SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/LandsatBias"),
         std::vector<double>());
      std::vector<double> gains = dv_cast<std::vector<double> >(pMetadata->getAttributeByPath(
         SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/LandsatScale"),
         std::vector<double>());
      if (gains.size() != biases.size())
      {
         return factors;
      }
      for (unsigned int i = 0; i < biases.size(); ++i)
      {
         factors.push_back(std::make_pair(gains[i], biases[i]));
      }
      return factors;
   }

};

std::vector<double> Landsat::determineReflectanceConversionFactors(const DynamicObject* pMetadata,
   LandsatImageType type, const std::vector<unsigned int>& validBands)
{
   std::vector<double> tempFactors;
   std::string spacecraft = dv_cast<std::string>(
      pMetadata->getAttributeByPath("LANDSAT_MTL/L1_METADATA_FILE/PRODUCT_METADATA/SPACECRAFT_ID"), "");
   bool parseSunElevError = false;
   double solarElevationAngleInDegrees = StringUtilities::fromXmlString<double>(
      dv_cast<std::string>(pMetadata->getAttributeByPath(
      "LANDSAT_MTL/L1_METADATA_FILE/PRODUCT_PARAMETERS/SUN_ELEVATION"), ""), &parseSunElevError);
   const DateTime* pDateTime = dv_cast<DateTime>(&pMetadata->getAttributeByPath(COLLECTION_DATE_TIME_METADATA_PATH));
   if (spacecraft == "Landsat5" && !parseSunElevError && pDateTime != NULL)
   {
      if (type == Landsat::LANDSAT_VNIR)
      {
         tempFactors.push_back(determineL5ReflectanceConversionFactor(solarElevationAngleInDegrees,
            Landsat::L5_TM1, pDateTime));
         tempFactors.push_back(determineL5ReflectanceConversionFactor(solarElevationAngleInDegrees,
            Landsat::L5_TM2, pDateTime));
         tempFactors.push_back(determineL5ReflectanceConversionFactor(solarElevationAngleInDegrees,
            Landsat::L5_TM3, pDateTime));
         tempFactors.push_back(determineL5ReflectanceConversionFactor(solarElevationAngleInDegrees,
            Landsat::L5_TM4, pDateTime));
         tempFactors.push_back(determineL5ReflectanceConversionFactor(solarElevationAngleInDegrees,
            Landsat::L5_TM5, pDateTime));
         tempFactors.push_back(determineL5ReflectanceConversionFactor(solarElevationAngleInDegrees,
            Landsat::L5_TM7, pDateTime));
      }
   }
   else if (spacecraft == "Landsat7" && !parseSunElevError && pDateTime != NULL)
   {
      if (type == Landsat::LANDSAT_VNIR)
      {
         tempFactors.push_back(determineL7ReflectanceConversionFactor(solarElevationAngleInDegrees,
            Landsat::L7_ETM1, pDateTime));
         tempFactors.push_back(determineL7ReflectanceConversionFactor(solarElevationAngleInDegrees,
            Landsat::L7_ETM2, pDateTime));
         tempFactors.push_back(determineL7ReflectanceConversionFactor(solarElevationAngleInDegrees,
            Landsat::L7_ETM3, pDateTime));
         tempFactors.push_back(determineL7ReflectanceConversionFactor(solarElevationAngleInDegrees,
            Landsat::L7_ETM4, pDateTime));
         tempFactors.push_back(determineL7ReflectanceConversionFactor(solarElevationAngleInDegrees,
            Landsat::L7_ETM5, pDateTime));
         tempFactors.push_back(determineL7ReflectanceConversionFactor(solarElevationAngleInDegrees,
            Landsat::L7_ETM7, pDateTime));
      }
      else if (type == Landsat::LANDSAT_PAN)
      {
         tempFactors.push_back(determineL7ReflectanceConversionFactor(solarElevationAngleInDegrees,
            Landsat::L7_PAN, pDateTime));
      }
   }

   //subset out the factors based upon the valid band indices
   std::vector<double> factors;
   for (std::vector<unsigned int>::const_iterator validBandIter = validBands.begin();
      validBandIter != validBands.end();
      ++validBandIter)
   {
      if (*validBandIter < tempFactors.size())
      {
         factors.push_back(tempFactors[*validBandIter]);
      }
   }

   return factors;
}

double Landsat::getL5SolarIrradiance(L5BandsType band, bool& error)
{
   error = false;
   switch(band)
   {
   case L5_TM1:
      return 1957;
   case L5_TM2:
      return 1829;
   case L5_TM3:
      return 1557;
   case L5_TM4:
      return 1047;
   case L5_TM5:
      return 219.3;
   case L5_TM6:
      error = true;
      return 1.0;
   case L5_TM7:
      return 74.52;
   default:
      error = true;
      return 1.0;
   };
}

double Landsat::determineL5ReflectanceConversionFactor(double solarElevationAngleInDegrees,
   L5BandsType band, const DateTime* pDate)
{
   bool error = false;
   double solarIrradiance = getL5SolarIrradiance(band, error);
   if (error || pDate == NULL)
   {
      return 1.0;
   }
   return SpectralUtilities::determineReflectanceConversionFactor(solarElevationAngleInDegrees, solarIrradiance,
      *pDate);
}

double Landsat::getL7SolarIrradiance(L7BandsType band, bool& error)
{
   error = false;
   switch(band)
   {
   case L7_ETM1:
      return 1997;
   case L7_ETM2:
      return 1812;
   case L7_ETM3:
      return 1533;
   case L7_ETM4:
      return 1039;
   case L7_ETM5:
      return 230.8;
   case L7_ETM61:
      error = true;
      return 1.0;
   case L7_ETM62:
      error = true;
      return 1.0;
   case L7_ETM7:
      return 84.90;
   case L7_PAN:
      return 1362;
   default:
      error = true;
      return 1.0;
   };
}

double Landsat::determineL7ReflectanceConversionFactor(double solarElevationAngleInDegrees,
   L7BandsType band, const DateTime* pDate)
{
   bool error = false;
   double solarIrradiance = getL7SolarIrradiance(band, error);
   if (error || pDate == NULL)
   {
      return 1.0;
   }
   return SpectralUtilities::determineReflectanceConversionFactor(solarElevationAngleInDegrees, solarIrradiance,
      *pDate);
}

void Landsat::getL5TemperatureConstants(double& K1, double& K2)
{
   K1 = 607.76;
   K2 = 1260.56;
}

void Landsat::getL7TemperatureConstants(double& K1, double& K2)
{
   K1 = 666.09;
   K2 = 1282.71;
}

bool Landsat::getTemperatureConstants(const DynamicObject* pMetadata, LandsatImageType imageType,
   double& K1, double& K2)
{
   std::string spacecraft = dv_cast<std::string>(
      pMetadata->getAttributeByPath("LANDSAT_MTL/L1_METADATA_FILE/PRODUCT_METADATA/SPACECRAFT_ID"), "");
   if (imageType != Landsat::LANDSAT_TIR)
   {
      return false;
   }
   if (spacecraft == "Landsat5")
   {
      getL5TemperatureConstants(K1, K2);
      return true;
   }
   if (spacecraft == "Landsat7")
   {
      getL7TemperatureConstants(K1, K2);
      return true;
   }
   return false;
}
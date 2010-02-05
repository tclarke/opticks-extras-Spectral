/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtCore/QStringList>
#include <QtGui/QInputDialog>

#include "DesktopServices.h"
#include "FileResource.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "SpectralVersion.h"
#include "WavelengthTextImporter.h"

using namespace std;

REGISTER_PLUGIN_BASIC(SpectralWavelength, WavelengthTextImporter);

WavelengthTextImporter::WavelengthTextImporter()
{
   setName("Wavelength Text Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setDescription("Loads wavelength values from a file");
   setDescriptorId("{B41E83DC-B630-4806-A2DD-FEBFEAED4E9C}");
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

WavelengthTextImporter::~WavelengthTextImporter()
{
}

bool WavelengthTextImporter::getInputSpecification(PlugInArgList*& pArgList)
{
   bool bSuccess = WavelengthImporter::getInputSpecification(pArgList);
   if (bSuccess == true)
   {
      string description = "Valid wavelength units are \"Microns\", \"Nanometers\", "
         "\"Inverse Centimeters\", and \"Custom\".  If this arg value is not set or "
         "an invalid value is set, ";
      if (isBatch() == false)
      {
         description += "the user is prompted for the units.";
      }
      else
      {
         description += "microns are assumed to be the units.";
      }

      VERIFY(pArgList != NULL);
      VERIFY(pArgList->addArg<string>("Wavelength Units", description));
   }

   return bSuccess;
}

bool WavelengthTextImporter::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   // Extract the units input arg
   if (pInArgList != NULL)
   {
      string units;
      pInArgList->getPlugInArgValue<string>("Wavelength Units", units);

      // Do not use StringUtilities so that existing wizards will continue
      // to succeed if the StringUtilities implementation changes
      if (units == "Microns")
      {
         mUnits = Wavelengths::MICRONS;
      }
      else if (units == "Nanometers")
      {
         mUnits = Wavelengths::NANOMETERS;
      }
      else if (units == "Inverse Centimeters")
      {
         mUnits = Wavelengths::INVERSE_CENTIMETERS;
      }
      else if (units == "Custom")
      {
         mUnits = Wavelengths::CUSTOM;
      }
   }

   return WavelengthImporter::execute(pInArgList, pOutArgList);
}

bool WavelengthTextImporter::loadWavelengths(Wavelengths& wavelengths, string& errorMessage) const
{
   string filename = getFilename();
   if (filename.empty() == true)
   {
      errorMessage = "The wavelength filename is empty.";
      return false;
   }

   FileResource pFile(filename.c_str(), "rt");
   if (pFile.get() == NULL)
   {
      errorMessage = "The wavelength file could not be opened for read.";
      return false;
   }

   // Read lines until there is a non-comment line
   char lineText[512];

   bool bLineIsComment = true;
   while (bLineIsComment == true)
   {
      char* pLine = fgets(lineText, sizeof(lineText), pFile.get());
      if ((pLine == NULL) || (lineText[strlen(lineText) - 1] != '\n'))
      {
         errorMessage = "The wavelength file is not in the appropriate format.";
         return false;
      }

      bLineIsComment = (lineText[0] == '#');
   }

   // Determine the number of columns in the line
   double dWavelengths[4];

   int columns = sscanf(lineText, "%lf %lf %lf %lf", &dWavelengths[0], &dWavelengths[1], &dWavelengths[2],
      &dWavelengths[3]);
   if ((columns < 1) || (columns > 4))
   {
      // Invalid number of columns
      errorMessage = "The wavelength file is not in the appropriate format.";
      return false;
   }

   // Determine the order of the start, center, and end wavelengths
   int startIndex = 0;
   int centerIndex = 1;
   int endIndex = 2;

   if (columns > 2)
   {
      // Shift the wavelength values left in the array for comparisons
      if (columns == 4)
      {
         dWavelengths[0] = dWavelengths[1];
         dWavelengths[1] = dWavelengths[2];
         dWavelengths[2] = dWavelengths[3];
      }

      if ((dWavelengths[0] <= dWavelengths[1]) && (dWavelengths[1] <= dWavelengths[2]))
      {
         // Order is start, center, end
         startIndex = 0;
         centerIndex = 1;
         endIndex = 2;
      }
      else if ((dWavelengths[0] <= dWavelengths[1])&& (dWavelengths[1] >= dWavelengths[2]) &&
         (dWavelengths[2] >= dWavelengths[0]))
      {
         // Order is start, end, center
         startIndex = 0;
         centerIndex = 2;
         endIndex = 1;
      }
      else if ((dWavelengths[0] >= dWavelengths[1]) && (dWavelengths[1] <= dWavelengths[2]) &&
         (dWavelengths[2] >= dWavelengths[0]))
      {
         // Order is center, start, end
         startIndex = 1;
         centerIndex = 0;
         endIndex = 2;
      }
      else if ((dWavelengths[0] >= dWavelengths[1]) && (dWavelengths[1] <= dWavelengths[2]) &&
         (dWavelengths[2] <= dWavelengths[0]))
      {
         // Order is end, start, center
         startIndex = 1;
         centerIndex = 2;
         endIndex = 0;
      }
      else if ((dWavelengths[0] <= dWavelengths[1]) && (dWavelengths[1] >= dWavelengths[2]) &&
         (dWavelengths[2] <= dWavelengths[0]))
      {
         // Order is center, end, start
         startIndex = 2;
         centerIndex = 0;
         endIndex = 1;
      }
      else if ((dWavelengths[0] >= dWavelengths[1]) && (dWavelengths[1] >= dWavelengths[2]))
      {
         // Order is end, center, start
         startIndex = 2;
         centerIndex = 1;
         endIndex = 0;
      }
   }

   // Read the wavelength values
   vector<double> startValues;
   vector<double> centerValues;
   vector<double> endValues;

   bool bEof = false;
   while (bEof == false)
   {
      int band = 0;
      int numValues = 0;

      switch (columns)
      {
         case 1:
            numValues = sscanf(lineText, "%lf", &dWavelengths[0]);
            if (numValues == 1)
            {
               centerValues.push_back(dWavelengths[0]);
            }
            break;

         case 2:
            numValues = sscanf(lineText, "%d %lf", &band, &dWavelengths[0]);
            if (numValues == 2)
            {
               centerValues.push_back(dWavelengths[0]);
               errorMessage = "The band numbers in the wavelength file will be ignored so that the order of "
                  "the wavelength values is preserved.";
            }
            break;

         case 3:
            numValues = sscanf(lineText, "%lf %lf %lf", &dWavelengths[0], &dWavelengths[1], &dWavelengths[2]);
            if (numValues == 3)
            {
               startValues.push_back(dWavelengths[startIndex]);
               centerValues.push_back(dWavelengths[centerIndex]);
               endValues.push_back(dWavelengths[endIndex]);
            }
            break;

         case 4:
            numValues = sscanf(lineText, "%d %lf %lf %lf", &band, &dWavelengths[0], &dWavelengths[1], &dWavelengths[2]);
            if (numValues == 4)
            {
               startValues.push_back(dWavelengths[startIndex]);
               centerValues.push_back(dWavelengths[centerIndex]);
               endValues.push_back(dWavelengths[endIndex]);
               errorMessage = "The band numbers in the wavelength file will be ignored so that the order of "
                  "the wavelength values is preserved.";
            }
            break;

         default:
            break;
      }

      // Read lines until there is a non-comment line
      bLineIsComment = true;
      while (bLineIsComment == true)
      {
         if (feof(pFile.get()) != 0)
         {
            bEof = true;
            break;
         }

         fscanf(pFile.get(), "%[^\n]\n", lineText);
         bLineIsComment = (lineText[0] == '#');
      }
   }

   // Get the value units
   Wavelengths::WavelengthUnitsType units = mUnits;
   if (units.isValid() == false)
   {
      if (isBatch() == false)
      {
         QStringList unitList;
         unitList.append(QString::fromStdString(StringUtilities::toDisplayString(Wavelengths::MICRONS)));
         unitList.append(QString::fromStdString(StringUtilities::toDisplayString(Wavelengths::NANOMETERS)));
         unitList.append(QString::fromStdString(StringUtilities::toDisplayString(Wavelengths::INVERSE_CENTIMETERS)));
         unitList.append(QString::fromStdString(StringUtilities::toDisplayString(Wavelengths::CUSTOM)));

         Service<DesktopServices> pDesktop;

         QString strUnits = QInputDialog::getItem(pDesktop->getMainWidget(), "Select Wavelength Units", "Units:",
            unitList, 0, false);
         if (strUnits.isEmpty() == false)
         {
            units = StringUtilities::fromDisplayString<Wavelengths::WavelengthUnitsType>(strUnits.toStdString());
         }
      }
      else
      {
         if (errorMessage.empty() == false)
         {
            errorMessage += "  ";
         }

         errorMessage += "The wavelength values are assumed to be in microns.";
         units = Wavelengths::MICRONS;
      }
   }

   // Set the values into the wavelengths
   wavelengths.setUnits(units);
   wavelengths.setStartValues(startValues, units);
   wavelengths.setCenterValues(centerValues, units);
   wavelengths.setEndValues(endValues, units);

   return !(wavelengths.isEmpty());
}

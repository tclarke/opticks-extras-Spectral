/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "DynamicObject.h"
#include "FileResource.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "SpectralVersion.h"
#include "WavelengthMetadataExporter.h"
#include "xmlwriter.h"

using namespace std;

REGISTER_PLUGIN_BASIC(SpectralWavelength, WavelengthMetadataExporter);

WavelengthMetadataExporter::WavelengthMetadataExporter()
{
   setName("Wavelength Metadata Exporter");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setDescription("Saves wavelength values as stored in metadata to a file");
   setDescriptorId("{C11A6DDB-E9BF-4133-BBA4-B215F24A8097}");
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

WavelengthMetadataExporter::~WavelengthMetadataExporter()
{
}

bool WavelengthMetadataExporter::saveWavelengths(const Wavelengths& wavelengths) const
{
   string filename = getFilename();
   if (filename.empty() == true)
   {
      return false;
   }

   // Version
   XMLWriter xml("Wavelengths");
   xml.addAttr("version", Wavelengths::WavelengthFileVersion());

   // Units
   xml.addAttr("units", StringUtilities::toXmlString(wavelengths.getUnits()));

   // Wavelengths
   unsigned int numWavelengths = 0;

   const vector<double>& startWavelengths = wavelengths.getStartValues();
   if (startWavelengths.empty() == false)
   {
      numWavelengths = startWavelengths.size();
   }

   const vector<double>& centerWavelengths = wavelengths.getCenterValues();
   if (centerWavelengths.empty() == false)
   {
      unsigned int numCenterWavelengths = centerWavelengths.size();
      if (numWavelengths > 0)
      {
         if (numWavelengths != numCenterWavelengths)
         {
            return false;
         }
      }
      else
      {
         numWavelengths = numCenterWavelengths;
      }
   }

   const vector<double>& endWavelengths = wavelengths.getEndValues();
   if (endWavelengths.empty() == false)
   {
      unsigned int numEndWavelengths = endWavelengths.size();
      if (numWavelengths > 0)
      {
         if (numWavelengths != numEndWavelengths)
         {
            return false;
         }
      }
      else
      {
         numWavelengths = numEndWavelengths;
      }
   }

   for (unsigned int i = 0; i < numWavelengths; ++i)
   {
      xml.pushAddPoint(xml.addElement("value"));

      if (startWavelengths.empty() == false)
      {
         xml.addAttr("start", startWavelengths[i]);
      }

      if (centerWavelengths.empty() == false)
      {
         xml.addAttr("center", centerWavelengths[i]);
      }

      if (endWavelengths.empty() == false)
      {
         xml.addAttr("end", endWavelengths[i]);
      }

      xml.popAddPoint();
   }

   // Open the file for writing
   FileResource pFile(filename.c_str(), "wt");
   if (pFile.get() == NULL)
   {
      return false;
   }

   // Write the data to the file
   xml.writeToFile(pFile.get());

   return true;
}

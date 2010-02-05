/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "FileResource.h"
#include "PlugInRegistration.h"
#include "SpectralVersion.h"
#include "WavelengthTextExporter.h"

using namespace std;

REGISTER_PLUGIN_BASIC(SpectralWavelength, WavelengthTextExporter);

WavelengthTextExporter::WavelengthTextExporter()
{
   setName("Wavelength Text Exporter");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setDescription("Saves wavelength values to a file");
   setDescriptorId("{F2B48F8D-C78F-4FB0-9C21-52A2CB635BB3}");
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

WavelengthTextExporter::~WavelengthTextExporter()
{
}

bool WavelengthTextExporter::saveWavelengths(const Wavelengths& wavelengths) const
{
   const vector<double>& startValues = wavelengths.getStartValues();
   const vector<double>& centerValues = wavelengths.getCenterValues();
   const vector<double>& endValues = wavelengths.getEndValues();

   if (centerValues.empty() == true)
   {
      return false;
   }

   bool bThreeColumn = false;
   if ((startValues.empty() == false) && (endValues.empty() == false))
   {
      if ((startValues.size() == centerValues.size()) && (endValues.size() == centerValues.size()))
      {
         bThreeColumn = true;
      }
   }

   // Get the filename
   string filename = getFilename();
   if (filename.empty() == true)
   {
      return false;
   }

   // Open the file for writing
   FileResource pFile(filename.c_str(), "wt");
   if (pFile.get() == NULL)
   {
      return false;
   }

   // Save the wavelengths
   for (vector<double>::size_type i = 0; i < centerValues.size(); ++i)
   {
      if (bThreeColumn == true)
      {
         string startValue = StringUtilities::toXmlString(startValues[i]);
         string centerValue = StringUtilities::toXmlString(centerValues[i]);
         string endValue = StringUtilities::toXmlString(endValues[i]);
         fprintf(pFile.get(), "%s\t%s\t%s\n", startValue.c_str(), centerValue.c_str(), endValue.c_str());
      }
      else
      {
         string centerValue = StringUtilities::toXmlString(centerValues[i]);
         fprintf(pFile.get(), "%s\n", centerValue.c_str());
      }
   }

   return true;
}

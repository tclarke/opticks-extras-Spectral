/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtCore/QString>

#include "DynamicObject.h"
#include "PlugInRegistration.h"
#include "SpectralVersion.h"
#include "WavelengthMetadataImporter.h"
#include "xmlreader.h"

using namespace std;
XERCES_CPP_NAMESPACE_USE

REGISTER_PLUGIN_BASIC(SpectralWavelength, WavelengthMetadataImporter);

WavelengthMetadataImporter::WavelengthMetadataImporter()
{
   setName("Wavelength Metadata Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setDescription("Loads wavelength values in metadata format from a file");
   setDescriptorId("{F7683C81-97B4-4C91-B6F4-9C617254D534}");
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

WavelengthMetadataImporter::~WavelengthMetadataImporter()
{
}

bool WavelengthMetadataImporter::loadWavelengths(Wavelengths& wavelengths, string& errorMessage) const
{
   string filename = getFilename();
   if (filename.empty() == true)
   {
      errorMessage = "The wavelength filename is empty.";
      return false;
   }

   XmlReader xml(NULL, false);

   XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* pDocument = xml.parse(filename);
   if (pDocument != NULL)
   {
      DOMElement* pRootElement = pDocument->getDocumentElement();
      if (pRootElement != NULL)
      {
         // Check for a wavelengths file
         if (XMLString::equals(pRootElement->getNodeName(), X("Wavelengths")) == false)
         {
            errorMessage = "The file is not a valid wavelength file.";
            return false;
         }

         // Version
         unsigned int version =
            StringUtilities::fromXmlString<unsigned int>(A(pRootElement->getAttribute(X("version"))));
         if (version != Wavelengths::WavelengthFileVersion())
         {
            errorMessage = "The wavelength file version is not supported.";
            return false;
         }

         // Units
         bool bError = false;

         Wavelengths::WavelengthUnitsType units = StringUtilities::fromXmlString<Wavelengths::WavelengthUnitsType>(
            string(A(pRootElement->getAttribute(X("units")))), &bError);
         if (bError == true)
         {
            errorMessage = "Could not read the wavelength units in the file.";
            return false;
         }

         // Wavelengths
         vector<double> startWavelengths;
         vector<double> centerWavelengths;
         vector<double> endWavelengths;

         for (DOMNode* pChild = pRootElement->getFirstChild(); pChild != NULL; pChild = pChild->getNextSibling())
         {
            if (XMLString::equals(pChild->getNodeName(), X("value")))
            {
               DOMElement* pElement = static_cast<DOMElement*>(pChild);

               string start = A(pElement->getAttribute(X("start")));
               if (start.empty() == false)
               {
                  double wavelength = QString::fromStdString(start).toDouble();
                  startWavelengths.push_back(wavelength);
               }

               string center = A(pElement->getAttribute(X("center")));
               if (center.empty() == false)
               {
                  double wavelength = QString::fromStdString(center).toDouble();
                  centerWavelengths.push_back(wavelength);
               }

               string end = A(pElement->getAttribute(X("end")));
               if (end.empty() == false)
               {
                  double wavelength = QString::fromStdString(end).toDouble();
                  endWavelengths.push_back(wavelength);
               }
            }
         }

         // Set the values in the Wavelengths object
         wavelengths.setUnits(units);
         wavelengths.setStartValues(startWavelengths, units);
         wavelengths.setCenterValues(centerWavelengths, units);
         wavelengths.setEndValues(endWavelengths, units);

         return !(wavelengths.isEmpty());
      }
   }

   errorMessage = "The wavelength file is not in the appropriate format.";
   return false;
}

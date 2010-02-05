/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "Wavelengths.h"
#include "StringUtilitiesMacros.h"

// To and from string implementations
namespace StringUtilities
{
ENUM_MAPPING_PRE_DEFINITIONS(Wavelengths::WavelengthUnitsType, WavelengthsWavelengthUnitsType)
ENUM_MAPPING_TO_DISPLAY_STRING(Wavelengths::WavelengthUnitsType, WavelengthsWavelengthUnitsType)
ENUM_MAPPING_TO_DISPLAY_STRING_VEC(Wavelengths::WavelengthUnitsType)
ENUM_MAPPING_TO_XML_STRING(Wavelengths::WavelengthUnitsType, WavelengthsWavelengthUnitsType)
ENUM_MAPPING_TO_XML_STRING_VEC(Wavelengths::WavelengthUnitsType)
ENUM_MAPPING_FROM_DISPLAY_STRING(Wavelengths::WavelengthUnitsType, WavelengthsWavelengthUnitsType)
ENUM_MAPPING_FROM_DISPLAY_STRING_VEC(Wavelengths::WavelengthUnitsType)
ENUM_MAPPING_FROM_XML_STRING_VEC(Wavelengths::WavelengthUnitsType)
ENUM_GET_ALL_VALUES(Wavelengths::WavelengthUnitsType, WavelengthsWavelengthUnitsType)
ENUM_GET_ALL_VALUES_DISPLAY_STRING(Wavelengths::WavelengthUnitsType, WavelengthsWavelengthUnitsType)
ENUM_GET_ALL_VALUES_XML_STRING(Wavelengths::WavelengthUnitsType, WavelengthsWavelengthUnitsType)

ENUM_MAPPING_FUNCTION(WavelengthsWavelengthUnitsType)
ADD_ENUM_MAPPING(Wavelengths::MICRONS, "Microns", "microns")
ADD_ENUM_MAPPING(Wavelengths::NANOMETERS, "Nanometers", "nanometers")
ADD_ENUM_MAPPING(Wavelengths::INVERSE_CENTIMETERS, "Inverse Centimeters", "inverse_centimeters")
ADD_ENUM_MAPPING(Wavelengths::CUSTOM, "Custom", "custom")
END_ENUM_MAPPING()

template<>
Wavelengths::WavelengthUnitsType fromXmlString<Wavelengths::WavelengthUnitsType>(std::string value,
                                                                                                  bool* pError)
{
   if (pError != NULL)
   {
      *pError = false;
   }

   WavelengthsWavelengthUnitsTypeMapping values = getWavelengthsWavelengthUnitsTypeMapping();

   WavelengthsWavelengthUnitsTypeMapping::iterator foundValue =
      std::find_if(values.begin(), values.end(), FindTupleValue<2, string>(value));
   if (foundValue != values.end())
   {
      return foundValue->get<0>();
   }

   // compatibility values
   if (value == "um" || value == "µm" || value == "micrometers")
   {
      return Wavelengths::MICRONS;
   }
   else if (value == "nm")
   {
      return Wavelengths::NANOMETERS;
   }
   else if (value == "1/cm" || value == "cm-1" || value == "reciprocal centimeters" ||
      value == "wave number" || value == "k")
   {
      return Wavelengths::INVERSE_CENTIMETERS;
   }

   if (pError != NULL)
   {
      *pError = true;
   }

   return Wavelengths::WavelengthUnitsType();
}

};
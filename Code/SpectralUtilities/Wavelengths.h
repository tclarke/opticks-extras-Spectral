/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef WAVELENGTHS_H
#define WAVELENGTHS_H

#include "AttachmentPtr.h"
#include "DynamicObject.h"
#include "EnumWrapper.h"
#include "StringUtilities.h"

#include <vector>

class RasterElement;

class Wavelengths
{
public:
   /**
    *  Units for the spectral data wavelengths.
    *
    *  Wavelength values are defined and stored in microns.  However, they can
    *  be displayed to the user in several formats.
    */
   enum WavelengthUnitsTypeEnum { MICRONS, NANOMETERS, INVERSE_CENTIMETERS, CUSTOM };

   typedef EnumWrapper<WavelengthUnitsTypeEnum> WavelengthUnitsType;

   Wavelengths(DynamicObject* pWavelengthData);
   ~Wavelengths();

   void setStartValues(const std::vector<double>& startValues, WavelengthUnitsType valueUnits);
   const std::vector<double>& getStartValues() const;
   bool hasStartValues() const;

   void setCenterValues(const std::vector<double>& centerValues, WavelengthUnitsType valueUnits);
   const std::vector<double>& getCenterValues() const;
   bool hasCenterValues() const;

   void setEndValues(const std::vector<double>& endValues, WavelengthUnitsType valueUnits);
   const std::vector<double>& getEndValues() const;
   bool hasEndValues() const;

   void setUnits(WavelengthUnitsType units);
   WavelengthUnitsType getUnits() const;

   bool isEmpty() const;
   void scaleValues(double dScaleFactor);
   void calculateFwhm(double dConstant = 1.0);
   std::vector<double> getFwhm();

   bool initializeFromDynamicObject(const DynamicObject* pData);
   bool applyToDynamicObject(DynamicObject* pData) const;
   bool applyToDataset(RasterElement* pDataset) const;

   static double convertValue(double value, WavelengthUnitsType valueUnits, WavelengthUnitsType newUnits);
   static std::string WavelengthType() { return "Wavelength"; }
   static std::string WavelengthsArg() { return "Wavelengths"; }
   static std::string WavelengthFileArg() { return "Wavelength File"; }
   static unsigned int WavelengthFileVersion() { return 1; }

protected:
   std::vector<double>& getEditableStartValues();
   std::vector<double>& getEditableCenterValues();
   std::vector<double>& getEditableEndValues();
   void setActualUnits(WavelengthUnitsType units);
   WavelengthUnitsType getActualUnits(bool bVerify = true) const;
   void setDisplayUnits(WavelengthUnitsType units);
   WavelengthUnitsType getDisplayUnits(bool bVerify = true) const;
   void convertToDisplayUnits();
   bool copyWavelengthData(const DynamicObject* pSourceData, DynamicObject* pDestinationData) const;

private:
   AttachmentPtr<DynamicObject> mpWavelengthData;
};

namespace StringUtilities
{
   template<>
   std::string toDisplayString(const Wavelengths::WavelengthUnitsType& value, bool* pError);

   template<>
   std::string toXmlString(const Wavelengths::WavelengthUnitsType& value, bool* pError);

   template<>
   Wavelengths::WavelengthUnitsType fromDisplayString<Wavelengths::WavelengthUnitsType>(std::string valueText,
      bool* pError);

   template<>
   Wavelengths::WavelengthUnitsType fromXmlString<Wavelengths::WavelengthUnitsType>(std::string valueText,
      bool* pError);

   template<>
   std::string toDisplayString(const std::vector<Wavelengths::WavelengthUnitsType>& value, bool* pError);

   template<>
   std::string toXmlString(const std::vector<Wavelengths::WavelengthUnitsType>& value, bool* pError);

   template<>
   std::vector<Wavelengths::WavelengthUnitsType> fromDisplayString<std::vector<Wavelengths::WavelengthUnitsType> >(
      std::string valueText, bool* pError);

   template<>
   std::vector<Wavelengths::WavelengthUnitsType> fromXmlString<std::vector<Wavelengths::WavelengthUnitsType> >(
      std::string valueText, bool* pError);
}

#endif

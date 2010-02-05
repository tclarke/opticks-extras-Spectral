/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "DataVariant.h"
#include "DynamicObject.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "SpecialMetadata.h"
#include "Wavelengths.h"

#include <string>
using namespace std;

#define WAVELENGTH_ACTUAL_UNITS_METADATA_PATH   "Spectral/Wavelengths/Actual Units"
#define WAVELENGTH_DISPLAY_UNITS_METADATA_PATH  "Spectral/Wavelengths/Display Units"

Wavelengths::Wavelengths(DynamicObject* pWavelengthData) :
   mpWavelengthData(pWavelengthData)
{
   convertToDisplayUnits();
}

Wavelengths::~Wavelengths()
{
}

void Wavelengths::setStartValues(const vector<double>& startValues, WavelengthUnitsType valueUnits)
{
   if (mpWavelengthData.get() != NULL)
   {
      vector<double> values;

      WavelengthUnitsType units = getUnits();
      if ((valueUnits.isValid() == true) && (valueUnits != units))
      {
         for (vector<double>::size_type i = 0; i < startValues.size(); ++i)
         {
            values.push_back(Wavelengths::convertValue(startValues[i], valueUnits, units));
         }
      }
      else
      {
         values = startValues;
      }

      mpWavelengthData->setAttributeByPath(START_WAVELENGTHS_METADATA_PATH, values);
   }
}

const vector<double>& Wavelengths::getStartValues() const
{
   return const_cast<Wavelengths*>(this)->getEditableStartValues();
}

bool Wavelengths::hasStartValues() const
{
   return !(getStartValues().empty());
}

void Wavelengths::setCenterValues(const vector<double>& centerValues, WavelengthUnitsType valueUnits)
{
   if (mpWavelengthData.get() != NULL)
   {
      vector<double> values;

      WavelengthUnitsType units = getUnits();
      if ((valueUnits.isValid() == true) && (valueUnits != units))
      {
         for (vector<double>::size_type i = 0; i < centerValues.size(); ++i)
         {
            values.push_back(Wavelengths::convertValue(centerValues[i], valueUnits, units));
         }
      }
      else
      {
         values = centerValues;
      }

      mpWavelengthData->setAttributeByPath(CENTER_WAVELENGTHS_METADATA_PATH, centerValues);
   }
}

const vector<double>& Wavelengths::getCenterValues() const
{
   return const_cast<Wavelengths*>(this)->getEditableCenterValues();
}

bool Wavelengths::hasCenterValues() const
{
   return !(getCenterValues().empty());
}

void Wavelengths::setEndValues(const vector<double>& endValues, WavelengthUnitsType valueUnits)
{
   if (mpWavelengthData.get() != NULL)
   {
      vector<double> values;

      WavelengthUnitsType units = getUnits();
      if ((valueUnits.isValid() == true) && (valueUnits != units))
      {
         for (vector<double>::size_type i = 0; i < endValues.size(); ++i)
         {
            values.push_back(Wavelengths::convertValue(endValues[i], valueUnits, units));
         }
      }
      else
      {
         values = endValues;
      }

      mpWavelengthData->setAttributeByPath(END_WAVELENGTHS_METADATA_PATH, endValues);
   }
}

const vector<double>& Wavelengths::getEndValues() const
{
   return const_cast<Wavelengths*>(this)->getEditableEndValues();
}

bool Wavelengths::hasEndValues() const
{
   return !(getEndValues().empty());
}

void Wavelengths::setUnits(WavelengthUnitsType units)
{
   setActualUnits(units);
   setDisplayUnits(units);
}

Wavelengths::WavelengthUnitsType Wavelengths::getUnits() const
{
   if (mpWavelengthData.get() == NULL)
   {
      return WavelengthUnitsType();
   }

   WavelengthUnitsType actualUnits = getActualUnits();
   WavelengthUnitsType displayUnits = getDisplayUnits();
   VERIFYRV(actualUnits == displayUnits, WavelengthUnitsType());

   return actualUnits;
}

bool Wavelengths::isEmpty() const
{
   if ((hasStartValues() == false) && (hasCenterValues() == false) && (hasEndValues() == false))
   {
      return true;
   }

   return false;
}

void Wavelengths::scaleValues(double dScaleFactor)
{
   // Apply the scale factor to all wavelength values
   vector<double>& startValues = getEditableStartValues();
   for (vector<double>::size_type i = 0; i < startValues.size(); ++i)
   {
      startValues[i] *= dScaleFactor;
   }

   vector<double>& centerValues = getEditableCenterValues();
   for (vector<double>::size_type i = 0; i < centerValues.size(); ++i)
   {
      centerValues[i] *= dScaleFactor;
   }

   vector<double>& endValues = getEditableEndValues();
   for (vector<double>::size_type i = 0; i < endValues.size(); ++i)
   {
      endValues[i] *= dScaleFactor;
   }

   // Set to custom units since the values changed, but do not store the
   // scale factor so that the wavelengths can be saved in custom units
   setUnits(CUSTOM);
}

void Wavelengths::calculateFwhm(double dConstant)
{
   const vector<double>& centerValues = getCenterValues();
   if (centerValues.size() < 2)
   {
      return;
   }

   vector<double>& startValues = getEditableStartValues();
   vector<double>& endValues = getEditableEndValues();

   startValues.clear();
   endValues.clear();

   // Calculate the FWHM values
   vector<double>::size_type index1 = 0;
   vector<double>::size_type index2 = 1;
   for (index1 = 0, index2 = 1; index1 < (centerValues.size() - 1), index2 < centerValues.size(); ++index1, ++index2)
   {
      // Start value
      double dStart = centerValues[index1] - ((centerValues[index2] - centerValues[index1]) * dConstant) / 2.0;
      if (dStart < 0.0)
      {
         dStart = 0.0;
      }

      startValues.push_back(dStart);

      // End value
      double dEnd = centerValues[index1] + ((centerValues[index2] - centerValues[index1]) * dConstant) / 2.0;
      if (dEnd < 0.0)
      {
         dEnd = 0.0;
      }

      endValues.push_back(dEnd);
   }

   // Calculate the FWHM values for the last wavelength
   vector<double>::size_type index = centerValues.size() - 1;

   double dStart = centerValues[index] - ((centerValues[index] - centerValues[index - 1]) * dConstant) / 2.0;
   if (dStart < 0.0)
   {
      dStart = 0.0;
   }

   startValues.push_back(dStart);

   double dEnd = centerValues[index] + ((centerValues[index] - centerValues[index - 1]) * dConstant) / 2.0;
   if (dEnd < 0.0)
   {
      dEnd = 0.0;
   }

   endValues.push_back(dEnd);
}

vector<double> Wavelengths::getFwhm()
{
   if (!hasStartValues() || !hasEndValues())
   {
      calculateFwhm();
   }
   vector<double> fwhm;
   const vector<double>& start = getStartValues();
   const vector<double>& end = getEndValues();
   for (vector<double>::size_type idx = 0; idx < start.size() && idx < end.size(); idx++)
   {
      fwhm.push_back(end[idx] - start[idx]);
   }
   return fwhm;
}

bool Wavelengths::initializeFromDynamicObject(const DynamicObject* pData)
{
   if (mpWavelengthData.get() == NULL)
   {
      return false;
   }

   bool bSuccess = copyWavelengthData(pData, mpWavelengthData.get());
   if (bSuccess == true)
   {
      convertToDisplayUnits();
   }

   return bSuccess;
}

bool Wavelengths::applyToDynamicObject(DynamicObject* pData) const
{
   if (mpWavelengthData.get() == NULL)
   {
      return false;
   }

   return copyWavelengthData(mpWavelengthData.get(), pData);
}

bool Wavelengths::applyToDataset(RasterElement* pDataset) const
{
   if (pDataset == NULL)
   {
      return false;
   }

   // Ensure that the number of wavelengths matches the number of bands in the data set
   RasterDataDescriptor* pDescriptor = dynamic_cast<RasterDataDescriptor*>(pDataset->getDataDescriptor());
   VERIFY(pDescriptor != NULL);

   unsigned int numBands = pDescriptor->getBandCount();

   if (((hasStartValues() == true) && (getStartValues().size() != numBands)) ||
      ((hasCenterValues() == true) && (getCenterValues().size() != numBands)) ||
      ((hasEndValues() == true) && (getEndValues().size() != numBands)))
   {
      return false;
   }

   // Convert the values to microns
   WavelengthUnitsType units = getActualUnits();
   const_cast<Wavelengths*>(this)->setActualUnits(MICRONS);

   // Set the wavelength values into the dataset metadata
   DynamicObject* pMetadata = pDataset->getMetadata();
   VERIFY(pMetadata != NULL);

   bool bSuccess = applyToDynamicObject(pMetadata);

   // Convert the values back to the original units
   const_cast<Wavelengths*>(this)->setActualUnits(units);

   return bSuccess;
}

double Wavelengths::convertValue(double value, WavelengthUnitsType valueUnits, WavelengthUnitsType newUnits)
{
   if ((valueUnits != newUnits) && (valueUnits.isValid() == true) && (valueUnits != CUSTOM) &&
      (newUnits.isValid() == true) && (newUnits != CUSTOM))
   {
      // Convert the value to microns
      switch (valueUnits)
      {
         case Wavelengths::NANOMETERS:
            value *= 0.001;
            break;

         case Wavelengths::INVERSE_CENTIMETERS:
            if (value != 0.0)
            {
               value = (1.0 / value) * 10000.0;
            }
            break;

         default:
            break;
      }

      // Convert the value from microns to the new units
      switch (newUnits)
      {
         case Wavelengths::NANOMETERS:
            value *= 1000.0;
            break;

         case Wavelengths::INVERSE_CENTIMETERS:
            if (value != 0.0)
            {
               value = 1.0 / (value * 0.0001);
            }
            break;

         default:
            break;
      }
   }

   return value;
}

vector<double>& Wavelengths::getEditableStartValues()
{
   if (mpWavelengthData.get() != NULL)
   {
      vector<double>* pValues =
         dv_cast<vector<double> >(&mpWavelengthData->getAttributeByPath(START_WAVELENGTHS_METADATA_PATH));
      if (pValues != NULL)
      {
         return *pValues;
      }
   }

   static vector<double> emptyVector;
   return emptyVector;
}

vector<double>& Wavelengths::getEditableCenterValues()
{
   if (mpWavelengthData.get() != NULL)
   {
      vector<double>* pValues =
         dv_cast<vector<double> >(&mpWavelengthData->getAttributeByPath(CENTER_WAVELENGTHS_METADATA_PATH));
      if (pValues != NULL)
      {
         return *pValues;
      }
   }

   static vector<double> emptyVector;
   return emptyVector;
}

vector<double>& Wavelengths::getEditableEndValues()
{
   if (mpWavelengthData.get() != NULL)
   {
      vector<double>* pValues =
         dv_cast<vector<double> >(&mpWavelengthData->getAttributeByPath(END_WAVELENGTHS_METADATA_PATH));
      if (pValues != NULL)
      {
         return *pValues;
      }
   }

   static vector<double> emptyVector;
   return emptyVector;
}

void Wavelengths::setActualUnits(WavelengthUnitsType units)
{
   if (mpWavelengthData.get() == NULL)
   {
      return;
   }

   WavelengthUnitsType currentUnits = getActualUnits(false);
   if (units != currentUnits)
   {
      // Convert the values
      if ((units.isValid() == true) && (units != CUSTOM))
      {
         vector<double>& startValues = getEditableStartValues();
         for (vector<double>::size_type i = 0; i < startValues.size(); ++i)
         {
            startValues[i] = Wavelengths::convertValue(startValues[i], currentUnits, units);
         }

         vector<double>& centerValues = getEditableCenterValues();
         for (vector<double>::size_type i = 0; i < centerValues.size(); ++i)
         {
            centerValues[i] = Wavelengths::convertValue(centerValues[i], currentUnits, units);
         }

         vector<double>& endValues = getEditableEndValues();
         for (vector<double>::size_type i = 0; i < endValues.size(); ++i)
         {
            endValues[i] = Wavelengths::convertValue(endValues[i], currentUnits, units);
         }
      }

      // Set the units
      mpWavelengthData->setAttributeByPath(WAVELENGTH_ACTUAL_UNITS_METADATA_PATH,
         StringUtilities::toXmlString(units));
   }
}

Wavelengths::WavelengthUnitsType Wavelengths::getActualUnits(bool bVerify) const
{
   WavelengthUnitsType actualUnits;
   if (mpWavelengthData.get() != NULL)
   {
      const string* pActualUnits =
         dv_cast<string>(&mpWavelengthData->getAttributeByPath(WAVELENGTH_ACTUAL_UNITS_METADATA_PATH));
      if (pActualUnits != NULL)
      {
         actualUnits = StringUtilities::fromXmlString<Wavelengths::WavelengthUnitsType>(*pActualUnits);
      }

      if (bVerify == true)
      {
         VERIFYRV(actualUnits.isValid(), WavelengthUnitsType());
      }
   }

   return actualUnits;
}

void Wavelengths::setDisplayUnits(WavelengthUnitsType units)
{
   if (mpWavelengthData.get() == NULL)
   {
      return;
   }

   WavelengthUnitsType currentUnits = getDisplayUnits(false);
   if (units != currentUnits)
   {
      mpWavelengthData->setAttributeByPath(WAVELENGTH_DISPLAY_UNITS_METADATA_PATH,
         StringUtilities::toXmlString(units));
   }
}

Wavelengths::WavelengthUnitsType Wavelengths::getDisplayUnits(bool bVerify) const
{
   WavelengthUnitsType displayUnits;
   if (mpWavelengthData.get() != NULL)
   {
      const string* pDisplayUnits =
         dv_cast<string>(&mpWavelengthData->getAttributeByPath(WAVELENGTH_DISPLAY_UNITS_METADATA_PATH));
      if (pDisplayUnits != NULL)
      {
         displayUnits = StringUtilities::fromXmlString<Wavelengths::WavelengthUnitsType>(*pDisplayUnits);
      }

      if (bVerify == true)
      {
         VERIFYRV(displayUnits.isValid(), WavelengthUnitsType());
      }
   }

   return displayUnits;
}

void Wavelengths::convertToDisplayUnits()
{
   if (mpWavelengthData.get() == NULL)
   {
      return;
   }

   WavelengthUnitsType actualUnits = getActualUnits(false);
   WavelengthUnitsType displayUnits = getDisplayUnits(false);

   if (displayUnits.isValid() == true)
   {
      setActualUnits(displayUnits);
   }
   else if (actualUnits.isValid() == true)
   {
      setDisplayUnits(actualUnits);
   }
   else
   {
      setUnits(MICRONS);
   }
}

bool Wavelengths::copyWavelengthData(const DynamicObject* pSourceData, DynamicObject* pDestinationData) const
{
   if ((pSourceData == NULL) || (pDestinationData == NULL))
   {
      return false;
   }

   // Clear existing wavelength data in the destination DynamicObject
   pDestinationData->removeAttributeByPath(START_WAVELENGTHS_METADATA_PATH);
   pDestinationData->removeAttributeByPath(CENTER_WAVELENGTHS_METADATA_PATH);
   pDestinationData->removeAttributeByPath(END_WAVELENGTHS_METADATA_PATH);
   pDestinationData->removeAttributeByPath(WAVELENGTH_ACTUAL_UNITS_METADATA_PATH);
   pDestinationData->removeAttributeByPath(WAVELENGTH_DISPLAY_UNITS_METADATA_PATH);

   // Copy the wavelength data from the source DynamicObject into the destination DynamicObject
   bool bSuccess = true;

   const DataVariant& startValues = pSourceData->getAttributeByPath(START_WAVELENGTHS_METADATA_PATH);
   if (startValues.isValid() == true)
   {
      bSuccess = pDestinationData->setAttributeByPath(START_WAVELENGTHS_METADATA_PATH, startValues);
   }

   if (bSuccess == true)
   {
      const DataVariant& centerValues = pSourceData->getAttributeByPath(CENTER_WAVELENGTHS_METADATA_PATH);
      if (centerValues.isValid() == true)
      {
         bSuccess = pDestinationData->setAttributeByPath(CENTER_WAVELENGTHS_METADATA_PATH, centerValues);
      }
   }

   if (bSuccess == true)
   {
      const DataVariant& endValues = pSourceData->getAttributeByPath(END_WAVELENGTHS_METADATA_PATH);
      if (endValues.isValid() == true)
      {
         bSuccess = pDestinationData->setAttributeByPath(END_WAVELENGTHS_METADATA_PATH, endValues);
      }
   }

   if (bSuccess == true)
   {
      const DataVariant& actualUnits = pSourceData->getAttributeByPath(WAVELENGTH_ACTUAL_UNITS_METADATA_PATH);
      if (actualUnits.isValid() == true)
      {
         bSuccess = pDestinationData->setAttributeByPath(WAVELENGTH_ACTUAL_UNITS_METADATA_PATH, actualUnits);
      }
   }

   if (bSuccess == true)
   {
      const DataVariant& displayUnits = pSourceData->getAttributeByPath(WAVELENGTH_DISPLAY_UNITS_METADATA_PATH);
      if (displayUnits.isValid() == true)
      {
         bSuccess = pDestinationData->setAttributeByPath(WAVELENGTH_DISPLAY_UNITS_METADATA_PATH, displayUnits);
      }
   }

   return bSuccess;
}

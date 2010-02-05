/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "WavelengthUnitsComboBox.h"
#include "StringUtilities.h"

#include <string>
using namespace std;

WavelengthUnitsComboBox::WavelengthUnitsComboBox(QWidget* pParent) :
   QComboBox(pParent)
{
   // Initialization
   setEditable(false);
   addItem(QString::fromStdString(StringUtilities::toDisplayString(Wavelengths::MICRONS)));
   addItem(QString::fromStdString(StringUtilities::toDisplayString(Wavelengths::NANOMETERS)));
   addItem(QString::fromStdString(StringUtilities::toDisplayString(Wavelengths::INVERSE_CENTIMETERS)));
   addItem(QString::fromStdString(StringUtilities::toDisplayString(Wavelengths::CUSTOM)));

   // Connections
   VERIFYNR(connect(this, SIGNAL(activated(int)), this, SLOT(translateActivated(int))));
}

WavelengthUnitsComboBox::~WavelengthUnitsComboBox()
{
}

void WavelengthUnitsComboBox::setUnits(Wavelengths::WavelengthUnitsType units)
{
   QString strUnits = QString::fromStdString(StringUtilities::toDisplayString(units));

   int index = findText(strUnits);
   setCurrentIndex(index);
}

Wavelengths::WavelengthUnitsType WavelengthUnitsComboBox::getUnits() const
{
   Wavelengths::WavelengthUnitsType units;
   if (currentIndex() != -1)
   {
      string unitsText = currentText().toStdString();
      units = StringUtilities::fromDisplayString<Wavelengths::WavelengthUnitsType>(unitsText);
   }

   return units;
}

void WavelengthUnitsComboBox::translateActivated(int index)
{
   if (index != -1)
   {
      string unitsText = currentText().toStdString();

      Wavelengths::WavelengthUnitsType units =
         StringUtilities::fromDisplayString<Wavelengths::WavelengthUnitsType>(unitsText);
      emit unitsActivated(units);
   }
}

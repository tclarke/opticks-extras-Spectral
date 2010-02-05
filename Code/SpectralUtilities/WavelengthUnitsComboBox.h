/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef WAVELENGTHUNITSCOMBOBOX_H
#define WAVELENGTHUNITSCOMBOBOX_H

#include "Wavelengths.h"

#include <QtGui/QComboBox>

class WavelengthUnitsComboBox : public QComboBox
{
   Q_OBJECT

public:
   WavelengthUnitsComboBox(QWidget* pParent = NULL);
   ~WavelengthUnitsComboBox();

   void setUnits(Wavelengths::WavelengthUnitsType units);
   Wavelengths::WavelengthUnitsType getUnits() const;

signals:
   void unitsActivated(Wavelengths::WavelengthUnitsType units);

private slots:
   void translateActivated(int index);
};

#endif

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef WAVELENGTHEDITORDLG_H
#define WAVELENGTHEDITORDLG_H

#include <QtGui/QDialog>
#include <QtGui/QPushButton>
#include <QtGui/QTreeWidget>

#include "ObjectResource.h"
#include "Wavelengths.h"

class DynamicObject;
class RasterElement;
class WavelengthUnitsComboBox;

class WavelengthEditorDlg : public QDialog
{
   Q_OBJECT

public:
   WavelengthEditorDlg(QWidget* pParent = NULL);
   WavelengthEditorDlg(RasterElement* pDataset, QWidget* pParent = NULL);
   ~WavelengthEditorDlg();

   const Wavelengths& getWavelengths() const;

public slots:
   void accept();

protected slots:
   void loadWavelengths();
   void saveWavelengths();
   void calculateFwhm();
   void applyScaleFactor();
   void convertWavelengths(Wavelengths::WavelengthUnitsType newUnits);
   void updateWavelengths();
   void updateCaption();
   void help();

private:
   void instantiate();

   RasterElement* mpDataset;
   QString mWavelengthFilename;
   FactoryResource<DynamicObject> mpWavelengthData;
   Wavelengths mWavelengths;

   QTreeWidget* mpWavelengthTree;
   QPushButton* mpSaveButton;
   QPushButton* mpFwhmButton;
   QPushButton* mpScaleButton;
   WavelengthUnitsComboBox* mpUnitsCombo;

   static QString mMetadataFilter;
   static QString mTextFilter;
};

#endif

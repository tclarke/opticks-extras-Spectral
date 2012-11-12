/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef LOCATEDIALOG_H
#define LOCATEDIALOG_H

#include "SpectralLibraryMatch.h"
#include "SpectralLibraryMatchOptions.h"

#include <map>
#include <string>

#include <QtCore/QString>
#include <QtGui/QDialog>

class AoiElement;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QDoubleSpinBox;

class LocateDialog : public QDialog
{
   Q_OBJECT

public:
   LocateDialog(const RasterElement* pRaster, QWidget* pParent = 0);
   virtual ~LocateDialog();

   virtual void accept();
   SpectralLibraryMatch::LocateAlgorithm getLocateAlgorithm() const;
   double getThreshold() const;
   std::string getOutputLayerName() const;
   AoiElement* getAoi() const;

protected slots:
   void algorithmChanged(const QString& algName);
   void thresholdChanged(double value);

private:
   const RasterElement* mpRaster;
   std::map<std::string, float> mLocateThresholds;
   QString mLayerNameBase;
   QComboBox* mpAlgCombo;
   QDoubleSpinBox* mpThreshold;
   QLineEdit* mpOutputLayerName;
   QCheckBox* mpUseAoi;
   QComboBox* mpAoiCombo;
   QCheckBox* mpSaveSettings;
};

#endif
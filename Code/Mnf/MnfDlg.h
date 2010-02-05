/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef MNFDLG_H
#define MNFDLG_H

#include <QtGui/QDialog>

#include "TypesFile.h"

#include <vector>
#include <string>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStringList;

class MnfDlg : public QDialog
{
   Q_OBJECT

public:
   MnfDlg(const std::vector<std::string>& aoiList, unsigned int ulBands, QWidget* parent = 0);
   ~MnfDlg();

   std::string getNoiseStatisticsMethod() const;
   std::string getTransformFilename() const;
   std::string getRoiName() const;

   bool selectNumComponentsFromPlot();
   unsigned int getNumComponents() const;

   void setNoiseStatisticsMethods(QStringList& methods);

protected slots:
   void browse();

private:
   QRadioButton* mpCalculateRadio;
   QComboBox* mpMethodCombo;
   QRadioButton* mpFileRadio;
   QLineEdit* mpFileEdit;
   QSpinBox* mpComponentsSpin;
   QCheckBox* mpRoiCheck;
   QComboBox* mpRoiCombo;
   QCheckBox* mpFromSnrPlot;
};

#endif

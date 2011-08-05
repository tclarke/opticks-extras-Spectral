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

class FileBrowser;
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
   MnfDlg(const std::string& saveFilename, const std::vector<std::string>& aoiList, unsigned int ulBands,
      QWidget* pParent = 0);
   virtual ~MnfDlg();

   std::string getNoiseStatisticsMethod() const;
   std::string getTransformFilename() const;
   std::string getRoiName() const;
   std::string getCoefficientsFilename() const;

   bool selectNumComponentsFromPlot();
   unsigned int getNumComponents() const;

   void setNoiseStatisticsMethods(QStringList& methods);

public slots:
   virtual void accept();

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
   FileBrowser* mpCoefficientsFilename;
};

#endif

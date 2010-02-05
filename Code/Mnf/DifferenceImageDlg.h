/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef DIFFERENCEIMAGEDLG_H
#define DIFFERENCEIMAGEDLG_H

#include <QtGui/QDialog>

#include <vector>
#include <string>

class QDoubleSpinBox;
class QComboBox;
class QRadioButton;
class RasterElement;

class DifferenceImageDlg : public QDialog
{
   Q_OBJECT

public:
   DifferenceImageDlg(const RasterElement* pRaster,
      QWidget* parent = 0);
   ~DifferenceImageDlg();

   bool useAutomaticSelection();
   float getBandFractionThreshold() const;
   std::string getAoiName() const;

   virtual void accept();

protected slots:
   void methodChanged(bool useAuto);

private:
   QRadioButton* mpAutoRadio;
   QDoubleSpinBox* mpBandFractionSpin;
   QRadioButton* mpAoiRadio;
   QComboBox* mpAoiCombo;
};

#endif

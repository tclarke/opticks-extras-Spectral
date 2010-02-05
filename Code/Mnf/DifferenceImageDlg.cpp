/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AoiElement.h"
#include "AppVerify.h"
#include "DifferenceImageDlg.h"
#include "ModelServices.h"
#include "RasterElement.h"
#include "TypeConverter.h"

#include <QtGui/QComboBox>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QDoubleSpinBox>
#include <QtGui/QGridLayout>
#include <QtGui/QGroupBox>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QMessageBox>
#include <QtGui/QRadioButton>

using namespace std;

DifferenceImageDlg::DifferenceImageDlg(const RasterElement* pRaster, QWidget* parent) :
   QDialog(parent)
{
   setWindowTitle("Select Area for Noise Estimation");

   // Method
   QGroupBox* pMethodGroup = new QGroupBox("Method", this);
   mpAutoRadio = new QRadioButton("Automatic Selection:", pMethodGroup);
   QLabel* pSpinLabel = new QLabel("Band Fraction Threshold:", pMethodGroup);
   mpBandFractionSpin = new QDoubleSpinBox(pMethodGroup);
   mpBandFractionSpin->setDecimals(1);
   mpBandFractionSpin->setRange(0.0, 1.0);
   mpBandFractionSpin->setSingleStep(0.1);
   mpBandFractionSpin->setToolTip("Criteria for selecting pixels to use in estimation of the noise.");
   VERIFYNRV(connect(mpAutoRadio, SIGNAL(toggled(bool)), pSpinLabel, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(mpAutoRadio, SIGNAL(toggled(bool)), mpBandFractionSpin, SLOT(setEnabled(bool))));

   // AOI
   mpAoiRadio = new QRadioButton("Manual Selection:", pMethodGroup);
   mpAoiRadio->setFocusPolicy(Qt::StrongFocus);
   QLabel* pAoiLabel = new QLabel("AOI:");
   mpAoiCombo = new QComboBox(pMethodGroup);
   mpAoiCombo->setEditable(false);
   mpAoiCombo->setMinimumWidth(200);
   VERIFYNRV(connect(mpAoiRadio, SIGNAL(toggled(bool)), mpAoiCombo, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(mpAoiRadio, SIGNAL(toggled(bool)), pAoiLabel, SLOT(setEnabled(bool))));

   QGridLayout* pMethodLayout = new QGridLayout(pMethodGroup);
   pMethodLayout->setMargin(10);
   pMethodLayout->setSpacing(5);
   pMethodLayout->addWidget(mpAutoRadio, 0, 0, 1, 3);
   pMethodLayout->addWidget(pSpinLabel, 1, 1, 1, 2);
   pMethodLayout->addWidget(mpBandFractionSpin, 1, 3);
   pMethodLayout->addWidget(mpAoiRadio, 2, 0, 1, 3);
   pMethodLayout->addWidget(pAoiLabel, 3, 1);
   pMethodLayout->addWidget(mpAoiCombo, 3, 2, 1, 2);

   // Horizontal line
   QFrame* pHLine = new QFrame(this);
   pHLine->setFrameStyle(QFrame::HLine | QFrame::Sunken);

   // OK and Cancel buttons
   QDialogButtonBox* pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
   VERIFYNRV(connect(pButtonBox, SIGNAL(accepted()), this, SLOT(accept())));
   VERIFYNRV(connect(pButtonBox, SIGNAL(rejected()), this, SLOT(reject())));

   // Layout
   QVBoxLayout* pLayout = new QVBoxLayout(this);
   pLayout->setMargin(10);
   pLayout->setSpacing(5);
   pLayout->addWidget(pMethodGroup);
   pLayout->addWidget(pHLine);
   pLayout->addWidget(pButtonBox);

   // Initialization
   setModal(true);
   resize(300, 250);

#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : Enable auto-mask option when it's available(rforehan)")
   mpBandFractionSpin->setValue(0.8);
   mpAutoRadio->setChecked(false);
   mpAutoRadio->setEnabled(false);
   mpBandFractionSpin->setEnabled(false);
   pSpinLabel->setEnabled(false);

   mpAoiRadio->setChecked(true);
   mpAoiCombo->setEnabled(true);
   pAoiLabel->setEnabled(true);

   if (pRaster != NULL)
   {
      vector<string> aoiNames = Service<ModelServices>()->getElementNames(const_cast<RasterElement*>(pRaster),
         TypeConverter::toString<AoiElement>());
      for (vector<string>::const_iterator iter = aoiNames.begin(); iter != aoiNames.end(); ++iter)
      {
         mpAoiCombo->addItem(QString::fromStdString(*iter));
      }
   }
}

DifferenceImageDlg::~DifferenceImageDlg()
{
}

float DifferenceImageDlg::getBandFractionThreshold() const
{
   return static_cast<float>(mpBandFractionSpin->value());
}

string DifferenceImageDlg::getAoiName() const
{
   string strAoiName;
   if (mpAoiRadio->isChecked())
   {
      strAoiName = mpAoiCombo->currentText().toStdString();
   }

   return strAoiName;
}

void DifferenceImageDlg::accept()
{
   if (mpAoiRadio->isChecked() && mpAoiCombo->currentText().isEmpty())
   {
      QMessageBox::warning(this, "No AOI Selected", "No AOI is available to use."
         " Either select Automatic Selection or exit plug-in, create AOI and restart the plug-in.");
      return;
   }

   if (mpAutoRadio->isChecked())
   {
      QMessageBox::warning(this, "Automatic Noise Mask", "Sorry this option is not yet available."
         " Currently this plug-in only supports manual selection.");
      return;
   }
   QDialog::accept();
}

void DifferenceImageDlg::methodChanged(bool useAuto)
{
   if (useAuto)
   {
      mpBandFractionSpin->setEnabled(true);
      mpAoiCombo->setEnabled(false);
   }
   else
   {
      mpBandFractionSpin->setEnabled(false);
      mpAoiCombo->setEnabled(true);
   }
}

bool DifferenceImageDlg::useAutomaticSelection()
{
   return mpAutoRadio->isChecked();
}

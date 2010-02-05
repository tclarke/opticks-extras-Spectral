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
#include "ModelServices.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "StatisticsDlg.h"
#include "TypeConverter.h"

#include <QtGui/QDialogButtonBox>
#include <QtGui/QGridLayout>
#include <QtGui/QGroupBox>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QMessageBox>
#include <QtGui/QPushButton>

using namespace std;

StatisticsDlg::StatisticsDlg(string rasterName, QWidget* parent) :
   QDialog(parent)
{
   setWindowTitle("Noise Covariance");

   // Source
   QGroupBox* pSourceGroup = new QGroupBox("Data Source", this);
   mpRasterCombo = new QComboBox(pSourceGroup);
   mpRasterCombo->setEditable(false);
   mpRasterCombo->setMinimumWidth(200);
   mpRasterCombo->setInsertPolicy(QComboBox::InsertAlphabetically);
   VERIFYNRV(connect(mpRasterCombo, SIGNAL(currentIndexChanged(const QString&)),
      this, SLOT(rasterChanged(const QString&))));
   
   QPushButton* pLoadRaster = new QPushButton("Load...", pSourceGroup);
   VERIFYNRV(connect(pLoadRaster, SIGNAL(clicked()), this, SLOT(loadRaster())));

   QGridLayout* pSourceLayout = new QGridLayout(pSourceGroup);
   pSourceLayout->setMargin(10);
   pSourceLayout->setSpacing(5);
   pSourceLayout->addWidget(mpRasterCombo, 0, 0, 1, 4);
   pSourceLayout->addWidget(pLoadRaster, 1, 3);

   // Subset
   QGroupBox* pSubsetGroup = new QGroupBox("Subset", this);

   // Skip factors
   mpFactorRadio = new QRadioButton("Skip Factors:", pSubsetGroup);
   mpFactorRadio->setFocusPolicy(Qt::StrongFocus);

   QLabel* pRowLabel = new QLabel("Row:", pSubsetGroup);
   mpRowSpin = new QSpinBox(pSubsetGroup);
   mpRowSpin->setFixedWidth(50);
   mpRowSpin->setMinimum(1);

   QLabel* pColumnLabel = new QLabel("Column:", pSubsetGroup);
   mpColumnSpin = new QSpinBox(pSubsetGroup);
   mpColumnSpin->setFixedWidth(50);
   mpColumnSpin->setMinimum(1);

   VERIFYNRV(connect(mpFactorRadio, SIGNAL(toggled(bool)), pRowLabel, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(mpFactorRadio, SIGNAL(toggled(bool)), mpRowSpin, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(mpFactorRadio, SIGNAL(toggled(bool)), pColumnLabel, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(mpFactorRadio, SIGNAL(toggled(bool)), mpColumnSpin, SLOT(setEnabled(bool))));

   // AOI
   mpAoiRadio = new QRadioButton("AOI:", pSubsetGroup);
   mpAoiRadio->setFocusPolicy(Qt::StrongFocus);

   mpAoiCombo = new QComboBox(pSubsetGroup);
   mpAoiCombo->setEditable(false);
   mpAoiCombo->setMinimumWidth(200);

   VERIFYNRV(connect(mpAoiRadio, SIGNAL(toggled(bool)), mpAoiCombo, SLOT(setEnabled(bool))));

   QGridLayout* pSubsetGrid = new QGridLayout(pSubsetGroup);
   pSubsetGrid->setMargin(10);
   pSubsetGrid->setSpacing(5);
   pSubsetGrid->setColumnMinimumWidth(0, 14);
   pSubsetGrid->addWidget(mpFactorRadio, 0, 0, 1, 4);
   pSubsetGrid->addWidget(pRowLabel, 1, 1);
   pSubsetGrid->addWidget(mpRowSpin, 1, 2);
   pSubsetGrid->addWidget(pColumnLabel, 2, 1);
   pSubsetGrid->addWidget(mpColumnSpin, 2, 2);
   pSubsetGrid->addWidget(mpAoiRadio, 3, 0, 1, 4);
   pSubsetGrid->addWidget(mpAoiCombo, 4, 1, 1, 3);
   pSubsetGrid->setColumnStretch(3, 10);

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
   pLayout->addWidget(pSourceGroup);
   pLayout->addWidget(pSubsetGroup);
   pLayout->addStretch();
   pLayout->addWidget(pHLine);
   pLayout->addWidget(pButtonBox);

   // Initialization
   setModal(true);
   resize(300, 250);
   Service<ModelServices> pModel;
   vector<DataElement*> elements = pModel->getElements(TypeConverter::toString<RasterElement>());
   vector<string> rasterNames;
   for (vector<DataElement*>::iterator it = elements.begin(); it != elements.end(); ++it)
   {
      string elementName = (*it)->getName();
      if (elementName != rasterName)
      {
         rasterNames.push_back(elementName);
      }
   }
   for (vector<string>::const_iterator it = rasterNames.begin(); it != rasterNames.end(); ++it)
   {
      mpRasterCombo->addItem(QString::fromStdString(*it));
   }

   mpFactorRadio->setChecked(true);
   mpAoiCombo->setEnabled(false);
}

StatisticsDlg::~StatisticsDlg()
{
}

int StatisticsDlg::getRowFactor() const
{
   int iFactor = -1;
   if (mpFactorRadio->isChecked())
   {
      iFactor = mpRowSpin->value();
   }

   return iFactor;
}

int StatisticsDlg::getColumnFactor() const
{
   int iFactor = -1;
   if (mpFactorRadio->isChecked())
   {
      iFactor = mpColumnSpin->value();
   }

   return iFactor;
}

string StatisticsDlg::getAoiName() const
{
   string strAoiName;
   if (mpAoiRadio->isChecked())
   {
      strAoiName = mpAoiCombo->currentText().toStdString();
   }

   return strAoiName;
}

void StatisticsDlg::loadRaster()
{
   RasterDataDescriptor* pDescriptor(NULL);
   ImporterResource importer("Auto Importer", NULL, false);
   if (importer->execute())
   {
      vector<DataElement*> elements = importer->getImportedElements();
      QString firstAddedName;
      for (vector<DataElement*>::iterator it = elements.begin(); it != elements.end(); ++it)
      {
         string addedName = (*it)->getName();
         if (firstAddedName.isEmpty())
         {
            firstAddedName = QString::fromStdString(addedName);
         }
         mpRasterCombo->addItem(QString::fromStdString(addedName));
      }

      if (firstAddedName.isEmpty() == false)
      {
         mpRasterCombo->setCurrentIndex(mpRasterCombo->findText(firstAddedName));
      }
   }
}

string StatisticsDlg::getDarkCurrentDataName() const
{
   return mpRasterCombo->currentText().toStdString();
}

void StatisticsDlg::accept()
{
   if (mpRasterCombo->currentText().isEmpty())
   {
      QMessageBox::warning(this, "No Data Source Selected", "You have to select the dark current raster element."
         " If it is not loaded, hit the Load button to import it.");
      return;
   }

   QDialog::accept();
}

void StatisticsDlg::rasterChanged(const QString& rasterName)
{
   mpAoiCombo->clear();
   Service<ModelServices> pModel;
   RasterElement* pRaster = dynamic_cast<RasterElement*>(pModel->getElement(
      rasterName.toStdString(), TypeConverter::toString<RasterElement>(), NULL));
   if (pRaster == NULL)
   {
      return;
   }

   vector<string> aoiNames = pModel->getElementNames(pRaster, TypeConverter::toString<AoiElement>());
   for (vector<string>::iterator it = aoiNames.begin(); it != aoiNames.end(); ++it)
   {
      mpAoiCombo->addItem(QString::fromStdString(*it));
   }
}
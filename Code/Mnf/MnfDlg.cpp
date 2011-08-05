/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "ConfigurationSettings.h"
#include "FileBrowser.h"
#include "Filename.h"
#include "FileResource.h"
#include "MnfDlg.h"
#include "StringUtilities.h"

#include <QtCore/QStringList>
#include <QtGui/QBitmap>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QFileDialog>
#include <QtGui/QLineEdit>
#include <QtGui/QGroupBox>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QMessageBox>
#include <QtGui/QPixmap>
#include <QtGui/QPushButton>
#include <QtGui/QRadioButton>
#include <QtGui/QSpinBox>

using namespace std;

MnfDlg::MnfDlg(const std::string& saveFilename, const vector<string>& aoiList, unsigned int ulBands, QWidget* pParent) :
   QDialog(pParent)
{
   // Caption
   setWindowTitle("Minimum Noise Fraction Transform");

   // Transform
   QGroupBox* pTransformGroup = new QGroupBox("Transform", this);

   mpCalculateRadio = new QRadioButton("Calculate", pTransformGroup);
   mpCalculateRadio->setFocusPolicy(Qt::StrongFocus);
   QLabel* pMethodLabel = new QLabel("Noise Statistics:", pTransformGroup);

   mpMethodCombo = new QComboBox(pTransformGroup);
   mpMethodCombo->setEditable(false);

   QHBoxLayout* pMethodLayout = new QHBoxLayout();
   pMethodLayout->setMargin(0);
   pMethodLayout->setSpacing(5);
   pMethodLayout->addWidget(pMethodLabel);
   pMethodLayout->addWidget(mpMethodCombo);
   pMethodLayout->addStretch(10);

   QLabel* pSaveLabel = new QLabel("Save coefficients to:", pTransformGroup);
   mpCoefficientsFilename = new FileBrowser(pTransformGroup);
   mpCoefficientsFilename->setMinimumWidth(250);
   mpCoefficientsFilename->setBrowseExistingFile(false);
   mpCoefficientsFilename->setBrowseCaption("Select MNF Transform Filename");
   mpCoefficientsFilename->setBrowseFileFilters("MNF Files (*.mnf);;AllFiles (*)");

   QHBoxLayout* pSaveLayout = new QHBoxLayout();
   pSaveLayout->setMargin(0);
   pSaveLayout->setSpacing(5);
   pSaveLayout->addWidget(pSaveLabel);
   pSaveLayout->addWidget(mpCoefficientsFilename, 10);

   mpFileRadio = new QRadioButton("Load From File", pTransformGroup);
   mpFileRadio->setFocusPolicy(Qt::StrongFocus);

   mpFileEdit = new QLineEdit(pTransformGroup);
   mpFileEdit->setMinimumWidth(250);

   QIcon icnBrowse(":/icons/Open");
   QPushButton* pBrowseButton = new QPushButton(icnBrowse, QString(), pTransformGroup);
   pBrowseButton->setFixedWidth(27);
   VERIFYNRV(connect(pBrowseButton, SIGNAL(clicked()), this, SLOT(browse())));

   QHBoxLayout* pFileLayout = new QHBoxLayout();
   pFileLayout->setMargin(0);
   pFileLayout->setSpacing(5);
   pFileLayout->addWidget(mpFileEdit, 10);
   pFileLayout->addWidget(pBrowseButton);

   QGridLayout* pTransformGrid = new QGridLayout(pTransformGroup);
   pTransformGrid->setMargin(10);
   pTransformGrid->setSpacing(5);
   pTransformGrid->setColumnMinimumWidth(0, 13);
   pTransformGrid->addWidget(mpCalculateRadio, 0, 0, 1, 2);
   pTransformGrid->addLayout(pMethodLayout, 1, 1);
   pTransformGrid->addLayout(pSaveLayout, 2, 1);
   pTransformGrid->addWidget(mpFileRadio, 3, 0, 1, 2);
   pTransformGrid->addLayout(pFileLayout, 4, 1);
   pTransformGrid->setRowStretch(5, 10);

   VERIFYNRV(connect(mpCalculateRadio, SIGNAL(toggled(bool)), pMethodLabel, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(mpCalculateRadio, SIGNAL(toggled(bool)), mpMethodCombo, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(mpCalculateRadio, SIGNAL(toggled(bool)), pSaveLabel, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(mpCalculateRadio, SIGNAL(toggled(bool)), mpCoefficientsFilename, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(mpFileRadio, SIGNAL(toggled(bool)), mpFileEdit, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(mpFileRadio, SIGNAL(toggled(bool)), pBrowseButton, SLOT(setEnabled(bool))));

   // Output box
   QGroupBox* pOutputGroup = new QGroupBox("Output", this);
   QLabel* pComponentsLabel = new QLabel("Number of Components:", pOutputGroup);
   mpComponentsSpin = new QSpinBox(pOutputGroup);
   mpComponentsSpin->setMinimum(1);
   mpComponentsSpin->setMaximum(ulBands);
   mpComponentsSpin->setSingleStep(1);
   mpComponentsSpin->setFixedWidth(60);

   mpFromSnrPlot = new QCheckBox("from SNR Plot", pOutputGroup);
   mpFromSnrPlot->setChecked(false);

   QHBoxLayout* pCompLayout = new QHBoxLayout();
   pCompLayout->setMargin(0);
   pCompLayout->setSpacing(5);
   pCompLayout->addWidget(pComponentsLabel, 0, Qt::AlignLeft);
   pCompLayout->addWidget(mpComponentsSpin);
   pCompLayout->addStretch();

   QVBoxLayout* pLayout = new QVBoxLayout();
   pOutputGroup->setLayout(pLayout);
   pLayout->setMargin(10);
   pLayout->setSpacing(5);
   pLayout->addLayout(pCompLayout);
   pLayout->addWidget(mpFromSnrPlot);;
   pLayout->addStretch();

   VERIFYNRV(connect(mpFromSnrPlot, SIGNAL(toggled(bool)), mpComponentsSpin, SLOT(setDisabled(bool))));
   VERIFYNRV(connect(mpCalculateRadio, SIGNAL(toggled(bool)), mpFromSnrPlot, SLOT(setEnabled(bool))));

   // ROI
   mpRoiCheck = new QCheckBox("Region of Interest (ROI):", pOutputGroup);
   mpRoiCombo = new QComboBox(pOutputGroup);
   mpRoiCombo->setEditable(false);
   mpRoiCombo->setMinimumWidth(150);

   for (vector<string>::const_iterator iter = aoiList.begin(); iter != aoiList.end(); ++iter)
   {
      mpRoiCombo->addItem(QString::fromStdString(*iter));
   }

   QHBoxLayout* pRoiLayout = new QHBoxLayout();
   pRoiLayout->setMargin(0);
   pRoiLayout->setSpacing(5);
   pRoiLayout->addWidget(mpRoiCheck);
   pRoiLayout->addWidget(mpRoiCombo);
   pRoiLayout->addStretch(10);

   VERIFYNRV(connect(mpRoiCheck, SIGNAL(toggled(bool)), mpRoiCombo, SLOT(setEnabled(bool))));

   // Horizontal line
   QFrame* pLine = new QFrame(this);
   pLine->setFrameStyle(QFrame::HLine | QFrame::Sunken);

   // OK and Cancel buttons
   QDialogButtonBox* pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
      Qt::Horizontal, this);
   VERIFYNRV(connect(pButtonBox, SIGNAL(accepted()), this, SLOT(accept())));
   VERIFYNRV(connect(pButtonBox, SIGNAL(rejected()), this, SLOT(reject())));

   // Layout
   QGridLayout* pGrid = new QGridLayout(this);
   pGrid->setMargin(10);
   pGrid->setSpacing(10);
   pGrid->addWidget(pTransformGroup, 0, 0);
   pGrid->addWidget(pOutputGroup, 0, 1);
   pGrid->addLayout(pRoiLayout, 1, 0, 1, 2);
   pGrid->addWidget(pLine, 2, 0, 1, 2, Qt::AlignBottom);
   pGrid->addWidget(pButtonBox, 3, 0, 1, 2);
   pGrid->setRowStretch(2, 10);
   pGrid->setColumnStretch(0, 10);

   // Initialization
   mpCalculateRadio->setChecked(true);
   mpFileEdit->setEnabled(false);
   pBrowseButton->setEnabled(false);
   mpComponentsSpin->setValue(ulBands);
   mpRoiCombo->setEnabled(false);
   if (saveFilename.empty() == false)
   {
      mpCoefficientsFilename->setFilename(QString::fromStdString(saveFilename));
   }
}

MnfDlg::~MnfDlg()
{
}

string MnfDlg::getNoiseStatisticsMethod() const
{
   string strMethod;
   if (mpCalculateRadio->isChecked())
   {
      strMethod = mpMethodCombo->currentText().toStdString();
   }

   return strMethod;
}

string MnfDlg::getTransformFilename() const
{
   string strFilename;
   if (mpFileRadio->isChecked())
   {
      strFilename = mpFileEdit->text().toStdString();
   }

   return strFilename;
}

unsigned int MnfDlg::getNumComponents() const
{
   unsigned int ulComponents = 0;
   ulComponents = static_cast<unsigned int>(mpComponentsSpin->value());

   return ulComponents;
}

string MnfDlg::getRoiName() const
{
   string strRoiName;
   if (mpRoiCheck->isChecked())
   {
      strRoiName = mpRoiCombo->currentText().toStdString();
   }

   return strRoiName;
}

void MnfDlg::browse()
{
   QString importPath;
   const Filename* pImportPath = ConfigurationSettings::getSettingImportPath();
   if (pImportPath != NULL)
   {
      importPath = QString::fromStdString(pImportPath->getFullPathAndName());
   }

   QString strFilename = QFileDialog::getOpenFileName(this, "Select MNF Transform File", importPath,
      "MNF files (*.mnf);;All Files (*)");
   if (strFilename.isEmpty())
   {
      return;
   }

   FileResource pFile(strFilename.toStdString().c_str(), "rt");
   if (pFile.get() == NULL)
   {
      QMessageBox::critical(this, "MNF", "Unable to open file:\n" + strFilename);
      return; 
   }

   int lnumBands = 0;
   int lnumComponents = 0;
   int numFieldsRead = 0;
   numFieldsRead = fscanf(pFile, "%d\n", &lnumBands);
   if (numFieldsRead != 1)
   {
      QMessageBox::critical(this, "MNF", "Unable to read from file:\n" + strFilename);
      return;
   }

   numFieldsRead = fscanf(pFile, "%d\n", &lnumComponents);
   if (numFieldsRead != 1)
   {
      QMessageBox::critical(this, "MNF", "Unable to read from file:\n" + strFilename);
      return;
   }

   unsigned int ulMaxBands = mpComponentsSpin->maximum();
   if (lnumBands != ulMaxBands)
   {
      QString message;
      message.sprintf("File-> %s\ncontains MNF results for %d bands.\nThere are %d bands loaded for this image.",
         strFilename, lnumBands, ulMaxBands);
      message = "Error: Mismatch on number of bands!\n" + message;
      QMessageBox::critical(this, "MNF", message);
      return;
   }

   mpComponentsSpin->setMaximum(lnumComponents);
   mpComponentsSpin->setValue(lnumComponents);
   mpFileEdit->setText(strFilename);
}

bool MnfDlg::selectNumComponentsFromPlot()
{
   return mpFromSnrPlot->isChecked();
}

void MnfDlg::setNoiseStatisticsMethods(QStringList& methods)
{
   mpMethodCombo->clear();
   mpMethodCombo->addItems(methods);
}

void MnfDlg::accept()
{
   if (mpFileRadio->isChecked() && mpFileEdit->text().isEmpty())
   {
      QMessageBox::critical(this, windowTitle(), "The filename for the transform coefficients to use is invalid.");
      return;
   }

   if (mpCalculateRadio->isChecked() && mpCoefficientsFilename->getFilename().isEmpty())
   {
      QString msg = "No filename is specified for saving the transform coefficients.\n"
         "Do you want to continue without saving the coefficients?";
      if (QMessageBox::warning(this, windowTitle(), msg, QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
      {
         return;
      }
   }

   QDialog::accept();
}

std::string MnfDlg::getCoefficientsFilename() const
{
   return mpCoefficientsFilename->getFilename().toStdString();
}
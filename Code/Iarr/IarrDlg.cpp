/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtGui/QComboBox>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QFrame>
#include <QtGui/QLabel>
#include <QtGui/QMessageBox>
#include <QtGui/QPushButton>
#include <QtGui/QRadioButton>
#include <QtGui/QSpinBox>
#include <QtGui/QVBoxLayout>

#include "AppVerify.h"
#include "FileBrowser.h"
#include "IarrDlg.h"
#include "LabeledSection.h"
#include "StringUtilities.h"

using namespace std;

IarrDlg::IarrDlg(const string& defaultFilename, const vector<string>& aoiNames,
   int maxRowStepFactor, int maxColumnStepFactor, bool isDouble, bool inMemory, QWidget* pParent) :
   QDialog(pParent)
{
   setModal(true);
   setWindowTitle("IARR");

   // "Average Calculation" Begin
   LabeledSection* pAverageCalculationSection = new LabeledSection("Average Calculation", this);

   mpInputFileRadio = new QRadioButton("Input File:", this);
   mpInputFileBrowser = new FileBrowser(this);
   mpInputFileBrowser->setEnabled(false);

   mpAoiRadio = new QRadioButton("Area of Interest:", this);
   mpAoiRadio->setEnabled(false);
   mpAoiCombo = new QComboBox(this);
   mpAoiCombo->setEnabled(false);

   mpFullExtentsRadio = new QRadioButton("Full Extents:", this);

   QLabel* pRowStepFactorLabel = new QLabel("Row Step Factor:", this);
   mpRowStepFactor = new QSpinBox(this);
   mpRowStepFactor->setEnabled(false);

   QLabel* pColumnStepFactorLabel = new QLabel("Column Step Factor:", this);
   mpColumnStepFactor = new QSpinBox(this);
   mpColumnStepFactor->setEnabled(false);

   QGridLayout* pAverageCalculationLayout = new QGridLayout;
   pAverageCalculationLayout->addWidget(pAverageCalculationSection, 0, 0, 1, -1);
   pAverageCalculationLayout->addWidget(mpInputFileRadio, 1, 0);
   pAverageCalculationLayout->addWidget(mpInputFileBrowser, 1, 1, 1, -1);
   pAverageCalculationLayout->addWidget(mpAoiRadio, 2, 0);
   pAverageCalculationLayout->addWidget(mpAoiCombo, 2, 1, 1, -1);
   pAverageCalculationLayout->addWidget(mpFullExtentsRadio, 3, 0);
   pAverageCalculationLayout->addWidget(pRowStepFactorLabel, 3, 1);
   pAverageCalculationLayout->addWidget(mpRowStepFactor, 3, 2);
   pAverageCalculationLayout->addWidget(pColumnStepFactorLabel, 4, 1);
   pAverageCalculationLayout->addWidget(mpColumnStepFactor, 4, 2);
   // "Average Calculation" End


   // "Output Options" Begin
   LabeledSection* pOutputOptionsSection = new LabeledSection("Output Options", this);

   QLabel* pOutputDataTypeLabel = new QLabel("Data Type:", this);
   mpOutputDataTypeCombo = new QComboBox(this);

   QLabel* pProcessingLocationLabel = new QLabel("Processing Location:", this);
   mpProcessingLocationCombo = new QComboBox(this);

   QLabel* pOutputFileLabel = new QLabel("Output File:", this);
   mpOutputFileBrowser = new FileBrowser(this);

   QGridLayout* pProcessingOptionsLayout = new QGridLayout;
   pProcessingOptionsLayout->addWidget(pOutputOptionsSection, 0, 0, 1, -1);
   pProcessingOptionsLayout->addWidget(pOutputDataTypeLabel, 1, 0);
   pProcessingOptionsLayout->addWidget(mpOutputDataTypeCombo, 1, 1, 1, 4);
   pProcessingOptionsLayout->addWidget(pProcessingLocationLabel, 2, 0);
   pProcessingOptionsLayout->addWidget(mpProcessingLocationCombo, 2, 1, 1, 4);
   pProcessingOptionsLayout->addWidget(pOutputFileLabel, 3, 0);
   pProcessingOptionsLayout->addWidget(mpOutputFileBrowser, 3, 1, 1, 4);
   // "Processing Options" End


   // Button Box Begin
   QDialogButtonBox* pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
      Qt::Horizontal, this);
   // Button Box End


   // Overall Layout Begin
   QFrame* pLine = new QFrame(this);
   pLine->setFrameStyle(QFrame::HLine | QFrame::Sunken);

   QVBoxLayout* pOverallLayout = new QVBoxLayout(this);
   pOverallLayout->addLayout(pAverageCalculationLayout);
   pOverallLayout->addSpacing(2);
   pOverallLayout->addLayout(pProcessingOptionsLayout);
   pOverallLayout->addSpacing(2);
   pOverallLayout->addWidget(pLine);
   pOverallLayout->addWidget(pButtonBox);
   pOverallLayout->setMargin(10);
   pOverallLayout->setSpacing(5);
   pOverallLayout->addStretch(10);
   // Overall Layout End


   // Make GUI connections
   VERIFYNRV(connect(mpInputFileRadio, SIGNAL(toggled(bool)), mpInputFileBrowser, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(mpInputFileRadio, SIGNAL(toggled(bool)), mpOutputFileBrowser, SLOT(setDisabled(bool))));

   VERIFYNRV(connect(mpAoiRadio, SIGNAL(toggled(bool)), mpAoiCombo, SLOT(setEnabled(bool))));

   VERIFYNRV(connect(mpFullExtentsRadio, SIGNAL(toggled(bool)), mpRowStepFactor, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(mpFullExtentsRadio, SIGNAL(toggled(bool)), mpColumnStepFactor, SLOT(setEnabled(bool))));

   VERIFYNRV(connect(pButtonBox, SIGNAL(accepted()), this, SLOT(accept())));
   VERIFYNRV(connect(pButtonBox, SIGNAL(rejected()), this, SLOT(reject())));


   // Initialize values in all appropriate widgets.
   // This must be done after making the connections or the member variables won't be initialized properly

   // Set the default name of the input file
   mpInputFileBrowser->setFilename(QString::fromStdString(defaultFilename));

   // Populate AOI names, selecting the first AOI in the list by default
   if (aoiNames.empty() == false)
   {
      mpAoiRadio->setEnabled(true);
      for (vector<string>::const_iterator iter = aoiNames.begin(); iter != aoiNames.end(); ++iter)
      {
         mpAoiCombo->addItem(QString::fromStdString(*iter));
      }
   }

   // Set the format for the step factor spin boxes
   mpRowStepFactor->setRange(1, maxRowStepFactor);
   mpColumnStepFactor->setRange(1, maxColumnStepFactor);

   // Populate the available output data types, selecting the appropriate default
   if (isDouble == true)
   {
      mpOutputDataTypeCombo->addItem(QString::fromStdString(StringUtilities::toDisplayString(FLT8BYTES)));
      mpOutputDataTypeCombo->addItem(QString::fromStdString(StringUtilities::toDisplayString(FLT4BYTES)));
   }
   else
   {
      mpOutputDataTypeCombo->addItem(QString::fromStdString(StringUtilities::toDisplayString(FLT4BYTES)));
      mpOutputDataTypeCombo->addItem(QString::fromStdString(StringUtilities::toDisplayString(FLT8BYTES)));
   }

   // Populate the available output processing locations, selecting the appropriate default
   if (inMemory == true)
   {
      mpProcessingLocationCombo->addItem(QString::fromStdString(StringUtilities::toDisplayString(IN_MEMORY)));
      mpProcessingLocationCombo->addItem(QString::fromStdString(StringUtilities::toDisplayString(ON_DISK)));
   }
   else
   {
      mpProcessingLocationCombo->addItem(QString::fromStdString(StringUtilities::toDisplayString(ON_DISK)));
      mpProcessingLocationCombo->addItem(QString::fromStdString(StringUtilities::toDisplayString(IN_MEMORY)));
   }

   // Set the default name of the output file
   mpOutputFileBrowser->setFilename(QString::fromStdString(defaultFilename));


   // Enable the appropriate radio button
   if (QFile::exists(QString::fromStdString(defaultFilename)) == true)
   {
      mpInputFileRadio->setChecked(true);
   }
   else if (aoiNames.empty() == false)
   {
      mpAoiRadio->setChecked(true);
   }
   else
   {
      mpFullExtentsRadio->setChecked(true);
   }
}

IarrDlg::~IarrDlg()
{
}

QString IarrDlg::getInputFilename() const
{
   if (mpInputFileBrowser->isEnabled() == true)
   {
      return mpInputFileBrowser->getFilename();
   }

   return QString();
}

QString IarrDlg::getAoiName() const
{
   if (mpAoiCombo->isEnabled() == true)
   {
      return mpAoiCombo->currentText();
   }

   return QString();
}

unsigned int IarrDlg::getRowStepFactor() const
{
   if (mpRowStepFactor->isEnabled() == true)
   {
      return static_cast<unsigned int>(mpRowStepFactor->value());
   }

   return 1;
}

unsigned int IarrDlg::getColumnStepFactor() const
{
   if (mpColumnStepFactor->isEnabled() == true)
   {
      return static_cast<unsigned int>(mpColumnStepFactor->value());
   }

   return 1;
}

EncodingType IarrDlg::getOutputDataType() const
{
   return StringUtilities::fromDisplayString<EncodingType>(
      mpOutputDataTypeCombo->currentText().toStdString());
}

ProcessingLocation IarrDlg::getProcessingLocation() const
{
   return StringUtilities::fromDisplayString<ProcessingLocation>(
      mpProcessingLocationCombo->currentText().toStdString());
}

QString IarrDlg::getOutputFilename() const
{
   if (mpOutputFileBrowser->isEnabled() == true)
   {
      return mpOutputFileBrowser->getFilename();
   }

   return QString();
}

void IarrDlg::accept()
{
   const QString inputFilename = getInputFilename();
   if (inputFilename.isEmpty() == false)
   {
      if (QFile::exists(inputFilename) == false)
      {
         QMessageBox::warning(this, "Invalid Input", "The specified input file cannot be found.");
         return;
      }
   }

   QDialog::accept();
}

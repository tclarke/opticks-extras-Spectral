/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtCore/QFileInfo>
#include <QtCore/QStringList>
#include <QtGui/QBitmap>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QFileDialog>
#include <QtGui/QHeaderView>
#include <QtGui/QInputDialog>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QMessageBox>

#include "AppVerify.h"
#include "ConfigurationSettings.h"
#include "DesktopServices.h"
#include "DynamicObject.h"
#include "Filename.h"
#include "PlugInArgList.h"
#include "PlugInResource.h"
#include "Progress.h"
#include "RasterElement.h"
#include "WavelengthEditor.h"
#include "WavelengthEditorDlg.h"
#include "Wavelengths.h"
#include "WavelengthUnitsComboBox.h"

#include <limits>
#include <vector>
using namespace std;

QString WavelengthEditorDlg::mMetadataFilter = "Wavelength Metadata Files (*.wmd)";
QString WavelengthEditorDlg::mTextFilter = "Wavelength Files (*.wav *.wave)";

WavelengthEditorDlg::WavelengthEditorDlg(QWidget* pParent) :
   QDialog(pParent),
   mpDataset(NULL),
   mWavelengths(mpWavelengthData.get())
{
   instantiate();
}

WavelengthEditorDlg::WavelengthEditorDlg(RasterElement* pDataset, QWidget* pParent) :
   QDialog(pParent),
   mpDataset(pDataset),
   mWavelengths(mpWavelengthData.get())
{
   instantiate();
}

WavelengthEditorDlg::~WavelengthEditorDlg()
{
}

const Wavelengths& WavelengthEditorDlg::getWavelengths() const
{
   return mWavelengths;
}

void WavelengthEditorDlg::accept()
{
   // Apply the wavelengths to the data set
   if (mpDataset != NULL)
   {
      if (mWavelengths.applyToDataset(mpDataset) == false)
      {
         QMessageBox::critical(this, "Wavelength Editor", "The wavelengths could not be applied to the data set.  "
            "The number of wavelength values may not match the number of bands in the data set.");
         return;
      }
   }

   QDialog::accept();
}

void WavelengthEditorDlg::loadWavelengths()
{
   // Get the default import directory
   Service<ConfigurationSettings> pSettings;
   string key = ConfigurationSettings::getSettingPluginWorkingDirectoryKey(Wavelengths::WavelengthType());

   const Filename* pDirectory = pSettings->getSetting(key).getPointerToValue<Filename>();
   if (pDirectory == NULL)
   {
      pDirectory = ConfigurationSettings::getSettingImportPath();
   }

   QString initialDirectory;
   if (pDirectory != NULL)
   {
      initialDirectory = QString::fromStdString(pDirectory->getFullPathAndName());
   }

   // Get the filename from the user
   QString strCaption = "Load Wavelengths";
   QString strSelectedFilter;

   QString strFilename = QFileDialog::getOpenFileName(this, strCaption, initialDirectory,
      mMetadataFilter + ";;" + mTextFilter + ";;All Files (*)", &strSelectedFilter);
   if (strFilename.isEmpty() == true)
   {
      return;
   }

   FactoryResource<Filename> pFilename;
   pFilename->setFullPathAndName(strFilename.toStdString());

   // Update the import directory
   QFileInfo fileInfo(strFilename);
   QString importDirectory = fileInfo.absolutePath();
   if (importDirectory.isEmpty() == false)
   {
      FactoryResource<Filename> pImportDirectory;
      pImportDirectory->setFullPathAndName(importDirectory.toStdString());
      pSettings->setSessionSetting(key, *pImportDirectory.get());
   }

   // Load as metadata in XML format
   ExecutableResource pImporter;
   bool bSuccess = false;

   if (strSelectedFilter != mTextFilter)
   {
      pImporter = ExecutableResource("Wavelength Metadata Importer", string(), NULL, false);
      pImporter->getInArgList().setPlugInArgValue(Wavelengths::WavelengthFileArg(), pFilename.get());

      bSuccess = pImporter->execute();
   }

   // Load as an ASCII text file
   if ((bSuccess == false) && (strSelectedFilter != mMetadataFilter))
   {
      pImporter = ExecutableResource("Wavelength Text Importer", string(), NULL, false);
      pImporter->getInArgList().setPlugInArgValue(Wavelengths::WavelengthFileArg(), pFilename.get());

      bSuccess = pImporter->execute();
   }

   if (bSuccess == true)
   {
      // Update the wavelength values
      const DynamicObject* pWavelengthData =
         pImporter->getOutArgList().getPlugInArgValue<DynamicObject>("Wavelengths");

      mWavelengths.initializeFromDynamicObject(pWavelengthData);
      updateWavelengths();

      // Update the caption
      if (mpDataset == NULL)
      {
         mWavelengthFilename = strFilename;
         updateCaption();
      }
   }
   else
   {
      // Report the error to the user
      Progress* pProgress = pImporter->getProgress();
      if (pProgress != NULL)
      {
         string message;
         int percent = 0;
         ReportingLevel level;
         pProgress->getProgress(message, percent, level);

         if ((message.empty() == false) && (level == ERRORS))
         {
            QMessageBox::critical(this, "Wavelength Editor", QString::fromStdString(message));
         }
      }
   }
}

void WavelengthEditorDlg::saveWavelengths()
{
   // Get the default export directory
   Service<ConfigurationSettings> pSettings;
   string key = ConfigurationSettings::getSettingPluginWorkingDirectoryKey(Wavelengths::WavelengthType());

   const Filename* pDirectory = pSettings->getSetting(key).getPointerToValue<Filename>();
   if (pDirectory == NULL)
   {
      pDirectory = ConfigurationSettings::getSettingExportPath();
   }

   QString initialDirectory;
   if (pDirectory != NULL)
   {
      initialDirectory = QString::fromStdString(pDirectory->getFullPathAndName());
   }

   // Get the filename from the user
   QString strCaption = "Save Wavelengths";
   QString strSelectedFilter;

   QString strFilename = QFileDialog::getSaveFileName(this, strCaption, initialDirectory,
      mMetadataFilter + ";;" + mTextFilter, &strSelectedFilter);
   if (strFilename.isEmpty() == true)
   {
      return;
   }

   // Update the export directory
   QFileInfo fileInfo(strFilename);
   QString exportDirectory = fileInfo.absolutePath();
   if (exportDirectory.isEmpty() == false)
   {
      FactoryResource<Filename> pExportDirectory;
      pExportDirectory->setFullPathAndName(exportDirectory.toStdString());
      pSettings->setSessionSetting(key, *pExportDirectory.get());
   }

   // Save the wavelengths
   ExecutableResource pExporter;
   if (strSelectedFilter == mMetadataFilter)
   {
      pExporter = ExecutableResource("Wavelength Metadata Exporter", string(), NULL, false);

      if (strFilename.endsWith(".wmd") == false)
      {
         strFilename.append(".wmd");
      }
   }
   else if (strSelectedFilter == mTextFilter)
   {
      if (QMessageBox::question(this, "Wavelength Editor", "The wavelength text file format does "
         "not contain units information.  To save the wavelength units in addition to the values, "
         "save the file in the wavelengths metadata format instead.\n\nDo you want to continue?",
         QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
      {
         return;
      }

      pExporter = ExecutableResource("Wavelength Text Exporter", string(), NULL, false);

      if ((strFilename.endsWith(".wav") == false) && (strFilename.endsWith(".wave") == false))
      {
         strFilename.append(".wav");
      }
   }

   FactoryResource<Filename> pFilename;
   pFilename->setFullPathAndName(strFilename.toStdString());

   pExporter->getInArgList().setPlugInArgValue(Wavelengths::WavelengthsArg(), mpWavelengthData.get());
   pExporter->getInArgList().setPlugInArgValue(Wavelengths::WavelengthFileArg(), pFilename.get());

   bool bSuccess = pExporter->execute();
   if (bSuccess == true)
   {
      // Update the caption
      if (mpDataset == NULL)
      {
         mWavelengthFilename = strFilename;
         updateCaption();
      }
   }

   // Report to the user that the exporter is finished
   Progress* pProgress = pExporter->getProgress();
   if (pProgress != NULL)
   {
      string message;
      int percent = 0;
      ReportingLevel level;
      pProgress->getProgress(message, percent, level);

      if (message.empty() == false)
      {
         if (level == NORMAL)
         {
            QMessageBox::information(this, "Wavelength Editor", QString::fromStdString(message));
         }
         else if (level == ERRORS)
         {
            QMessageBox::critical(this, "Wavelength Editor", QString::fromStdString(message));
         }
      }
   }
}

void WavelengthEditorDlg::calculateFwhm()
{
   const vector<double>& centerValues = mWavelengths.getCenterValues();
   if (centerValues.size() < 2)
   {
      QMessageBox::critical(this, "Wavelength Editor", "At least two center wavelength values are required "
         "to calculate the FWHM values.");
      return;
   }

   bool bAccepted = false;

   double dConstant = QInputDialog::getDouble(this, "Calculate FWHM", "FWHM Constant:", 1.0,
      -numeric_limits<double>::max(), numeric_limits<double>::max(), 2, &bAccepted);
   if (bAccepted == true)
   {
      mWavelengths.calculateFwhm(dConstant);
      updateWavelengths();
   }
}

void WavelengthEditorDlg::applyScaleFactor()
{
   bool bAccepted = false;

   double dScale = QInputDialog::getDouble(this, "Apply Scale", "Wavelength Scale Factor:", 1.0,
      -numeric_limits<double>::max(), numeric_limits<double>::max(), 2, &bAccepted);
   if (bAccepted == true)
   {
      mWavelengths.scaleValues(dScale);
      updateWavelengths();
   }
}

void WavelengthEditorDlg::convertWavelengths(Wavelengths::WavelengthUnitsType newUnits)
{
   mWavelengths.setUnits(newUnits);
   updateWavelengths();
}

void WavelengthEditorDlg::updateWavelengths()
{
   mpWavelengthTree->clear();

   // Values
   bool bWavelengths = !(mWavelengths.isEmpty());
   if (bWavelengths == true)
   {
      const vector<double>& startValues = mWavelengths.getStartValues();
      const vector<double>& centerValues = mWavelengths.getCenterValues();
      const vector<double>& endValues = mWavelengths.getEndValues();

      vector<double>::size_type numWavelengths = max(startValues.size(), centerValues.size());
      numWavelengths = max(numWavelengths, endValues.size());

      for (vector<double>::size_type i = 0; i < numWavelengths; ++i)
      {
         QTreeWidgetItem* pItem = new QTreeWidgetItem(mpWavelengthTree);
         if (pItem != NULL)
         {
            if (i < startValues.size())
            {
               pItem->setText(0, QString::number(startValues[i]));
            }

            if (i < centerValues.size())
            {
               pItem->setText(1, QString::number(centerValues[i]));
            }

            if (i < endValues.size())
            {
               pItem->setText(2, QString::number(endValues[i]));
            }
         }
      }
   }

   // Buttons
   mpSaveButton->setEnabled(bWavelengths);
   mpFwhmButton->setEnabled(bWavelengths);
   mpScaleButton->setEnabled(bWavelengths);

   // Units
   Wavelengths::WavelengthUnitsType units = mWavelengths.getUnits();
   mpUnitsCombo->setUnits(units);
}

void WavelengthEditorDlg::updateCaption()
{
   QString strTitle = "Wavelength Editor";
   if (mpDataset != NULL)
   {
      string name = mpDataset->getDisplayName();
      if (name.empty() == true)
      {
         name = mpDataset->getName();
      }

      if (name.empty() == false)
      {
         strTitle += " - " + QString::fromStdString(name);
      }
   }
   else if (mWavelengthFilename.isEmpty() == false)
   {
      QFileInfo fileInfo(mWavelengthFilename);
      strTitle += " - " + fileInfo.fileName();
   }

   setWindowTitle(strTitle);
}

void WavelengthEditorDlg::instantiate()
{
   // Wavelengths
   QLabel* pWavelengthLabel = new QLabel("Wavelengths:", this);
   mpWavelengthTree = new QTreeWidget(this);

   QHeaderView* pHeader = mpWavelengthTree->header();
   if (pHeader != NULL)
   {
      pHeader->setDefaultSectionSize(125);
      pHeader->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
      pHeader->setStretchLastSection(false);
   }

   QStringList columnNames;
   columnNames.append("Min Wavelengths");
   columnNames.append("Center Wavelengths");
   columnNames.append("Max Wavelengths");

   mpWavelengthTree->setColumnCount(columnNames.count());
   mpWavelengthTree->setHeaderLabels(columnNames);
   mpWavelengthTree->setRootIsDecorated(false);
   mpWavelengthTree->setSelectionMode(QAbstractItemView::SingleSelection);
   mpWavelengthTree->setAllColumnsShowFocus(true);
   mpWavelengthTree->setSortingEnabled(false);

   // Wavelength buttons
   QPushButton* pLoadButton = new QPushButton(QIcon(":/icons/Open"), " Load...", this);
   mpSaveButton = new QPushButton(QIcon(":/icons/Save"), " Save...", this);
   mpFwhmButton = new QPushButton("FWHM", this);
   mpScaleButton = new QPushButton("Scale...", this);

   // Units
   QLabel* pUnitsLabel = new QLabel("Units:", this);
   mpUnitsCombo = new WavelengthUnitsComboBox(this);

   // Horizontal line
   QFrame* pHorizontalLine = new QFrame(this);
   pHorizontalLine->setFrameStyle(QFrame::HLine | QFrame::Sunken);

   // Buttons
   QDialogButtonBox* pButtonBox = new QDialogButtonBox(Qt::Horizontal, this);

   if (mpDataset != NULL)
   {
      QPushButton* pOk = new QPushButton("OK", this);
      QPushButton* pCancel = new QPushButton("Cancel", this);
      pButtonBox->addButton(pOk, QDialogButtonBox::AcceptRole);
      pButtonBox->addButton(pCancel, QDialogButtonBox::RejectRole);
      VERIFYNR(connect(pOk, SIGNAL(clicked()), this, SLOT(accept())));
      VERIFYNR(connect(pCancel, SIGNAL(clicked()), this, SLOT(reject())));
   }
   else
   {
      QPushButton* pClose = new QPushButton("Close", this);
      pButtonBox->addButton(pClose, QDialogButtonBox::RejectRole);
      VERIFYNR(connect(pClose, SIGNAL(clicked()), this, SLOT(reject())));
   }

   // Layout
   QHBoxLayout* pUnitsLayout = new QHBoxLayout();
   pUnitsLayout->setMargin(0);
   pUnitsLayout->setSpacing(5);
   pUnitsLayout->addWidget(pUnitsLabel);
   pUnitsLayout->addWidget(mpUnitsCombo);
   pUnitsLayout->addStretch();

   QGridLayout* pGrid = new QGridLayout(this);
   pGrid->setMargin(10);
   pGrid->setSpacing(5);
   pGrid->addWidget(pWavelengthLabel, 0, 0);
   pGrid->addWidget(mpWavelengthTree, 1, 0, 4, 2);
   pGrid->addWidget(pLoadButton, 1, 2);
   pGrid->addWidget(mpSaveButton, 2, 2, Qt::AlignTop);
   pGrid->addWidget(mpFwhmButton, 3, 2);
   pGrid->addWidget(mpScaleButton, 4, 2);
   pGrid->setRowMinimumHeight(5, 5);
   pGrid->addLayout(pUnitsLayout, 6, 0, 1, 2);
   pGrid->setRowMinimumHeight(7, 5);
   pGrid->addWidget(pHorizontalLine, 8, 0, 1, 3);
   pGrid->setRowMinimumHeight(9, 5);
   pGrid->addWidget(pButtonBox, 10, 0, 1, 3);
   pGrid->setRowStretch(2, 10);
   pGrid->setColumnStretch(1, 10);

   // Initialization
   if (mpDataset != NULL)
   {
      const DynamicObject* pMetadata = mpDataset->getMetadata();
      if (pMetadata != NULL)
      {
         mWavelengths.initializeFromDynamicObject(pMetadata);
      }
   }

   setModal(true);
   resize(500, 300);
   updateCaption();
   updateWavelengths();

   // Connections
   VERIFYNR(connect(pLoadButton, SIGNAL(clicked()), this, SLOT(loadWavelengths())));
   VERIFYNR(connect(mpSaveButton, SIGNAL(clicked()), this, SLOT(saveWavelengths())));
   VERIFYNR(connect(mpFwhmButton, SIGNAL(clicked()), this, SLOT(calculateFwhm())));
   VERIFYNR(connect(mpScaleButton, SIGNAL(clicked()), this, SLOT(applyScaleFactor())));
   VERIFYNR(connect(mpUnitsCombo, SIGNAL(unitsActivated(Wavelengths::WavelengthUnitsType)), this,
      SLOT(convertWavelengths(Wavelengths::WavelengthUnitsType))));
   if (WavelengthEditor::hasSettingWavelengthEditorHelp())
   {
      pButtonBox->addButton(QDialogButtonBox::Help);
      VERIFYNR(connect(pButtonBox, SIGNAL(helpRequested()), this, SLOT(help())));
   }
}

void WavelengthEditorDlg::help()
{
   Service<DesktopServices> pDesktop;
   Service<ConfigurationSettings> pSettings;

   string helpFile = pSettings->getHome() + WavelengthEditor::getSettingWavelengthEditorHelp();
   pDesktop->displayHelp(helpFile);
}

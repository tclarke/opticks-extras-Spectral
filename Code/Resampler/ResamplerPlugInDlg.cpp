/*
 * The information in this file is
 * Copyright(c) 2012 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "ConfigurationSettings.h"
#include "DataElement.h"
#include "DesktopServices.h"
#include "FileBrowser.h"
#include "Filename.h"
#include "Importer.h"
#include "LabeledSection.h"
#include "LayerList.h"
#include "ModelServices.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInResource.h"
#include "RasterElement.h"
#include "ResamplerOptions.h"
#include "ResamplerPlugInDlg.h"
#include "Signature.h"
#include "SignatureSelector.h"
#include "SpatialDataView.h"
#include "StringUtilities.h"
#include "TypeConverter.h"
#include "Wavelengths.h"

#include <QtCore/QList>
#include <QtCore/QListIterator>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QDoubleSpinBox>
#include <QtGui/QFrame>
#include <QtGui/QGridLayout>
#include <QtGui/QGroupBox>
#include <QtGui/QLabel>
#include <QtGui/QMessageBox>
#include <QtGui/QPushButton>
#include <QtGui/QRadioButton>
#include <QtGui/QTreeWidget>
#include <QtGui/QTreeWidgetItem>

ResamplerPlugInDlg::ResamplerPlugInDlg(QWidget* pParent) :
   QDialog(pParent, Qt::WindowCloseButtonHint),
   mpSignatures(NULL)
{
   setWindowTitle("Spectral Resampler");

   QGridLayout* pMainLayout = new QGridLayout(this);

   // signatures widgets
   QGroupBox* pSigGroup = new QGroupBox("Signatures to resample:");
   mpSignatures = new QTreeWidget(pSigGroup);
   mpSignatures->setColumnCount(1);
   mpSignatures->setHeaderHidden(true);
   mpSignatures->setSelectionMode(QAbstractItemView::ExtendedSelection);
   mpSignatures->setAllColumnsShowFocus(true);
   mpSignatures->setRootIsDecorated(false);
   mpSignatures->setSortingEnabled(false);
   mpSignatures->setToolTip("This list displays the spectral signatures to be resampled.");

   QPushButton* pAdd = new QPushButton("Add...", pSigGroup);
   QPushButton* pRemove = new QPushButton("Remove", pSigGroup);
   QPushButton* pClear = new QPushButton("Clear All", pSigGroup);

   QGridLayout* pSigGrid = new QGridLayout(pSigGroup);
   pSigGrid->setMargin(10);
   pSigGrid->setSpacing(5);
   pSigGrid->addWidget(mpSignatures, 0, 0, 4, 1);
   pSigGrid->addWidget(pAdd, 0, 1, 1, 1);
   pSigGrid->addWidget(pRemove, 1, 1, 1, 1);
   pSigGrid->addWidget(pClear, 3, 1, 1, 1);
   pSigGrid->setColumnStretch(0, 10);
   pSigGrid->setRowStretch(2, 10);

   // wavelengths widgets
   QGroupBox* pWavelengths = new QGroupBox("Wavelength source:", this);
   QGridLayout* pWaveGrid = new QGridLayout(pWavelengths);
   mpUseDataSource = new QRadioButton("Data:", pWavelengths);
   mpWavelengthsElement = new QComboBox(pWavelengths);
   mpUseFileSource = new QRadioButton("File :", pWavelengths);
   mpWavelengthsFilename = new FileBrowser(pWavelengths);
   pWaveGrid->setMargin(10);
   pWaveGrid->setSpacing(5);
   pWaveGrid->addWidget(mpUseDataSource, 0, 0);
   pWaveGrid->addWidget(mpWavelengthsElement, 0, 1, 1, 4);
   pWaveGrid->addWidget(mpUseFileSource, 1, 0);
   pWaveGrid->addWidget(mpWavelengthsFilename, 1, 1, 1, 4);
   pWaveGrid->setColumnStretch(1, 10);

   // resampling options widgets
   QGroupBox* pOptions = new QGroupBox("Resampler Options", this);
   QGridLayout* pOptionsGrid = new QGridLayout(pOptions);
   QLabel* pMethodsLabel = new QLabel("Resampling Method:", pOptions);
   mpResampleMethod = new QComboBox(pOptions);
   QLabel* pDropOutLabel = new QLabel("Drop Out Window:", pOptions);
   mpDropOutWindow = new QDoubleSpinBox(pOptions);
   mpDropOutWindow->setSuffix(" µm");
   QLabel* pFwhmLabel = new QLabel("Full Width Half Max:", pOptions);
   mpFwhm = new QDoubleSpinBox(pOptions);
   mpFwhm->setSuffix(" µm");
   mpFwhm->setEnabled(false);
   mpUseFillValue = new QCheckBox("Use fill value:");
   mpUseFillValue->setToolTip("Check to ensure the resampled signatures have a value for every wavelength\ncenter. "
      "If an input signature does not have spectral coverage for one of the\ntarget wavelengths, the fill value "
      "will be assigned to that wavelength.");
   mpFillValue = new QDoubleSpinBox;
   mpFillValue->setToolTip("The value to be assigned to wavelengths for which\nthe signature being resampled "
      "does not have spectral coverage.");
   mpFillValue->setRange(-std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
   mpFillValue->setEnabled(false);

   pOptionsGrid->setMargin(10);
   pOptionsGrid->setSpacing(5);
   pOptionsGrid->addWidget(pMethodsLabel, 0, 0);
   pOptionsGrid->addWidget(mpResampleMethod, 0, 1, 1, 3);
   pOptionsGrid->addWidget(pDropOutLabel, 1, 0);
   pOptionsGrid->addWidget(mpDropOutWindow, 1, 1, 1, 3);
   pOptionsGrid->addWidget(pFwhmLabel, 2, 0);
   pOptionsGrid->addWidget(mpFwhm, 2, 1, 1, 3);
   pOptionsGrid->addWidget(mpUseFillValue, 3, 0);
   pOptionsGrid->addWidget(mpFillValue, 3, 1, 1, 3);
   pOptionsGrid->setColumnStretch(1, 10);
   pOptionsGrid->setRowStretch(3, 10);

   // dividing line
   QFrame* pLine = new QFrame(this);
   pLine->setFrameShape(QFrame::HLine);
   pLine->setFrameShadow(QFrame::Sunken);

   // ok/cancel buttons
   QDialogButtonBox* pButtons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);

   pMainLayout->addWidget(pSigGroup, 0, 0, 4, 2);
   pMainLayout->addWidget(pWavelengths, 4, 0, 2, 4);
   pMainLayout->addWidget(pOptions, 6, 0, 3, 4);
   pMainLayout->addWidget(pLine, 9, 0, 1, 4);
   pMainLayout->addWidget(pButtons, 10, 0, 1, 4);

   VERIFYNR(connect(pAdd, SIGNAL(clicked()), this, SLOT(addSignatures())));
   VERIFYNR(connect(pRemove, SIGNAL(clicked()), this, SLOT(removeSignatures())));
   VERIFYNR(connect(pClear, SIGNAL(clicked()), this, SLOT(clearAllSignatures())));
   VERIFYNR(connect(mpUseDataSource, SIGNAL(toggled(bool)), mpWavelengthsElement, SLOT(setEnabled(bool))));
   VERIFYNR(connect(mpUseFileSource, SIGNAL(toggled(bool)), mpWavelengthsFilename, SLOT(setEnabled(bool))));
   VERIFYNR(connect(mpWavelengthsFilename, SIGNAL(filenameChanged(const QString&)),
      this, SLOT(checkValidWaveFile(const QString&))));
   VERIFYNR(connect(mpResampleMethod, SIGNAL(currentIndexChanged(const QString&)),
      this, SLOT(methodChanged(const QString&))));
   VERIFYNR(connect(pButtons, SIGNAL(accepted()), this, SLOT(accept())));
   VERIFYNR(connect(mpUseFillValue, SIGNAL(toggled(bool)), mpFillValue, SLOT(setEnabled(bool))));
   VERIFYNR(connect(pButtons, SIGNAL(rejected()), this, SLOT(reject())));

   initialize();
   if (mpWavelengthsElement->count() > 0)
   {
      mpUseDataSource->setChecked(true);
   }
   else
   {
      mpUseFileSource->setChecked(true);
   }
}

ResamplerPlugInDlg::~ResamplerPlugInDlg()
{}

std::vector<Signature*> ResamplerPlugInDlg::getSignaturesToResample() const
{
   std::vector<Signature*> signatures;
   int numSigs = mpSignatures->topLevelItemCount();
   signatures.reserve(numSigs);
   for (int index = 0; index < numSigs; ++index)
   {
      QVariant variant = mpSignatures->topLevelItem(index)->data(0, Qt::UserRole);
      if (variant.isValid())
      {
         signatures.push_back(variant.value<Signature*>());
      }
   }
   return signatures;
}

const DataElement* ResamplerPlugInDlg::getWavelengthsElement() const
{
   if (mpUseDataSource->isChecked())
   {
      int index = mpWavelengthsElement->currentIndex();
      QVariant variant = mpWavelengthsElement->itemData(index);
      if (variant.isValid())
      {
         return variant.value<DataElement*>();
      }
   }

   return NULL;
}

std::string ResamplerPlugInDlg::getWavelengthsFilename() const
{
   if (mpUseFileSource->isChecked())
   {
      return mpWavelengthsFilename->getFilename().toStdString();
   }

   return std::string();
}

std::string ResamplerPlugInDlg::getResamplingMethod() const
{
   return mpResampleMethod->currentText().toStdString();
}

double ResamplerPlugInDlg::getDropOutWindow() const
{
   return mpDropOutWindow->value();
}

double ResamplerPlugInDlg::getFWHM() const
{
   return mpFwhm->value();
}

void ResamplerPlugInDlg::addSignatures()
{
   SignatureSelector dlg(NULL, this);
   dlg.setWindowTitle("Select Signatures for Resampling");
   if (dlg.exec() == QDialog::Accepted)
   {
      std::vector<Signature*> signatures = dlg.getExtractedSignatures();
      for (std::vector<Signature*>::const_iterator it = signatures.begin(); it != signatures.end(); ++it)
      {
         // if not in list, add
         if (mpSignatures->findItems(QString::fromStdString((*it)->getName()), Qt::MatchExactly).isEmpty())
         {
            QTreeWidgetItem* pItem = new QTreeWidgetItem(mpSignatures);
            pItem->setText(0, QString::fromStdString((*it)->getName()));
            pItem->setData(0, Qt::UserRole, QVariant::fromValue(*it));
         }
      }
   }
}

void ResamplerPlugInDlg::removeSignatures()
{
   QList<QTreeWidgetItem*> items = mpSignatures->selectedItems();
   QListIterator<QTreeWidgetItem*> it(items);
   while (it.hasNext())
   {
      delete it.next();
   }
}

void ResamplerPlugInDlg::clearAllSignatures()
{
   mpSignatures->clear();
}

void ResamplerPlugInDlg::initialize()
{
   mpWavelengthsElement->setEnabled(false);
   mpWavelengthsFilename->setEnabled(false);
   mpWavelengthsFilename->setBrowseExistingFile(true);
   mpWavelengthsFilename->setBrowseDirectory(
      QString::fromStdString(ConfigurationSettings::getSettingImportPath()->getFullPathAndName()));
   QString filters = "Wavelength Metadata files (*.wmd);;Wavelength files (*.wav *.wave);;Text files (*.txt);;"
      "All files (*)";
   mpWavelengthsFilename->setBrowseFileFilters(filters);

   // find raster elements with wavelength info
   std::vector<DataElement*> data = Service<ModelServices>()->getElements(TypeConverter::toString<RasterElement>());
   for (std::vector<DataElement*>::const_iterator iter = data.begin(); iter != data.end(); ++iter)
   {
      RasterElement* pRaster = dynamic_cast<RasterElement*>(*iter);
      if (pRaster != NULL)
      {
         if (Wavelengths::getNumWavelengths(pRaster->getMetadata()) > 0)
         {
            mpWavelengthsElement->addItem(QString::fromStdString(pRaster->getDisplayName(true)),
               QVariant::fromValue(*iter));
         }
      }
   }

   // add signatures to the list
   data = Service<ModelServices>()->getElements(TypeConverter::toString<Signature>());
   for (std::vector<DataElement*>::const_iterator iter = data.begin(); iter != data.end(); ++iter)
   {
      Signature* pSig = dynamic_cast<Signature*>(*iter);
      if (pSig != NULL)
      {
         DataVariant variant = pSig->getData("Wavelength");
         if (variant.isValid())
         {
            std::vector<double> wavelengths;
            if (variant.getValue(wavelengths))
            {
               if (wavelengths.size() > 1)
               {
                  mpWavelengthsElement->addItem(QString::fromStdString(pSig->getDisplayName(true)),
                     QVariant::fromValue(*iter));
               }
            }
         }
      }
   }

   // now check if current view is a spatial data view and set box index to the primary raster for the view
   SpatialDataView* pView =
      dynamic_cast<SpatialDataView*>(Service<DesktopServices>()->getCurrentWorkspaceWindowView());
   if (pView != NULL)
   {
      LayerList* pLayerList = pView->getLayerList();
      if (pLayerList != NULL)
      {
         RasterElement* pPrimary = pLayerList->getPrimaryRasterElement();
         if (pPrimary != NULL)
         {
            int index = mpWavelengthsElement->findText(QString::fromStdString(pPrimary->getDisplayName(true)));
            if (index < 0)  // current view's primary raster might not contain wavelength info and won't be in box
            {
               index = 0;   // set to first item
            }
            mpWavelengthsElement->setCurrentIndex(index);
         }
      }
   }

   // set up the methods combo
   QStringList methods;
   methods << QString::fromStdString(ResamplerOptions::LinearMethod())
           << QString::fromStdString(ResamplerOptions::CubicSplineMethod())
           << QString::fromStdString(ResamplerOptions::GaussianMethod());
   mpResampleMethod->addItems(methods);
   int index = mpResampleMethod->findText(QString::fromStdString(ResamplerOptions::getSettingResamplerMethod()));
   if (index < 0)
   {
      index = 0;
   }
   mpResampleMethod->setCurrentIndex(index);

   mpDropOutWindow->setValue(ResamplerOptions::getSettingDropOutWindow());
   mpFwhm->setValue(ResamplerOptions::getSettingFullWidthHalfMax());
   mpUseFillValue->setChecked(ResamplerOptions::getSettingUseFillValue());
   mpFillValue->setValue(ResamplerOptions::getSettingSignatureFillValue());
}

void ResamplerPlugInDlg::checkValidWaveFile(const QString& filename)
{
   if (filename.isEmpty())  // ok to blank out the filename
   {
      return;
   }

   FactoryResource<Filename> pFilename;
   pFilename->setFullPathAndName(filename.toStdString());
   std::string importerName;
   std::string extension;
   std::vector<std::string> extensionStrs =
      StringUtilities::split(StringUtilities::toLower(pFilename->getExtension()), '.');
   if (extensionStrs.empty() == false)
   {
      extension = extensionStrs.back();
   }
   if (extension == "wmd")
   {
      importerName = "Wavelength Metadata Importer";
   }
   else
   {
      importerName = "Wavelength Text Importer";
   }
   ExecutableResource pImporter(importerName, std::string());
   pImporter->getInArgList().setPlugInArgValue(Wavelengths::WavelengthFileArg(), pFilename.get());

   if (pImporter->execute() == false)
   {
      mpWavelengthsFilename->setFilename("");
      QMessageBox::warning(this, "Spectral Resampler", "File: " + filename +
         " doesn't appear to be a valid wavelengths file.");
   }
}

void ResamplerPlugInDlg::methodChanged(const QString& methodName)
{
   mpFwhm->setEnabled(methodName.toStdString() == ResamplerOptions::GaussianMethod());
}

void ResamplerPlugInDlg::accept()
{
   if (mpSignatures->topLevelItemCount() == 0)
   {
      QMessageBox::warning(this, "Spectral Resampler", "No signatures selected to be resampled.");
      return;
   }

   if (getWavelengthsElement() == NULL && getWavelengthsFilename().empty())
   {
      QMessageBox::warning(this, "Spectral Resampler", "No wavelength source specified.");
      return;
   }
   QDialog::accept();
}

bool ResamplerPlugInDlg::getUseFillValue() const
{
   return mpUseFillValue->isChecked();
}

double ResamplerPlugInDlg::getFillValue() const
{
   return mpFillValue->value();
}

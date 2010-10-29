/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AoiElement.h"
#include "DesktopServices.h"
#include "Layer.h"
#include "LayerList.h"
#include "LocateDialog.h"
#include "ModelServices.h"
#include "RasterElement.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "TypeConverter.h"
#include "Window.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QDoubleSpinBox>
#include <QtGui/QFrame>
#include <QtGui/QGridLayout>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>

LocateDialog::LocateDialog(const RasterElement* pRaster, QWidget* pParent) :
   QDialog(pParent, Qt::WindowCloseButtonHint),
   mpRaster(pRaster),
   mLayerNameBase("Spectral Library Match Locate Results - "),
   mpAlgCombo(NULL),
   mpThreshold(NULL),
   mpOutputLayerName(NULL),
   mpUseAoi(NULL),
   mpAoiCombo(NULL),
   mpSaveSettings(NULL)
{
   setWindowTitle("Locate Matched Signatures Settings");

   // layout
   QGridLayout* pGrid = new QGridLayout(this);
   pGrid->setSpacing(5);
   pGrid->setMargin(10);

   QLabel* pNameLabel = new QLabel("Dataset:", this);
   QLabel* pDataLabel = new QLabel(QString::fromStdString(pRaster->getDisplayName(true)), this);
   pDataLabel->setToolTip(QString::fromStdString(pRaster->getName()));
   QLabel* pAlgLabel = new QLabel("Algorithm:", this);
   mpAlgCombo = new QComboBox(this);
   QLabel* pThresLabel = new QLabel("Threshold:", this);
   mpThreshold = new QDoubleSpinBox(this);
   mpThreshold->setSingleStep(0.1);
   QLabel* pLayerLabel = new QLabel("Output Layer Name:", this);
   mpOutputLayerName = new QLineEdit(this);
   mpUseAoi = new QCheckBox("Area of Interest:", this);
   mpUseAoi->setToolTip("Check box to limit the Locate function to an AOI");
   mpAoiCombo = new QComboBox(this);
   mpAoiCombo->setEnabled(false);
   mpSaveSettings = new QCheckBox("Save the algorithm and threshold settings", this);
   QFrame* pLineSeparator = new QFrame(this);
   pLineSeparator->setFrameStyle(QFrame::HLine | QFrame::Sunken);
   QDialogButtonBox* pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
      Qt::Horizontal, this);

   pGrid->addWidget(pNameLabel, 0, 0, Qt::AlignRight);
   pGrid->addWidget(pDataLabel, 0, 1);
   pGrid->addWidget(pAlgLabel, 1, 0, Qt::AlignRight);
   pGrid->addWidget(mpAlgCombo, 1, 1);
   pGrid->addWidget(pThresLabel, 2, 0, Qt::AlignRight);
   pGrid->addWidget(mpThreshold, 2, 1);
   pGrid->addWidget(pLayerLabel, 3, 0, Qt::AlignRight);
   pGrid->addWidget(mpOutputLayerName, 3, 1);
   pGrid->addWidget(mpUseAoi, 4, 0, Qt::AlignRight);
   pGrid->addWidget(mpAoiCombo, 4, 1);
   pGrid->addWidget(mpSaveSettings, 5, 1);
   pGrid->addWidget(pLineSeparator, 7, 0, 1, 2);
   pGrid->addWidget(pButtons, 8, 0, 1, 2, Qt::AlignRight);
   pGrid->setRowStretch(6, 10);
   pGrid->setColumnStretch(1, 10);

   // initialize algorithm combo
   std::vector<std::string> algNames =
      StringUtilities::getAllEnumValuesAsDisplayString<SpectralLibraryMatch::LocateAlgorithm>();
   for (std::vector<std::string>::const_iterator it = algNames.begin(); it != algNames.end(); ++it)
   {
      mpAlgCombo->addItem(QString::fromStdString(*it));
   }

   // set up algorithm threshold map
   std::vector<std::string> algorithmNames =
      StringUtilities::getAllEnumValuesAsDisplayString<SpectralLibraryMatch::LocateAlgorithm>();
   for (std::vector<std::string>::iterator it = algorithmNames.begin();
      it != algorithmNames.end(); ++it)
   {
      float threshold(0.0f);
      switch (StringUtilities::fromDisplayString<SpectralLibraryMatch::LocateAlgorithm>(*it))
      {
      case SpectralLibraryMatch::SLLA_CEM:
         threshold = SpectralLibraryMatchOptions::getSettingLocateCemThreshold();
         break;

      case SpectralLibraryMatch::SLLA_SAM:
         threshold = SpectralLibraryMatchOptions::getSettingLocateSamThreshold();
         break;

      default:
         threshold = 0.0f;
         break;
      }

      mLocateThresholds.insert(std::pair<std::string, float>(*it, threshold));
   }

   // load aoi combo
   std::vector<DataElement*> aois =
      Service<ModelServices>()->getElements(pRaster, TypeConverter::toString<AoiElement>());
   for (std::vector<DataElement*>::const_iterator it = aois.begin(); it != aois.end(); ++it)
   {
      mpAoiCombo->addItem(QString::fromStdString((*it)->getName()));
   }

   // try to determine the active aoi layer and set combo to the element for that layer
   std::vector<Window*> windows;
   SpatialDataView* pView(NULL);
   Service<DesktopServices>()->getWindows(SPATIAL_DATA_WINDOW, windows);
   for (std::vector<Window*>::iterator it = windows.begin(); it != windows.end(); ++it)
   {
      SpatialDataWindow* pWindow = dynamic_cast<SpatialDataWindow*>(*it);
      if (pWindow != NULL)
      {
         SpatialDataView* pTmpView = dynamic_cast<SpatialDataView*>(pWindow->getView());
         if (pTmpView != NULL)
         {
            LayerList* pLayerList = pTmpView->getLayerList();
            if (pLayerList != NULL)
            {
               if (pRaster == pLayerList->getPrimaryRasterElement())
               {
                  pView = pTmpView;
                  break;
               }
            }
         }
      }
   }

   if (pView != NULL)
   {
      Layer* pLayer = pView->getActiveLayer();
      if (pLayer != NULL)
      {
         DataElement* pElement = pLayer->getDataElement();
         if (pElement != NULL)
         {
            std::string elementName = pElement->getName();
            int index = mpAoiCombo->findText(QString::fromStdString(elementName));
            if (index != -1)
            {
               mpAoiCombo->setCurrentIndex(index);
            }
         }
      }
   }
   if (mpAoiCombo->count() == 0)
   {
      mpUseAoi->setEnabled(false);
   }


   // Initialize From Settings
   SpectralLibraryMatch::LocateAlgorithm locType =
      StringUtilities::fromXmlString<SpectralLibraryMatch::LocateAlgorithm>(
      SpectralLibraryMatchOptions::getSettingLocateAlgorithm());
   mpAlgCombo->setCurrentIndex(mpAlgCombo->findText(QString::fromStdString(
      StringUtilities::toDisplayString<SpectralLibraryMatch::LocateAlgorithm>(locType))));
   mpThreshold->setValue(mLocateThresholds[mpAlgCombo->currentText().toStdString()]);
   std::string layerName = mLayerNameBase;
   switch (locType)
   {
   case SpectralLibraryMatch::SLLA_CEM:
      layerName += "CEM";
      break;

   case SpectralLibraryMatch::SLLA_SAM:
      layerName += "SAM";
      break;

   default:
      layerName += "Unknown Algorithm";
      break;
   }
   mpOutputLayerName->setText(QString::fromStdString(layerName));

   // connections
   VERIFYNR(connect(pButtons, SIGNAL(accepted()), this, SLOT(accept())));
   VERIFYNR(connect(pButtons, SIGNAL(rejected()), this, SLOT(reject())));
   VERIFYNR(connect(mpAlgCombo, SIGNAL(currentIndexChanged(const QString&)),
      this, SLOT(algorithmChanged(const QString&))));
   VERIFYNR(connect(mpThreshold, SIGNAL(valueChanged(double)),
      this, SLOT(thresholdChanged(double))));
   VERIFYNR(connect(mpUseAoi, SIGNAL(toggled(bool)), mpAoiCombo, SLOT(setEnabled(bool))));
}

LocateDialog::~LocateDialog()
{}

void LocateDialog::accept()
{
   if (mpSaveSettings->isChecked())
   {
      SpectralLibraryMatch::LocateAlgorithm locType =
         StringUtilities::fromDisplayString<SpectralLibraryMatch::LocateAlgorithm>(
         mpAlgCombo->currentText().toStdString());
      SpectralLibraryMatchOptions::setSettingLocateAlgorithm(
         StringUtilities::toXmlString<SpectralLibraryMatch::LocateAlgorithm>(locType));
      for (std::map<std::string, float>::iterator it = mLocateThresholds.begin();
         it != mLocateThresholds.end(); ++it)
      {
         float threshold = it->second;
         switch (StringUtilities::fromDisplayString<SpectralLibraryMatch::LocateAlgorithm>(it->first))
         {
         case SpectralLibraryMatch::SLLA_CEM:
            SpectralLibraryMatchOptions::setSettingLocateCemThreshold(threshold);
            break;

         case SpectralLibraryMatch::SLLA_SAM:
            SpectralLibraryMatchOptions::setSettingLocateSamThreshold(threshold);
            break;

         default:
            // no setting for any other value
            break;
         }
      }
   }

   QDialog::accept();
}

void LocateDialog::algorithmChanged(const QString& text)
{
   mpThreshold->setValue(mLocateThresholds[text.toStdString()]);
   std::string layerName = mLayerNameBase;
   switch (StringUtilities::fromDisplayString<SpectralLibraryMatch::LocateAlgorithm>(text.toStdString()))
   {
   case SpectralLibraryMatch::SLLA_CEM:
      layerName += "CEM";
      break;

   case SpectralLibraryMatch::SLLA_SAM:
      layerName += "SAM";
      break;

   default:
      layerName += "Unknown Algorithm";
      break;
   }
   mpOutputLayerName->setText(QString::fromStdString(layerName));
}

void LocateDialog::thresholdChanged(double value)
{
   mLocateThresholds[mpAlgCombo->currentText().toStdString()] = value;
}

SpectralLibraryMatch::LocateAlgorithm LocateDialog::getLocateAlgorithm() const
{
   return StringUtilities::fromDisplayString<SpectralLibraryMatch::LocateAlgorithm>(
      mpAlgCombo->currentText().toStdString());
}

double LocateDialog::getThreshold() const
{
   return mpThreshold->value();
}

std::string LocateDialog::getOutputLayerName() const
{
   return mpOutputLayerName->text().toStdString();
}

AoiElement* LocateDialog::getAoi() const
{
   if (mpUseAoi->isChecked() == false)
   {
      return NULL;
   }

   std::string aoiName = mpAoiCombo->currentText().toStdString();
   if (aoiName.empty())
   {
      return NULL;
   }

   return dynamic_cast<AoiElement*>(Service<ModelServices>()->getElement(aoiName,
      TypeConverter::toString<AoiElement>(), mpRaster));
}

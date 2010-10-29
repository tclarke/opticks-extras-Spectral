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
#include "DataElement.h"
#include "DesktopServices.h"
#include "Layer.h"
#include "LayerList.h"
#include "MatchIdDlg.h"
#include "ModelServices.h"
#include "PlugInManagerServices.h"
#include "RasterElement.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "SpectralLibraryManager.h"
#include "SpectralLibraryMatch.h"
#include "SpectralLibraryMatchOptions.h"
#include "TypeConverter.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QFrame>
#include <QtGui/QGridLayout>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QPushButton>
#include <QtGui/QSpinBox>

MatchIdDlg::MatchIdDlg(const RasterElement* pRaster, QWidget* pParent) :
   QDialog(pParent, Qt::WindowCloseButtonHint),
   mpRaster(pRaster),
   mpAoiCombo(NULL),
   mpMatchEachCheckBox(NULL),
   mpLimitByMaxNum(NULL),
   mpMaxMatchesSpinBox(NULL),
   mpLimitByThreshold(NULL),
   mpThresholdLimit(NULL),
   mpEditLayerName(NULL),
   mpAlgorithmCombo(NULL),
   mpSaveSettings(NULL)
{
   setWindowTitle("Spectral Library Match");

   // layout
   QGridLayout* pGrid = new QGridLayout(this);
   pGrid->setSpacing(5);
   pGrid->setMargin(10);

   // dataset name area
   QLabel* pNameLabel = new QLabel("Dataset:", this);
   QLabel* pDataLabel = new QLabel(QString::fromStdString(pRaster->getDisplayName(true)), this);
   pDataLabel->setToolTip(QString::fromStdString(pRaster->getName()));

   // aoi area
   QLabel* pAoiLabel = new QLabel("AOI:", this);
   mpAoiCombo = new QComboBox(this);
   mpMatchEachCheckBox = new QCheckBox("Match each pixel in AOI", this);
   mpMatchEachCheckBox->setChecked(true);

   // layer name area
   QLabel* pLayerLabel = new QLabel("Layer name:", this);
   mpEditLayerName = new QLineEdit("Spectral Library Match results", this);
   VERIFYNR(connect(mpMatchEachCheckBox, SIGNAL(toggled(bool)), mpEditLayerName, SLOT(setEnabled(bool))));

   // algorithm area
   QLabel* pAlgLabel = new QLabel("Match algorithm:", this);
   mpAlgorithmCombo = new QComboBox(this);
   mpLimitByMaxNum = new QCheckBox("Limit matches to max number:", this);
   mpMaxMatchesSpinBox = new QSpinBox(this);
   mpMaxMatchesSpinBox->setRange(1, 100);
   mpLimitByThreshold = new QCheckBox("Limit to matches below threshold:", this);
   mpThresholdLimit = new QDoubleSpinBox(this);
   mpThresholdLimit->setSingleStep(0.1);
   mpThresholdLimit->setRange(0.0, 90.0);
   VERIFYNR(connect(mpLimitByMaxNum, SIGNAL(toggled(bool)), mpMaxMatchesSpinBox, SLOT(setEnabled(bool))));
   VERIFYNR(connect(mpLimitByThreshold, SIGNAL(toggled(bool)), mpThresholdLimit, SLOT(setEnabled(bool))));

   // save settings
   mpSaveSettings = new QCheckBox("Save settings", this);

   // button area
   QFrame* pLineSeparator = new QFrame(this);
   pLineSeparator->setFrameStyle(QFrame::HLine | QFrame::Sunken);
   QDialogButtonBox* pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                     Qt::Horizontal, this);
   QPushButton* pEditLibButton = new QPushButton("Edit Library...", this);
   pButtons->addButton(pEditLibButton, QDialogButtonBox::ActionRole);
   VERIFYNR(connect(pButtons, SIGNAL(accepted()), this, SLOT(accept())));
   VERIFYNR(connect(pButtons, SIGNAL(rejected()), this, SLOT(reject())));

   pGrid->addWidget(pNameLabel, 0, 0, Qt::AlignRight);
   pGrid->addWidget(pDataLabel, 0, 1, 1, 2);
   pGrid->addWidget(pAoiLabel, 1, 0, Qt::AlignRight);
   pGrid->addWidget(mpAoiCombo, 1, 1, 1, 2);
   pGrid->addWidget(mpMatchEachCheckBox, 2, 1, 1, 2);
   pGrid->addWidget(pLayerLabel, 3, 0, Qt::AlignRight);
   pGrid->addWidget(mpEditLayerName, 3, 1, 1, 2);
   pGrid->addWidget(pAlgLabel, 4, 0, Qt::AlignRight);
   pGrid->addWidget(mpAlgorithmCombo, 4, 1, 1, 2);
   pGrid->addWidget(mpLimitByMaxNum, 5, 1);
   pGrid->addWidget(mpMaxMatchesSpinBox, 5, 2, Qt::AlignLeft);
   pGrid->addWidget(mpLimitByThreshold, 6, 1);
   pGrid->addWidget(mpThresholdLimit, 6, 2, Qt::AlignLeft);
   pGrid->addWidget(mpSaveSettings, 7, 1, Qt::AlignLeft);
   pGrid->addWidget(pLineSeparator, 9, 0, 1, 3);
   pGrid->addWidget(pButtons, 10, 0, 1, 3, Qt::AlignRight);
   pGrid->setRowStretch(8, 10);
   pGrid->setColumnStretch(2, 10);

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
         SpatialDataView* pTmpView = pWindow->getSpatialDataView();
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
            mpAoiCombo->setCurrentIndex(index);
         }
      }
   }

   // load algorithm combobox and set current index to user option
   std::vector<std::string> algNames =
      StringUtilities::getAllEnumValuesAsDisplayString<SpectralLibraryMatch::MatchAlgorithm>();
   for (std::vector<std::string>::iterator it = algNames.begin(); it!= algNames.end(); ++it)
   {
      mpAlgorithmCombo->addItem(QString::fromStdString(*it));
   }
   std::string strAlg = StringUtilities::toDisplayString<SpectralLibraryMatch::MatchAlgorithm>(
      StringUtilities::fromXmlString<SpectralLibraryMatch::MatchAlgorithm>(
      SpectralLibraryMatchOptions::getSettingMatchAlgorithm()));
   mpAlgorithmCombo->setCurrentIndex(mpAlgorithmCombo->findText(QString::fromStdString(strAlg)));
   mpMatchEachCheckBox->setChecked(SpectralLibraryMatchOptions::getSettingMatchEachPixel());

   // set max matches
   mpLimitByMaxNum->setChecked(SpectralLibraryMatchOptions::getSettingLimitByMaxNum());
   mpMaxMatchesSpinBox->setValue(SpectralLibraryMatchOptions::getSettingMaxDisplayed());
   mpMaxMatchesSpinBox->setEnabled(mpLimitByMaxNum->isChecked());

   // set threshold limit
   mpLimitByThreshold->setChecked(SpectralLibraryMatchOptions::getSettingLimitByThreshold());
   mpThresholdLimit->setValue(SpectralLibraryMatchOptions::getSettingMatchThreshold());
   mpThresholdLimit->setEnabled(mpLimitByThreshold->isChecked());

   // connect edit lib button to library manager
   std::vector<PlugIn*> plugIns = Service<PlugInManagerServices>()->getPlugInInstances(
      SpectralLibraryMatch::getNameLibraryManagerPlugIn());
   if (!plugIns.empty())
   {
      SpectralLibraryManager* pLibMgr = dynamic_cast<SpectralLibraryManager*>(plugIns.front());
      if (pLibMgr != NULL)
      {
         VERIFYNR(connect(pEditLibButton, SIGNAL(clicked()), pLibMgr, SLOT(editSpectralLibrary())));
      }
   }
}

MatchIdDlg::~MatchIdDlg()
{}

void MatchIdDlg::accept()
{
   if (mpAoiCombo->currentText().isEmpty())
   {
      Service<DesktopServices>()->showMessageBox("Spectral Library Match",
         "You must select the AOI to be matched.");
      return;
   }

   // check that spectral library has signatures loaded
   std::vector<PlugIn*> plugIns = Service<PlugInManagerServices>()->getPlugInInstances(
      SpectralLibraryMatch::getNameLibraryManagerPlugIn());
   if (!plugIns.empty())
   {
      SpectralLibraryManager* pLibMgr = dynamic_cast<SpectralLibraryManager*>(plugIns.front());
      if (pLibMgr != NULL)
      {
         if (pLibMgr->isEmpty())
         {
            Service<DesktopServices>()->showMessageBox("Spectral Library Match", "The spectral library is empty. "
               "Click on the Edit Library button to add signatures to the library.");
            return;
         }
      }
   }

   AoiElement* pAoi = getAoi();
   if (pAoi == NULL)
   {
      Service<DesktopServices>()->showMessageBox("Spectral Library Match", "Unable to access the selected AOI.");
      return;
   }

   // save to options
   if (mpSaveSettings->isChecked())
   {
      SpectralLibraryMatchOptions::setSettingMatchAlgorithm(
         StringUtilities::toXmlString(StringUtilities::fromDisplayString<SpectralLibraryMatch::MatchAlgorithm>(
         mpAlgorithmCombo->currentText().toStdString())));
      SpectralLibraryMatchOptions::setSettingMatchEachPixel(mpMatchEachCheckBox->isChecked());
      SpectralLibraryMatchOptions::setSettingLimitByMaxNum(mpLimitByMaxNum->isChecked());
      SpectralLibraryMatchOptions::setSettingMaxDisplayed(mpMaxMatchesSpinBox->value());
      SpectralLibraryMatchOptions::setSettingLimitByThreshold(mpLimitByThreshold->isChecked());
      SpectralLibraryMatchOptions::setSettingMatchThreshold(mpThresholdLimit->value());
   }

   QDialog::accept();
}

AoiElement* MatchIdDlg::getAoi() const
{
   Service<ModelServices> pModel;
   return dynamic_cast<AoiElement*>(pModel->getElement(mpAoiCombo->currentText().toStdString(),
      TypeConverter::toString<AoiElement>(), const_cast<RasterElement*>(mpRaster)));
}

bool MatchIdDlg::getMatchEachPixel() const
{
   return mpMatchEachCheckBox->isChecked();
}

bool MatchIdDlg::getLimitByNumber() const
{
   return mpLimitByMaxNum->isChecked();
}

int MatchIdDlg::getMaxMatches() const
{
   return mpMaxMatchesSpinBox->value();
}

bool MatchIdDlg::getLimitByThreshold() const
{
   return mpLimitByThreshold->isChecked();
}

double MatchIdDlg::getThresholdLimit() const
{
   return mpThresholdLimit->value();
}

std::string MatchIdDlg::getLayerName() const
{
   return mpEditLayerName->text().toStdString() + " - " + mpAlgorithmCombo->currentText().toStdString();
}

SpectralLibraryMatch::MatchAlgorithm MatchIdDlg::getMatchAlgorithm() const
{
   return StringUtilities::fromDisplayString<SpectralLibraryMatch::MatchAlgorithm>(
      mpAlgorithmCombo->currentText().toStdString());
}

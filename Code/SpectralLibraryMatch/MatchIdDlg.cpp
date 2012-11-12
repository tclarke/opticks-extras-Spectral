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
   mLayerNameBase("Spectral Library Match Results - "),
   mpAoiCombo(NULL),
   mpMatchEachCheckBox(NULL),
   mpLimitByMaxNum(NULL),
   mpMaxMatchesSpinBox(NULL),
   mpLimitByThreshold(NULL),
   mpThreshold(NULL),
   mpOutputLayerName(NULL),
   mpAlgCombo(NULL),
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
   mpOutputLayerName = new QLineEdit(this);
   VERIFYNR(connect(mpMatchEachCheckBox, SIGNAL(toggled(bool)), mpOutputLayerName, SLOT(setEnabled(bool))));

   // algorithm area
   QLabel* pAlgLabel = new QLabel("Match algorithm:", this);
   mpAlgCombo = new QComboBox(this);
   mpLimitByMaxNum = new QCheckBox("Limit matches to max number:", this);
   mpMaxMatchesSpinBox = new QSpinBox(this);
   mpMaxMatchesSpinBox->setRange(1, 100);
   mpLimitByThreshold = new QCheckBox("Limit to matches below threshold:", this);
   mpThreshold = new QDoubleSpinBox(this);
   mpThreshold->setSingleStep(0.1);
   mpThreshold->setRange(0.0, 90.0);
   VERIFYNR(connect(mpLimitByMaxNum, SIGNAL(toggled(bool)), mpMaxMatchesSpinBox, SLOT(setEnabled(bool))));
   VERIFYNR(connect(mpLimitByThreshold, SIGNAL(toggled(bool)), mpThreshold, SLOT(setEnabled(bool))));

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
   pGrid->addWidget(mpOutputLayerName, 3, 1, 1, 2);
   pGrid->addWidget(pAlgLabel, 4, 0, Qt::AlignRight);
   pGrid->addWidget(mpAlgCombo, 4, 1, 1, 2);
   pGrid->addWidget(mpLimitByMaxNum, 5, 1);
   pGrid->addWidget(mpMaxMatchesSpinBox, 5, 2, Qt::AlignLeft);
   pGrid->addWidget(mpLimitByThreshold, 6, 1);
   pGrid->addWidget(mpThreshold, 6, 2, Qt::AlignLeft);
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
         // set index to first entry just in case the active layer is not an AOI layer
         mpAoiCombo->setCurrentIndex(0);
         AoiElement* pElement = dynamic_cast<AoiElement*>(pLayer->getDataElement());
         if (pElement != NULL)
         {
            std::string elementName = pElement->getName();
            int index = mpAoiCombo->findText(QString::fromStdString(elementName));
            mpAoiCombo->setCurrentIndex(index);
         }
      }
   }

   // load algorithm combobox
   std::vector<std::string> algNames =
      StringUtilities::getAllEnumValuesAsDisplayString<SpectralLibraryMatch::MatchAlgorithm>();
   for (std::vector<std::string>::iterator it = algNames.begin(); it!= algNames.end(); ++it)
   {
      mpAlgCombo->addItem(QString::fromStdString(*it));
   }

   // set up algorithm threshold map
   std::vector<std::string> algorithmNames =
      StringUtilities::getAllEnumValuesAsDisplayString<SpectralLibraryMatch::MatchAlgorithm>();
   for (std::vector<std::string>::iterator it = algorithmNames.begin();
      it != algorithmNames.end(); ++it)
   {
      float threshold(0.0f);
      switch (StringUtilities::fromDisplayString<SpectralLibraryMatch::MatchAlgorithm>(*it))
      {
      case SpectralLibraryMatch::SLMA_SAM:
         threshold = SpectralLibraryMatchOptions::getSettingMatchSamThreshold();
         break;

      case SpectralLibraryMatch::SLMA_WBI:
         threshold = SpectralLibraryMatchOptions::getSettingMatchWbiThreshold();
         break;

      default:
         // nothing to do
         break;
      }

      mMatchThresholds.insert(std::pair<std::string, float>(*it, threshold));
   }

   // set current index to user option
   SpectralLibraryMatch::MatchAlgorithm matchAlg =
      StringUtilities::fromXmlString<SpectralLibraryMatch::MatchAlgorithm>(
      SpectralLibraryMatchOptions::getSettingMatchAlgorithm());
   std::string strAlg = StringUtilities::toDisplayString<SpectralLibraryMatch::MatchAlgorithm>(matchAlg);
   mpAlgCombo->setCurrentIndex(mpAlgCombo->findText(QString::fromStdString(strAlg)));
   mpMatchEachCheckBox->setChecked(SpectralLibraryMatchOptions::getSettingMatchEachPixel());

   // set max matches
   mpLimitByMaxNum->setChecked(SpectralLibraryMatchOptions::getSettingLimitByMaxNum());
   mpMaxMatchesSpinBox->setValue(SpectralLibraryMatchOptions::getSettingMaxDisplayed());
   mpMaxMatchesSpinBox->setEnabled(mpLimitByMaxNum->isChecked());

   // set threshold limit
   mpLimitByThreshold->setChecked(SpectralLibraryMatchOptions::getSettingLimitByThreshold());
   mpThreshold->setValue(mMatchThresholds[mpAlgCombo->currentText().toStdString()]);
   QString layerName = mLayerNameBase;
   switch (matchAlg)
   {
   case SpectralLibraryMatch::SLMA_SAM:
      layerName += "SAM";
      break;

   case SpectralLibraryMatch::SLMA_WBI:
      layerName += "WBI";
      break;

   default:
      layerName += "Unknown Algorithm";
      break;
   }
   mpOutputLayerName->setText(layerName);
   mpThreshold->setEnabled(mpLimitByThreshold->isChecked());

   // connections
   VERIFYNR(connect(mpAlgCombo, SIGNAL(currentIndexChanged(const QString&)),
      this, SLOT(algorithmChanged(const QString&))));
   VERIFYNR(connect(mpThreshold, SIGNAL(valueChanged(double)),
      this, SLOT(thresholdChanged(double))));

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
      SpectralLibraryMatch::MatchAlgorithm matchAlg =
         StringUtilities::fromDisplayString<SpectralLibraryMatch::MatchAlgorithm>(
         mpAlgCombo->currentText().toStdString());
      SpectralLibraryMatchOptions::setSettingMatchAlgorithm(StringUtilities::toXmlString(matchAlg));
      SpectralLibraryMatchOptions::setSettingMatchEachPixel(mpMatchEachCheckBox->isChecked());
      SpectralLibraryMatchOptions::setSettingLimitByMaxNum(mpLimitByMaxNum->isChecked());
      SpectralLibraryMatchOptions::setSettingMaxDisplayed(mpMaxMatchesSpinBox->value());
      SpectralLibraryMatchOptions::setSettingLimitByThreshold(mpLimitByThreshold->isChecked());
      switch (matchAlg)
      {
      case SpectralLibraryMatch::SLMA_SAM:
         SpectralLibraryMatchOptions::setSettingMatchSamThreshold(mpThreshold->value());
         break;

      case SpectralLibraryMatch::SLMA_WBI:
         SpectralLibraryMatchOptions::setSettingMatchWbiThreshold(mpThreshold->value());
         break;

      default:   // nothing to do
         break;
      }
   }

   QDialog::accept();
}

void MatchIdDlg::algorithmChanged(const QString& text)
{
   mpThreshold->setValue(mMatchThresholds[text.toStdString()]);
   QString layerName = mLayerNameBase;
   switch (StringUtilities::fromDisplayString<SpectralLibraryMatch::MatchAlgorithm>(text.toStdString()))
   {
   case SpectralLibraryMatch::SLMA_SAM:
      layerName += "SAM";
      break;

   case SpectralLibraryMatch::SLMA_WBI:
      layerName += "WBI";
      break;

   default:
      layerName += "Unknown Algorithm";
      break;
   }
   mpOutputLayerName->setText(layerName);
}

void MatchIdDlg::thresholdChanged(double value)
{
   mMatchThresholds[mpAlgCombo->currentText().toStdString()] = value;
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
   return mpThreshold->value();
}

std::string MatchIdDlg::getOutputLayerName() const
{
   return mpOutputLayerName->text().toStdString();
}

SpectralLibraryMatch::MatchAlgorithm MatchIdDlg::getMatchAlgorithm() const
{
   return StringUtilities::fromDisplayString<SpectralLibraryMatch::MatchAlgorithm>(
      mpAlgCombo->currentText().toStdString());
}

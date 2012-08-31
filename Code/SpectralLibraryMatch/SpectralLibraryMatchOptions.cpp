/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "LabeledSection.h"
#include "OptionQWidgetWrapper.h"
#include "PlugInRegistration.h"
#include "SpectralLibraryMatchOptions.h"
#include "StringUtilities.h"

#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QDoubleSpinBox>
#include <QtGui/QGridLayout>
#include <QtGui/QLabel>
#include <QtGui/QSpinBox>

REGISTER_PLUGIN(SpectralSpectralLibraryMatch, SpectralLibraryMatchOptions,
                OptionQWidgetWrapper<SpectralLibraryMatchOptions>);

SpectralLibraryMatchOptions::SpectralLibraryMatchOptions()
{
   // match options section
   QWidget* pMatchWidget = new QWidget(this);
   QGridLayout* pMatchLayout = new QGridLayout(pMatchWidget);
   QLabel* pMatchAlgLabel = new QLabel("Algorithm:", pMatchWidget);
   mpMatchAlgCombo = new QComboBox(pMatchWidget);
   mpMatchEachPixel = new QCheckBox("Match each pixel in AOI", pMatchWidget);
   mpLimitByMaxNum = new QCheckBox("Limit by number:", pMatchWidget);
   mpLimitByMaxNum->setToolTip("Check to only display up to this number of matches");
   mpMaxDisplayed = new QSpinBox(pMatchWidget);
   mpLimitByThreshold = new QCheckBox("Limit by threshold:", pMatchWidget);
   mpLimitByThreshold->setToolTip("Check to only display matches below the threshold");
   mpMatchThreshold = new QDoubleSpinBox(pMatchWidget);
   mpMatchThreshold->setSingleStep(0.1);
   mpMatchThreshold->setToolTip("Limit the displayed matches to signatures\n"
      "with match values less than this threshold");
   mpAutoclear = new QCheckBox("Autoclear Results", pMatchWidget);
   mpAutoclear->setToolTip("Check to clear existing results before adding new results.\nIf not checked, new results "
      "will be added to existing results.");
   pMatchLayout->setMargin(0);
   pMatchLayout->setSpacing(5);
   pMatchLayout->addWidget(pMatchAlgLabel, 0, 0, Qt::AlignRight);
   pMatchLayout->addWidget(mpMatchAlgCombo, 0, 1);
   pMatchLayout->setColumnStretch(1, 10);
   pMatchLayout->addWidget(mpMatchEachPixel, 1, 1);
   pMatchLayout->addWidget(mpLimitByMaxNum, 2, 0);
   pMatchLayout->addWidget(mpMaxDisplayed, 2, 1);
   pMatchLayout->addWidget(mpLimitByThreshold, 3, 0);
   pMatchLayout->addWidget(mpMatchThreshold, 3, 1);
   pMatchLayout->addWidget(mpAutoclear, 4, 0);
   LabeledSection* pMatchSection = new LabeledSection(pMatchWidget, "Spectral Library Match Options", this);

   // locate options section
   QWidget* pLocateWidget = new QWidget(this);
   QGridLayout* pLocateLayout = new QGridLayout(pLocateWidget);
   QLabel* pLocAlgLabel = new QLabel("Algorithm:", pLocateWidget);
   mpLocateAlgCombo = new QComboBox(pLocateWidget);
   QLabel* pThresLabel = new QLabel("Locate Threshold:", pLocateWidget);
   mpLocateThreshold = new QDoubleSpinBox(pLocateWidget);
   mpLocateThreshold->setSingleStep(0.1);
   mpDisplayLocateOptions = new QCheckBox("Display Locate options before running", pLocateWidget);
   mpDisplayLocateOptions->setToolTip("Check this box to display the Locate options dialog "
      "before each run of the Locate function.\nUncheck it to suppress the dialog and use the current settings.");

   pLocateLayout->setMargin(0);
   pLocateLayout->setSpacing(5);
   pLocateLayout->addWidget(pLocAlgLabel, 0, 0, Qt::AlignRight);
   pLocateLayout->addWidget(mpLocateAlgCombo, 0, 1);
   pLocateLayout->setColumnStretch(1, 10);
   pLocateLayout->addWidget(pThresLabel, 2, 0);
   pLocateLayout->addWidget(mpLocateThreshold, 2, 1);
   pLocateLayout->addWidget(mpDisplayLocateOptions, 3, 1);
   LabeledSection* pLocateSection = new LabeledSection(pLocateWidget, "Locate Matched Signatures Options", this);

   addSection(pMatchSection);
   addSection(pLocateSection);
   addStretch(10);
   setSizeHint(100, 100);

   // initialize algorithm combo boxes
   std::vector<std::string> algNames =
      StringUtilities::getAllEnumValuesAsDisplayString<SpectralLibraryMatch::MatchAlgorithm>();
   for (std::vector<std::string>::const_iterator it = algNames.begin(); it != algNames.end(); ++it)
   {
      mpMatchAlgCombo->addItem(QString::fromStdString(*it));
   }
   algNames = StringUtilities::getAllEnumValuesAsDisplayString<SpectralLibraryMatch::LocateAlgorithm>();
   for (std::vector<std::string>::const_iterator it = algNames.begin(); it != algNames.end(); ++it)
   {
      mpLocateAlgCombo->addItem(QString::fromStdString(*it));
   }

   // connections
   VERIFYNR(connect(mpLocateAlgCombo, SIGNAL(currentIndexChanged(const QString&)),
      this, SLOT(locateAlgorithmChanged(const QString&))));
   VERIFYNR(connect(mpLimitByMaxNum, SIGNAL(toggled(bool)),
      mpMaxDisplayed, SLOT(setEnabled(bool))));
   VERIFYNR(connect(mpLimitByThreshold, SIGNAL(toggled(bool)),
      mpMatchThreshold, SLOT(setEnabled(bool))));
   VERIFYNR(connect(mpLocateThreshold, SIGNAL(valueChanged(double)),
      this, SLOT(locateThresholdChanged(double))));

   // set up locate algorithm threshold map
   std::vector<std::string> locateAlgorithmNames =
      StringUtilities::getAllEnumValuesAsDisplayString<SpectralLibraryMatch::LocateAlgorithm>();
   for (std::vector<std::string>::iterator it = locateAlgorithmNames.begin();
      it != locateAlgorithmNames.end(); ++it)
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

   // Initialize From Settings
   SpectralLibraryMatch::MatchAlgorithm matType = StringUtilities::fromXmlString<SpectralLibraryMatch::MatchAlgorithm>(
      SpectralLibraryMatchOptions::getSettingMatchAlgorithm());
   mpMatchAlgCombo->setCurrentIndex(mpMatchAlgCombo->findText(QString::fromStdString(
      StringUtilities::toDisplayString<SpectralLibraryMatch::MatchAlgorithm>(matType))));
   bool matchEach = SpectralLibraryMatchOptions::getSettingMatchEachPixel();
   mpMatchEachPixel->setChecked(matchEach);
   bool limit = SpectralLibraryMatchOptions::getSettingLimitByMaxNum();
   mpLimitByMaxNum->setChecked(limit);
   mpMaxDisplayed->setValue(SpectralLibraryMatchOptions::getSettingMaxDisplayed());
   mpMaxDisplayed->setEnabled(limit);
   limit = SpectralLibraryMatchOptions::getSettingLimitByThreshold();
   mpLimitByThreshold->setChecked(limit);
   mpMatchThreshold->setValue(SpectralLibraryMatchOptions::getSettingMatchThreshold());
   mpMatchThreshold->setEnabled(limit);
   bool autoClear = SpectralLibraryMatchOptions::getSettingAutoclear();
   mpAutoclear->setChecked(autoClear);
   SpectralLibraryMatch::LocateAlgorithm locType =
      StringUtilities::fromXmlString<SpectralLibraryMatch::LocateAlgorithm>(
      SpectralLibraryMatchOptions::getSettingLocateAlgorithm());
   mpLocateAlgCombo->setCurrentIndex(mpLocateAlgCombo->findText(QString::fromStdString(
      StringUtilities::toDisplayString<SpectralLibraryMatch::LocateAlgorithm>(locType))));
   mpLocateThreshold->setValue(mLocateThresholds[mpLocateAlgCombo->currentText().toStdString()]);
   mpDisplayLocateOptions->setChecked(SpectralLibraryMatchOptions::getSettingDisplayLocateOptions());
}

SpectralLibraryMatchOptions::~SpectralLibraryMatchOptions()
{}

void SpectralLibraryMatchOptions::applyChanges()
{
   SpectralLibraryMatchOptions::setSettingMatchEachPixel(mpMatchEachPixel->isChecked());
   SpectralLibraryMatchOptions::setSettingLimitByMaxNum(mpLimitByMaxNum->isChecked());
   SpectralLibraryMatchOptions::setSettingMaxDisplayed(mpMaxDisplayed->value());
   SpectralLibraryMatchOptions::setSettingLimitByThreshold(mpLimitByThreshold->isChecked());
   SpectralLibraryMatchOptions::setSettingMatchThreshold(mpMatchThreshold->value());
   SpectralLibraryMatchOptions::setSettingAutoclear(mpAutoclear->isChecked());
   SpectralLibraryMatch::MatchAlgorithm matType =
      StringUtilities::fromDisplayString<SpectralLibraryMatch::MatchAlgorithm>(
      mpMatchAlgCombo->currentText().toStdString());
   SpectralLibraryMatchOptions::setSettingMatchAlgorithm(
      StringUtilities::toXmlString<SpectralLibraryMatch::MatchAlgorithm>(matType));
   SpectralLibraryMatch::LocateAlgorithm locType =
      StringUtilities::fromDisplayString<SpectralLibraryMatch::LocateAlgorithm>(
      mpLocateAlgCombo->currentText().toStdString());
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
   SpectralLibraryMatchOptions::setSettingDisplayLocateOptions(mpDisplayLocateOptions->isChecked());
}

void SpectralLibraryMatchOptions::locateAlgorithmChanged(const QString& text)
{
   mpLocateThreshold->setValue(mLocateThresholds[text.toStdString()]);
}

void SpectralLibraryMatchOptions::locateThresholdChanged(double value)
{
   mLocateThresholds[mpLocateAlgCombo->currentText().toStdString()] = value;
}

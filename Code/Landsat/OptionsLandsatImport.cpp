/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "LabeledSection.h"
#include "OptionsLandsatImport.h"
#include "OptionQWidgetWrapper.h"
#include "PlugInRegistration.h"

#include <QtGui/QCheckBox>
#include <QtGui/QLabel>
#include <QtGui/QVBoxLayout>

REGISTER_PLUGIN(SpectralLandsat, OptionsLandsatImport, OptionQWidgetWrapper<OptionsLandsatImport>());

OptionsLandsatImport::OptionsLandsatImport() :
   QWidget(NULL)
{
   std::vector<std::pair<std::string, std::string> > entries;
   entries.push_back(std::make_pair("Default Load VNIR Digital Numbers", "vnir-DN"));
   entries.push_back(std::make_pair("Default Load VNIR At Sensor Radiance", "vnir-Radiance"));
   entries.push_back(std::make_pair("Default Load VNIR At Sensor Reflectance", "vnir-Reflectance"));
   entries.push_back(std::make_pair("Default Load PAN Digital Numbers", "pan-DN"));
   entries.push_back(std::make_pair("Default Load PAN At Sensor Radiance", "pan-Radiance"));
   entries.push_back(std::make_pair("Default Load PAN At Sensor Reflectance", "pan-Reflectance"));
   entries.push_back(std::make_pair("Default Load TIR Digital Numbers", "tir-DN"));
   entries.push_back(std::make_pair("Default Load TIR At Sensor Radiance", "tir-Radiance"));
   entries.push_back(std::make_pair("Default Load TIR At Sensor Temperature", "tir-Temperature"));

   QWidget* pWidget = new QWidget(this);
   QVBoxLayout* pLayout = new QVBoxLayout(pWidget);
   pLayout->setMargin(0);
   pLayout->setSpacing(5);
   std::vector<std::string> defaultImport = OptionsLandsatImport::getSettingDefaultImport();
   for (std::vector<std::pair<std::string, std::string> >::const_iterator iter = entries.begin();
      iter != entries.end();
      ++iter)
   {
      QCheckBox* pCheckBox = new QCheckBox(QString::fromStdString(iter->first), this);
      if (std::find(defaultImport.begin(), defaultImport.end(), iter->second) != defaultImport.end())
      {
         pCheckBox->setChecked(true);
      }
      pLayout->addWidget(pCheckBox);
      mCheckBoxes.push_back(std::make_pair(pCheckBox, iter->second));
   }
   pLayout->addStretch(10);
   LabeledSection* pSection = new LabeledSection(pWidget,
      "Default Landsat GeoTiff load", this);

   // Dialog layout
   QVBoxLayout* pDlgLayout = new QVBoxLayout(this);
   pDlgLayout->setMargin(0);
   pDlgLayout->setSpacing(10);
   pDlgLayout->addWidget(pSection);
   pDlgLayout->addStretch(10);
}

OptionsLandsatImport::~OptionsLandsatImport()
{}

void OptionsLandsatImport::applyChanges()
{
   std::vector<std::string> defaultImport;
   for (std::vector<std::pair<QCheckBox*, std::string> >::const_iterator iter = mCheckBoxes.begin();
      iter != mCheckBoxes.end();
      ++iter)
   {
      if (iter->first->isChecked())
      {
         defaultImport.push_back(iter->second);
      }
   }
   OptionsLandsatImport::setSettingDefaultImport(defaultImport);
}

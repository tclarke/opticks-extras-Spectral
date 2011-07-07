/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "LabeledSection.h"
#include "OptionsDgImport.h"
#include "OptionQWidgetWrapper.h"
#include "PlugInRegistration.h"

#include <QtGui/QCheckBox>
#include <QtGui/QLabel>
#include <QtGui/QVBoxLayout>

REGISTER_PLUGIN(SpectralDgFormats, OptionsWV2Import, OptionQWidgetWrapper<OptionsWV2Import>());
REGISTER_PLUGIN(SpectralDgFormats, OptionsQB2Import, OptionQWidgetWrapper<OptionsQB2Import>());

OptionsWV2Import::OptionsWV2Import() :
   QWidget(NULL)
{
   std::vector<std::pair<std::string, std::string> > entries;
   entries.push_back(std::make_pair("Default Load Digital Numbers", "DN"));
   entries.push_back(std::make_pair("Default Load At Sensor Radiance", "Radiance"));
   entries.push_back(std::make_pair("Default Load At Sensor Reflectance", "Reflectance"));

   QWidget* pWidget = new QWidget(this);
   QVBoxLayout* pLayout = new QVBoxLayout(pWidget);
   pLayout->setMargin(0);
   pLayout->setSpacing(5);
   std::vector<std::string> defaultImport = OptionsWV2Import::getSettingDefaultWV2Import();
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
      "Default WorldView-2 load", this);

   // Dialog layout
   QVBoxLayout* pDlgLayout = new QVBoxLayout(this);
   pDlgLayout->setMargin(0);
   pDlgLayout->setSpacing(10);
   pDlgLayout->addWidget(pSection);
   pDlgLayout->addStretch(10);
}

OptionsWV2Import::~OptionsWV2Import()
{}

void OptionsWV2Import::applyChanges()
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
   OptionsWV2Import::setSettingDefaultWV2Import(defaultImport);
}

OptionsQB2Import::OptionsQB2Import() :
   QWidget(NULL)
{
   std::vector<std::pair<std::string, std::string> > entries;
   entries.push_back(std::make_pair("Default Load Digital Numbers", "DN"));
   entries.push_back(std::make_pair("Default Load At Sensor Radiance", "Radiance"));
   entries.push_back(std::make_pair("Default Load At Sensor Reflectance", "Reflectance"));

   QWidget* pWidget = new QWidget(this);
   QVBoxLayout* pLayout = new QVBoxLayout(pWidget);
   pLayout->setMargin(0);
   pLayout->setSpacing(5);
   std::vector<std::string> defaultImport = OptionsQB2Import::getSettingDefaultQB2Import();
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
      "Default QuickBird-2 load", this);

   // Dialog layout
   QVBoxLayout* pDlgLayout = new QVBoxLayout(this);
   pDlgLayout->setMargin(0);
   pDlgLayout->setSpacing(10);
   pDlgLayout->addWidget(pSection);
   pDlgLayout->addStretch(10);
}

OptionsQB2Import::~OptionsQB2Import()
{}

void OptionsQB2Import::applyChanges()
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
   OptionsQB2Import::setSettingDefaultQB2Import(defaultImport);
}
/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef OPTIONSDGMPORT_H
#define OPTIONSDGMPORT_H

#include "ConfigurationSettings.h"
#include "SpectralVersion.h"

#include <QtGui/QWidget>

#include <string>
#include <utility>
#include <vector>

class QCheckBox;

class OptionsWV2Import : public QWidget
{
   Q_OBJECT

public:
   SETTING(DefaultWV2Import, DgFormats, std::vector<std::string>, std::vector<std::string>());
   OptionsWV2Import();
   ~OptionsWV2Import();

   void applyChanges();

   static const std::string& getName()
   {
      static std::string var = "WV-2 Import Options";
      return var;
   }

   static const std::string& getOptionName()
   {
      static std::string var = "Import/WorldView-2";
      return var;
   }

   static const std::string& getDescription()
   {
      static std::string var = "Widget to display DigitalGlobe WorldView-2 import options";
      return var;
   }

   static const std::string& getShortDescription()
   {
      static std::string var = "Widget to display DigitalGlobe WorldView-2 import options";
      return var;
   }

   static const std::string& getCreator()
   {
      static std::string var = "Ball Aerospace & Technologies Corp.";
      return var;
   }

   static const std::string& getCopyright()
   {
      static std::string var = SPECTRAL_COPYRIGHT;
      return var;
   }

   static const std::string& getVersion()
   {
      static std::string var = SPECTRAL_VERSION_NUMBER;
      return var;
   }

   static bool isProduction()
   {
      return SPECTRAL_IS_PRODUCTION_RELEASE;
   }

   static const std::string& getDescriptorId()
   {
      static std::string var = "{401FF60B-B7A4-4797-9698-C3034C327C39}";
      return var;
   }

private:
   std::vector<std::pair<QCheckBox*, std::string> > mCheckBoxes;
};

class OptionsQB2Import : public QWidget
{
   Q_OBJECT

public:
   SETTING(DefaultQB2Import, DgFormats, std::vector<std::string>, std::vector<std::string>());
   OptionsQB2Import();
   ~OptionsQB2Import();

   void applyChanges();

   static const std::string& getName()
   {
      static std::string var = "QB-2 Import Options";
      return var;
   }

   static const std::string& getOptionName()
   {
      static std::string var = "Import/QuickBird-2";
      return var;
   }

   static const std::string& getDescription()
   {
      static std::string var = "Widget to display DigitalGlobe QuickBird-2 import options";
      return var;
   }

   static const std::string& getShortDescription()
   {
      static std::string var = "Widget to display DigitalGlobe QuickBird-2 import options";
      return var;
   }

   static const std::string& getCreator()
   {
      static std::string var = "Ball Aerospace & Technologies Corp.";
      return var;
   }

   static const std::string& getCopyright()
   {
      static std::string var = SPECTRAL_COPYRIGHT;
      return var;
   }

   static const std::string& getVersion()
   {
      static std::string var = SPECTRAL_VERSION_NUMBER;
      return var;
   }

   static bool isProduction()
   {
      return SPECTRAL_IS_PRODUCTION_RELEASE;
   }

   static const std::string& getDescriptorId()
   {
      static std::string var = "{F1470D52-E3F9-44E6-89B6-B8F64A0B65FF}";
      return var;
   }

private:
   std::vector<std::pair<QCheckBox*, std::string> > mCheckBoxes;
};

#endif
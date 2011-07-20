/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef OPTIONSLANDSATIMPORT_H
#define OPTIONSLANDSATIMPORT_H

#include "ConfigurationSettings.h"
#include "SpectralVersion.h"

#include <QtGui/QWidget>

#include <string>
#include <utility>
#include <vector>

class QCheckBox;

class OptionsLandsatImport : public QWidget
{
   Q_OBJECT

public:
   SETTING(DefaultImport, Landsat, std::vector<std::string>, std::vector<std::string>());
   OptionsLandsatImport();
   ~OptionsLandsatImport();

   void applyChanges();

   static const std::string& getName()
   {
      static std::string var = "Landsat Import Options";
      return var;
   }

   static const std::string& getOptionName()
   {
      static std::string var = "Import/Landsat";
      return var;
   }

   static const std::string& getDescription()
   {
      static std::string var = "Widget to display Landsat import options";
      return var;
   }

   static const std::string& getShortDescription()
   {
      static std::string var = "Widget to display Landsat import options";
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
      static std::string var = "{95115C6B-F35C-4EEf-A136-5C5285A76926}";
      return var;
   }

private:
   std::vector<std::pair<QCheckBox*, std::string> > mCheckBoxes;
};

#endif
/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SPECTRALLIBRARYMATCHOPTIONS_H
#define SPECTRALLIBRARYMATCHOPTIONS_H

#include "ConfigurationSettings.h"
#include "LabeledSectionGroup.h"
#include "SpectralLibraryMatch.h"
#include "SpectralVersion.h"
#include "TypesFile.h"

#include <map>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;

class SpectralLibraryMatchOptions : public LabeledSectionGroup
{
   Q_OBJECT

public:
   SpectralLibraryMatchOptions();
   ~SpectralLibraryMatchOptions();

   SETTING(MatchAlgorithm, SpectralLibraryMatch, std::string, 
      StringUtilities::toXmlString<SpectralLibraryMatch::MatchAlgorithm>(SpectralLibraryMatch::SLMA_SAM));
   SETTING(MatchEachPixel, SpectralLibraryMatch, bool, true);
   SETTING(LimitByMaxNum, SpectralLibraryMatch, bool, true);
   SETTING(MaxDisplayed, SpectralLibraryMatch, unsigned int, 5);
   SETTING(LimitByThreshold, SpectralLibraryMatch, bool, true);
   SETTING(MatchThreshold, SpectralLibraryMatch, float, 5.0f);
   SETTING(LocateAlgorithm, SpectralLibraryMatch, std::string, 
      StringUtilities::toXmlString<SpectralLibraryMatch::LocateAlgorithm>(SpectralLibraryMatch::SLLA_SAM));
   SETTING(LocateSamThreshold, SpectralLibraryMatch, float, 5.0f);
   SETTING(LocateCemThreshold, SpectralLibraryMatch, float, 0.5f);
   SETTING(DisplayLocateOptions, SpectralLibraryMatch, bool, false);
   SETTING(Autoclear, SpectralLibraryMatch, bool, false);

   void applyChanges();

   static const std::string& getName()
   {
      static std::string var = "Spectral Library Match Options";
      return var;
   }

   static const std::string& getOptionName()
   {
      static std::string var = "Tools/Spectral Library Match";
      return var;
   }

   static const std::string& getDescription()
   {
      static std::string var = "Widget to display Spectral Library Match options";
      return var;
   }

   static const std::string& getShortDescription()
   {
      static std::string var = "Widget to display Spectral Library Match options";
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
      static std::string var = "{E9821B7C-5E06-4d3b-B6F7-1AD949FA8E41}";
      return var;
   }

protected slots:
   void locateAlgorithmChanged(const QString& text);
   void locateThresholdChanged(double value);

private:
   QComboBox* mpMatchAlgCombo;
   QCheckBox* mpMatchEachPixel;
   QCheckBox* mpLimitByMaxNum;
   QSpinBox* mpMaxDisplayed;
   QCheckBox* mpLimitByThreshold;
   QDoubleSpinBox* mpMatchThreshold;
   QCheckBox* mpAutoclear;
   QComboBox* mpLocateAlgCombo;
   QDoubleSpinBox* mpLocateThreshold;
   QCheckBox* mpDisplayLocateOptions;
   std::map<std::string, float> mLocateThresholds;
};

#endif

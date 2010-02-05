/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RESAMPLEROPTIONS_H
#define RESAMPLEROPTIONS_H

#include "ConfigurationSettings.h"
#include "LabeledSectionGroup.h"
#include "SpectralVersion.h"

class QComboBox;
class QDoubleSpinBox;

class ResamplerOptions : public LabeledSectionGroup
{
   Q_OBJECT

public:
   ResamplerOptions();
   ~ResamplerOptions();

   static std::string LinearMethod() { return "Linear"; }
   static std::string CubicSplineMethod() { return "Cubic Spline"; }
   static std::string GaussianMethod() { return "Gaussian"; }

   SETTING(ResamplerMethod, Resampler, std::string, LinearMethod());
   SETTING(DropOutWindow, Resampler, double, 0.05);
   SETTING(FullWidthHalfMax, Resampler, double, 0.01);

   void applyChanges();

   static const std::string& getName()
   {
      static std::string var = "Resampler Options";
      return var;
   }

   static const std::string& getOptionName()
   {
      static std::string var = "Resampler";
      return var;
   }

   static const std::string& getDescription()
   {
      static std::string var = "Widget to display Resampler options";
      return var;
   }

   static const std::string& getShortDescription()
   {
      static std::string var = "Widget to display Resampler options";
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
      static std::string var = "{6941EBD3-C62A-401f-99E5-561B7C7254D2}";
      return var;
   }

public slots:
   void currentIndexChanged(int newIndex);

private:
   QComboBox* mpMethod;
   QDoubleSpinBox* mpDropOutWindow;
   QDoubleSpinBox* mpFullWidthHalfMax;
};

#endif

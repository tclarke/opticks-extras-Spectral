/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SIGNATUREWINDOWOPTIONS_H
#define SIGNATUREWINDOWOPTIONS_H

#include "ColorType.h"
#include "ConfigurationSettings.h"
#include "LabeledSectionGroup.h"
#include "SpectralVersion.h"

class CustomColorButton;
class QCheckBox;
class QRadioButton;

class SignatureWindowOptions : public LabeledSectionGroup
{
public:
   SignatureWindowOptions();
   ~SignatureWindowOptions();

   SETTING(UseAoiColorForAverage, SignatureWindow, bool, false);                  // default = false, use fixed color
   SETTING(AoiAverageColor, SignatureWindow, ColorType, ColorType(255, 0, 0));    // default = red
   SETTING(UseAoiColorForAoiSignatures, SignatureWindow, bool, false);            // default = false, use fixed color
   SETTING(AoiSignaturesColor, SignatureWindow, ColorType, ColorType(0, 0, 0));   // default = black
   SETTING(PixelSignaturesColor, SignatureWindow, ColorType, ColorType(0, 0, 0)); // default = black
   SETTING(RescaleOnAdd, SignatureWindow, bool, true);
   SETTING(PinSignaturePlot, SignatureWindow, bool, false);
   SETTING(ScaleToFirstSignature, SignatureWindow, bool, false);

   void applyChanges();

   static const std::string& getName()
   {
      static std::string var = "Signature Window Options";
      return var;
   }

   static const std::string& getOptionName()
   {
      static std::string var = "Windows/Signature";
      return var;
   }

   static const std::string& getDescription()
   {
      static std::string var = "Widget to display Signature Window options";
      return var;
   }

   static const std::string& getShortDescription()
   {
      static std::string var = "Widget to display Signature Window options";
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
      static std::string var = "{4986A63A-030E-477d-A018-86F2F74E701B}";
      return var;
   }

private:
   QRadioButton* mpUseFixedColorForAvg;
   CustomColorButton* mpAoiAverageColor;
   QRadioButton* mpUseAoiColorForAverage;
   QRadioButton* mpUseFixedColorForAoiSigs;
   CustomColorButton* mpAoiSignaturesColor;
   QRadioButton* mpUseAoiColorForAoiSignatures;
   CustomColorButton* mpPixelSignaturesColor;
   QCheckBox* mpRescaleOnAdd;
   QCheckBox* mpScaleToFirst;
   QCheckBox* mpPinSigPlot;
};

#endif

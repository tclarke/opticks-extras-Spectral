/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef PROPERTIESSIGNATUREPLOTOBJECT_H
#define PROPERTIESSIGNATUREPLOTOBJECT_H

#include <QtGui/QCheckBox>
#include <QtGui/QLabel>
#include <QtGui/QSpinBox>

#include "LabeledSectionGroup.h"

#include <string>

class CustomColorButton;
class SessionItem;
class SignaturePlotObject;

class PropertiesSignaturePlotObject : public LabeledSectionGroup
{
   Q_OBJECT

public:
   PropertiesSignaturePlotObject();
   ~PropertiesSignaturePlotObject();

   bool initialize(SessionItem* pSessionItem);
   bool applyChanges();

   static const std::string& getName();
   static const std::string& getPropertiesName();
   static const std::string& getDescription();
   static const std::string& getShortDescription();
   static const std::string& getCreator();
   static const std::string& getCopyright();
   static const std::string& getVersion();
   static const std::string& getDescriptorId();
   static bool isProduction();

protected slots:
   void enableRegionProperties(bool bEnable);

private:
   SignaturePlotObject* mpPlot;

   // Regions
   QCheckBox* mpRegionCheck;
   QLabel* mpRegionOpacityLabel;
   QSpinBox* mpRegionOpacitySpin;
   QLabel* mpRegionColorLabel;
   CustomColorButton* mpRegionColorButton;
   QCheckBox* mpRescaleOnAdd;
   QCheckBox* mpScaleToFirst;
   QCheckBox* mpResampleToDataset;
};

#endif

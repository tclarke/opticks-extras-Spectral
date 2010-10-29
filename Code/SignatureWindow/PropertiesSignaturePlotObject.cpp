/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtGui/QLayout>

#include "CustomColorButton.h"
#include "LabeledSection.h"
#include "PlotView.h"
#include "PlotWidget.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PropertiesQWidgetWrapper.h"
#include "PropertiesSignaturePlotObject.h"
#include "SignaturePlotObject.h"
#include "SignatureWindow.h"
#include "SpectralVersion.h"

using namespace std;

REGISTER_PLUGIN(SpectralSignatureWindow, PropertiesSignaturePlotObject, PropertiesQWidgetWrapper<PropertiesSignaturePlotObject>());

PropertiesSignaturePlotObject::PropertiesSignaturePlotObject() :
   LabeledSectionGroup(NULL),
   mpPlot(NULL)
{
   // Regions
   QWidget* pRegionWidget = new QWidget(this);

   mpRegionCheck = new QCheckBox("Shade regions of non-loaded bands", pRegionWidget);

   mpRegionOpacityLabel = new QLabel("Opacity:", pRegionWidget);
   mpRegionOpacitySpin = new QSpinBox(pRegionWidget);
   mpRegionOpacitySpin->setRange(0, 255);
   mpRegionOpacitySpin->setSingleStep(1);

   mpRegionColorLabel = new QLabel("Color:", pRegionWidget);
   mpRegionColorButton = new CustomColorButton(pRegionWidget);
   mpRegionColorButton->usePopupGrid(true);

   LabeledSection* pRegionSection = new LabeledSection(pRegionWidget, "Regions", this);

   // Layout
   QGridLayout* pRegionGrid = new QGridLayout(pRegionWidget);
   pRegionGrid->setMargin(0);
   pRegionGrid->setSpacing(5);
   pRegionGrid->addWidget(mpRegionCheck, 0, 0, 1, 3);
   pRegionGrid->addWidget(mpRegionOpacityLabel, 1, 1);
   pRegionGrid->addWidget(mpRegionOpacitySpin, 1, 2, Qt::AlignLeft);
   pRegionGrid->addWidget(mpRegionColorLabel, 2, 1);
   pRegionGrid->addWidget(mpRegionColorButton, 2, 2, Qt::AlignLeft);
   pRegionGrid->setColumnMinimumWidth(0, 15);
   pRegionGrid->setRowStretch(3, 10);
   pRegionGrid->setColumnStretch(2, 10);

   // Rescale
   QWidget* pRescaleWidget = new QWidget(this);
   mpRescaleOnAdd = new QCheckBox("Rescale on addition", pRescaleWidget);
   mpRescaleOnAdd->setToolTip("Check to enable rescaling the plot when a new signature is added.");
   mpScaleToFirst = new QCheckBox("Scale to first signature", pRescaleWidget);
   mpScaleToFirst->setToolTip("Check to enable scaling signatures in the plot to the first signature added.");
   LabeledSection* pRescaleSection = new LabeledSection(pRescaleWidget, "Scaling Options", this);
   QVBoxLayout* pRescaleLayout = new QVBoxLayout(pRescaleWidget);
   pRescaleLayout->setMargin(0);
   pRescaleLayout->setSpacing(5);
   pRescaleLayout->addWidget(mpRescaleOnAdd);
   pRescaleLayout->addWidget(mpScaleToFirst);

   // Initialization
   addSection(pRegionSection);
   addSection(pRescaleSection);
   addStretch(10);
   setSizeHint(325, 125);

   // Connections
   VERIFYNR(connect(mpRegionCheck, SIGNAL(toggled(bool)), this, SLOT(enableRegionProperties(bool))));
}

PropertiesSignaturePlotObject::~PropertiesSignaturePlotObject()
{
}

bool PropertiesSignaturePlotObject::initialize(SessionItem* pSessionItem)
{
   mpPlot = NULL;

   PlotWidget* pPlotWidget = dynamic_cast<PlotWidget*>(pSessionItem);
   if (pPlotWidget != NULL)
   {
      Service<PlugInManagerServices> pManager;

      vector<PlugIn*> plugIns = pManager->getPlugInInstances("Signature Window");
      if (plugIns.size() == 1)
      {
         SignatureWindow* pPlugIn = dynamic_cast<SignatureWindow*>(plugIns.front());
         if (pPlugIn != NULL)
         {
            mpPlot = pPlugIn->getSignaturePlot(pPlotWidget);
         }
      }
   }

   if (mpPlot == NULL)
   {
      return false;
   }

   // Regions
   mpRegionCheck->setChecked(mpPlot->areRegionsDisplayed());
   mpRegionColorButton->setColor(mpPlot->getRegionColor());
   mpRegionOpacitySpin->setValue(mpPlot->getRegionOpacity());
   enableRegionProperties(mpPlot->areRegionsDisplayed());

   // Rescale
   mpRescaleOnAdd->setChecked(mpPlot->getRescaleOnAdd());
   mpScaleToFirst->setChecked(mpPlot->getScaleToFirst());

   return true;
}

bool PropertiesSignaturePlotObject::applyChanges()
{
   if (mpPlot == NULL)
   {
      return false;
   }

   // Regions
   mpPlot->displayRegions(mpRegionCheck->isChecked());
   mpPlot->setRegionOpacity(mpRegionOpacitySpin->value());
   mpPlot->setRegionColor(mpRegionColorButton->getColor());

   // Rescale
   mpPlot->setRescaleOnAdd(mpRescaleOnAdd->isChecked());
   mpPlot->setScaleToFirst(mpScaleToFirst->isChecked());

   // Refresh the plot
   PlotWidget* pPlotWidget = mpPlot->getPlotWidget();
   if (pPlotWidget != NULL)
   {
      PlotView* pPlotView = pPlotWidget->getPlot();
      if (pPlotView != NULL)
      {
         pPlotView->refresh();
      }
   }

   return true;
}

const string& PropertiesSignaturePlotObject::getName()
{
   static string name = "Signature Plot Properties";
   return name;
}

const string& PropertiesSignaturePlotObject::getPropertiesName()
{
   static string propertiesName = "Signature Plot";
   return propertiesName;
}

const string& PropertiesSignaturePlotObject::getDescription()
{
   static string description = "General setting properties of a signature plot";
   return description;
}

const string& PropertiesSignaturePlotObject::getShortDescription()
{
   static string description;
   return description;
}

const string& PropertiesSignaturePlotObject::getCreator()
{
   static string creator = "Ball Aerospace & Technologies Corp.";
   return creator;
}

const string& PropertiesSignaturePlotObject::getCopyright()
{
   static string copyright = SPECTRAL_COPYRIGHT;
   return copyright;
}

const string& PropertiesSignaturePlotObject::getVersion()
{
   static string version = SPECTRAL_VERSION_NUMBER;
   return version;
}

const string& PropertiesSignaturePlotObject::getDescriptorId()
{
   static string id = "{36CD787A-AA6B-4B1C-96B2-BB32E6C0254E}";
   return id;
}

bool PropertiesSignaturePlotObject::isProduction()
{
   return SPECTRAL_IS_PRODUCTION_RELEASE;
}

void PropertiesSignaturePlotObject::enableRegionProperties(bool bEnable)
{
   mpRegionOpacityLabel->setEnabled(bEnable);
   mpRegionOpacitySpin->setEnabled(bEnable);
   mpRegionColorLabel->setEnabled(bEnable);
   mpRegionColorButton->setEnabled(bEnable);
}

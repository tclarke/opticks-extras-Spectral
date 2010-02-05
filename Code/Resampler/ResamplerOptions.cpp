/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtGui/QComboBox>
#include <QtGui/QGridLayout>
#include <QtGui/QLabel>
#include <QtGui/QSpinBox>

#include "AppVerify.h"
#include "LabeledSection.h"
#include "OptionQWidgetWrapper.h"
#include "PlugInRegistration.h"
#include "ResamplerOptions.h"

#include <limits>

using namespace std;

REGISTER_PLUGIN(SpectralResampler, ResamplerOptions, OptionQWidgetWrapper<ResamplerOptions>);

ResamplerOptions::ResamplerOptions()
{
   QLabel* pMethod = new QLabel("Resampling Method");
   mpMethod = new QComboBox;
   mpMethod->addItem(QString::fromStdString(LinearMethod()));
   mpMethod->addItem(QString::fromStdString(CubicSplineMethod()));
   mpMethod->addItem(QString::fromStdString(GaussianMethod()));
   mpMethod->setCurrentIndex(-1);

   QLabel* pDropOutWindow = new QLabel("Drop Out Window");
   mpDropOutWindow = new QDoubleSpinBox;
   mpDropOutWindow->setRange(0.0, numeric_limits<double>::max());
   mpDropOutWindow->setDecimals(6);
   mpDropOutWindow->setSuffix(" µm");

   QLabel* pFullWidthHalfMax = new QLabel("FWHM");
   mpFullWidthHalfMax = new QDoubleSpinBox;
   mpFullWidthHalfMax->setRange(0.0, numeric_limits<double>::max());
   mpFullWidthHalfMax->setDecimals(6);
   mpFullWidthHalfMax->setSuffix(" µm");

   QWidget* pLayoutWidget = new QWidget(this);
   QGridLayout* pGridLayout = new QGridLayout(pLayoutWidget);
   pGridLayout->addWidget(pMethod, 0, 0);
   pGridLayout->addWidget(mpMethod, 0, 1);
   pGridLayout->addWidget(pDropOutWindow, 1, 0);
   pGridLayout->addWidget(mpDropOutWindow, 1, 1);
   pGridLayout->addWidget(pFullWidthHalfMax, 2, 0);
   pGridLayout->addWidget(mpFullWidthHalfMax, 2, 1);

   LabeledSection* pSection = new LabeledSection(pLayoutWidget, "Resampler Options", this);
   addSection(pSection);
   addStretch(10);
   setSizeHint(100, 100);

   VERIFYNRV(connect(mpMethod, SIGNAL(currentIndexChanged(int)), this, SLOT(currentIndexChanged(int))));
   mpDropOutWindow->setValue(ResamplerOptions::getSettingDropOutWindow());
   mpFullWidthHalfMax->setValue(ResamplerOptions::getSettingFullWidthHalfMax());

   const string resamplerMethod = ResamplerOptions::getSettingResamplerMethod();
   if (resamplerMethod == ResamplerOptions::LinearMethod())
   {
      mpMethod->setCurrentIndex(0);
   }
   else if (resamplerMethod == ResamplerOptions::CubicSplineMethod())
   {
      mpMethod->setCurrentIndex(1);
   }
   else if (resamplerMethod == ResamplerOptions::GaussianMethod())
   {
      mpMethod->setCurrentIndex(2);
   }
}

ResamplerOptions::~ResamplerOptions()
{
   // Do nothing
}

void ResamplerOptions::applyChanges()
{
   ResamplerOptions::setSettingResamplerMethod(mpMethod->currentText().toStdString());
   ResamplerOptions::setSettingDropOutWindow(mpDropOutWindow->value());
   ResamplerOptions::setSettingFullWidthHalfMax(mpFullWidthHalfMax->value());
}

void ResamplerOptions::currentIndexChanged(int newIndex)
{
   mpFullWidthHalfMax->setEnabled(mpMethod->currentText().toStdString() == GaussianMethod());
}

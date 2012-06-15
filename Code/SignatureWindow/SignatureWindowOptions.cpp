/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QGridLayout>
#include <QtGui/QGroupBox>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QRadioButton>

#include "AppVerify.h"
#include "CustomColorButton.h"
#include "LabeledSection.h"
#include "OptionQWidgetWrapper.h"
#include "PlugInRegistration.h"
#include "SignatureWindowOptions.h"

REGISTER_PLUGIN(SpectralSignatureWindow, SignatureWindowOptions, OptionQWidgetWrapper<SignatureWindowOptions>);

SignatureWindowOptions::SignatureWindowOptions()
{
   // AOI average signature section
   QWidget* pAvgWidget = new QWidget(this);
   QButtonGroup* pAvgGroup = new QButtonGroup(pAvgWidget);
   QVBoxLayout* pAvgLayout = new QVBoxLayout(pAvgWidget);
   QWidget* pAvgColorWidget = new QWidget(pAvgWidget);
   QHBoxLayout* pAvgColorLayout = new QHBoxLayout(pAvgColorWidget);
   mpUseFixedColorForAvg = new QRadioButton("Fixed Color:", pAvgColorWidget);
   mpAoiAverageColor = new CustomColorButton(pAvgColorWidget);
   mpAoiAverageColor->usePopupGrid(true);
   pAvgColorLayout->setMargin(0);
   pAvgColorLayout->setSpacing(5);
   pAvgColorLayout->addWidget(mpUseFixedColorForAvg);
   pAvgColorLayout->addWidget(mpAoiAverageColor);
   pAvgColorLayout->addStretch(10);
   mpUseAoiColorForAverage = new QRadioButton("Use AOI Color", pAvgWidget);
   pAvgLayout->setMargin(0);
   pAvgLayout->setSpacing(5);
   pAvgLayout->addWidget(pAvgColorWidget);
   pAvgLayout->addWidget(mpUseAoiColorForAverage);
   pAvgLayout->addStretch(10);
   pAvgGroup->addButton(mpUseFixedColorForAvg);
   pAvgGroup->addButton(mpUseAoiColorForAverage);
   LabeledSection* pAvgSection = new LabeledSection(pAvgWidget, "AOI Average Signature Plot Color", this);

   // AOI Signatures section
   QWidget* pAoiSigWidget = new QWidget(this);
   QButtonGroup* pAoiSigGroup = new QButtonGroup(pAoiSigWidget);
   QVBoxLayout* pAoiSigLayout = new QVBoxLayout(pAoiSigWidget);
   QWidget* pAoiSigColorWidget = new QWidget(pAoiSigWidget);
   QHBoxLayout* pAoiSigColorLayout = new QHBoxLayout(pAoiSigColorWidget);
   mpUseFixedColorForAoiSigs = new QRadioButton("Fixed Color:", pAoiSigColorWidget);
   mpAoiSignaturesColor = new CustomColorButton(pAoiSigColorWidget);
   mpAoiSignaturesColor->usePopupGrid(true);
   pAoiSigColorLayout->setMargin(0);
   pAoiSigColorLayout->setSpacing(5);
   pAoiSigColorLayout->addWidget(mpUseFixedColorForAoiSigs);
   pAoiSigColorLayout->addWidget(mpAoiSignaturesColor);
   pAoiSigColorLayout->addStretch(10);
   mpUseAoiColorForAoiSignatures = new QRadioButton("Use AOI Color", pAoiSigWidget);
   pAoiSigLayout->setMargin(0);
   pAoiSigLayout->setSpacing(5);
   pAoiSigLayout->addWidget(pAoiSigColorWidget);
   pAoiSigLayout->addWidget(mpUseAoiColorForAoiSignatures);
   pAoiSigLayout->addStretch(10);
   pAoiSigGroup->addButton(mpUseFixedColorForAoiSigs);
   pAoiSigGroup->addButton(mpUseAoiColorForAoiSignatures);
   LabeledSection* pAoiSigSection = new LabeledSection(pAoiSigWidget, "AOI Signatures Plot Color", this);

   // Pixel Signatures section
   QWidget* pPixelSigWidget = new QWidget(this);
   QHBoxLayout* pPixelSigLayout = new QHBoxLayout(pPixelSigWidget);
   QLabel* pPixelSigColorLabel = new QLabel("Color:", pPixelSigWidget);
   mpPixelSignaturesColor = new CustomColorButton(pPixelSigWidget);
   mpPixelSignaturesColor->usePopupGrid(true);
   pPixelSigLayout->setMargin(0);
   pPixelSigLayout->setSpacing(5);
   pPixelSigLayout->addWidget(pPixelSigColorLabel);
   pPixelSigLayout->addWidget(mpPixelSignaturesColor);
   pPixelSigLayout->addStretch(10);
   LabeledSection* pPixelSigSection = new LabeledSection(pPixelSigWidget, "Pixel Signatures Plot Color", this);

   // Signature window options section
   QWidget* pSigWinWidget = new QWidget(this);

   mpSigUnitsCombo = new QComboBox(pSigWinWidget);
   mpSigUnitsCombo->setEditable(false);
   mpSigUnitsCombo->addItem("Band Numbers");
   mpSigUnitsCombo->addItem("Wavelengths");

   mpResampleToDataset = new QCheckBox("Resample signatures to the dataset", pSigWinWidget);
   mpRescaleOnAdd = new QCheckBox("Rescale plot after adding signature", pSigWinWidget);
   mpScaleToFirst = new QCheckBox("Scale signatures to first signature", pSigWinWidget);
   mpPinSigPlot = new QCheckBox("Pin Signature Window to single plot", pSigWinWidget);

   QGridLayout* pSigWinLayout = new QGridLayout(pSigWinWidget);
   pSigWinLayout->setMargin(0);
   pSigWinLayout->setSpacing(5);
   pSigWinLayout->addWidget(new QLabel("Signature Units:", pSigWinWidget), 0, 0);
   pSigWinLayout->addWidget(mpSigUnitsCombo, 0, 1);
   pSigWinLayout->addWidget(mpResampleToDataset, 1, 0, 1, 3);
   pSigWinLayout->addWidget(mpRescaleOnAdd, 2, 0, 1, 3);
   pSigWinLayout->addWidget(mpScaleToFirst, 3, 0, 1, 3);
   pSigWinLayout->addWidget(mpPinSigPlot, 4, 0, 1, 3);
   pSigWinLayout->setRowStretch(5, 10);
   pSigWinLayout->setColumnStretch(2, 10);
   LabeledSection* pRescaleSection = new LabeledSection(pSigWinWidget, "Signature Window Options", this);

   addSection(pAvgSection);
   addSection(pAoiSigSection);
   addSection(pPixelSigSection);
   addSection(pRescaleSection);
   addStretch(10);
   setSizeHint(100, 100);

   // connections
   VERIFYNR(connect(mpUseFixedColorForAvg, SIGNAL(toggled(bool)), mpAoiAverageColor, SLOT(setEnabled(bool))));
   VERIFYNR(connect(mpUseFixedColorForAoiSigs, SIGNAL(toggled(bool)), mpAoiSignaturesColor, SLOT(setEnabled(bool))));

   // Initialize From Settings
   ColorType color = SignatureWindowOptions::getSettingAoiAverageColor();
   mpAoiAverageColor->setColor(color);
   bool useAoiColor = SignatureWindowOptions::getSettingUseAoiColorForAverage();
   mpUseFixedColorForAvg->setChecked(!useAoiColor);
   mpAoiAverageColor->setEnabled(!useAoiColor);
   mpUseAoiColorForAverage->setChecked(useAoiColor);
   color = SignatureWindowOptions::getSettingAoiSignaturesColor();
   mpAoiSignaturesColor->setColor(color);
   useAoiColor = SignatureWindowOptions::getSettingUseAoiColorForAoiSignatures();
   mpUseFixedColorForAoiSigs->setChecked(!useAoiColor);
   mpAoiSignaturesColor->setEnabled(!useAoiColor);
   mpUseAoiColorForAoiSignatures->setChecked(useAoiColor);
   color = SignatureWindowOptions::getSettingPixelSignaturesColor();
   mpPixelSignaturesColor->setColor(color);
   mpSigUnitsCombo->setCurrentIndex(SignatureWindowOptions::getSettingDisplayWavelengths() == true ? 1 : 0);
   mpResampleToDataset->setChecked(SignatureWindowOptions::getSettingResampleSignaturesToDataset());
   mpRescaleOnAdd->setChecked(SignatureWindowOptions::getSettingRescaleOnAdd());
   mpScaleToFirst->setChecked(SignatureWindowOptions::getSettingScaleToFirstSignature());
   mpPinSigPlot->setChecked(SignatureWindowOptions::getSettingPinSignaturePlot());
}

SignatureWindowOptions::~SignatureWindowOptions()
{}

void SignatureWindowOptions::applyChanges()
{
   SignatureWindowOptions::setSettingAoiAverageColor(mpAoiAverageColor->getColorType());
   SignatureWindowOptions::setSettingUseAoiColorForAverage(mpUseAoiColorForAverage->isChecked());
   SignatureWindowOptions::setSettingAoiSignaturesColor(mpAoiSignaturesColor->getColorType());
   SignatureWindowOptions::setSettingUseAoiColorForAoiSignatures(mpUseAoiColorForAoiSignatures->isChecked());
   SignatureWindowOptions::setSettingPixelSignaturesColor(mpPixelSignaturesColor->getColorType());
   SignatureWindowOptions::setSettingDisplayWavelengths(mpSigUnitsCombo->currentIndex() == 1 ? true : false);
   SignatureWindowOptions::setSettingResampleSignaturesToDataset(mpResampleToDataset->isChecked());
   SignatureWindowOptions::setSettingRescaleOnAdd(mpRescaleOnAdd->isChecked());
   SignatureWindowOptions::setSettingScaleToFirstSignature(mpScaleToFirst->isChecked());
   SignatureWindowOptions::setSettingPinSignaturePlot(mpPinSigPlot->isChecked());
}

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AlgorithmPattern.h"
#include "AoiElement.h"
#include "AppVerify.h"
#include "BitMask.h"
#include "BitMaskIterator.h"
#include "RasterElement.h"
#include "SignatureSet.h"
#include "SpectralSignatureSelector.h"
#include "SpectralUtilities.h"
#include "TypeConverter.h"
#include <QtCore/QVariant>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QDoubleSpinBox>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QListView>
#include <QtGui/QMessageBox>
#include <QtGui/QPushButton>

#include <string>
#include <vector>
using namespace std;

SpectralSignatureSelector::SpectralSignatureSelector(RasterElement* pCube, Progress* pProgress, QWidget* pParent,
                                                     QAbstractItemView::SelectionMode mode, bool addApply,
                                                     const string& customButtonLabel) :
   SignatureSelector(pProgress, pParent, mode, addApply, customButtonLabel),
   mpNameEdit(NULL),
   mpThreshold(NULL),
   mpAoiCheck(NULL),
   mpAoiCombo(NULL),
   mpPseudocolorCheck(NULL),
   mpRunner(NULL),
   mpCube(pCube)
{
   // Initialization
   setWindowTitle("Spectral Signature Selection");
   addCustomType("AOI");

   // Connections
   VERIFYNRV(connect(this, SIGNAL(selectionChanged()), this, SLOT(setModified())));
}

SpectralSignatureSelector::SpectralSignatureSelector(RasterElement* pCube, AlgorithmRunner* pRunner,
                                                     Progress* pProgress, const string& resultsName,
                                                     bool pseudocolor, bool addApply, QWidget* pParent,
                                                     const string& customButtonLabel) :
   SignatureSelector(pProgress, pParent, QListView::ExtendedSelection, addApply, customButtonLabel),
   mpRunner(pRunner),
   mpCube(pCube)
{
   // Output results name
   QLabel* pNameLabel = new QLabel("Output Layer Name:", this);
   mpNameEdit = new QLineEdit(QString::fromStdString(resultsName), this);

   // Threshold
   QLabel* pThresholdLabel = new QLabel("Threshold:", this);
   mpThreshold = new QDoubleSpinBox(this);
   mpThreshold->setRange(0.0, 180.0);
   mpThreshold->setDecimals(4);
   mpThreshold->setValue(5.0);
   mpThreshold->setToolTip("Enter/Modify the threshold value that the algorithm will use here.");
   mpThreshold->setWhatsThis(mpThreshold->toolTip());

   // AOI
   mpAoiCheck = new QCheckBox("Area of Interest:", this);
   mpAoiCombo = new QComboBox(this);
   mpAoiCombo->setEditable(false);

   // Pseudocolor
   mpPseudocolorCheck = new QCheckBox("Combine results from multiple spectra into a single pseudocolor layer", this);
   mpPseudocolorCheck->setChecked(pseudocolor);
   mpPseudocolorCheck->setToolTip("This option combines the results from "
      "multiple signatures into one results layer. This is only available when more than one signature has been selected. ");
   mpPseudocolorCheck->setWhatsThis(mpPseudocolorCheck->toolTip());

   // Layout
   QGridLayout* pGrid = getLayout();
   if (pGrid != NULL)
   {
      pGrid->setSpacing(5);
      pGrid->addWidget(pNameLabel, 0, 0);
      pGrid->addWidget(mpNameEdit, 0, 1);
      pGrid->addWidget(pThresholdLabel, 1, 0);
      pGrid->addWidget(mpThreshold, 1, 1);
      pGrid->addWidget(mpAoiCheck, 2, 0);
      pGrid->addWidget(mpAoiCombo, 2, 1);
      pGrid->addWidget(mpPseudocolorCheck, 3, 0, 1, 2);
      pGrid->setColumnStretch(1, 10);
   }

   addCustomType("AOI");

   // Initialization
   resize(575, 350);
   setWindowTitle("Spectral Signature Selection");
   setNameText("Target Signature");

   refreshAoiList();
   mpAoiCombo->setEnabled(false);
   mpPseudocolorCheck->setEnabled(false);

   // Connections
   VERIFYNRV(connect(mpAoiCheck, SIGNAL(toggled(bool)), mpAoiCombo, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(this, SIGNAL(selectionChanged()), this, SLOT(enablePseudocolorCheckBox())));

   VERIFYNRV(connect(mpThreshold, SIGNAL(valueChanged(double)), this, SLOT(setModified())));
   VERIFYNRV(connect(mpNameEdit, SIGNAL(textChanged(const QString&)), this, SLOT(setModified())));
   VERIFYNRV(connect(mpAoiCheck, SIGNAL(toggled(bool)), this, SLOT(setModified())));
   VERIFYNRV(connect(mpAoiCombo, SIGNAL(activated(int)), this, SLOT(setModified())));
   VERIFYNRV(connect(this, SIGNAL(selectionChanged()), this, SLOT(setModified())));
   VERIFYNRV(connect(mpPseudocolorCheck, SIGNAL(toggled(bool)), this, SLOT(setModified())));
}

SpectralSignatureSelector::~SpectralSignatureSelector()
{
}

string SpectralSignatureSelector::getResultsName() const
{
   string resultsName;
   if (mpNameEdit != NULL)
   {
      resultsName = mpNameEdit->text().toStdString();
   }

   return resultsName;
}

double SpectralSignatureSelector::getThreshold() const
{
   double threshold = 0.0;
   if (mpThreshold != NULL)
   {
      threshold = mpThreshold->value();
   }

   return threshold;
}

void SpectralSignatureSelector::setThreshold(double val)
{
   if (mpThreshold != NULL)
   {
      mpThreshold->setValue(val);
   }
}

bool SpectralSignatureSelector::getModified() const
{
   return isApplyButtonEnabled();
}

AoiElement* SpectralSignatureSelector::getAoi() const
{
   if ((mpAoiCheck == NULL) || (mpAoiCombo == NULL) || (!mpAoiCheck->isChecked()))
   {
      return NULL;
   }

   QVariant data = mpAoiCombo->itemData(mpAoiCombo->currentIndex());
   return reinterpret_cast<AoiElement*>(data.value<void*>());
}

vector<Signature*> SpectralSignatureSelector::getSignatures() const
{
   vector<Signature*> sigs;
   if (getCurrentFormatType() == "AOI")
   {
      foreach(QTreeWidgetItem* pItem, const_cast<SpectralSignatureSelector*>(this)->getSignatureList()->selectedItems())
      {
         if (pItem != NULL)
         {
            string aoiName = pItem->text(0).toStdString();
            string sigName = aoiName + " signature";
            AoiElement* pAoi = static_cast<AoiElement*>(Service<ModelServices>()->getElement(
               aoiName, TypeConverter::toString<AoiElement>(), mpCube));
            if (pAoi == NULL)
            {
               continue;
            }
            Signature* pSig = static_cast<Signature*>(Service<ModelServices>()->getElement(
               sigName, TypeConverter::toString<Signature>(), mpCube));
            if (pSig == NULL)
            {
               pSig = static_cast<Signature*>(Service<ModelServices>()->createElement(
                  sigName, TypeConverter::toString<Signature>(), mpCube));
            }
            if (pSig == NULL)
            {
               continue;
            }
            SpectralUtilities::convertAoiToSignature(pAoi, pSig, mpCube);
            sigs.push_back(pSig);
         }
      }
      return sigs;
   }
   return SignatureSelector::getSignatures();
}

void SpectralSignatureSelector::usePseudocolorLayer(bool bPseudocolor)
{
   if (mpPseudocolorCheck != NULL)
   {
      mpPseudocolorCheck->setChecked(bPseudocolor);
   }
}

bool SpectralSignatureSelector::isPseudocolorLayerUsed() const
{
   if (mpPseudocolorCheck == NULL)
   {
      return false;
   }

   return mpPseudocolorCheck->isEnabled() && mpPseudocolorCheck->isChecked();
}

bool SpectralSignatureSelector::validateInputs()
{
   int iSignatures = getNumSelectedSignatures();
   if (iSignatures < 1)
   {
      QMessageBox::critical(this, windowTitle(), "Please select at least one target signature.");
      return false;
   }

   if (mpNameEdit != NULL)
   {
      QString strResults = mpNameEdit->text();
      if (strResults.isEmpty())
      {
         QMessageBox::critical(this, windowTitle(), "Please select a name for the output results layer.");
         mpNameEdit->setFocus();
         return false;
      }
   }

   if ((mpAoiCheck != NULL) && (mpAoiCombo != NULL) && (mpAoiCheck->isChecked()))
   {
      AoiElement* pElement = reinterpret_cast<AoiElement*>(
         mpAoiCombo->itemData(mpAoiCombo->currentIndex()).value<void*>());
      if (pElement == NULL)
      {
         QMessageBox::critical(this, windowTitle(), "The selected AOI is invalid for this cube!");
         return false;
      }
      BitMaskIterator it(pElement->getSelectedPoints(), mpCube);
      if (it.getCount() == 0)
      {
         QMessageBox::critical(this, windowTitle(), "The selected AOI is empty!");
         return false;
      }
   }

   return true;
}

void SpectralSignatureSelector::enablePseudocolorCheckBox()
{
   if (mpPseudocolorCheck == NULL)
   {
      return;
   }

   vector<Signature*> sigVector = getExtractedSignatures();
   int count = 0;
   for (vector<Signature*>::const_iterator sig = sigVector.begin(); sig != sigVector.end(); ++sig)
   {
      if (*sig != NULL)
      {
         count++;
      }
      if (count > 1)
      {
         mpPseudocolorCheck->setEnabled(true);
         return;
      }
   }
   mpPseudocolorCheck->setEnabled(false);
}

void SpectralSignatureSelector::accept()
{
   if (validateInputs())
   {
      SignatureSelector::accept();
   }
}

void SpectralSignatureSelector::apply()
{
   if (validateInputs())
   {
      if (mpRunner != NULL)
      {
         if (mpRunner->runAlgorithmFromGuiInputs())
         {
            enableApplyButton(false);
         }
      }
   }
}

void SpectralSignatureSelector::setModified()
{
   enableApplyButton(true);
}

void SpectralSignatureSelector::refreshAoiList()
{
   if (mpAoiCombo == NULL)
   {
      return;
   }

   vector<DataElement*> aois = Service<ModelServices>()->getElements(mpCube, TypeConverter::toString<AoiElement*>());
   mpAoiCombo->clear();
   for (vector<DataElement*>::iterator aoi = aois.begin(); aoi != aois.end(); ++aoi)
   {
      AoiElement* pElement = model_cast<AoiElement*>(*aoi);
      if (pElement != NULL)
      {
         mpAoiCombo->addItem(QString::fromStdString(pElement->getName()),
            QVariant::fromValue<void*>(reinterpret_cast<void*>(pElement)));
      }
   }
}

void SpectralSignatureSelector::updateSignatureList()
{
   SignatureSelector::updateSignatureList();
   if (getCurrentFormatType() == "AOI")
   {
      getSignatureList()->clear();
      vector<DataElement*> aois = Service<ModelServices>()->getElements(mpCube,
         TypeConverter::toString<AoiElement>());
      for (vector<DataElement*>::const_iterator aoi = aois.begin(); aoi != aois.end(); ++aoi)
      {
         if (*aoi != NULL)
         {
            QTreeWidgetItem* pItem = new QTreeWidgetItem(getSignatureList());
            string nm = (*aoi)->getDisplayName();
            if (nm.empty())
            {
               nm = (*aoi)->getName();
            }
            pItem->setText(0, QString::fromStdString(nm));
         }
      }
   }
}

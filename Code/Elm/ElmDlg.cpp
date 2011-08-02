/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <boost/any.hpp>

#include <QtGui/QComboBox>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QFileDialog>
#include <QtGui/QGroupBox>
#include <QtGui/QInputDialog>
#include <QtGui/QLayout>
#include <QtGui/QLineEdit>
#include <QtGui/QListWidget>
#include <QtGui/QMessageBox>
#include <QtGui/QPushButton>
#include <QtGui/QRadioButton>
#include <QtGui/QToolTip>
#include <QtGui/QWhatsThis>

#include "AoiLayer.h"
#include "AppVerify.h"
#include "BitMask.h"
#include "BitMaskIterator.h"
#include "ConfigurationSettings.h"
#include "DesktopServices.h"
#include "ElmDlg.h"
#include "ElmInteractive.h"
#include "FileBrowser.h"
#include "LocationType.h"
#include "Signature.h"
#include "SignatureSelector.h"
#include "SpatialDataView.h"
#include "StringUtilities.h"
#include "Subject.h"

using namespace std;

ElmDlg::ElmDlg(SpatialDataView* pView, ElmInteractive* pElmInteractive, QWidget* pParent) :
   QDialog(pParent),
   mPixelOffset(1.0, 1.0),
   mMaxDisplayedPixels(10000),
   mCurrentIndex(-1),
   mpElmInteractive(pElmInteractive),
   mpView(pView, SIGNAL_NAME(Subject, Deleted), Slot(this, &ElmDlg::viewDeleted))
{
   // Initialization
   VERIFYNRV(mpView.get() != NULL);
   VERIFYNRV(mpElmInteractive != NULL);
   setWindowTitle(QString::fromStdString("Empirical Line Method - " + mpView->getDisplayName()));
   mpAoiElement.addSignal(SIGNAL_NAME(Subject, Modified), Slot(this, &ElmDlg::aoiModified));
   mpAoiElement.addSignal(SIGNAL_NAME(Subject, Deleted), Slot(this, &ElmDlg::aoiDeleted));

   // "Use Existing Gains/Offsets File" Layout Begin
   mpExistingFileBrowser = new FileBrowser;

   QHBoxLayout* pUseExistingFileLayout = new QHBoxLayout;
   pUseExistingFileLayout->addWidget(mpExistingFileBrowser);

   QGroupBox* pUseExistingFileGroup = new QGroupBox;
   pUseExistingFileGroup->setLayout(pUseExistingFileLayout);
   pUseExistingFileGroup->setEnabled(false);
   // "Use Existing Gains/Offsets File" Layout End


   // Element Layout Begin
   mpElementComboBox = new QComboBox;
   mpElementComboBox->setToolTip("This is the list of available ELM Elements.");
   mpElementComboBox->setWhatsThis("This is the list of available ELM Elements. "
      "ELM elements are simply a group of selected pixels, similar to an AOI, that specify the location "
      "of an object whose signature matches a corresponding input reflectance signature. "
      "At least two elements must be created to run the ELM algorithm.");

   QPushButton* pNewElementButton = new QPushButton("New Element");
   pNewElementButton->setToolTip("Click this button to create a new ELM element.");
   pNewElementButton->setWhatsThis("Click this button to create a new ELM element. "
      "Pixels must be selected and a corresponding signature must be selected for each element. "
      "At least two elements must be created to run the ELM algorithm.");

   QPushButton* pDeleteElementButton = new QPushButton("Delete Element");
   pDeleteElementButton->setToolTip("Click this button to delete the currently selected ELM Element.");
   pDeleteElementButton->setWhatsThis("Click this button to delete the currently selected ELM Element. "
      "The Element will be removed from the list and its pixels will be deleted from the scene.");

   QVBoxLayout* pElementLayout = new QVBoxLayout;
   pElementLayout->addWidget(mpElementComboBox);
   pElementLayout->addWidget(pNewElementButton);
   pElementLayout->addWidget(pDeleteElementButton);

   QGroupBox* pElementGroup = new QGroupBox("Current Element");
   pElementGroup->setLayout(pElementLayout);
   // Element Layout End


   // Pixel Layout Begin
   mpPixelList = new QListWidget;
   mpPixelList->setSortingEnabled(false);
   mpPixelList->setSelectionMode(QAbstractItemView::ExtendedSelection);
   mpPixelList->setToolTip("When pixels in the scene are selected, their coordinates are listed in this box.");
   mpPixelList->setWhatsThis("When pixels in the scene are selected, their coordinates are listed in this box. "
      "To remove pixels from the list, highlight the pixels to remove and click the \"Delete Pixels\" button.");

   QPushButton* pDeletePixelButton = new QPushButton("Delete Pixels");
   pDeletePixelButton->setToolTip("Click this button to exclude the currently selected "
      "pixel(s) in the list from any further processing.");
   pDeletePixelButton->setWhatsThis("Click this button to exclude the currently selected "
      "pixel(s) in the list from any further processing. The pixels will be removed from "
      "the list so that they cannot be considered during execution.");

   QVBoxLayout* pPixelLayout = new QVBoxLayout;
   pPixelLayout->addWidget(mpPixelList);
   pPixelLayout->addWidget(pDeletePixelButton);

   QGroupBox* pPixelGroup = new QGroupBox(QString("Pixels (Up to %1 displayed)").arg(mMaxDisplayedPixels));
   pPixelGroup->setLayout(pPixelLayout);
   // Pixel Layout End


   // Signature Layout Begin
   mpSignature = new QLineEdit;
   mpSignature->setReadOnly(true);

   QPushButton* pSignatureButton = new QPushButton("Change...");
   pSignatureButton->setToolTip("Select the corresponding reflectance signature for the selected Element.");
   pSignatureButton->setWhatsThis("Select the corresponding reflectance signature for the selected Element. ");

   QHBoxLayout* pSignatureLayout = new QHBoxLayout;
   pSignatureLayout->addWidget(mpSignature);
   pSignatureLayout->addWidget(pSignatureButton);

   QGroupBox* pSignatureGroup = new QGroupBox("Signature");
   pSignatureGroup->setLayout(pSignatureLayout);
   // Signature Layout End


   QGridLayout* pCalculateLayout = new QGridLayout;
   pCalculateLayout->addWidget(pElementGroup, 1, 0);
   pCalculateLayout->addWidget(pPixelGroup, 1, 1, 2, 1);
   pCalculateLayout->addWidget(pSignatureGroup, 3, 0, 1, 3);

   QGroupBox* pCalculateGroup = new QGroupBox;
   pCalculateGroup->setLayout(pCalculateLayout);
   pCalculateGroup->setEnabled(false);
   // "Calculate Gains/Offsets" Layout End


   // Button Box Begin
   QDialogButtonBox* pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
      Qt::Horizontal, this);
   // Button Box End


   // Overall Layout Begin
   mpUseExistingFileRadio = new QRadioButton("Use Existing Gains/Offsets File");
   mpCalculateRadio = new QRadioButton("Calculate Gains/Offsets");

   QVBoxLayout* pOverallLayout = new QVBoxLayout(this);
   pOverallLayout->setMargin(10);
   pOverallLayout->setSpacing(5);
   pOverallLayout->addWidget(mpUseExistingFileRadio);
   pOverallLayout->addWidget(pUseExistingFileGroup);
   pOverallLayout->addWidget(mpCalculateRadio);
   pOverallLayout->addWidget(pCalculateGroup);
   pOverallLayout->addWidget(pButtonBox);
   // Overall Layout End


   // Make GUI connections
   VERIFYNRV(connect(mpUseExistingFileRadio, SIGNAL(toggled(bool)), pUseExistingFileGroup, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(mpCalculateRadio, SIGNAL(toggled(bool)), pCalculateGroup, SLOT(setEnabled(bool))));
   VERIFYNRV(connect(mpElementComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(currentIndexChanged(int))));
   VERIFYNRV(connect(pNewElementButton, SIGNAL(clicked()), this, SLOT(newElement())));
   VERIFYNRV(connect(pDeleteElementButton, SIGNAL(clicked()), this, SLOT(deleteElement())));
   if (ElmCore::hasSettingElmHelp())
   {
      pButtonBox->addButton(QDialogButtonBox::Help);
      VERIFYNRV(connect(pButtonBox, SIGNAL(helpRequested()), this, SLOT(help())));
   }
   VERIFYNRV(connect(pButtonBox, SIGNAL(accepted()), this, SLOT(accept())));
   VERIFYNRV(connect(pButtonBox, SIGNAL(rejected()), this, SLOT(reject())));
   VERIFYNRV(connect(pDeletePixelButton, SIGNAL(clicked()), this, SLOT(deletePixels())));
   VERIFYNRV(connect(pSignatureButton, SIGNAL(clicked()), this, SLOT(selectSignature())));


   // AOI toolbar defaults
   Service<DesktopServices> pDesktopServices;
   pDesktopServices->setAoiSelectionTool(RECTANGLE_OBJECT, DRAW);


   // Create an initial element
   newElement();


   // Enable the appropriate GroupBox
   const QString defaultFilename = QString::fromStdString(mpElmInteractive->getDefaultGainsOffsetsFilename());
   if (QFile::exists(defaultFilename) == true)
   {
      mpUseExistingFileRadio->setChecked(true);
      mpExistingFileBrowser->setFilename(defaultFilename);
   }
   else
   {
      mpCalculateRadio->setChecked(true);
   }
}

ElmDlg::~ElmDlg()
{
   // Set the current index to be invalid.
   // This must be done while deleting elements.
   setCurrentIndex(-1);
   for (vector<ElmElement*>::iterator iter = mElements.begin(); iter != mElements.end(); ++iter)
   {
      delete *iter;
   }
}

void ElmDlg::viewDeleted(Subject& subject, const std::string& signal, const boost::any& value)
{
   Step* pStep = mpElmInteractive->getLogStep();
   if (pStep != NULL)
   {
      pStep->finalize(Message::Abort, "Spatial data view closed");
   }

   reject();
}

void ElmDlg::aoiModified(Subject& subject, const std::string& signal, const boost::any& value)
{
   updatePixelList();
}

void ElmDlg::aoiDeleted(Subject& subject, const std::string& signal, const boost::any& value)
{
   refresh();
}

void ElmDlg::updatePixelList()
{
   mpPixelList->clear();
   if (mpAoiElement.get() != NULL)
   {
      const RasterElement* pElement = mpElmInteractive->getRasterElement();
      const BitMask* pMask = mpAoiElement->getSelectedPoints();
      VERIFYNRV(pMask != NULL);
      BitMaskIterator it(pMask, pElement);

      QStringList pixels;
      while (it != it.end())
      {
         LocationType aoiPoint;
         it.getPixelLocation(aoiPoint);
         pixels.push_back(QString::fromStdString(
            StringUtilities::toDisplayString<LocationType>(aoiPoint + mPixelOffset)));
         if (pixels.size() == static_cast<int>(mMaxDisplayedPixels))
         {
            break;
         }
         ++it;
      }

      if (pixels.isEmpty() == false)
      {
         mpPixelList->addItems(pixels);
      }
   }
}

void ElmDlg::currentIndexChanged(int index)
{
   ElmElement* pCurrentElement = getCurrentElement();
   if (pCurrentElement != NULL)
   {
      // Hide the layer.
      pCurrentElement->hideLayer();
   }

   // Update mCurrentIndex and load the new element.
   mCurrentIndex = index;
   refresh();
}

void ElmDlg::refresh()
{
   AoiElement* pAoiElement = NULL;
   ElmElement* pCurrentElement = getCurrentElement();
   if (pCurrentElement == NULL)
   {
      mpSignature->setText(QString());
      mpAoiElement.reset(NULL);
   }
   else
   {
      const Signature* pSignature = pCurrentElement->getSignature();
      if (pSignature == NULL)
      {
         mpSignature->setText(QString());
      }
      else
      {
         mpSignature->setText(QString::fromStdString(pSignature->getDisplayName()));
      }

      mpAoiElement.reset(pCurrentElement->getAoiElement());
      pCurrentElement->showLayer();
   }

   updatePixelList();
}

void ElmDlg::newElement()
{
   while (mElements.size() <= static_cast<unsigned int>(mpElementComboBox->count()))
   {
      ElmElement* pNewElement = new ElmElement(mpView.get());
      mElements.push_back(pNewElement);
   }

   mpElementComboBox->insertItem(mpElementComboBox->count(), QString::number(mpElementComboBox->count() + 1));
   setCurrentIndex(mpElementComboBox->count() - 1);
}

void ElmDlg::deleteElement()
{
   const int oldIndex = mCurrentIndex;
   ElmElement* pCurrentElement = getCurrentElement();
   if (oldIndex >= 0 && pCurrentElement != NULL)
   {
      // Set the current index to be invalid.
      // This must be done while deleting elements.
      setCurrentIndex(-1);

      // Delete pCurrentElement and erase it from mElements.
      delete pCurrentElement;
      mElements.erase(std::find(mElements.begin(), mElements.end(), pCurrentElement));

      // Remove the last element from the combo box.
      mpElementComboBox->removeItem(mpElementComboBox->count() - 1);

      // If the user deleted the last element, create a new one.
      if (mpElementComboBox->count() == 0)
      {
         newElement();
      }

      setCurrentIndex(oldIndex);
   }
}

void ElmDlg::accept()
{
   mGainsOffsetsFilename.clear();
   vector<Signature*> pSignatures;
   vector<AoiElement*> pAoiElements;

   if (mpUseExistingFileRadio->isChecked() == true)
   {
      mGainsOffsetsFilename = mpExistingFileBrowser->getFilename();
      if (mGainsOffsetsFilename.isEmpty() == true)
      {
         QMessageBox::warning(this, windowTitle(), "Please specify an existing Gains/Offsets file.");
         return;
      }
   }
   else if (mpCalculateRadio->isChecked() == true)
   {
      for (vector<ElmElement*>::iterator iter = mElements.begin(); iter != mElements.end(); ++iter)
      {
         VERIFYNRV(*iter != NULL);
         pSignatures.push_back((*iter)->getSignature());
         pAoiElements.push_back((*iter)->getAoiElement());
      }
   }

   if (mpElmInteractive->executeElm(mGainsOffsetsFilename.toStdString(), pSignatures, pAoiElements) == false)
   {
      // If there was an error in execution, keep the display active.
      return;
   }

   Step* pStep = mpElmInteractive->getLogStep();
   if (pStep != NULL)
   {
      pStep->finalize(Message::Success);
   }

   QDialog::accept();
   mpElmInteractive->abort();
}

void ElmDlg::reject()
{
   Step* pStep = mpElmInteractive->getLogStep();
   if (pStep != NULL)
   {
      const string& abortMessage = pStep->getFailureMessage();
      if (abortMessage.empty() == true)
      {
         pStep->finalize(Message::Abort, "ELM dialog canceled");
      }
   }

   QDialog::reject();
   mpElmInteractive->abort();
}

void ElmDlg::deletePixels()
{
   if (mpAoiElement.get() != NULL)
   {
      QList<QListWidgetItem*> selectedItems = mpPixelList->selectedItems();
      if (selectedItems.size() > 0)
      {
         vector<LocationType> selectedPixels;
         for (QList<QListWidgetItem*>::iterator iter = selectedItems.begin(); iter != selectedItems.end(); ++iter)
         {
            LocationType location = StringUtilities::fromDisplayString<LocationType>((*iter)->text().toStdString());
            selectedPixels.push_back(location - mPixelOffset);
         }

         mpAoiElement->removePoints(selectedPixels);
      }
   }
}

void ElmDlg::selectSignature()
{
   ElmElement* pCurrentElement = getCurrentElement();
   if (pCurrentElement == NULL)
   {
      return;
   }

   SignatureSelector sigSelector(mpElmInteractive->getProgress(), this, QAbstractItemView::SingleSelection);
   if (sigSelector.exec() == QDialog::Accepted)
   {
      vector<Signature*> pSignatures = sigSelector.getSignatures();
      if (pSignatures.empty() == false)
      {
         Signature* pSignature = pSignatures.front();
         if (pSignature != NULL && pSignature->isKindOf("Signature") == true)
         {
            pCurrentElement->setSignature(pSignature);

            string sigName = pSignature->getDisplayName();
            if (sigName.empty() == true)
            {
               sigName = pSignature->getName();
            }

            mpSignature->setText(QString::fromStdString(sigName));
         }
         else
         {
            QMessageBox::critical(this, windowTitle(), "Please choose a Signature file.");
         }
      }
   }
}

void ElmDlg::setCurrentIndex(int index)
{
   index = min(index, mpElementComboBox->count() - 1);
   mpElementComboBox->setCurrentIndex(index);
}

ElmElement* ElmDlg::getCurrentElement()
{
   if (mCurrentIndex >= 0 && static_cast<unsigned int>(mCurrentIndex) < mElements.size())
   {
      return mElements[mCurrentIndex];
   }

   return NULL;
}

void ElmDlg::help()
{
   Service<DesktopServices> pDesktop;
   Service<ConfigurationSettings> pSettings;

   string helpFile = pSettings->getHome() + ElmCore::getSettingElmHelp();
   pDesktop->displayHelp(helpFile);
}

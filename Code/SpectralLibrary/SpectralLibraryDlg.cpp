/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtGui/QApplication>
#include <QtGui/QBitmap>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QFrame>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QMessageBox>

#include "AppVerify.h"
#include "ConfigurationSettings.h"
#include "CustomTreeWidget.h"
#include "DesktopServices.h"
#include "FileDescriptor.h"
#include "LibraryBuilder.h"
#include "MessageLog.h"
#include "ModelServices.h"
#include "PlugInArgList.h"
#include "PlugInResource.h"
#include "SignatureSelector.h"
#include "SignatureSet.h"
#include "Slot.h"
#include "SpectralLibraryDlg.h"

using namespace std;

SpectralLibraryDlg::SpectralLibraryDlg(Progress* pProgress, Step* pStep, QWidget* pParent) :
   QDialog(pParent),
   mpProgress(pProgress),
   mpStep(pStep),
   mpSigSelector(NULL)
{
   QFont ftBold = QApplication::font();
   ftBold.setBold(true);

   // Libraries
   QLabel* pLibraryLabel = new QLabel("Libraries:", this);
   pLibraryLabel->setFont(ftBold);

   QStringList columnNames;
   columnNames.append("Name");
   columnNames.append("File");

   mpLibraryTree = new CustomTreeWidget(this);
   mpLibraryTree->setColumnCount(columnNames.count());
   mpLibraryTree->setHeaderLabels(columnNames);
   mpLibraryTree->setSelectionMode(QAbstractItemView::SingleSelection);
   mpLibraryTree->setAllColumnsShowFocus(true);
   mpLibraryTree->setRootIsDecorated(false);
   mpLibraryTree->setSortingEnabled(true);
   mpLibraryTree->sortByColumn(0, Qt::AscendingOrder);
   mpLibraryTree->setGridlinesShown(Qt::Horizontal | Qt::Vertical, true);
   mpLibraryTree->setToolTip("This list displays the spectral libraries that are "
      "currently loaded in the session.");
   mpLibraryTree->setWhatsThis("This list displays the spectral libraries that are "
      "currently loaded in the session.  The user can edit the library name by clicking in "
      "the appropriate cell of the selected spectral library.");

   QHeaderView* pHeader = mpLibraryTree->header();
   if (pHeader != NULL)
   {
      pHeader->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
      pHeader->resizeSection(0, 150);
      pHeader->setStretchLastSection(true);
      pHeader->setSortIndicatorShown(true);
   }

   QPushButton* pNewLibButton = new QPushButton("&New", this);
   pNewLibButton->setToolTip("Click this button to create a new spectral library.");
   pNewLibButton->setWhatsThis("Click this button to create a new spectral library.  "
      "The user can edit the library's name by clicking in the appropriate cell of the "
      "selected spectral library.");

   mpDeleteLibButton = new QPushButton("&Delete", this);
   mpDeleteLibButton->setToolTip("Click this button to remove the spectral library from the list.");
   mpDeleteLibButton->setWhatsThis("Click this button to remove the spectral library from the list.");

   QPushButton* pLoadLibButton = new QPushButton(QIcon(":/icons/Open"), " &Load...", this);
   pLoadLibButton->setToolTip("Click this button to import a spectral library from a file.");
   pLoadLibButton->setWhatsThis("Click this button to import a spectral library from a file.");

   mpSaveLibButton = new QPushButton(QIcon(":/icons/Save"), " &Save...", this);
   mpSaveLibButton->setToolTip("Click this button to export the currently "
      "selected spectral library to a file.");
   mpSaveLibButton->setWhatsThis("Click this button to export the currently "
      "selected spectral library to a file.");

   QVBoxLayout* pLibraryLayout = new QVBoxLayout();
   pLibraryLayout->setMargin(0);
   pLibraryLayout->setSpacing(5);
   pLibraryLayout->addWidget(pNewLibButton);
   pLibraryLayout->addWidget(mpDeleteLibButton);
   pLibraryLayout->addStretch();
   pLibraryLayout->addWidget(pLoadLibButton);
   pLibraryLayout->addWidget(mpSaveLibButton);

   // Signatures
   QLabel* pSignatureLabel = new QLabel("Signatures:", this);
   pSignatureLabel->setFont(ftBold);

   mpSignatureTree = new CustomTreeWidget(this);
   mpSignatureTree->setColumnCount(columnNames.count());
   mpSignatureTree->setHeaderLabels(columnNames);
   mpSignatureTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
   mpSignatureTree->setAllColumnsShowFocus(true);
   mpSignatureTree->setRootIsDecorated(false);
   mpSignatureTree->setSortingEnabled(true);
   mpSignatureTree->sortByColumn(0, Qt::AscendingOrder);
   mpSignatureTree->setGridlinesShown(Qt::Horizontal | Qt::Vertical, true);
   mpSignatureTree->setToolTip("This list displays the signatures that are "
      "contained within the currently selected spectral library.");
   mpSignatureTree->setWhatsThis("This list displays the signatures that are contained within "
      "the currently selected spectral library.  The user can edit the signature name by in "
      "clicking the appropriate cell of the selected signature.");

   pHeader = mpSignatureTree->header();
   if (pHeader != NULL)
   {
      pHeader->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
      pHeader->resizeSection(0, 150);
      pHeader->setStretchLastSection(true);
      pHeader->setSortIndicatorShown(true);
   }

   mpAddSigButton = new QPushButton("&Add...", this);
   mpAddSigButton->setToolTip("Click this button to add additional signatures to the currently "
      "selected spectral library.");
   mpAddSigButton->setWhatsThis("Click this button to add additional signatures to the currently "
      "selected spectral library.");

   mpRemoveSigButton = new QPushButton("&Remove", this);
   mpRemoveSigButton->setToolTip("Click this button to remove the selected signature(s) from the "
      "currently selected spectral library.");
   mpRemoveSigButton->setWhatsThis("Click this button to remove the selected signature(s) from the "
      "currently selected spectral library.  Removing the signature from the library does not "
      "delete the signature.");

   mpSaveSigButton = new QPushButton(QIcon(":/icons/Save"), " &Save...", this);
   mpSaveSigButton->setToolTip("Click this button to save the selected signature(s) to a file.");
   mpSaveSigButton->setWhatsThis("Click this button to save the selected signature(s) to a file.");

   QVBoxLayout* pSignatureLayout = new QVBoxLayout();
   pSignatureLayout->setMargin(0);
   pSignatureLayout->setSpacing(5);
   pSignatureLayout->addWidget(mpAddSigButton);
   pSignatureLayout->addWidget(mpRemoveSigButton);
   pSignatureLayout->addStretch();
   pSignatureLayout->addWidget(mpSaveSigButton);

   // Horizontal line
   QFrame* pLine = new QFrame(this);
   pLine->setFrameStyle(QFrame::HLine | QFrame::Sunken);

   // Button Box
   QDialogButtonBox* pButtonBox = new QDialogButtonBox(Qt::Horizontal, this);

   // Close button. This "close" button has AcceptRole instead of RejectRole,
   // so we need to create it explicitly.
   QPushButton* pCloseButton = new QPushButton("&Close", this);
   pButtonBox->addButton(pCloseButton, QDialogButtonBox::AcceptRole);
   pCloseButton->setDefault(true);
   pCloseButton->setFocus();

   // Layout
   QGridLayout* pGrid = new QGridLayout(this);
   pGrid->setMargin(10);
   pGrid->setSpacing(5);
   pGrid->addWidget(pLibraryLabel, 0, 0, 1, 2);
   pGrid->addWidget(mpLibraryTree, 1, 0);
   pGrid->addLayout(pLibraryLayout, 1, 1);
   pGrid->addWidget(pSignatureLabel, 2, 0, 1, 2);
   pGrid->addWidget(mpSignatureTree, 3, 0);
   pGrid->addLayout(pSignatureLayout, 3, 1);
   pGrid->addWidget(pLine, 4, 0, 1, 2);
   pGrid->addWidget(pButtonBox, 5, 0, 1, 2);
   pGrid->setColumnStretch(0, 10);

   // Initialization
   setWindowTitle("Spectral Library Builder");
   setModal(true);
   resize(600, 400);

   Service<ModelServices> pModel;

   vector<DataElement*> signatureSets = pModel->getElements("SignatureSet");
   for (vector<DataElement*>::size_type i = 0; i < signatureSets.size(); ++i)
   {
      SignatureSet* pSignatureSet = static_cast<SignatureSet*>(signatureSets[i]);
      if (pSignatureSet != NULL)
      {
         addLibrary(pSignatureSet);
      }
   }

   QTreeWidgetItem* pItem = mpLibraryTree->topLevelItem(0);
   if (pItem != NULL)
   {
      mpLibraryTree->setItemSelected(pItem, true);
   }

   updateSignatureList();

   // Connections
   VERIFYNR(connect(mpLibraryTree, SIGNAL(itemSelectionChanged()), this, SLOT(updateSignatureList())));
   VERIFYNR(connect(mpLibraryTree, SIGNAL(cellTextChanged(QTreeWidgetItem*, int)), this,
      SLOT(updateSignatureData(QTreeWidgetItem*, int))));
   VERIFYNR(connect(pNewLibButton, SIGNAL(clicked()), this, SLOT(createLibrary())));
   VERIFYNR(connect(mpDeleteLibButton, SIGNAL(clicked()), this, SLOT(deleteLibrary())));
   VERIFYNR(connect(pLoadLibButton, SIGNAL(clicked()), this, SLOT(loadLibrary())));
   VERIFYNR(connect(mpSaveLibButton, SIGNAL(clicked()), this, SLOT(saveLibrary())));
   VERIFYNR(connect(mpSignatureTree, SIGNAL(itemSelectionChanged()), this, SLOT(enableSignatureButtons())));
   VERIFYNR(connect(mpSignatureTree, SIGNAL(cellTextChanged(QTreeWidgetItem*, int)), this,
      SLOT(updateSignatureData(QTreeWidgetItem*, int))));
   VERIFYNR(connect(mpAddSigButton, SIGNAL(clicked()), this, SLOT(addSignature())));
   VERIFYNR(connect(mpRemoveSigButton, SIGNAL(clicked()), this, SLOT(removeSignature())));
   VERIFYNR(connect(mpSaveSigButton, SIGNAL(clicked()), this, SLOT(saveSignature())));
   if (LibraryBuilder::hasSettingSpectralLibraryHelp())
   {
      pButtonBox->addButton(QDialogButtonBox::Help);
      VERIFYNR(connect(pButtonBox, SIGNAL(helpRequested()), this, SLOT(help())));
   }
   VERIFYNR(connect(pCloseButton, SIGNAL(clicked()), this, SLOT(accept())));
}

SpectralLibraryDlg::~SpectralLibraryDlg()
{
   // Detach all libraries and signatures by removing the tree widget items
   while (mLibraries.empty() == false)
   {
      map<QTreeWidgetItem*, SignatureSet*>::iterator iter = mLibraries.begin();
      if (iter != mLibraries.end())
      {
         SignatureSet* pSignatureSet = iter->second;
         if (pSignatureSet != NULL)
         {
            removeLibrary(pSignatureSet);
         }
      }
   }
}

void SpectralLibraryDlg::abortSearch()
{
   if (mpSigSelector != NULL)
   {
      mpSigSelector->abortSearch();
   }
}

bool SpectralLibraryDlg::addLibrary(SignatureSet* pSignatureSet)
{
   if (pSignatureSet == NULL)
   {
      return false;
   }

   mpLibraryTree->closeActiveCellWidget(true);
   mpSignatureTree->closeActiveCellWidget(true);

   // Do not add the library if it is already in the list
   map<QTreeWidgetItem*, SignatureSet*>::iterator iter;
   for (iter = mLibraries.begin(); iter != mLibraries.end(); ++iter)
   {
      SignatureSet* pCurrentSet = iter->second;
      if (pSignatureSet == pCurrentSet)
      {
         return false;
      }
   }

   // Create the tree widget item and add it to the map
   QString strName = QString::fromStdString(pSignatureSet->getName());
   QString strFile = QString::fromStdString(pSignatureSet->getFilename());

   QTreeWidgetItem* pItem = new QTreeWidgetItem(mpLibraryTree);
   if (pItem != NULL)
   {
      pItem->setText(0, strName);
      pItem->setText(1, strFile);
      mpLibraryTree->setCellWidgetType(pItem, 0, CustomTreeWidget::LINE_EDIT);

      pSignatureSet->attach(SIGNAL_NAME(Subject, Modified), Slot(this, &SpectralLibraryDlg::updateLibraryName));
      pSignatureSet->attach(SIGNAL_NAME(Subject, Deleted), Slot(this, &SpectralLibraryDlg::removeLibraryItem));
      mLibraries[pItem] = pSignatureSet;
      return true;
   }

   return false;
}

bool SpectralLibraryDlg::addSignature(Signature* pSignature)
{
   if (pSignature == NULL)
   {
      return false;
   }

   // Do not add the signature if it is already in the list
   map<QTreeWidgetItem*, Signature*>::iterator iter;
   for (iter = mSignatures.begin(); iter != mSignatures.end(); ++iter)
   {
      Signature* pCurrentSignature = iter->second;
      if (pCurrentSignature == pSignature)
      {
         return false;
      }
   }

   // Create the tree widget item and add it to the map
   QString strName = QString::fromStdString(pSignature->getName());
   QString strFile = QString::fromStdString(pSignature->getFilename());

   QTreeWidgetItem* pItem = new QTreeWidgetItem(mpSignatureTree);
   if (pItem != NULL)
   {
      pItem->setText(0, strName);
      pItem->setText(1, strFile);
      mpSignatureTree->setCellWidgetType(pItem, 0, CustomTreeWidget::LINE_EDIT);

      pSignature->attach(SIGNAL_NAME(Subject, Modified), Slot(this, &SpectralLibraryDlg::updateSignatureName));
      pSignature->attach(SIGNAL_NAME(Subject, Deleted), Slot(this, &SpectralLibraryDlg::removeSignatureItem));
      mSignatures[pItem] = pSignature;
      return true;
   }

   return false;
}

bool SpectralLibraryDlg::removeLibrary(SignatureSet* pSignatureSet)
{
   if (pSignatureSet == NULL)
   {
      return false;
   }

   mpLibraryTree->closeActiveCellWidget(true);
   mpSignatureTree->closeActiveCellWidget(true);

   map<QTreeWidgetItem*, SignatureSet*>::iterator iter;
   for (iter = mLibraries.begin(); iter != mLibraries.end(); ++iter)
   {
      SignatureSet* pCurrentSet = iter->second;
      if (pCurrentSet == pSignatureSet)
      {
         // Detach the library
         pSignatureSet->detach(SIGNAL_NAME(Subject, Modified), Slot(this, &SpectralLibraryDlg::updateLibraryName));
         pSignatureSet->detach(SIGNAL_NAME(Subject, Deleted), Slot(this, &SpectralLibraryDlg::removeLibraryItem));

         // Remove all signature tree widget items
         while (mSignatures.empty() == false)
         {
            map<QTreeWidgetItem*, Signature*>::iterator sigIter = mSignatures.begin();
            if (sigIter != mSignatures.end())
            {
               Signature* pSignature = sigIter->second;
               if (pSignature != NULL)
               {
                  removeSignature(pSignature);
               }
            }
         }

         // Delete the library tree widget item
         QTreeWidgetItem* pItem = iter->first;
         if (pItem != NULL)
         {
            delete pItem;
         }

         // Remove the item from the map
         mLibraries.erase(iter);
         return true;
      }
   }

   return false;
}

bool SpectralLibraryDlg::removeSignature(Signature* pSignature)
{
   if (pSignature == NULL)
   {
      return false;
   }

   mpLibraryTree->closeActiveCellWidget(true);
   mpSignatureTree->closeActiveCellWidget(true);

   map<QTreeWidgetItem*, Signature*>::iterator iter;
   for (iter = mSignatures.begin(); iter != mSignatures.end(); ++iter)
   {
      Signature* pCurrentSignature = iter->second;
      if (pCurrentSignature == pSignature)
      {
         // Detach the signature
         pSignature->detach(SIGNAL_NAME(Subject, Modified), Slot(this, &SpectralLibraryDlg::updateSignatureName));
         pSignature->detach(SIGNAL_NAME(Subject, Deleted), Slot(this, &SpectralLibraryDlg::removeSignatureItem));

         // Delete the tree widget item
         QTreeWidgetItem* pItem = iter->first;
         if (pItem != NULL)
         {
            delete pItem;
         }

         // Remove the item from the map
         mSignatures.erase(iter);
         return true;
      }
   }

   return false;
}

SignatureSet* SpectralLibraryDlg::getSelectedLibrary() const
{
   QList<QTreeWidgetItem*> selectedItems = mpLibraryTree->selectedItems();
   if (selectedItems.empty() == true)
   {
      return NULL;
   }

   QTreeWidgetItem* pItem = selectedItems.front();
   if (pItem != NULL)
   {
      map<QTreeWidgetItem*, SignatureSet*>::const_iterator iter = mLibraries.find(pItem);
      if (iter != mLibraries.end())
      {
         return (iter->second);
      }
   }

   return NULL;
}

Signature* SpectralLibraryDlg::getSignature(QTreeWidgetItem* pItem)
{
   if (pItem == NULL)
   {
      return NULL;
   }

   Signature* pSignature = NULL;

   QTreeWidget* pTreeWidget = pItem->treeWidget();
   if (pTreeWidget == mpLibraryTree)
   {
      pSignature = mLibraries[pItem];
   }
   else if (pTreeWidget == mpSignatureTree)
   {
      pSignature = mSignatures[pItem];
   }

   return pSignature;
}

bool SpectralLibraryDlg::saveSignatures(const vector<Signature*>& signatures, const QStringList& sigFilenames,
                                        const QString& strExporter)
{
   if ((signatures.empty() == true) || (sigFilenames.isEmpty() == true) || (strExporter.isEmpty() == true))
   {
      return false;
   }

   if (signatures.size() != sigFilenames.count())
   {
      return false;
   }

   if (mpStep != NULL)
   {
      unsigned int numberOfSigs = static_cast<unsigned int>(signatures.size());
      mpStep->addProperty("Number of Signatures", numberOfSigs);

      for (int i = 0; i < sigFilenames.count(); ++i)
      {
         QString strMessage = "Signature Filename " + QString::number(i);
         QString strFilename = sigFilenames[i];
         mpStep->addProperty(strMessage.toStdString(), strFilename.toStdString());

         strMessage = strFilename + " Wavelengths";
         vector<double> wavelengths;
         signatures[i]->getData("Wavelengths").getValue(wavelengths);
         mpStep->addProperty(strMessage.toStdString(), wavelengths);
      }
   }

   if (mpProgress != NULL)
   {
      mpProgress->updateProgress("Saving signature files...", 0, NORMAL);
   }

   ExporterResource pExporter(strExporter.toStdString(), mpProgress, false);
   bool bOverwrite = false;
   bool bSuccess = false;

   for (vector<Signature*>::size_type i = 0; i < signatures.size(); ++i)
   {
      // Get the output signature filename
      string sigFile = "";

      QString strFilename = sigFilenames[i];
      strFilename.replace(QRegExp("\\\\"), "/");
      if (strFilename.isEmpty() == false)
      {
         // Append a file extension if necessary
         QFileInfo fileInfo(strFilename);
         if (fileInfo.isDir() == true)
         {
            continue;
         }

         QString strFileExtension = fileInfo.suffix();
         QString strFilters = QString::fromStdString(pExporter->getDefaultExtensions());

         QFileInfo filterInfo(strFilters);
         QString strDefault = filterInfo.completeSuffix();

         int iParenLocation = strDefault.indexOf(")");
         int iSpaceLocation = strDefault.indexOf(" ");

         int iIndex = iParenLocation;
         if (iParenLocation != -1)
         {
            if ((iSpaceLocation < iParenLocation) && (iSpaceLocation != -1))
            {
               iIndex = iSpaceLocation;
            }

            strDefault.truncate(static_cast<unsigned int>(iIndex));
            QString strDefaultExtension = strDefault.trimmed();
            if ((strDefaultExtension != strFileExtension) && (strDefaultExtension != "*"))
            {
               strFilename += "." + strDefaultExtension;
            }
         }

         // Prompt for overwrite
         fileInfo.setFile(strFilename);
         if (fileInfo.exists() == true)
         {
            if ((bOverwrite == false) && (signatures.size() > 1))
            {
               QMessageBox::StandardButton button = QMessageBox::question(this, windowTitle(),
                  strFilename + " already exists.\nDo you want to replace it?",
                  QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No);
               if (button == QMessageBox::YesToAll)
               {
                  bOverwrite = true;
               }
               else if (button == QMessageBox::No)
               {
                  if (mpProgress != NULL)
                  {
                     QString strError = "The " + strFilename + " file was not saved.";
                     mpProgress->updateProgress(strError.toStdString(), 0, WARNING);
                  }

                  continue;
               }
            }
         }

         sigFile = strFilename.toStdString();
      }

      // Set the filename and the signature to export
      FactoryResource<FileDescriptor> pFileDescriptor;
      pFileDescriptor->setFilename(sigFile);

      pExporter->setFileDescriptor(pFileDescriptor.get());
      pExporter->setItem(signatures[i]);

      // Launch the exporter
      bSuccess = pExporter->execute();
      if (bSuccess == false)
      {
         break;
      }

      // Update progress
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress("Saving signature files...", i * 100 / signatures.size(), NORMAL);
      }
   }

   if (bSuccess == true)
   {
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress("Saving signature files complete!", 100, NORMAL);
      }
   }

   return bSuccess;
}

QString SpectralLibraryDlg::getSignatureExporterName() const
{
   string exporterType = PlugInManagerServices::ExporterType();
   string exporterSubtype = "Signature";
   string dialogCaption = "Select Signature Exporter";

   ExecutableResource selectPlugIn("Select Plug-In", string(), mpProgress, false);
   selectPlugIn->getInArgList().setPlugInArgValue("Plug-In Type", &exporterType);
   selectPlugIn->getInArgList().setPlugInArgValue("Plug-In Subtype", &exporterSubtype);
   selectPlugIn->getInArgList().setPlugInArgValue("Dialog Caption", &dialogCaption);

   if (selectPlugIn->execute() == true)
   {
      string exporterName;
      selectPlugIn->getOutArgList().getPlugInArgValue<string>("Plug-In Name", exporterName);
      if (exporterName.empty() == false)
      {
         return QString::fromStdString(exporterName);
      }
   }

   return QString();
}

void SpectralLibraryDlg::updateLibraryName(Subject& subject, const string& signal, const boost::any& value)
{
   SignatureSet* pSignatureSet = dynamic_cast<SignatureSet*>(&subject);
   if (pSignatureSet != NULL)
   {
      QString strName = QString::fromStdString(pSignatureSet->getName());

      map<QTreeWidgetItem*, SignatureSet*>::iterator iter;
      for (iter = mLibraries.begin(); iter != mLibraries.end(); ++iter)
      {
         SignatureSet* pCurrentSet = iter->second;
         if (pCurrentSet == pSignatureSet)
         {
            QTreeWidgetItem* pItem = iter->first;
            if (pItem != NULL)
            {
               pItem->setText(0, strName);
            }
         }
      }
   }
}

void SpectralLibraryDlg::updateSignatureName(Subject& subject, const string& signal, const boost::any& value)
{
   Signature* pSignature = dynamic_cast<Signature*>(&subject);
   if (pSignature != NULL)
   {
      QString strName = QString::fromStdString(pSignature->getName());

      map<QTreeWidgetItem*, Signature*>::iterator iter;
      for (iter = mSignatures.begin(); iter != mSignatures.end(); ++iter)
      {
         Signature* pCurrentSignature = iter->second;
         if (pCurrentSignature == pSignature)
         {
            QTreeWidgetItem* pItem = iter->first;
            if (pItem != NULL)
            {
               pItem->setText(0, strName);
            }
         }
      }
   }
}

void SpectralLibraryDlg::removeLibraryItem(Subject& subject, const string& signal, const boost::any& value)
{
   SignatureSet* pSignatureSet = dynamic_cast<SignatureSet*>(&subject);
   if (pSignatureSet != NULL)
   {
      removeLibrary(pSignatureSet);
   }
}

void SpectralLibraryDlg::removeSignatureItem(Subject& subject, const string& signal, const boost::any& value)
{
   Signature* pSignature = dynamic_cast<Signature*>(&subject);
   if (pSignature != NULL)
   {
      removeSignature(pSignature);
   }
}

void SpectralLibraryDlg::accept()
{
   mpLibraryTree->closeActiveCellWidget(true);
   mpSignatureTree->closeActiveCellWidget(true);

   QDialog::accept();
}

void SpectralLibraryDlg::reject()
{
   mpLibraryTree->closeActiveCellWidget(false);
   mpSignatureTree->closeActiveCellWidget(false);

   QDialog::reject();
}

void SpectralLibraryDlg::createLibrary()
{
   SignatureSet* pSignatureSet = NULL;
   Service<ModelServices> pModel;

   int libraryNumber = 1;
   while (pSignatureSet == NULL)
   {
      QString strName = "Spectral Library " + QString::number(libraryNumber++);
      pSignatureSet = static_cast<SignatureSet*>(pModel->createElement(strName.toStdString(), "SignatureSet", NULL));
   }

   if (pSignatureSet == NULL)
   {
      QMessageBox::critical(this, windowTitle(), "Could not create a new spectral library!");
   }
   else
   {
      addLibrary(pSignatureSet);
   }
}

void SpectralLibraryDlg::deleteLibrary()
{
   mpLibraryTree->closeActiveCellWidget(true);
   mpSignatureTree->closeActiveCellWidget(true);

   // Get the selected library
   SignatureSet* pSignatureSet = getSelectedLibrary();
   if (pSignatureSet != NULL)
   {
      // Remove the library tree widget item
      removeLibrary(pSignatureSet);

      // Destroy the library
      Service<ModelServices> pModel;
      pModel->destroyElement(pSignatureSet);
   }
}

void SpectralLibraryDlg::loadLibrary()
{
   mpLibraryTree->closeActiveCellWidget(true);
   mpSignatureTree->closeActiveCellWidget(true);

   vector<DataElement*> importedLibraries;

   Service<DesktopServices> pDesktop;
   if (pDesktop->importFile("Signature Set", NULL, importedLibraries) == true)
   {
      vector<DataElement*>::iterator iter;
      for (iter = importedLibraries.begin(); iter != importedLibraries.end(); ++iter)
      {
         SignatureSet* pSignatureSet = dynamic_cast<SignatureSet*>(*iter);
         if (pSignatureSet != NULL)
         {
            addLibrary(pSignatureSet);
         }
      }
   }
}

void SpectralLibraryDlg::saveLibrary()
{
   mpLibraryTree->closeActiveCellWidget(true);
   mpSignatureTree->closeActiveCellWidget(true);

   SignatureSet* pSignatureSet = getSelectedLibrary();
   if (pSignatureSet != NULL)
   {
      Service<DesktopServices> pDesktop;
      pDesktop->exportSessionItem(pSignatureSet);
   }
}

void SpectralLibraryDlg::enableSignatureButtons()
{
   QList<QTreeWidgetItem*> selectedLibraries = mpLibraryTree->selectedItems();
   if (selectedLibraries.empty() == true)
   {
      return;
   }

   QList<QTreeWidgetItem*> selectedSignatures = mpSignatureTree->selectedItems();
   mpRemoveSigButton->setEnabled(!selectedSignatures.empty());
   mpSaveSigButton->setEnabled(!selectedSignatures.empty());
}

void SpectralLibraryDlg::addSignature()
{
   mpLibraryTree->closeActiveCellWidget(true);
   mpSignatureTree->closeActiveCellWidget(true);

   mpSigSelector = new SignatureSelector(mpProgress, this);
   if (mpSigSelector == NULL)
   {
      return;
   }

   if (mpSigSelector->exec() == QDialog::Accepted)
   {
      // Add the signatures to the selected library
      vector<Signature*> signatures = mpSigSelector->getSignatures();
      for (vector<Signature*>::iterator iter = signatures.begin(); iter != signatures.end(); ++iter)
      {
         Signature* pSignature = *iter;
         if (pSignature != NULL)
         {
            // Do not add the current library to itself
            QList<QTreeWidgetItem*> selectedItems = mpLibraryTree->selectedItems();
            if (selectedItems.empty() == false)
            {
               QTreeWidgetItem* pItem = selectedItems.front();
               if (pItem != NULL)
               {
                  Signature* pLibrary = getSignature(pItem);
                  if (pLibrary == pSignature)
                  {
                     QMessageBox::warning(this, windowTitle(), "Cannot add the spectral library to itself!");
                  }
                  else
                  {
                     if (addSignature(pSignature) == true)
                     {
                        // Add the signature to the currently selected signature set
                        SignatureSet* pSignatureSet = getSelectedLibrary();
                        if (pSignatureSet != NULL)
                        {
                           pSignatureSet->insertSignature(pSignature);
                        }

                        // Add a new item for an added library
                        pSignatureSet = dynamic_cast<SignatureSet*>(pSignature);
                        if (pSignatureSet != NULL)
                        {
                           addLibrary(pSignatureSet);
                        }
                     }
                  }
               }
            }
         }
      }
   }

   delete mpSigSelector;
   mpSigSelector = NULL;
}

void SpectralLibraryDlg::removeSignature()
{
   mpLibraryTree->closeActiveCellWidget(true);
   mpSignatureTree->closeActiveCellWidget(true);

   QList<QTreeWidgetItem*> selectedSignatures = mpSignatureTree->selectedItems();
   for (int i = 0; i < selectedSignatures.count(); ++i)
   {
      QTreeWidgetItem* pItem = selectedSignatures[i];
      if (pItem != NULL)
      {
         map<QTreeWidgetItem*, Signature*>::iterator iter = mSignatures.find(pItem);
         if (iter != mSignatures.end())
         {
            Signature* pSignature = iter->second;
            if (pSignature != NULL)
            {
               // Remove the signature from the signature set
               SignatureSet* pSignatureSet = getSelectedLibrary();
               if (pSignatureSet != NULL)
               {
                  pSignatureSet->removeSignature(pSignature);
               }

               // Remove the tree widget item
               removeSignature(pSignature);
            }
         }
      }
   }
}

void SpectralLibraryDlg::saveSignature()
{
   mpLibraryTree->closeActiveCellWidget(true);
   mpSignatureTree->closeActiveCellWidget(true);

   vector<Signature*> saveSigs;

   QList<QTreeWidgetItem*> selectedSignatures = mpSignatureTree->selectedItems();
   for (int i = 0; i < selectedSignatures.count(); ++i)
   {
      QTreeWidgetItem* pItem = selectedSignatures[i];
      if (pItem != NULL)
      {
         map<QTreeWidgetItem*, Signature*>::iterator iter = mSignatures.find(pItem);
         if (iter != mSignatures.end())
         {
            Signature* pSignature = iter->second;
            if (pSignature != NULL)
            {
               saveSigs.push_back(pSignature);
            }
         }
      }
   }

   if (saveSigs.size() == 1)
   {
      Signature* pSignature = saveSigs.front();
      if (pSignature != NULL)
      {
         Service<DesktopServices> pDesktop;
         pDesktop->exportSessionItem(pSignature);
      }
   }
   else if (saveSigs.empty() == false)
   {
      // Get the exporter
      QString strExporter = getSignatureExporterName();
      if (strExporter.isEmpty() == true)
      {
         return;
      }

      // Assign unique filenames for the selected signatures
      QStringList sigFilenames;
      for (vector<Signature*>::size_type i = 0; i < saveSigs.size(); ++i)
      {
         QString strFilename;

         Signature* pSignature = saveSigs[i];
         if (pSignature != NULL)
         {
            string filename = pSignature->getFilename();
            if (filename.empty() == false)
            {
               strFilename = QString::fromStdString(filename);
            }
            else
            {
               string sigName = pSignature->getName();
               if (sigName.empty() == false)
               {
                  // Check if the signature name is a file
                  QFileInfo fileInfo = QFileInfo(QString::fromStdString(sigName));
                  if (fileInfo.isFile() == false)
                  {
                     // Replace invalid filename characters
                     string::size_type pos = sigName.find(":");
                     while (pos != string::npos)
                     {
                        sigName.replace(pos, 1, "_");
                        pos = sigName.find(":", pos + 1);
                     }

                     pos = sigName.find("\\");
                     while (pos != string::npos)
                     {
                        sigName.replace(pos, 1, "_");
                        pos = sigName.find("\\", pos + 1);
                     }

                     pos = sigName.find("/");
                     while (pos != string::npos)
                     {
                        sigName.replace(pos, 1, "_");
                        pos = sigName.find("/", pos + 1);
                     }

                     // Set the filename
                     strFilename = QDir::currentPath() + "/" + QString::fromStdString(sigName);
                     strFilename.replace(QRegExp("\\\\"), "/");
                  }
                  else
                  {
                     strFilename = QString::fromStdString(sigName);
                  }
               }
               else
               {
                  strFilename = "Signature " + QString::number(i + 1);
               }
            }
         }

         if (strFilename.isEmpty() == false)
         {
            sigFilenames.append(strFilename);
         }
      }

      saveSignatures(saveSigs, sigFilenames, strExporter);
   }
}

void SpectralLibraryDlg::updateSignatureList()
{
   while (mSignatures.empty() == false)
   {
      map<QTreeWidgetItem*, Signature*>::iterator iter = mSignatures.begin();
      if (iter != mSignatures.end())
      {
         Signature* pSignature = iter->second;
         if (pSignature != NULL)
         {
            removeSignature(pSignature);
         }
      }
   }

   SignatureSet* pSignatureSet = getSelectedLibrary();

   mpDeleteLibButton->setEnabled(pSignatureSet != NULL);
   mpSaveLibButton->setEnabled(pSignatureSet != NULL);
   mpSignatureTree->setEnabled(pSignatureSet != NULL);
   mpAddSigButton->setEnabled(pSignatureSet != NULL);
   mpRemoveSigButton->setEnabled(pSignatureSet != NULL);
   mpSaveSigButton->setEnabled(pSignatureSet != NULL);

   if (pSignatureSet == NULL)
   {
      return;
   }

   vector<Signature*> signatures = pSignatureSet->getSignatures();
   for (vector<Signature*>::size_type i = 0; i < signatures.size(); ++i)
   {
      Signature* pSignature = signatures[i];
      if (pSignature != NULL)
      {
         addSignature(pSignature);
      }
   }

   enableSignatureButtons();
}

void SpectralLibraryDlg::updateSignatureData(QTreeWidgetItem* pItem, int iColumn)
{
   if (pItem == NULL)
   {
      return;
   }

   Signature* pSignature = getSignature(pItem);
   if (pSignature != NULL)
   {
      QString strItem = pItem->text(iColumn);
      if (strItem.isEmpty() == false)
      {
         string itemText = strItem.toStdString();
         if (iColumn == 0)
         {
            Service<ModelServices> pModel;
            pModel->setElementName(pSignature, itemText);
         }
      }
   }
}

void SpectralLibraryDlg::help()
{
   Service<DesktopServices> pDesktop;
   Service<ConfigurationSettings> pSettings;

   string helpFile = pSettings->getHome() + LibraryBuilder::getSettingSpectralLibraryHelp();
   pDesktop->displayHelp(helpFile);
}

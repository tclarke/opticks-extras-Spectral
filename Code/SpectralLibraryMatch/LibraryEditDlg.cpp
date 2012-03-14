/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "LibraryEditDlg.h"
#include "MessageLog.h"
#include "Signature.h"
#include "SignatureSelector.h"
#include "Slot.h"
#include "Subject.h"

#include <QtCore/QList>
#include <QtCore/QListIterator>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QGridLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QPushButton>
#include <QtGui/QTreeWidget>
#include <QtGui/QTreeWidgetItem>

LibraryEditDlg::LibraryEditDlg(const std::vector<Signature*>& signatures, QWidget* pParent) :
   mpTree(NULL),
   QDialog(pParent, Qt::WindowCloseButtonHint)
{
   setWindowTitle("Spectral Library Editor");
   mpTree = new QTreeWidget(this);
   QStringList columnNames;
   columnNames << "Signatures";
   mpTree->setColumnCount(columnNames.count());
   mpTree->setHeaderLabels(columnNames);
   mpTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
   mpTree->setAllColumnsShowFocus(true);
   mpTree->setRootIsDecorated(true);
   mpTree->setSortingEnabled(false);
   mpTree->setToolTip("This list displays the spectral library matches for in-scene spectra.");

   QHeaderView* pHeader = mpTree->header();
   if (pHeader != NULL)
   {
      pHeader->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
      pHeader->resizeSection(0, 150);
   }

   QDialogButtonBox* pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
      Qt::Vertical, this);
   QPushButton* pAdd = new QPushButton("Add...", this);
   QPushButton* pRemove = new QPushButton("Remove", this);
   pButtons->addButton(pAdd, QDialogButtonBox::ActionRole);
   pButtons->addButton(pRemove, QDialogButtonBox::ActionRole);

   QGridLayout* pGrid = new QGridLayout(this);
   pGrid->addWidget(mpTree, 0, 0, 6, 1);
   pGrid->addWidget(pButtons, 0, 1, 6, 1);

   VERIFYNR(connect(pButtons, SIGNAL(accepted()), this, SLOT(accept())));
   VERIFYNR(connect(pButtons, SIGNAL(rejected()), this, SLOT(reject())));
   VERIFYNR(connect(pAdd, SIGNAL(clicked()), this, SLOT(addSignatures())));
   VERIFYNR(connect(pRemove, SIGNAL(clicked()), this, SLOT(removeSignatures())));

   addSignatures(signatures);
}

LibraryEditDlg::~LibraryEditDlg()
{
   int numSigs = mpTree->topLevelItemCount();
   for (int index = 0; index < numSigs; ++index)
   {
      QVariant variant = mpTree->topLevelItem(index)->data(0, Qt::UserRole);
      if (variant.isValid())
      {
         VERIFYNR(variant.value<Signature*>()->detach(SIGNAL_NAME(Subject, Deleted),
            Slot(this, &LibraryEditDlg::signatureDeleted)));
      }
   }
}

void LibraryEditDlg::addSignatures(const std::vector<Signature*>& signatures)
{
   for (std::vector<Signature*>::const_iterator it = signatures.begin(); it != signatures.end(); ++it)
   {
      // if not in list, add
      if (mpTree->findItems(QString::fromStdString((*it)->getName()), Qt::MatchExactly).isEmpty())
      {
         QTreeWidgetItem* pItem = new QTreeWidgetItem(mpTree);
         pItem->setText(0, QString::fromStdString((*it)->getName()));
         pItem->setData(0, Qt::UserRole, QVariant::fromValue(*it));
         VERIFYNR((*it)->attach(SIGNAL_NAME(Subject, Deleted), Slot(this, &LibraryEditDlg::signatureDeleted)));
      }
   }
}

void LibraryEditDlg::addSignatures()
{
   SignatureSelector dlg(NULL, this);
   dlg.setWindowTitle("Select Signatures for Spectral Library Matching");
   if (dlg.exec() == QDialog::Accepted)
   {
      addSignatures(dlg.getExtractedSignatures());
   }
}

void LibraryEditDlg::removeSignatures()
{
   QList<QTreeWidgetItem*> items = mpTree->selectedItems();
   QListIterator<QTreeWidgetItem*> it(items);
   while (it.hasNext())
   {
      QTreeWidgetItem* pItem = it.next();
      QVariant variant = pItem->data(0, Qt::UserRole);
      if (variant.isValid())
      {
         VERIFYNR(variant.value<Signature*>()->detach(SIGNAL_NAME(Subject, Deleted),
            Slot(this, &LibraryEditDlg::signatureDeleted)));
      }
      delete pItem;
   }
}

std::vector<Signature*> LibraryEditDlg::getSignatures() const
{
   int numSigs = mpTree->topLevelItemCount();
   std::vector<Signature*> signatures;
   signatures.reserve(numSigs);
   for (int index = 0; index < numSigs; ++index)
   {
      QVariant variant = mpTree->topLevelItem(index)->data(0, Qt::UserRole);
      if (variant.isValid())
      {
         signatures.push_back(variant.value<Signature*>());
      }
   }
   return signatures;
}

void LibraryEditDlg::signatureDeleted(Subject& subject, const std::string& signal, const boost::any& value)
{
   Signature* pSignature = dynamic_cast<Signature*>(&subject);
   if (pSignature != NULL && signal == "Subject::Deleted")
   {
      VERIFYNR(pSignature->detach(SIGNAL_NAME(Subject, Deleted), Slot(this, &LibraryEditDlg::signatureDeleted)));
      QList<QTreeWidgetItem*> items =
         mpTree->findItems(QString::fromStdString(pSignature->getName()), Qt::MatchExactly);
      if (items.isEmpty() == false)
      {
         QListIterator<QTreeWidgetItem*> it(items);
         while (it.hasNext())
         {
            delete it.next();
         }
      }
   }
}

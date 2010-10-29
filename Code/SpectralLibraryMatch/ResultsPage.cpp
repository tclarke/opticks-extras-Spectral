/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "ColorType.h"
#include "ResultsPage.h"
#include "Signature.h"

#include <QtGui/QIcon>
#include <QtCore/QList>
#include <QtCore/QListIterator>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QBrush>
#include <QtGui/QHeaderView>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QPixmap>
#include <QtGui/QTreeWidgetItem>

namespace
{
   static const int gcSignatureNameColumn(0);
   static const int gcAlgorithmNameColumn(1);
}

ResultsPage::ResultsPage(QWidget* pParent) :
   QTreeWidget(pParent)
{
   QStringList columnNames;
   columnNames << "Signature" << "Algorithm Value";
   setColumnCount(columnNames.count());
   setHeaderLabels(columnNames);
   setSelectionMode(QAbstractItemView::ExtendedSelection);
   setAllColumnsShowFocus(true);
   setRootIsDecorated(true);
   setSortingEnabled(false);
   setToolTip("This list displays the spectral library matches for in-scene spectra.");

   QHeaderView* pHeader = header();
   if (pHeader != NULL)
   {
      pHeader->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
      pHeader->resizeSection(0, 250);
   }
}

ResultsPage::~ResultsPage()
{}

void ResultsPage::addResults(SpectralLibraryMatch::MatchResults& theResults,
                             const std::map<Signature*, ColorType>& colorMap)
{
   if (theResults.isValid() == false)
   {
      return;
   }

   QTreeWidgetItem* pParentItem = findResults(theResults.mTargetName, theResults.mAlgorithmUsed);
   if (pParentItem == NULL)
   {
      pParentItem = new QTreeWidgetItem(this);
      pParentItem->setText(gcSignatureNameColumn, QString::fromStdString(theResults.mTargetName));
      pParentItem->setText(gcAlgorithmNameColumn, QString::fromStdString(
         StringUtilities::toDisplayString<SpectralLibraryMatch::MatchAlgorithm>(theResults.mAlgorithmUsed)));
   }
   else
   {
      for (int i = pParentItem->childCount() - 1; i >= 0; --i)
      {
         delete pParentItem->child(i);
      }
   }

   std::map<Signature*, ColorType>::const_iterator mit;
   if (theResults.mResults.empty())
   {
      QTreeWidgetItem* pItem = new QTreeWidgetItem(pParentItem);
      pItem->setText(gcSignatureNameColumn, "No Matches found");
   }
   else
   {
      for (std::vector<std::pair<Signature*, float> >::iterator it = theResults.mResults.begin();
         it != theResults.mResults.end(); ++it)
      {
         QTreeWidgetItem* pItem = new QTreeWidgetItem(pParentItem);
         pItem->setText(gcSignatureNameColumn, QString::fromStdString(it->first->getDisplayName(true)));
         Signature* pSig = it->first;
         pItem->setData(0, Qt::UserRole, QVariant::fromValue(pSig));
         pItem->setToolTip(gcSignatureNameColumn, QString::fromStdString(it->first->getName()));
         mit = colorMap.find(it->first);
         if (mit != colorMap.end())
         {
            QColor borderColor = QColor(127, 157, 185);
            QPen pen(borderColor);
            QPixmap pix = QPixmap(16, 16);
            QRectF rect(0, 0, 15, 15);
            QPainter p;
            QBrush brush(COLORTYPE_TO_QCOLOR(mit->second));
            p.begin(&pix);
            p.fillRect(rect, brush);
            p.setPen(pen);
            p.drawRect(rect);
            p.end();

            pItem->setIcon(gcSignatureNameColumn, QIcon(pix));
         }
         pItem->setText(gcAlgorithmNameColumn, QString::number(it->second));
      }
   }
   pParentItem->setExpanded(true);
}

QTreeWidgetItem* ResultsPage::findResults(const std::string& sigName, SpectralLibraryMatch::MatchAlgorithm algType)
{
   if (sigName.empty() || algType.isValid() == false)
   {
      return NULL;
   }

   QList<QTreeWidgetItem*> itemList = findItems(QString::fromStdString(sigName), Qt::MatchExactly);
   QListIterator<QTreeWidgetItem*> it(itemList);
   while (it.hasNext())
   {
      QTreeWidgetItem* pCurrentItem = it.next();
      if (pCurrentItem->text(gcAlgorithmNameColumn).toStdString() ==
         StringUtilities::toDisplayString<SpectralLibraryMatch::MatchAlgorithm>(algType))
      {
         return pCurrentItem;
      }
   }

   return NULL;
}

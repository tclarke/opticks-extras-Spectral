/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "ColorType.h"
#include "ResultsItem.h"
#include "ResultsItemModel.h"
#include "ResultsPage.h"
#include "ResultsSortFilter.h"
#include "Signature.h"
#include "SpectralLibraryMatchOptions.h"
#include "StringUtilities.h"

#include <QtCore/QList>
#include <QtCore/QListIterator>
#include <QtCore/QModelIndex>
#include <QtCore/QModelIndexList>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QBrush>
#include <QtGui/QHeaderView>
#include <QtGui/QIcon>
#include <QtGui/QItemSelectionModel>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QPixmap>

#include <algorithm>
#include <string>

namespace
{
   static const int gcSignatureNameColumn(0);
   static const int gcAlgorithmNameColumn(1);
}

ResultsPage::ResultsPage(QWidget* pParent) :
   QTreeView(pParent),
   mAutoClear(SpectralLibraryMatchOptions::getSettingAutoclear())
{
   setRootIsDecorated(true);
   setSortingEnabled(true);
   setModel(new ResultsSortFilter(this));
   setSelectionMode(QAbstractItemView::ExtendedSelection);
   setSelectionBehavior(QAbstractItemView::SelectRows);
   setAllColumnsShowFocus(true);
   setToolTip("This list displays the spectral library matches for in-scene spectra.");

   QHeaderView* pHeader = header();
   if (pHeader != NULL)
   {
      pHeader->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
      pHeader->resizeSection(0, 250);
      pHeader->setMovable(false);
      pHeader->setSortIndicator(0, Qt::AscendingOrder);
      pHeader->setSortIndicatorShown(false);
   }
}

ResultsPage::~ResultsPage()
{}

void ResultsPage::addResults(const std::vector<SpectralLibraryMatch::MatchResults>& theResults,
                             const std::map<Signature*, ColorType>& colorMap, Progress* pProgress, bool* pAbort)
{
   if (getAutoClear())
   {
      clear();
   }
   ResultsSortFilter* pFilter = dynamic_cast<ResultsSortFilter*>(model());
   VERIFYNRV(pFilter != NULL);
   ResultsItemModel* pModel = dynamic_cast<ResultsItemModel*>(pFilter->sourceModel());
   VERIFYNRV(pModel != NULL);
   pModel->addResults(theResults, colorMap, pProgress, pAbort);
   if (pAbort == NULL || *pAbort == false)
   {
      expandAddedResults(theResults);
   }
}

void ResultsPage::clear()
{
   ResultsSortFilter* pFilter = dynamic_cast<ResultsSortFilter*>(model());
   ResultsItemModel* pModel = dynamic_cast<ResultsItemModel*>(pFilter->sourceModel());
   VERIFYNRV(pModel != NULL);
   pModel->clear();
}

void ResultsPage::getSelectedSignatures(std::vector<Signature*>& signatures)
{
   signatures.clear();
   QItemSelectionModel* pSelectModel = selectionModel();
   VERIFYNRV(pSelectModel != NULL);
   QModelIndexList selectedCol0 = pSelectModel->selectedRows();
   if (selectedCol0.isEmpty())
   {
      return;
   }
   QModelIndexList selectedCol1 = pSelectModel->selectedRows(1);

   std::vector<std::string> resultKeys;
   std::vector<Signature*>::iterator sit;
   ResultsSortFilter* pFilter = dynamic_cast<ResultsSortFilter*>(model());
   VERIFYNRV(pFilter != NULL);
   Signature* pSignature(NULL);
   for (int i = 0; i < selectedCol0.size(); ++i)
   {
      QModelIndex index = selectedCol0[i];
      if (index.isValid() == false)
      {
         continue;
      }
      QVariant variant = pFilter->data(index, Qt::UserRole);
      if (variant.isValid())
      {
         pSignature = variant.value<Signature*>();
         if (pSignature != NULL)  // a result of "No Matches found" will return NULL so skip them.
         {
            sit = std::find(signatures.begin(), signatures.end(), pSignature);
            if (sit == signatures.end())
            {
               signatures.push_back(pSignature);
            }
         }
      }
      else   // If item doesn't contain data (a Signature*), it's a pixel name item with children.
             // It was selected, so we need to get the data from its children to include those Signatures.
      {
         QVariant var = pFilter->data(index);
         std::string keyStr;
         if (var.isValid())
         {
            keyStr = var.toString().toStdString();
            index = selectedCol1[i];
            var = pFilter->data(index);
            if (var.isValid())
            {
               keyStr += var.toString().toStdString();
               resultKeys.push_back(keyStr);
            }
         }
      }
   }

   if (resultKeys.empty() == false)
   {
      ResultsItemModel* pModel = dynamic_cast<ResultsItemModel*>(pFilter->sourceModel());
      VERIFYNRV(pModel != NULL);
      for (unsigned int i = 0; i < resultKeys.size(); ++i)
      {
         const ResultsItem* pItem = pModel->getResult(resultKeys[i]);
         if (pItem != NULL)
         {
            for (int row = 0; row < pItem->rows(); ++row)
            {
               pSignature = pItem->getSignature(row);
               if (pSignature != NULL)
               {
                  sit = std::find(signatures.begin(), signatures.end(), pSignature);
                  if (sit == signatures.end())
                  {
                     signatures.push_back(pSignature);
                  }
               }
            }
         }
      }
   }
}

void ResultsPage::expandAddedResults(const std::vector<SpectralLibraryMatch::MatchResults>& added)
{
   if (added.empty())
   {
      return;
   }

   QSortFilterProxyModel* pFilterModel = dynamic_cast<QSortFilterProxyModel*>(model());
   VERIFYNRV(pFilterModel != NULL);
   ResultsItemModel* pModel = dynamic_cast<ResultsItemModel*>(pFilterModel->sourceModel());
   for (std::vector<SpectralLibraryMatch::MatchResults>::const_iterator it = added.begin(); it != added.end(); ++it)
   {
      QModelIndex index = pModel->getItemIndex(it->mTargetName, it->mAlgorithmUsed);
      expand(pFilterModel->mapFromSource(index));
   }
}

bool ResultsPage::getAutoClear() const
{
   return mAutoClear;
}

void ResultsPage::setAutoClear(bool enabled)
{
   mAutoClear = enabled;
}

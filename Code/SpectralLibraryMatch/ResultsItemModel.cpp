/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "PlugIn.h"
#include "PlugInManagerServices.h"
#include "Progress.h"
#include "ResultsItem.h"
#include "ResultsItemModel.h"
#include "Signature.h"
#include "SpectralLibraryManager.h"
#include "SpectralLibraryMatch.h"
#include "Subject.h"

#include <QtCore/QtAlgorithms>
#include <QtCore/QList>
#include <QtCore/QListIterator>
#include <QtCore/QString>
#include <QtGui/QBrush>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QPixmap>

ResultsItemModel::ResultsItemModel(QObject* pParent) :
   QAbstractItemModel(pParent)
{
   std::vector<PlugIn*> plugIns = Service<PlugInManagerServices>()->getPlugInInstances(
      SpectralLibraryMatch::getNameLibraryManagerPlugIn());
   if (plugIns.empty() == false)
   {
      SpectralLibraryManager* pLibMgr = dynamic_cast<SpectralLibraryManager*>(plugIns.front());
      if (pLibMgr != NULL)
      {
         VERIFYNR(pLibMgr->attach(SIGNAL_NAME(SpectralLibraryManager, SignatureDeleted),
            Slot(this, &ResultsItemModel::signatureDeleted)));
      }
   }
}

ResultsItemModel::~ResultsItemModel()
{
   std::vector<PlugIn*> plugIns = Service<PlugInManagerServices>()->getPlugInInstances(
      SpectralLibraryMatch::getNameLibraryManagerPlugIn());
   if (plugIns.empty() == false)
   {
      SpectralLibraryManager* pLibMgr = dynamic_cast<SpectralLibraryManager*>(plugIns.front());
      if (pLibMgr != NULL)
      {
         VERIFYNR(pLibMgr->detach(SIGNAL_NAME(SpectralLibraryManager, SignatureDeleted),
            Slot(this, &ResultsItemModel::signatureDeleted)));
      }
   }
   clear();
}

void ResultsItemModel::addResults(const std::vector<SpectralLibraryMatch::MatchResults>& theResults,
   const std::map<Signature*, ColorType>& colorMap, Progress* pProgress, bool* pAbort)
{
   if (theResults.empty())
   {
      return;
   }

   bool findPrevious = (rowCount() > 0);  // don't look for previous results if model is empty.
   if (pProgress != NULL)
   {
      pProgress->updateProgress("Adding Match Results to Results Window...", 0, NORMAL);
   }
   unsigned int resultCount(0);
   unsigned int numResults = theResults.size();
   mResults.reserve(mResults.size() + numResults);
   beginResetModel();
   for (std::vector<SpectralLibraryMatch::MatchResults>::const_iterator it = theResults.begin();
      it != theResults.end(); ++it)
   {
      if (pAbort != NULL && *pAbort)
      {
         if (pProgress != NULL)
         {
            pProgress->updateProgress("Adding Match Results to Results Window canceled by user", 0, ABORT);
         }
         endResetModel();
         return;
      }

      ResultsItem* pItem(NULL);
      bool newItem(false);
      if (findPrevious)
      {
         pItem = findResult(it->mTargetName, it->mAlgorithmUsed);
      }
      if (pItem == NULL)
      {
         pItem = new ResultsItem();
         newItem = true;
      }

      pItem->setData(*it, colorMap);
      if (newItem)
      {
         mItemMap[getKeyString(it->mTargetName, it->mAlgorithmUsed)] = pItem;
         mResults.push_back(pItem);
      }

      if (pProgress != NULL)
      {
         ++resultCount;
         pProgress->updateProgress("Adding Match Results to Results Window...",
            resultCount * 100 / numResults, NORMAL);
      }
   }
   if (pProgress != NULL)
   {
      pProgress->updateProgress("Finished adding Match Results to Results Window.", 100, NORMAL);
   }
   endResetModel();
}

std::string ResultsItemModel::getKeyString(const std::string& sigName, SpectralLibraryMatch::MatchAlgorithm algType)
{
   return sigName + StringUtilities::toDisplayString<SpectralLibraryMatch::MatchAlgorithm>(algType);
}

ResultsItem* ResultsItemModel::findResult(const std::string& sigName, SpectralLibraryMatch::MatchAlgorithm algType)
{
   if (sigName.empty() || algType.isValid() == false)
   {
      return NULL;
   }

   std::map<std::string, ResultsItem*>::const_iterator it = mItemMap.find(getKeyString(sigName, algType));
   if (it != mItemMap.end())
   {
      return it->second;
   }

   return NULL;
}

QVariant ResultsItemModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid())
   {
      return QVariant();
   }

   ResultsItem* pItem = reinterpret_cast<ResultsItem*>(index.internalPointer());
   if (pItem == NULL)  // main node so only has display role 
   {
      if (role == Qt::DisplayRole)
      {
         pItem = getItem(index.row());
         if (index.column() == 0)
         {
            return QVariant(pItem->getTargetName());
         }
         else if (index.column() == 1)
         {
            return QVariant(pItem->getAlgorithmName());
         }
      }
   }
   else  // want a result
   {
      switch (role)
      {
      case Qt::DisplayRole:
         if (index.column() == 0)
         {
            return QVariant(pItem->getSignatureName(index.row()));
         }
         else if (index.column() == 1)
         {
            return QVariant(pItem->getValueStr(index.row()));
         }
         break;
      case Qt::UserRole:
         if (index.column() == 0)
         {
            return QVariant::fromValue(pItem->getSignature(index.row()));
         }
         break;
      case Qt::DecorationRole:
         if (index.column() == 0)
         {
            return QVariant(pItem->getIcon(index.row()));
         }
      }
   }

   return QVariant();
}

QVariant ResultsItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
   {
      switch (section)
      {
      case 0:
         return QVariant("Signature");
         break;

      case 1:
         return QVariant("Algorithm Value");
         break;

      default:  // fall thru to return QVariant()
         break;
      }
   }

   return QVariant();
}

QModelIndex ResultsItemModel::index(int row, int column, const QModelIndex& parent) const
{
   if (parent.isValid() == false)  // parent is root element
   {
      return createIndex(row, column);
   }

   if (parent.internalPointer() != NULL)
   {
      return QModelIndex();
   }

   ResultsItem* pItem = getItem(parent.row());
   return createIndex(row, column, pItem);
}

QModelIndex ResultsItemModel::parent(const QModelIndex& index) const
{
   if (index.isValid())  // not root item
   {
      ResultsItem* pItem = reinterpret_cast<ResultsItem*>(index.internalPointer());
      if (pItem != NULL)  // child result 
      {
         return createIndex(getRow(pItem), 0);
      }
   }

   return QModelIndex();
}

int ResultsItemModel::rowCount(const QModelIndex& parent) const
{
   if (parent.isValid() == false)  // root element
   {
      return mResults.size();
   }

   ResultsItem* pItem = reinterpret_cast<ResultsItem*>(parent.internalPointer());
   if (pItem == NULL)  // top level node
   {
      pItem = getItem(parent.row());
      return pItem->rows();
   }

   // no grand children
   return 0;
}

int ResultsItemModel::columnCount(const QModelIndex& parent) const
{
   return 2;
}

void ResultsItemModel::clear()
{
   beginResetModel();
   for (std::map<std::string, ResultsItem*>::iterator it = mItemMap.begin(); it != mItemMap.end(); ++it)
   {
      delete it->second;
   }
   mItemMap.clear();
   mResults.clear();
   endResetModel();
}

int ResultsItemModel::getRow(const ResultsItem* pItem) const
{
   std::vector<ResultsItem*>::const_iterator it = std::find(mResults.begin(), mResults.end(), pItem);
   if (it != mResults.end())
   {
      return static_cast<int>(it - mResults.begin());
   }

   return -1;  // note if item not found, return invalid row number
}

ResultsItem* ResultsItemModel::getItem(int row) const
{
   ResultsItem* pItem(NULL);
   if (static_cast<unsigned int>(row) < mResults.size())
   {
      pItem = mResults[row];
   }

   return pItem;
}

QModelIndex ResultsItemModel::getItemIndex(const std::string& name,
   const SpectralLibraryMatch::MatchAlgorithm& algoType)
{
   ResultsItem* pItem = findResult(name, algoType);
   return index(getRow(pItem), 0);
}

const ResultsItem* ResultsItemModel::getResult(const std::string& key) const
{
   std::map<std::string, ResultsItem*>::const_iterator it = mItemMap.find(key);
   if (it != mItemMap.end())
   {
      return it->second;
   }

   return NULL;
}

void ResultsItemModel::signatureDeleted(Subject& subject, const std::string& signal, const boost::any& value)
{
   Signature* pSignature = boost::any_cast<Signature*>(value);
   if (pSignature == NULL)
   {
      return;
   }

   beginResetModel();
   for (std::vector<ResultsItem*>::iterator iter = mResults.begin(); iter != mResults.end(); ++iter)
   {
      (*iter)->deleteResultsForSignature(pSignature);
   }
   endResetModel();
}
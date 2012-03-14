/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RESULTSITEMMODEL_H
#define RESULTSITEMMODEL_H

#include "ColorType.h"
#include "SpectralLibraryMatch.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QMetaType>
#include <QtCore/QModelIndex>
#include <QtCore/QVariant>

#include <boost/any.hpp>
#include <map>
#include <string>

class Progress;
class ResultsItem;
class Signature;
class Subject;

Q_DECLARE_METATYPE(Signature*)

class ResultsItemModel : public QAbstractItemModel
{
public:
   ResultsItemModel(QObject* pParent = 0);
   virtual ~ResultsItemModel();

   void addResults(const std::vector<SpectralLibraryMatch::MatchResults>& theResults,
      const std::map<Signature*, ColorType>& colorMap, Progress* pProgress, bool* pAbort);
   void clear();
   virtual QVariant data(const QModelIndex& index, int role) const;
   virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
   virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
   virtual QModelIndex parent(const QModelIndex& index) const;
   virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;
   virtual int columnCount(const QModelIndex& parent = QModelIndex()) const;
   QModelIndex getItemIndex(const std::string& name, const SpectralLibraryMatch::MatchAlgorithm& algoType);
   const ResultsItem* getResult(const std::string& key) const;

protected:
   ResultsItem* findResult(const std::string& sigName, SpectralLibraryMatch::MatchAlgorithm algType);
   std::string getKeyString(const std::string& sigName, SpectralLibraryMatch::MatchAlgorithm algType);
   int getRow(const ResultsItem* pItem) const;
   ResultsItem* getItem(int row) const;

   void signatureDeleted(Subject& subject, const std::string& signal, const boost::any& value);

private:
   std::map<std::string, ResultsItem*> mItemMap;  // provides fast find of a result item by pixel location and algorithm
   std::vector<ResultsItem*> mResults;            // provides fast access to item by row and getting row for an item
};

#endif

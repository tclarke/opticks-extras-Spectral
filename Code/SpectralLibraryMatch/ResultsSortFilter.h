/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RESULTSSORTFILTER_H
#define RESULTSSORTFILTER_H

#include "Location.h"

#include <QtCore/QVariant>
#include <QtGui/QSortFilterProxyModel>

class QModelIndex;
class QString;

class ResultsSortFilter : public QSortFilterProxyModel
{
   Q_OBJECT

public:
   ResultsSortFilter(QObject* pParent = 0);
   virtual ~ResultsSortFilter();

   virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;
   virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);

protected:
   virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const;
   Opticks::PixelLocation stringToPixel(QString& pixelString) const;
};

#endif

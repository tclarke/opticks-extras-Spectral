/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "ResultsItemModel.h"
#include "ResultsSortFilter.h"

#include <QtCore/QModelIndex>
#include <QtCore/QRegExp>
#include <QtCore/QString>
#include <QtCore/QStringList>

ResultsSortFilter::ResultsSortFilter(QObject* pParent) :
   QSortFilterProxyModel(pParent)
{
   setSourceModel(new ResultsItemModel(this));
   setDynamicSortFilter(true);
}

ResultsSortFilter::~ResultsSortFilter()
{}

bool ResultsSortFilter::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
   // only apply sort to top-level (pixel location string) items
   if (left.parent().isValid() || right.parent().isValid())  // parent index is invalid for top-level items
   {
      return false;  // leaves child items in original order
   }
   if (left.column() != 0 || right.column() != 0) // only sort by first column (0 based)
   {
      return false;
   }
   ResultsItemModel* pModel = dynamic_cast<ResultsItemModel*>(sourceModel());
   QString leftPixelStr = pModel->data(left, Qt::DisplayRole).value<QString>();
   QString rightPixelStr = pModel->data(right, Qt::DisplayRole).value<QString>();
   Opticks::PixelLocation leftPixel = stringToPixel(leftPixelStr);
   Opticks::PixelLocation rightPixel = stringToPixel(rightPixelStr);

   if (sortOrder() == Qt::AscendingOrder)    // sort using row major
   {
      if (leftPixel.mY < rightPixel.mY)     // left row < right row
      {
         return true;
      }
      else if (leftPixel.mY == rightPixel.mY && leftPixel.mX < rightPixel.mX)  // same row but left col < right col
      {
         return true;
      }
   }
   else  // sort using column major, but have to invert logic since this is being called for descending order
   {
      if (leftPixel.mX > rightPixel.mX)     // so will be sorted with left col < right col
      {
         return true;
      }
      else if (leftPixel.mX == rightPixel.mX && leftPixel.mY > rightPixel.mY)  // same col but sorted left row < right row
      {
         return true;
      }
   }

   return false;
}

Opticks::PixelLocation ResultsSortFilter::stringToPixel(QString& pixelString) const
{
   QStringList list = pixelString.split(QRegExp("[(, )]"), QString::SkipEmptyParts);

   // check for valid pixel string - if invalid, return large negative location so bad result goes to top of results
   if (list.isEmpty() || list.count() != 3 || list[0] != "Pixel")
   {
      return Opticks::PixelLocation(-999, -999);
   }

   Opticks::PixelLocation loc(list[1].toInt(), list[2].toInt());

   return loc;
}

QVariant ResultsSortFilter::headerData(int section, Qt::Orientation orientation, int role) const
{
   if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole))
   {
      if (section == 0)
      {
         QVariant data = sourceModel()->headerData(section, orientation, role);
         if (data.isValid() == false)
         {
            return QVariant();
         }
         QString label = data.toString();
         switch (sortOrder())
         {
         case Qt::AscendingOrder:
            label += "  (sorted by row)";
            break;

         case Qt::DescendingOrder:
            label += "  (sorted by column)";
            break;

         default:
            break;
         }
         return QVariant(label);
      }
      else if (section == 1)
      {
         return sourceModel()->headerData(section, orientation, role);
      }
   }

   return QVariant();
}

void ResultsSortFilter::sort(int column, Qt::SortOrder order)
{
   // only sort by first column (0 based)
   if (column == 0)
   {
      QSortFilterProxyModel::sort(column, order);
   }
}
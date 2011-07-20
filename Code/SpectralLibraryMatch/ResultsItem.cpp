/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "ColorType.h"
#include "ResultsItem.h"
#include "Signature.h"

#include <QtCore/QtAlgorithms>
#include <QtGui/QBrush>
#include <QtGui/QPainter>
#include <QtGui/QPen>

ResultsItem::ResultsItem()
{}

ResultsItem::~ResultsItem()
{}

void ResultsItem::setData(const SpectralLibraryMatch::MatchResults& data,
   const std::map<Signature*, ColorType>& colorMap)
{
   clear();
   mTargetName = QString::fromStdString(data.mTargetName);
   mAlgorithmName = QString::fromStdString(
      StringUtilities::toDisplayString<SpectralLibraryMatch::MatchAlgorithm>(data.mAlgorithmUsed));

   for (std::vector<std::pair<Signature*, float> >::const_iterator it = data.mResults.begin();
      it != data.mResults.end(); ++it)
   {
      mResults.push_back(*it);
      if (colorMap.empty() == false)  // only create icons if colorMap has entries
      {
         QColor color(Qt::white);  // set white as default in case signature doesn't have an entry
         std::map<Signature*, ColorType>::const_iterator mit = colorMap.find(it->first);
         if (mit != colorMap.end())
         {
            color = COLORTYPE_TO_QCOLOR(mit->second);
         }
         QColor borderColor = QColor(127, 157, 185);
         QPen pen(borderColor);
         QPixmap pix = QPixmap(16, 16);
         QRectF rect(0, 0, 15, 15);
         QPainter p;
         QBrush brush(color);
         p.begin(&pix);
         p.fillRect(rect, brush);
         p.setPen(pen);
         p.drawRect(rect);
         p.end();
         mIcons.push_back(QIcon(pix));
      }
   }
}

Signature* ResultsItem::getSignature(unsigned int row) const
{
   if (row < mResults.size())
   {
      return mResults[row].first;
   }

   return NULL;
}

QString ResultsItem::getValueStr(unsigned int row) const
{
   if (row < mResults.size())
   {
      return QString::number(mResults[row].second, 'f', 4);
   }

   return QString();
}

QIcon ResultsItem::getIcon(unsigned int row) const
{
   if (row < mIcons.size())
   {
      return mIcons[row];
   }

   return QIcon();
}

void ResultsItem::clear()
{
   mResults.clear();
   mIcons.clear();
}

int ResultsItem::rows() const
{
   int numRows = static_cast<int>(mResults.size());
   if (numRows == 0)
   {
      numRows = 1;  // no results, but still need to say 1 row so "No Matches found" can be shown for this result
   }
   return numRows;
}

QString ResultsItem::getTargetName() const
{
   return mTargetName;
}

QString ResultsItem::getAlgorithmName() const
{
   return mAlgorithmName;
}

QString ResultsItem::getSignatureName(unsigned int row) const
{
   if (mResults.empty())
   {
      return QString("No Match found");
   }
   if (row < mResults.size())
   {
      return QString::fromStdString(mResults[row].first->getDisplayName(true));
   }

   return QString();
};
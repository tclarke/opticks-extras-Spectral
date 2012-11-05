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

ResultsItem::ResultsItem(const QString& targetName, const QString& algorithmName) :
   mTargetName(targetName),
   mAlgorithmName(algorithmName)
{}

ResultsItem::~ResultsItem()
{}

void ResultsItem::setData(const SpectralLibraryMatch::MatchResults& data,
   const std::map<Signature*, ColorType>& colorMap)
{
   clear();

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

int ResultsItem::getRow(Signature* pSignature) const
{
   if (pSignature == NULL)
   {
      return -1;
   }

   int row = 0;
   for (std::vector<std::pair<Signature*, float> >::const_iterator iter = mResults.begin();
      iter != mResults.end();
      ++iter, ++row)
   {
      if (iter->first == pSignature)
      {
         return row;
      }
   }

   return -1;
}

void ResultsItem::clear()
{
   mResults.clear();
   mIcons.clear();
}

int ResultsItem::rows() const
{
   return static_cast<int>(mResults.size());
}

QString ResultsItem::getTargetName() const
{
   return mTargetName;
}

QString ResultsItem::getAlgorithmName() const
{
   return mAlgorithmName;
}

void ResultsItem::deleteResultsForSignature(Signature* pSignature)
{
   if (pSignature == NULL)
   {
      return;
   }

   for (std::vector<std::pair<Signature*, float> >::iterator iter = mResults.begin(); iter != mResults.end(); ++iter)
   {
      if (iter->first == pSignature)
      {
         mResults.erase(iter);
         break;
      }
   }
}
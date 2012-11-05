/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RESULTSITEM_H
#define RESULTSITEM_H

#include "SpectralLibraryMatch.h"

#include <QtCore/QString>
#include <QtGui/QIcon>

#include <map>
#include <vector>

class ColorType;
class Signature;

class ResultsItem
{
public:
   ResultsItem(const QString& targetName, const QString& algorithmName);
   virtual ~ResultsItem();

   void setData(const SpectralLibraryMatch::MatchResults& data, const std::map<Signature*, ColorType>& colorMap);
   QString getTargetName() const;
   QString getAlgorithmName() const;
   Signature* getSignature(unsigned int row) const;
   QString getValueStr(unsigned int row) const;
   QIcon getIcon(unsigned int row) const;
   int getRow(Signature* pSignature) const;
   int rows() const;
   void clear();
   void deleteResultsForSignature(Signature* pSignature);

private:
   QString mTargetName;
   QString mAlgorithmName;
   std::vector<std::pair<Signature*, float> > mResults;
   std::vector<QIcon> mIcons;
};

#endif

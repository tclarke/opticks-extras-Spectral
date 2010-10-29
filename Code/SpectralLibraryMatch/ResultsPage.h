/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RESULTSPAGE_H
#define RESULTSPAGE_H

#include "SpectralLibraryMatch.h"

#include <map>
#include <string>

#include <QtCore/QMetaType>
#include <QtGui/QTreeWidget>

class ColorType;
class QTreeWidgetItem;
class Signature;

Q_DECLARE_METATYPE(Signature*)

class ResultsPage : public QTreeWidget
{
   Q_OBJECT

public:
   ResultsPage(QWidget* pParent = 0);
   virtual ~ResultsPage();

   void addResults(SpectralLibraryMatch::MatchResults& theResults,
      const std::map<Signature*, ColorType>& colors);

protected:
   QTreeWidgetItem* findResults(const std::string& sigName, SpectralLibraryMatch::MatchAlgorithm algType);
};
#endif

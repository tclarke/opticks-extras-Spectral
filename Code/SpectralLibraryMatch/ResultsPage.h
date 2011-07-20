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
#include <vector>

#include <QtCore/QMetaType>
#include <QtGui/QTreeView>

class ColorType;
class Progress;
class Signature;

class ResultsPage : public QTreeView
{
   Q_OBJECT

public:
   ResultsPage(QWidget* pParent = 0);
   virtual ~ResultsPage();

   void addResults(const std::vector<SpectralLibraryMatch::MatchResults>& theResults,
      const std::map<Signature*, ColorType>& colorMap, Progress* pProgress, bool* pAbort);
   void clear();
   void getSelectedSignatures(std::vector<Signature*>& signatures);
   bool getAutoClear() const;

public slots:
   void setAutoClear(bool enabled);

protected:
   void expandAddedResults(const std::vector<SpectralLibraryMatch::MatchResults>& added);

private:
   bool mAutoClear;
};
#endif

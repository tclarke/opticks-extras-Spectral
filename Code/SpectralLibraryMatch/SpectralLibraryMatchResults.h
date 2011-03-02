/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SPECTRALLIBRARYMATCHRESULTS_H
#define SPECTRALLIBRARYMATCHRESULTS_H

#include "AttachmentPtr.h"
#include "DockWindowShell.h"
#include "SessionExplorer.h"
#include "SpectralLibraryMatch.h"

#include <boost/any.hpp>
#include <map>
#include <string>

class ColorType;
class Progress;
class QAction;
class QTabWidget;
class QWidget;
class RasterElement;
class ResultsPage;
class Signature;
class Subject;

class SpectralLibraryMatchResults : public DockWindowShell
{
   Q_OBJECT

public:
   SpectralLibraryMatchResults();
   virtual ~SpectralLibraryMatchResults();

   void addResults(SpectralLibraryMatch::MatchResults& theResults, Progress* pProgress = 0);
   void addResults(SpectralLibraryMatch::MatchResults& theResults, const std::map<Signature*, ColorType>& colorMap,
      Progress* pProgress = 0);

protected:
   virtual QWidget* createWidget();
   virtual QAction* createAction();
   ResultsPage* createPage(const RasterElement* pRaster);
   ResultsPage* getPage(const RasterElement* pRaster) const;
   void deletePage(const RasterElement* pRaster);
   void elementDeleted(Subject& subject, const std::string& signal, const boost::any& value);
   void elementModified(Subject& subject, const std::string& signal, const boost::any& value);
   std::vector<Signature*> getSelectedSignatures() const;
   const RasterElement* getRasterElementForCurrentPage() const;
   void updateContextMenu(Subject& subject, const std::string& signal, const boost::any& value);

protected slots:
   void clearPage();
   void expandAllPage();
   void collapseAllPage();
   void deletePage();
   void locateSignaturesInScene();
   void createAverageSignature();

private:
   QTabWidget* mpTabWidget;
   AttachmentPtr<SessionExplorer> mpExplorer;
   std::map<const RasterElement*, ResultsPage*> mPageMap;
};

#endif

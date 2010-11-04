/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SPECTRALLIBRARYMATCHTOOLS_H
#define SPECTRALLIBRARYMATCHTOOLS_H

#include "DesktopServices.h"
#include "ExecutableShell.h"
#include "Progress.h"

#include <boost/any.hpp>
#include <string>

#include <QtCore/QObject>

class MouseMode;
class RasterElement;
class SessionItemDeserializer;
class SessionItemSerializer;
class SpatialDataView;
class SpectralLibraryManager;
class SpectralLibraryMatchResults;
class PlugInArgList;

class SpectralLibraryMatchTools : public QObject, public ExecutableShell
{
   Q_OBJECT

public:
   SpectralLibraryMatchTools();
   virtual ~SpectralLibraryMatchTools();

   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
   virtual bool setBatch();

   bool serialize(SessionItemSerializer& serializer) const;
   bool deserialize(SessionItemDeserializer& deserializer);

protected:
   virtual bool eventFilter(QObject* pObject, QEvent* pEvent);
   void windowAdded(Subject& subject, const std::string& signal, const boost::any& value);
   void windowActivated(Subject& subject, const std::string& signal, const boost::any& value);
   void windowRemoved(Subject& subject, const std::string& signal, const boost::any& value);
   void layerActivated(Subject& subject, const std::string& signal, const boost::any& value);
   void updateProgress(const std::string& msg, int percent, ReportingLevel level);

   void addPixelMatchMode(SpatialDataView* pView);
   void removePixelMatchMode(SpatialDataView* pView);
   void enableActions();
   void plotResults(const RasterElement* pRaster, const Signature* pSignature,
      std::vector<std::pair<Signature*, float> > matches);

   void initializeConnections();

protected slots:
   void matchAoiPixels();
   void matchAoiAverageSpectrum();

private:
   Service<DesktopServices> mpDesktop;
   Progress* mpProgress;

   SpectralLibraryMatchResults* mpResults;
   PlugIn* mpSignatureWindow;
   SpectralLibraryManager* mpLibMgr;

   MouseMode* mpSpectralLibraryMatchMode;
   QAction* mpPixelMatchAction;
   QAction* mpAoiPixelMatchAction;
   QAction* mpAoiAverageMatchAction;
};
#endif

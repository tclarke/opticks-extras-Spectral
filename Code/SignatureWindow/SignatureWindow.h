/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SIGNATUREWINDOW_H
#define SIGNATUREWINDOW_H

#include <QtGui/QAction>

#include "AlgorithmShell.h"
#include "AttachmentPtr.h"
#include "DesktopServices.h"
#include "Progress.h"
#include "SessionExplorer.h"
#include "Wavelengths.h"
#include "Window.h"

#include <boost/any.hpp>
#include <string>
#include <vector>

class MouseMode;
class PlotWidget;
class RasterLayer;
class SessionItemDeserializer;
class SessionItemSerializer;
class Signature;
class SignaturePlotObject;
class SpatialDataView;
class SpatialDataWindow;

class SignatureWindow : public QObject, public AlgorithmShell, public Window::SessionItemDropFilter
{
   Q_OBJECT

public:
   SignatureWindow();
   ~SignatureWindow();

   bool setBatch();
   bool getInputSpecification(PlugInArgList*& pArgList);
   bool getOutputSpecification(PlugInArgList*& pArgList);
   bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
   bool abort();

   bool accept(SessionItem* pItem) const;
   bool serialize(SessionItemSerializer& serializer) const;
   bool deserialize(SessionItemDeserializer& deserializer);

protected:
   bool eventFilter(QObject* pObject, QEvent* pEvent);
   void dropSessionItem(Subject& subject, const std::string& signal, const boost::any& value);
   void updateContextMenu(Subject& subject, const std::string& signal, const boost::any& value);
   void windowAdded(Subject& subject, const std::string& signal, const boost::any& value);
   void windowActivated(Subject& subject, const std::string& signal, const boost::any& value);
   void windowRemoved(Subject& subject, const std::string& signal, const boost::any& value);
   void layerActivated(Subject& subject, const std::string& signal, const boost::any& value);
   void plotWindowShown(Subject& subject, const std::string& signal, const boost::any& value);
   void plotWindowHidden(Subject& subject, const std::string& signal, const boost::any& value);
   void plotSetAdded(Subject& subject, const std::string& signal, const boost::any& value);
   void plotWidgetAdded(Subject& subject, const std::string& signal, const boost::any& value);
   void plotWidgetDeleted(Subject& subject, const std::string& signal, const boost::any& value);
   void sessionRestored(Subject& subject, const std::string& signal, const boost::any& value);
   void updateProgress(const std::string& msg, int percent, ReportingLevel level);

   void addPixelSignatureMode(SpatialDataWindow* pWindow);
   void removePixelSignatureMode(SpatialDataWindow* pWindow);
   void enableActions();

   SignaturePlotObject* getSignaturePlot(const PlotWidget* pPlot) const;
   SignaturePlotObject* getSignaturePlot(const std::string& plotName);
   SignaturePlotObject* getSignaturePlotForAverage() const;
   bool setCurrentPlotSet(const std::string& plotsetName);
   std::string getPlotSetName() const;

   friend class PropertiesSignaturePlotObject;

protected slots:
   void displaySignatureWindow(bool bDisplay);
   void addDefaultPlot();
   void renameCurrentPlot();
   void deleteCurrentPlot();
   void displayAoiSignatures();
   void displayAoiAverageSig();
   void pinSignatureWindow(bool enable);

private:
   struct SignaturePlotObjectInitializer
   {
      SignaturePlotObjectInitializer() :
         mpPlotWidget(NULL),
         mWavelengthUnits(Wavelengths::MICRONS),
         mBandsDisplayed(false),
         mClearOnAdd(false),
         mRescaleOnAdd(true),
         mpRasterLayer(NULL),
         mRegionsDisplayed(false),
         mRegionColor(Qt::red),
         mRegionOpacity(35)
      {
      }

      PlotWidget* mpPlotWidget;
      std::vector<Signature*> mSignatures;
      Wavelengths::WavelengthUnitsType mWavelengthUnits;
      bool mBandsDisplayed;
      bool mClearOnAdd;
      bool mRescaleOnAdd;
      RasterLayer* mpRasterLayer;
      bool mRegionsDisplayed;
      QColor mRegionColor;
      int mRegionOpacity;
   };

   Service<DesktopServices> mpDesktop;
   AttachmentPtr<SessionExplorer> mpExplorer;
   Progress* mpProgress;
   std::string mSignatureWindowName;
   std::string mDefaultPlotSetName;

   QAction* mpWindowAction;
   QAction* mpPinSigPlotAction;
   MouseMode* mpPixelSignatureMode;
   QAction* mpPixelSignatureAction;
   QAction* mpAoiSignaturesAction;
   QAction* mpAoiAverageSigAction;
   bool mbNotifySigPlotObjectsOfAbort;  // used to prevent passing abort to plot when progress passed to SpectralUtilities

   std::vector<SignaturePlotObject*> mPlots;
   std::vector<SignaturePlotObjectInitializer> mSessionPlots;
};

#endif

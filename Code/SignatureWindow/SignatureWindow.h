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

#include "AttachmentPtr.h"
#include "DesktopServices.h"
#include "DockWindowShell.h"
#include "Progress.h"
#include "SessionExplorer.h"
#include "TypesFile.h"
#include "Window.h"

#include <boost/any.hpp>
#include <string>
#include <vector>

class ColorType;
class MouseMode;
class PlotWidget;
class RasterLayer;
class RasterElement;
class SessionItemDeserializer;
class SessionItemSerializer;
class Signature;
class SignaturePlotObject;
class SpatialDataWindow;

class SignatureWindow : public DockWindowShell, public Window::SessionItemDropFilter
{
   Q_OBJECT

public:
   SignatureWindow();
   virtual ~SignatureWindow();

   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
   virtual bool abort();

   virtual bool accept(SessionItem* pItem) const;
   virtual bool serialize(SessionItemSerializer& serializer) const;
   virtual bool deserialize(SessionItemDeserializer& deserializer);

protected:
   virtual QAction* createAction();
   virtual QWidget* createWidget();

   void addPlot(const RasterElement* pRaster, Signature* pSignature, const ColorType& color, bool clearBeforeAdd = false);
   bool eventFilter(QObject* pObject, QEvent* pEvent);
   void dropSessionItem(Subject& subject, const std::string& signal, const boost::any& value);
   void updateContextMenu(Subject& subject, const std::string& signal, const boost::any& value);
   void windowAdded(Subject& subject, const std::string& signal, const boost::any& value);
   void windowActivated(Subject& subject, const std::string& signal, const boost::any& value);
   void windowRemoved(Subject& subject, const std::string& signal, const boost::any& value);
   void layerActivated(Subject& subject, const std::string& signal, const boost::any& value);
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
         mWavelengthUnits(MICRONS),
         mBandsDisplayed(false),
         mClearOnAdd(false),
         mRescaleOnAdd(true),
         mpRasterLayer(NULL),
         mRegionsDisplayed(false),
         mRegionColor(Qt::red),
         mRegionOpacity(35)
      {}

      PlotWidget* mpPlotWidget;
      std::vector<Signature*> mSignatures;
      WavelengthUnitsType mWavelengthUnits;
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

   QAction* mpPinSigPlotAction;
   MouseMode* mpPixelSignatureMode;
   QAction* mpPixelSignatureAction;
   QAction* mpAoiSignaturesAction;
   QAction* mpAoiAverageSigAction;
   bool mbNotifySigPlotObjectsOfAbort;  // used to prevent passing abort to plot when progress passed to SpectralUtilities
   bool mbFirstTime;                    // used to identify first time plug-in is executed

   std::vector<SignaturePlotObject*> mPlots;
   std::vector<SignaturePlotObjectInitializer> mSessionPlots;
};

#endif

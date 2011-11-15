/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtGui/QBitmap>
#include <QtGui/QColorDialog>
#include <QtGui/QInputDialog>
#include <QtGui/QMenu>
#include <QtGui/QMessageBox>
#include <QtGui/QMouseEvent>

#include "AppAssert.h"
#include "AppConfig.h"
#include "AppVerify.h"
#include "Axis.h"
#include "Classification.h"
#include "ContextMenu.h"
#include "ContextMenuActions.h"
#include "Curve.h"
#include "CurveCollection.h"
#include "DataDescriptor.h"
#include "DataVariant.h"
#include "DesktopServices.h"
#include "Locator.h"
#include "ModelServices.h"
#include "MouseMode.h"
#include "ObjectResource.h"
#include "PlotView.h"
#include "PlotWidget.h"
#include "PlugInArgList.h"
#include "PlugInResource.h"
#include "PropertiesSignaturePlotObject.h"
#include "RegionObject.h"
#include "RasterElement.h"
#include "RasterDataDescriptor.h"
#include "RasterFileDescriptor.h"
#include "SessionExplorer.h"
#include "Signature.h"
#include "SignaturePlotObject.h"
#include "SignatureSelector.h"
#include "SignatureSet.h"
#include "SignatureWindowIcons.h"
#include "SignatureWindowOptions.h"
#include "Slot.h"
#include "SpecialMetadata.h"
#include "SpectralContextMenuActions.h"
#include "SpectralUtilities.h"
#include "TypeConverter.h"
#include "Units.h"
#include "Wavelengths.h"

#include <math.h>

#include <algorithm>
#include <limits>
using namespace std;

SignaturePlotObject::SignaturePlotObject(PlotWidget* pPlotWidget, Progress* pProgress, QObject* pParent) :
   QObject(pParent),
   mpExplorer(Service<SessionExplorer>().get(), SIGNAL_NAME(SessionExplorer, AboutToShowSessionItemContextMenu),
      Slot(this, &SignaturePlotObject::updateContextMenu)),
   mpPlotWidget(pPlotWidget),
   mpProgress(pProgress),
   mbAbort(false),
   mpSigSelector(NULL),
   mWaveUnits(MICRONS),
   meActiveBandColor(),
   mbClearOnAdd(false),
   mpRasterLayer(NULL),
   mpGrayscaleBandCollection(NULL),
   mpRgbBandCollection(NULL),
   mDisplayRegions(false),
   mRegionColor(Qt::red),
   mRegionOpacity(35),
   mpFirstSignature(NULL),
   mMinValue(0.0),
   mRange(0.0),
   mpSignatureUnitsMenu(NULL),
   mpWavelengthAction(NULL),
   mpBandDisplayAction(NULL),
   mpWaveUnitsMenu(NULL),
   mpMicronsAction(NULL),
   mpNanometersAction(NULL),
   mpCentimetersAction(NULL),
   mpDisplayModeMenu(NULL),
   mpGrayscaleAction(NULL),
   mpRgbAction(NULL),
   mpRescaleOnAdd(NULL),
   mpScaleToFirst(NULL)
{
   string shortcutContext = "Signature Window/Signature Plot";

   Service<DesktopServices> pDesktop;

   // Actions
   QActionGroup* pXAxisUnitsGroup = new QActionGroup(this);
   pXAxisUnitsGroup->setExclusive(true);
   VERIFYNR(connect(pXAxisUnitsGroup, SIGNAL(triggered(QAction*)), this, SLOT(displayBandNumbers())));

   string xAxisContext = shortcutContext + string("/X-Axis Values");

   mpBandDisplayAction = pXAxisUnitsGroup->addAction("Band &Numbers");
   mpBandDisplayAction->setAutoRepeat(false);
   mpBandDisplayAction->setCheckable(true);
   mpBandDisplayAction->setToolTip("Band Numbers");
   mpBandDisplayAction->setStatusTip("Displays the signature according to the spectral band numbers");
   pDesktop->initializeAction(mpBandDisplayAction, xAxisContext);

   mpWavelengthAction = pXAxisUnitsGroup->addAction("&Wavelengths");
   mpWavelengthAction->setAutoRepeat(false);
   mpWavelengthAction->setCheckable(true);
   mpWavelengthAction->setToolTip("Wavelengths");
   mpWavelengthAction->setStatusTip("Displays the signature according to its wavelengths");
   pDesktop->initializeAction(mpWavelengthAction, xAxisContext);

   QActionGroup* pWaveUnitsGroup = new QActionGroup(this);
   pWaveUnitsGroup->setExclusive(true);
   VERIFYNR(connect(pWaveUnitsGroup, SIGNAL(triggered(QAction*)), this, SLOT(setWavelengthUnits(QAction*))));

   string wavelengthsContext = xAxisContext + string("/Wavelengths");

   mpMicronsAction = pWaveUnitsGroup->addAction("&Microns");
   mpMicronsAction->setAutoRepeat(false);
   mpMicronsAction->setCheckable(true);
   mpMicronsAction->setToolTip("Microns");
   mpMicronsAction->setStatusTip("Displays the wavelength values in microns");
   pDesktop->initializeAction(mpMicronsAction, wavelengthsContext);

   mpNanometersAction = pWaveUnitsGroup->addAction("&Nanometers");
   mpNanometersAction->setAutoRepeat(false);
   mpNanometersAction->setCheckable(true);
   mpNanometersAction->setToolTip("Nanometers");
   mpNanometersAction->setStatusTip("Displays the wavelength values in nanometers");
   pDesktop->initializeAction(mpNanometersAction, wavelengthsContext);

   mpCentimetersAction = pWaveUnitsGroup->addAction("Inverse &Centimeters");
   mpCentimetersAction->setAutoRepeat(false);
   mpCentimetersAction->setCheckable(true);
   mpCentimetersAction->setToolTip("Inverse Centimeters");
   mpCentimetersAction->setStatusTip("Displays the wavelength values in inverse centimeters");
   pDesktop->initializeAction(mpCentimetersAction, wavelengthsContext);

   QActionGroup* pDisplayModeGroup = new QActionGroup(this);
   pWaveUnitsGroup->setExclusive(true);
   VERIFYNR(connect(pDisplayModeGroup, SIGNAL(triggered(QAction*)), this, SLOT(setDisplayMode(QAction*))));

   string displayModeContext = shortcutContext + string("/Display Mode");

   mpGrayscaleAction = pDisplayModeGroup->addAction("Gra&yscale");
   mpGrayscaleAction->setAutoRepeat(false);
   mpGrayscaleAction->setCheckable(true);
   mpGrayscaleAction->setToolTip("Grayscale");
   mpGrayscaleAction->setStatusTip("Sets the display mode to Grayscale for the current data set");
   pDesktop->initializeAction(mpGrayscaleAction, displayModeContext);

   mpRgbAction = pDisplayModeGroup->addAction("RG&B");
   mpRgbAction->setAutoRepeat(false);
   mpRgbAction->setCheckable(true);
   mpRgbAction->setToolTip("RGB");
   mpRgbAction->setStatusTip("Sets the display mode to RGB for the current data set");
   pDesktop->initializeAction(mpRgbAction, displayModeContext);

   // X-axis menu
   REQUIRE (mpPlotWidget != NULL);

   PlotView* pPlotView = mpPlotWidget->getPlot();
   REQUIRE(pPlotView != NULL);

   QWidget* pWidget = pPlotView->getWidget();
   REQUIRE(pWidget != NULL);

   mpSignatureUnitsMenu = new QMenu("Signature &Units", pWidget);
   mpSignatureUnitsMenu->addAction(mpBandDisplayAction);
   mpSignatureUnitsMenu->addAction(mpWavelengthAction);

   // Wavelength units menu
   mpWaveUnitsMenu = new QMenu("Wavelength &Units", pWidget);
   mpWaveUnitsMenu->addAction(mpMicronsAction);
   mpWaveUnitsMenu->addAction(mpNanometersAction);
   mpWaveUnitsMenu->addAction(mpCentimetersAction);

   // Display mode menu
   mpDisplayModeMenu = new QMenu("Display &Mode", pWidget);
   mpDisplayModeMenu->addAction(mpGrayscaleAction);
   mpDisplayModeMenu->addAction(mpRgbAction);

   // Plot
   mpRescaleOnAdd = new QAction("Rescale on addition", this);
   mpRescaleOnAdd->setAutoRepeat(false);
   mpRescaleOnAdd->setCheckable(true);
   mpRescaleOnAdd->setChecked(SignatureWindowOptions::getSettingRescaleOnAdd());
   mpRescaleOnAdd->setToolTip("Check to enable rescaling plot when a signature is added");
   mpRescaleOnAdd->setStatusTip("Enable/disable rescaling the plot when adding new signatures.");

   mpScaleToFirst = new QAction("Scale signatures to first signature", this);
   mpScaleToFirst->setAutoRepeat(false);
   mpScaleToFirst->setCheckable(true);
   mpScaleToFirst->setChecked(SignatureWindowOptions::getSettingScaleToFirstSignature());
   mpScaleToFirst->setToolTip("Check to enable scaling signatures in plot\nto the first signature that was added");
   mpScaleToFirst->setStatusTip("Enable/disable scaling signatures to the first signature.");
   VERIFYNR(connect(mpScaleToFirst, SIGNAL(toggled(bool)), this, SLOT(updatePlotForScaleToFirst(bool))));

   pPlotView->attach(SIGNAL_NAME(Subject, Modified), Slot(this, &SignaturePlotObject::plotModified));
   pWidget->installEventFilter(this);

   Locator* pLocator = pPlotView->getMouseLocator();
   if (pLocator != NULL)
   {
      pLocator->setStyle(Locator::VERTICAL_LOCATOR);
   }

   // Axes
   setXAxisTitle();
   setYAxisTitle();

   // Initialization
   enableBandCharacteristics(false);

   MouseMode* pSelectionMode = pPlotView->getMouseMode("SelectionMode");
   if (pSelectionMode != NULL)
   {
      QAction* pSelectionAction = pSelectionMode->getAction();
      if (pSelectionAction != NULL)
      {
         pSelectionAction->setText("Signature Selection");
      }
   }

   MouseMode* pLocatorMode = pPlotView->getMouseMode("LocatorMode");
   if (pLocatorMode != NULL)
   {
      QAction* pLocatorAction = pLocatorMode->getAction();
      if (pLocatorAction != NULL)
      {
         pLocatorAction->setText("Band Selection");
      }
   }

   // Connections
   mpPlotWidget->attach(SIGNAL_NAME(PlotWidget, AboutToShowContextMenu),
      Slot(this, &SignaturePlotObject::updateContextMenu));
   pDesktop->attach(SIGNAL_NAME(DesktopServices, AboutToShowPropertiesDialog),
      Slot(this, &SignaturePlotObject::updatePropertiesDialog));
}

SignaturePlotObject::~SignaturePlotObject()
{
   Service<DesktopServices> pDesktop;
   pDesktop->detach(SIGNAL_NAME(DesktopServices, AboutToShowPropertiesDialog),
      Slot(this, &SignaturePlotObject::updatePropertiesDialog));

   if (mpPlotWidget != NULL)
   {
      PlotView* pPlotView = mpPlotWidget->getPlot();
      if (pPlotView != NULL)
      {
         pPlotView->detach(SIGNAL_NAME(Subject, Modified), Slot(this, &SignaturePlotObject::plotModified));
      }

      mpPlotWidget->detach(SIGNAL_NAME(PlotWidget, AboutToShowContextMenu),
         Slot(this, &SignaturePlotObject::updateContextMenu));
   }

   clearSignatures();
   setRasterLayer(NULL);
}

void SignaturePlotObject::initializeFromPlot(const vector<Signature*>& signatures)
{
   mSignatures.clear();

   if (mpPlotWidget == NULL)
   {
      return;
   }

   PlotView* pPlotView = mpPlotWidget->getPlot();
   if (pPlotView == NULL)
   {
      return;
   }

   list<PlotObject*> plotObjects;
   pPlotView->getObjects(CURVE_COLLECTION, plotObjects);

   for (list<PlotObject*>::iterator iter = plotObjects.begin(); iter != plotObjects.end(); ++iter)
   {
      CurveCollection* pCollection = dynamic_cast<CurveCollection*>(*iter);
      if (pCollection != NULL)
      {
         string objectName;
         pCollection->getObjectName(objectName);
         if (objectName == "Grayscale Band")
         {
            mpGrayscaleBandCollection = pCollection;
         }
         else if (objectName == "RGB Bands")
         {
            mpRgbBandCollection = pCollection;
         }
         else
         {
            vector<Signature*>::const_iterator sigIter;
            for (sigIter = signatures.begin(); sigIter != signatures.end(); ++sigIter)
            {
               Signature* pSignature = *sigIter;
               if (pSignature != NULL)
               {
                  string sigName = pSignature->getName();
                  if (objectName == sigName)
                  {
                     if (mSpectralUnits.empty() == true)
                     {
                        const Units* pUnits = pSignature->getUnits("Reflectance");
                        if (pUnits != NULL)
                        {
                           const string& unitsName = pUnits->getUnitName();
                           if (unitsName.empty() == false)
                           {
                              mSpectralUnits = unitsName;
                              setYAxisTitle();
                           }
                        }
                     }

                     pSignature->attach(SIGNAL_NAME(Subject, Deleted),
                        Slot(this, &SignaturePlotObject::signatureDeleted));
                     pSignature->attach(SIGNAL_NAME(Subject, Modified),
                        Slot(this, &SignaturePlotObject::signatureModified));

                     mSignatures.insert(pSignature, pCollection);
                     break;
                  }
               }
            }
         }
      }
   }
}

PlotWidget* SignaturePlotObject::getPlotWidget() const
{
   return mpPlotWidget;
}

void SignaturePlotObject::attached(Subject& subject, const string& signal, const Slot& slot)
{
   if (dynamic_cast<Signature*>(&subject) != NULL)
   {
      signatureAttached(subject, signal, boost::any());
   }
}

void SignaturePlotObject::detached(Subject& subject, const string& signal, const Slot& slot)
{
   if (dynamic_cast<Signature*>(&subject) != NULL)
   {
      signatureDeleted(subject, signal, boost::any());
   }
}

void SignaturePlotObject::displayModeChanged(Subject& subject, const string& signal, const boost::any& value)
{
   if (&subject == mpRasterLayer.get())
   {
      updateDisplayMode();
   }
}

void SignaturePlotObject::displayedBandChanged(Subject& subject, const string& signal, const boost::any& value)
{
   if (&subject == mpRasterLayer.get())
   {
      updateDisplayedBands();
   }
}

void SignaturePlotObject::plotModified(Subject& subject, const string& signal, const boost::any& value)
{
   PlotView* pPlotView = dynamic_cast<PlotView*>(&subject);
   if (pPlotView != NULL)
   {
      updateBandCharacteristicsFromPlot();
   }
}

void SignaturePlotObject::signatureDeleted(Subject& subject, const string& signal, const boost::any& value)
{
   Signature* pSignature = dynamic_cast<Signature*>(&subject);
   if (NN(pSignature))
   {
      // Remove the signature from the plot
      removeSignature(pSignature, false);
   }
}

void SignaturePlotObject::signatureAttached(Subject& subject, const string& signal, const boost::any& value)
{
   Signature* pSignature = dynamic_cast<Signature*>(&subject);
   if (NN(pSignature))
   {
      // Update the plot classification for a classification change
      const Classification* pSignatureClass = pSignature->getClassification();
      if ((pSignatureClass != NULL) && (mpClassification.get() != NULL))
      {
         if (pSignatureClass->hasGreaterLevel(mpClassification.get()) == true)
         {
            mpClassification->setClassification(pSignatureClass);

            if (mpPlotWidget != NULL)
            {
               mpPlotWidget->setClassification(mpClassification.get());
            }
         }
      }
   }
}

void SignaturePlotObject::signatureModified(Subject& subject, const string& signal, const boost::any& value)
{
   Signature* pSignature = dynamic_cast<Signature*>(&subject);
   if (NN(pSignature))
   {
      // Remove the signature if the units are invalid
      const Units* pUnits = pSignature->getUnits("Reflectance");
      if (pUnits != NULL)
      {
         const string& unitName = pUnits->getUnitName();
         if (unitName != mSpectralUnits)
         {
            removeSignature(pSignature, false);
            return;
         }
      }

      // Update the plot object name for a name change
      CurveCollection* pCollection = NULL;

      QMap<Signature*, CurveCollection*>::Iterator iter = mSignatures.find(pSignature);
      if (iter != mSignatures.end())
      {
         string signatureName = pSignature->getDisplayName();
         if (signatureName.empty())
         {
            signatureName = pSignature->getName();
         }
         pCollection = iter.value();
         if (pCollection != NULL)
         {
            pCollection->setObjectName(signatureName);
         }
      }

      // Update the curve for a values change
      if (pCollection != NULL)
      {
         setSignaturePlotValues(pCollection, pSignature);
      }

      // Update the plot classification for a classification change
      const Classification* pSignatureClass = pSignature->getClassification();
      if ((pSignatureClass != NULL) && (mpClassification.get() != NULL))
      {
         if (pSignatureClass->hasGreaterLevel(mpClassification.get()) == true)
         {
            mpClassification->setClassification(pSignatureClass);

            if (mpPlotWidget != NULL)
            {
               mpPlotWidget->setClassification(mpClassification.get());
            }
         }
      }
   }
}

void SignaturePlotObject::updateContextMenu(Subject& subject, const string& signal, const boost::any& value)
{
   ContextMenu* pMenu = boost::any_cast<ContextMenu*>(value);
   if (pMenu == NULL)
   {
      return;
   }

   if (dynamic_cast<SessionExplorer*>(&subject) != NULL)
   {
      // Make sure there is only one selected item in the session explorer
      vector<SessionItem*> items = pMenu->getSessionItems();
      if (items.size() != 1)
      {
         return;
      }

      // Make sure the selected item is the plot widget for this object
      if (dynamic_cast<PlotWidget*>(items.front()) != mpPlotWidget)
      {
         return;
      }
   }
   else
   {
      // When the context menu is invoked for the plot widget, the session items for the menu should include one
      // plot widget and one plot view, so make sure that the plot widget is the plot widget for this object
      vector<PlotWidget*> plotWidgets = pMenu->getSessionItems<PlotWidget>();
      if ((plotWidgets.size() != 1) || (plotWidgets.front() != mpPlotWidget))
      {
         return;
      }
   }

   QObject* pParent = pMenu->getActionParent();

   // Add signature
   QPixmap pixOpenSig(SignatureWindowIcons::OpenSignatureIcon);
   QBitmap bmpOpenSig(SignatureWindowIcons::OpenSignatureMask);
   pixOpenSig.setMask(bmpOpenSig);

   QAction* pAddSignatureAction = new QAction(QIcon(pixOpenSig), "&Add Signature...", pParent);
   pAddSignatureAction->setAutoRepeat(false);
   pAddSignatureAction->setShortcut(QKeySequence("Ctrl+A"));
   VERIFYNR(connect(pAddSignatureAction, SIGNAL(triggered()), this, SLOT(addSignature())));
   pMenu->addActionBefore(pAddSignatureAction, SPECTRAL_SIGNATUREPLOT_ADD_SIG_ACTION, APP_PLOTWIDGET_PRINT_ACTION);

   // Save selected signature(s)
   QPixmap pixSaveSig(SignatureWindowIcons::SaveSignatureIcon);
   QBitmap bmpSaveSig(SignatureWindowIcons::SaveSignatureMask);
   pixSaveSig.setMask(bmpSaveSig);

   QAction* pSaveSignatureAction = new QAction(QIcon(pixSaveSig), "&Save Signatures...", pParent);
   pSaveSignatureAction->setAutoRepeat(false);
   pSaveSignatureAction->setShortcut(QKeySequence("Ctrl+S"));
   VERIFYNR(connect(pSaveSignatureAction, SIGNAL(triggered()), this, SLOT(saveSignatures())));
   pMenu->addActionAfter(pSaveSignatureAction, SPECTRAL_SIGNATUREPLOT_SAVE_SIG_ACTION,
      SPECTRAL_SIGNATUREPLOT_ADD_SIG_ACTION);

   // Save as signature library
   QAction* pSaveLibraryAction = new QAction("Save &As Library...", pParent);
   pSaveLibraryAction->setAutoRepeat(false);
   VERIFYNR(connect(pSaveLibraryAction, SIGNAL(triggered()), this, SLOT(saveSignatureLibrary())));
   pMenu->addActionAfter(pSaveLibraryAction, SPECTRAL_SIGNATUREPLOT_SAVE_LIBRARY_ACTION,
      SPECTRAL_SIGNATUREPLOT_SAVE_SIG_ACTION);

   // Select all
   QAction* pSelectAllAction = new QAction("Sel&ect All", pParent);
   pSelectAllAction->setAutoRepeat(false);
   VERIFYNR(connect(pSelectAllAction, SIGNAL(triggered()), this, SLOT(selectAllSignatures())));
   pMenu->addActionBefore(pSelectAllAction, SPECTRAL_SIGNATUREPLOT_SELECT_ALL_ACTION,
      APP_CARTESIANPLOT_CUSTOM_ZOOM_ACTION);

   // Deselect all
   QAction* pDeselectAllAction = new QAction("&Deselect All", pParent);
   pDeselectAllAction->setAutoRepeat(false);
   VERIFYNR(connect(pDeselectAllAction, SIGNAL(triggered()), this, SLOT(deselectAllSignatures())));
   pMenu->addActionAfter(pDeselectAllAction, SPECTRAL_SIGNATUREPLOT_DESELECT_ALL_ACTION,
      SPECTRAL_SIGNATUREPLOT_SELECT_ALL_ACTION);

   // Delete selected
   QAction* pDeleteSelectedAction = new QAction("Dele&te Selected", pParent);
   pDeleteSelectedAction->setAutoRepeat(false);
   VERIFYNR(connect(pDeleteSelectedAction, SIGNAL(triggered()), this, SLOT(removeSelectedSignatures())));
   pMenu->addActionAfter(pDeleteSelectedAction, SPECTRAL_SIGNATUREPLOT_DELETE_SELECTED_ACTION,
      SPECTRAL_SIGNATUREPLOT_DESELECT_ALL_ACTION);

   // Change color
   QAction* pChangeColorAction = new QAction("C&hange Color...", pParent);
   pChangeColorAction->setAutoRepeat(false);
   VERIFYNR(connect(pChangeColorAction, SIGNAL(triggered()), this, SLOT(changeSignaturesColor())));
   pMenu->addActionAfter(pChangeColorAction, SPECTRAL_SIGNATUREPLOT_CHANGE_COLOR_ACTION,
      SPECTRAL_SIGNATUREPLOT_DELETE_SELECTED_ACTION);

   // Clear
   QAction* pClearAction = new QAction("&Clear", pParent);
   pClearAction->setAutoRepeat(false);
   VERIFYNR(connect(pClearAction, SIGNAL(triggered()), this, SLOT(clearSignatures())));
   pMenu->addActionAfter(pClearAction, SPECTRAL_SIGNATUREPLOT_CLEAR_ACTION,
      SPECTRAL_SIGNATUREPLOT_CHANGE_COLOR_ACTION);

   // Separator
   QAction* pSeparatorAction = new QAction(pParent);
   pSeparatorAction->setSeparator(true);
   pMenu->addActionAfter(pSeparatorAction, SPECTRAL_SIGNATUREPLOT_SEPARATOR_ACTION,
      SPECTRAL_SIGNATUREPLOT_CLEAR_ACTION);

   // rescale on addition
   pMenu->addActionBefore(mpRescaleOnAdd, SPECTRAL_SIGNATUREPLOT_RESCALE_ON_ADD_ACTION,
      APP_PLOTVIEW_RESCALE_AXES_ACTION);

   // scale to first signature
   pMenu->addActionBefore(mpScaleToFirst, SPECTRAL_SIGNATUREPLOT_SCALE_TO_FIRST_ACTION,
      SPECTRAL_SIGNATUREPLOT_RESCALE_ON_ADD_ACTION);

   // Signature units
   pMenu->addActionBefore(mpSignatureUnitsMenu->menuAction(), SPECTRAL_SIGNATUREPLOT_SIG_UNITS_ACTION,
      APP_APPLICATIONWINDOW_EXPORT_ACTION);

   // Wavelength units
   pMenu->addActionAfter(mpWaveUnitsMenu->menuAction(), SPECTRAL_SIGNATUREPLOT_WAVE_UNITS_ACTION,
      SPECTRAL_SIGNATUREPLOT_SIG_UNITS_ACTION);

   // Separator
   QAction* pSeparator2Action = new QAction(pParent);
   pSeparator2Action->setSeparator(true);
   pMenu->addActionAfter(pSeparator2Action, SPECTRAL_SIGNATUREPLOT_SEPARATOR2_ACTION,
      SPECTRAL_SIGNATUREPLOT_WAVE_UNITS_ACTION);

   // Display mode
   pMenu->addActionAfter(mpDisplayModeMenu->menuAction(), SPECTRAL_SIGNATUREPLOT_DISPLAY_MODE_ACTION,
      SPECTRAL_SIGNATUREPLOT_SEPARATOR2_ACTION);

   // Separator
   QAction* pSeparator3Action = new QAction(pParent);
   pSeparator3Action->setSeparator(true);
   pMenu->addActionAfter(pSeparator3Action, SPECTRAL_SIGNATUREPLOT_SEPARATOR3_ACTION,
      SPECTRAL_SIGNATUREPLOT_DISPLAY_MODE_ACTION);
}

void SignaturePlotObject::updatePropertiesDialog(Subject& subject, const string& signal, const boost::any& value)
{
   if (mpPlotWidget == NULL)
   {
      return;
   }

   pair<SessionItem*, vector<string>*> properties = boost::any_cast<pair<SessionItem*, vector<string>*> >(value);

   SessionItem* pItem = properties.first;
   if ((dynamic_cast<PlotWidget*>(pItem) == mpPlotWidget) ||
      (dynamic_cast<PlotView*>(pItem) == mpPlotWidget->getPlot()))
   {
      vector<string>* pPlugInNames = properties.second;
      if (pPlugInNames != NULL)
      {
         pPlugInNames->push_back(PropertiesSignaturePlotObject::getName());
      }
   }
}

bool SignaturePlotObject::eventFilter(QObject* pObject, QEvent* pEvent)
{
   PlotView* pPlotView = NULL;
   QWidget* pViewWidget = NULL;

   if (mpPlotWidget != NULL)
   {
      pPlotView = mpPlotWidget->getPlot();
      if (pPlotView != NULL)
      {
         pViewWidget = pPlotView->getWidget();
      }
   }

   string modeName = "";
   if (pPlotView != NULL)
   {
      const MouseMode* pMouseMode = pPlotView->getCurrentMouseMode();
      if (pMouseMode != NULL)
      {
         pMouseMode->getName(modeName);
      }
   }

   if (pEvent != NULL)
   {
      if (pEvent->type() == QEvent::MouseButtonPress)
      {
         QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(pEvent);

         if (pMouseEvent->button() == Qt::LeftButton)
         {
            if ((pObject == pViewWidget) && (pPlotView != NULL))
            {
               if (modeName == "LocatorMode")
               {
                  QPoint ptMouse = pMouseEvent->pos();
                  if (pViewWidget != NULL)
                  {
                     ptMouse.setY(pViewWidget->height() - pMouseEvent->pos().y());
                  }

                  LocationType llCorner, ulCorner, urCorner, lrCorner;
                  pPlotView->getVisibleCorners(llCorner, ulCorner, urCorner, lrCorner);

                  double minY = llCorner.mY;
                  double maxY = ulCorner.mY;

                  double dataX = 0.0;
                  double dataY = 0.0;
                  pPlotView->translateScreenToData(ptMouse.x(), ptMouse.y(), dataX, dataY);

                  QColor clrActive;
                  meActiveBandColor = RasterChannelType();
                  if ((mpGrayscaleAction->isChecked() == true) && (dataY > minY) && (dataY < maxY))
                  {
                     meActiveBandColor = GRAY;
                     clrActive = Qt::darkGray;
                  }
                  else
                  {
                     double dYRedGreen = maxY - ((maxY - minY) / 3);
                     double dYGreenBlue = minY + ((maxY - minY) / 3);

                     if ((dataY <= maxY) && (dataY > dYRedGreen))
                     {
                        meActiveBandColor = RED;
                        clrActive = Qt::red;
                     }
                     else if ((dataY <= dYRedGreen) && (dataY > dYGreenBlue))
                     {
                        meActiveBandColor = GREEN;
                        clrActive = Qt::green;
                     }
                     else if ((dataY <= dYGreenBlue) && (dataY >= minY))
                     {
                        meActiveBandColor = BLUE;
                        clrActive = Qt::blue;
                     }
                     else
                     {
                        clrActive = Qt::black;
                     }
                  }

                  ColorType activeColor;
                  if (clrActive.isValid() == true)
                  {
                     activeColor.mRed = clrActive.red();
                     activeColor.mGreen = clrActive.green();
                     activeColor.mBlue = clrActive.blue();
                  }

                  Locator* pLocator = pPlotView->getMouseLocator();
                  if (pLocator != NULL)
                  {
                     LocationType locatorPoint = getClosestActiveBandLocation(ptMouse);
                     pLocator->setVisible(true);
                     pLocator->setLocation(locatorPoint);
                     pLocator->setColor(activeColor);
                     pLocator->setLineStyle(DASHED);
                  }

                  refresh();
                  return true;
               }
            }
         }
      }
      else if (pEvent->type() == QEvent::MouseMove)
      {
         QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(pEvent);

         if (pMouseEvent->buttons() == Qt::LeftButton)
         {
            if ((pObject == pViewWidget) && (pPlotView != NULL))
            {
               if (modeName == "LocatorMode")
               {
                  QPoint ptMouse = pMouseEvent->pos();
                  if (pViewWidget != NULL)
                  {
                     ptMouse.setY(pViewWidget->height() - pMouseEvent->pos().y());
                  }

                  Locator* pLocator = pPlotView->getMouseLocator();
                  if (pLocator != NULL)
                  {
                     LocationType locatorPoint = getClosestActiveBandLocation(ptMouse);
                     pLocator->setLocation(locatorPoint);
                  }

                  refresh();
                  return true;
               }
            }
         }
      }
      else if (pEvent->type() == QEvent::MouseButtonRelease)
      {
         QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(pEvent);

         if (pMouseEvent->button() == Qt::LeftButton)
         {
            if ((pObject == pViewWidget) && (pPlotView != NULL))
            {
               if (modeName == "LocatorMode")
               {
                  QPoint ptMouse = pMouseEvent->pos();
                  if (pViewWidget != NULL)
                  {
                     ptMouse.setY(pViewWidget->height() - pMouseEvent->pos().y());
                  }

                  DimensionDescriptor bandDim = getClosestActiveBand(ptMouse);
                  setDisplayBand(meActiveBandColor, bandDim);

                  meActiveBandColor = RasterChannelType();
                  updateBandCharacteristics();
               }

               refresh();
            }
         }
      }
   }

   return QObject::eventFilter(pObject, pEvent);
}

QString SignaturePlotObject::getPlotName() const
{
   QString strPlotName;
   if (mpPlotWidget != NULL)
   {
      PlotView* pPlotView = mpPlotWidget->getPlot();
      if (pPlotView != NULL)
      {
         string plotName = pPlotView->getName();
         if (plotName.empty() == false)
         {
            strPlotName = QString::fromStdString(plotName);
         }
      }
   }

   return strPlotName;
}

void SignaturePlotObject::insertSignature(Signature* pSignature, ColorType color)
{
   if (pSignature == NULL)
   {
      return;
   }

   SignatureSet* pSignatureSet = dynamic_cast<SignatureSet*>(pSignature);
   if (pSignatureSet != NULL)
   {
      const vector<Signature*>& signatures = pSignatureSet->getSignatures();
      for (vector<Signature*>::size_type i = 0; i < signatures.size(); ++i)
      {
         Signature* pCurrentSignature = signatures[i];
         if (pCurrentSignature != NULL)
         {
            insertSignature(pCurrentSignature, color);
         }
      }

      return;
   }

   if (isValidAddition(pSignature))
   {
      bool firstAddition = mSignatures.empty();
      if (firstAddition)  // first signature, so need to set the data units for the plot
      {
         // since units were checked in isValidAddition, don't need to check again
         const Units* pUnits = pSignature->getUnits("Reflectance");
         VERIFYNRV(pUnits != NULL);
         mSpectralUnits = pUnits->getUnitName();
         setYAxisTitle();
      }
 
      VERIFYNRV(mpPlotWidget != NULL);
      PlotView* pPlotView = mpPlotWidget->getPlot();
      if (pPlotView != NULL)
      {
         CurveCollection* pCollection = static_cast<CurveCollection*>(pPlotView->addObject(CURVE_COLLECTION, true));
         if (pCollection != NULL)
         {
            pSignature->attach(SIGNAL_NAME(Subject, Deleted), Slot(this, &SignaturePlotObject::signatureDeleted));
            pSignature->attach(SIGNAL_NAME(Subject, Modified), Slot(this, &SignaturePlotObject::signatureModified));

            mSignatures.insert(pSignature, pCollection);

            // Set the signature color
            if (!color.isValid())
            {
               color = ColorType(0, 0, 0);  // default to black
            }
            pCollection->setColor(color);

            // Set the object name
            string signatureName = pSignature->getDisplayName();
            if (signatureName.empty())
            {
               signatureName = pSignature->getName();
            }
            pCollection->setObjectName(signatureName);

            // Set the signature values in the plot
            setSignaturePlotValues(pCollection, pSignature);
            if (firstAddition)
            {
               pPlotView->zoomExtents();
            }
         }
      }
   }
}

void SignaturePlotObject::addSignatures(const vector<Signature*>& signatures, ColorType color)
{
   if (signatures.empty() == true)
   {
      return;
   }

   if (mbClearOnAdd == true)
   {
      clearSignatures();
   }

   // determine if this is the first addition of a signature
   bool firstAdd = mSignatures.empty();

   vector<Signature*>::const_iterator iter;
   size_t numSignatures = signatures.size();
   size_t count(0);
   updateProgress("Adding signatures to plot...", 0, NORMAL);
   for (iter = signatures.begin(); iter != signatures.end(); ++iter)
   {
      if (mbAbort)
      {
         updateProgress("Add signatures aborted.", 0, ABORT);
         mbAbort = false;
         return;
      }

      Signature* pSignature = *iter;
      if (pSignature != NULL)
      {
         insertSignature(pSignature, color);
      }
      ++count;
      updateProgress("Adding signatures to plot...", count * 100 / numSignatures, NORMAL);
   }

   if (mpPlotWidget != NULL)
   {
      PlotView* pPlotView = mpPlotWidget->getPlot();
      if (pPlotView != NULL)
      {
         if (firstAdd || mpRescaleOnAdd->isChecked())
         {
            pPlotView->zoomExtents();
         }
         pPlotView->refresh();
      }
   }

   updateProgress("Finished adding signatures to plot", 100, NORMAL);
}

void SignaturePlotObject::addSignature(Signature* pSignature, ColorType color)
{
   if (pSignature == NULL)
   {
      return;
   }

   if (mbClearOnAdd == true)
   {
      clearSignatures();
   }

   // determine if this is the first addition of a signature
   bool firstAdd = mSignatures.empty();

   // Insert the signature in the plot
   insertSignature(pSignature, color);

   // Update the plot view
   if (mpPlotWidget != NULL)
   {
      PlotView* pPlotView = mpPlotWidget->getPlot();
      if (pPlotView != NULL)
      {
         if (firstAdd || mpRescaleOnAdd->isChecked())
         {
            pPlotView->zoomExtents();
         }
         pPlotView->refresh();
      }
   }
}

void SignaturePlotObject::removeSignature(Signature* pSignature, bool bDelete)
{
   if (pSignature == NULL)
   {
      return;
   }

   bool bExists = containsSignature(pSignature);
   if (bExists == false)
   {
      return;
   }

   // Remove the signature from the map
   QMap<Signature*, CurveCollection*>::Iterator signaturesIter;
   signaturesIter = mSignatures.find(pSignature);
   if (signaturesIter != mSignatures.end())
   {
      CurveCollection* pCollection = signaturesIter.value();
      if (pCollection != NULL)
      {
         PlotView* pPlotView = NULL;
         if (mpPlotWidget != NULL)
         {
            pPlotView = mpPlotWidget->getPlot();
         }

         if (pPlotView != NULL)
         {
            pPlotView->deleteObject(pCollection);
         }
      }

      mSignatures.erase(signaturesIter);
   }
   if (pSignature == mpFirstSignature)
   {
      mpFirstSignature = NULL;
      mMinValue = 0.0;
      mRange = 0.0;
   }

   if (mSignatures.count() == 0)
   {
      // Reset the y-axis values
      mSpectralUnits.erase();
      setYAxisTitle();
   }

   // Detach the signature
   pSignature->detach(SIGNAL_NAME(Subject, Deleted), Slot(this, &SignaturePlotObject::signatureDeleted));
   pSignature->detach(SIGNAL_NAME(Subject, Modified), Slot(this, &SignaturePlotObject::signatureModified));

   // Delete the signature in the data model
   if (bDelete == true)
   {
      // Delete the corresponding signature if it exists
      if (pSignature != NULL)
      {
         Service<ModelServices> pModel;
         pModel->destroyElement(pSignature);
      }
   }

   // Redraw the plot
   refresh();
}

bool SignaturePlotObject::containsSignature(Signature* pSignature) const
{
   if (pSignature == NULL)
   {
      return false;
   }

   QMap<Signature*, CurveCollection*>::ConstIterator iter = mSignatures.find(pSignature);
   if (iter != mSignatures.end())
   {
      return true;
   }

   return false;
}

vector<Signature*> SignaturePlotObject::getSignatures() const
{
   vector<Signature*> signatures;

   QMap<Signature*, CurveCollection*>::ConstIterator iter = mSignatures.begin();
   while (iter != mSignatures.end())
   {
      Signature* pSignature = iter.key();
      if (pSignature != NULL)
      {
         signatures.push_back(pSignature);
      }

      iter++;
   }

   return signatures;
}

void SignaturePlotObject::selectSignature(Signature* pSignature, bool bSelect)
{
   if (pSignature == NULL)
   {
      return;
   }

   QMap<Signature*, CurveCollection*>::Iterator iter = mSignatures.find(pSignature);
   if (iter != mSignatures.end())
   {
      CurveCollection* pCollection = iter.value();
      if (pCollection != NULL)
      {
         pCollection->setSelected(bSelect);
         refresh();
      }
   }
}

void SignaturePlotObject::selectSignatures(const vector<Signature*>& signatures, bool bSelect)
{
   vector<Signature*>::const_iterator iter = signatures.begin();
   while (iter != signatures.end())
   {
      Signature* pSignature = *iter;
      if (pSignature != NULL)
      {
         selectSignature(pSignature, bSelect);
      }

      iter++;
   }
}

void SignaturePlotObject::selectAllSignatures(bool bSelect)
{
   if (mpPlotWidget != NULL)
   {
      PlotView* pPlotView = mpPlotWidget->getPlot();
      if (pPlotView != NULL)
      {
         pPlotView->selectObjects(bSelect);
      }
   }
}

bool SignaturePlotObject::isSignatureSelected(Signature* pSignature) const
{
   if (pSignature == NULL)
   {
      return false;
   }

   QMap<Signature*, CurveCollection*>::ConstIterator iter = mSignatures.find(pSignature);
   if (iter != mSignatures.end())
   {
      CurveCollection* pCollection = iter.value();
      if (pCollection != NULL)
      {
         bool bSelected = pCollection->isSelected();
         return bSelected;
      }
   }

   return false;
}

vector<Signature*> SignaturePlotObject::getSelectedSignatures() const
{
   vector<Signature*> selectedSignatures;

   QMap<Signature*, CurveCollection*>::ConstIterator iter = mSignatures.begin();
   while (iter != mSignatures.end())
   {
      Signature* pSignature = iter.key();
      if (pSignature != NULL)
      {
         bool bSelected = isSignatureSelected(pSignature);
         if (bSelected == true)
         {
            selectedSignatures.push_back(pSignature);
         }
      }

      iter++;
   }

   return selectedSignatures;
}

unsigned int SignaturePlotObject::getNumSignatures() const
{
   return static_cast<unsigned int>(mSignatures.count());
}

unsigned int SignaturePlotObject::getNumSelectedSignatures() const
{
   vector<Signature*> selectedSignatures = getSelectedSignatures();
   return static_cast<unsigned int>(selectedSignatures.size());
}

void SignaturePlotObject::selectAllSignatures()
{
   selectAllSignatures(true);
}

void SignaturePlotObject::deselectAllSignatures()
{
   selectAllSignatures(false);
}

void SignaturePlotObject::removeSelectedSignatures()
{
   vector<Signature*> signatures = getSelectedSignatures();
   for (vector<Signature*>::iterator iter = signatures.begin(); iter != signatures.end(); ++iter)
   {
      Signature* pSignature = *iter;
      if (pSignature != NULL)
      {
         removeSignature(pSignature, true);
      }
   }

   if (mpPlotWidget != NULL)
   {
      PlotView* pPlotView = mpPlotWidget->getPlot();
      if (pPlotView != NULL)
      {
         pPlotView->deleteSelectedObjects(false);
      }
   }

   refresh();
}

void SignaturePlotObject::clearSignatures()
{
   // Clear the plot
   PlotView* pPlotView = NULL;
   if (mpPlotWidget != NULL)
   {
      pPlotView = mpPlotWidget->getPlot();
   }

   if (pPlotView != NULL)
   {
      pPlotView->clear();
   }

   // Remove the signatures
   while (mSignatures.empty() == false)
   {
      QMap<Signature*, CurveCollection*>::iterator iter = mSignatures.begin();

      Signature* pSignature = iter.key();
      if (pSignature != NULL)
      {
         removeSignature(pSignature, false);
      }
   }
}

void SignaturePlotObject::abort()
{
   if (mpSigSelector != NULL)
   {
      mpSigSelector->abortSearch();
   }
   else
   {
      mbAbort = true;  // only set if sig selector not being aborted
   }
}

void SignaturePlotObject::changeSignaturesColor()
{
   QWidget* pParent = NULL;
   if (mpPlotWidget != NULL)
   {
      pParent = mpPlotWidget->getWidget();
   }

   vector<Signature*> signatures = getSelectedSignatures();
   if (signatures.empty() == true)
   {
      QMessageBox::critical(pParent, getPlotName(), "Please select at least one signature "
         "before changing the color!");
      return;
   }

   QColor clrNew = QColorDialog::getColor(Qt::black, pParent);
   if (clrNew.isValid() == true)
   {
      for (vector<Signature*>::iterator iter = signatures.begin(); iter != signatures.end(); ++iter)
      {
         Signature* pSignature = *iter;
         if (pSignature != NULL)
         {
            setSignatureColor(pSignature, clrNew, false);
         }
      }

      refresh();
   }
}

void SignaturePlotObject::setSignatureColor(Signature* pSignature, const QColor& clrSignature, bool bRedraw)
{
   if ((pSignature == NULL) || (clrSignature.isValid() == false))
   {
      return;
   }

   QMap<Signature*, CurveCollection*>::Iterator iter = mSignatures.find(pSignature);
   if (iter != mSignatures.end())
   {
      CurveCollection* pCollection = iter.value();
      if (pCollection != NULL)
      {
         ColorType signatureColor(clrSignature.red(), clrSignature.green(), clrSignature.blue());
         pCollection->setColor(signatureColor);
      }

      if (bRedraw == true)
      {
         refresh();
      }
   }
}

QColor SignaturePlotObject::getSignatureColor(Signature* pSignature) const
{
   QColor clrSignature;

   QMap<Signature*, CurveCollection*>::ConstIterator iter = mSignatures.find(pSignature);
   if (iter != mSignatures.end())
   {
      CurveCollection* pCollection = iter.value();
      if (pCollection != NULL)
      {
         ColorType signatureColor = pCollection->getColor();
         if (signatureColor.isValid() == true)
         {
            clrSignature.setRgb(signatureColor.mRed, signatureColor.mGreen, signatureColor.mBlue);
         }
      }
   }

   return clrSignature;
}

void SignaturePlotObject::setWavelengthUnits(WavelengthUnitsType units)
{
   if (units == mWaveUnits)
   {
      return;
   }

   if (units == MICRONS)
   {
      mpMicronsAction->activate(QAction::Trigger);
   }
   else if (units == NANOMETERS)
   {
      mpNanometersAction->activate(QAction::Trigger);
   }
   else if (units == INVERSE_CENTIMETERS)
   {
      mpCentimetersAction->activate(QAction::Trigger);
   }
}

void SignaturePlotObject::setWavelengthUnits(QAction* pAction)
{
   if (pAction == NULL)
   {
      return;
   }

   WavelengthUnitsType units;
   if (pAction == mpMicronsAction)
   {
      units = MICRONS;
   }
   else if (pAction == mpNanometersAction)
   {
      units = NANOMETERS;
   }
   else if (pAction == mpCentimetersAction)
   {
      units = INVERSE_CENTIMETERS;
   }
   else
   {
      return;
   }

   mWaveUnits = units;

   QMap<Signature*, CurveCollection*>::Iterator iter = mSignatures.begin();
   while (iter != mSignatures.end())
   {
      CurveCollection* pCollection = iter.value();
      if (pCollection != NULL)
      {
         setSignaturePlotValues(pCollection, iter.key());
      }

      iter++;
   }

   setXAxisTitle();

   if (mpPlotWidget != NULL)
   {
      PlotView* pPlotView = mpPlotWidget->getPlot();
      if (pPlotView != NULL)
      {
         pPlotView->zoomExtents();
      }
   }

   updateBandCharacteristics();
   refresh();
}

WavelengthUnitsType SignaturePlotObject::getWavelengthUnits() const
{
   return mWaveUnits;
}

QString SignaturePlotObject::getSpectralUnits() const
{
   QString strUnits;
   if (mSpectralUnits.empty() == false)
   {
      strUnits = QString::fromStdString(mSpectralUnits);
   }

   return strUnits;
}

void SignaturePlotObject::enableBandCharacteristics(bool bEnable)
{
   PlotView* pPlotView = NULL;
   if (mpPlotWidget != NULL)
   {
      pPlotView = mpPlotWidget->getPlot();
   }

   if (bEnable == false)
   {
      mpWavelengthAction->activate(QAction::Trigger);

      if (pPlotView != NULL)
      {
         // Displayed grayscale band lines
         if (mpGrayscaleBandCollection != NULL)
         {
            pPlotView->deleteObject(mpGrayscaleBandCollection);
            mpGrayscaleBandCollection = NULL;
         }

         // Displayed RGB band lines
         if (mpRgbBandCollection != NULL)
         {
            pPlotView->deleteObject(mpRgbBandCollection);
            mpRgbBandCollection = NULL;
         }

         // Bad band regions
         list<PlotObject*> regions;
         pPlotView->getObjects(REGION, regions);

         for (list<PlotObject*>::iterator iter = regions.begin(); iter != regions.end(); ++iter)
         {
            RegionObject* pRegion = dynamic_cast<RegionObject*>(*iter);
            if ((pRegion != NULL) && (pRegion->isPrimary() == false))
            {
               pPlotView->deleteObject(pRegion);
            }
         }
      }
   }
   else
   {
      if (SignatureWindowOptions::getSettingDisplayWavelengths() == true)
      {
         mpWavelengthAction->activate(QAction::Trigger);
      }
      else
      {
         mpBandDisplayAction->activate(QAction::Trigger);
      }

      if (pPlotView != NULL)
      {
         // Displayed grayscale band lines
         if (mpGrayscaleBandCollection == NULL)
         {
            mpGrayscaleBandCollection = static_cast<CurveCollection*>(pPlotView->addObject(CURVE_COLLECTION, false));
            if (mpGrayscaleBandCollection != NULL)
            {
               mpGrayscaleBandCollection->setObjectName("Grayscale Band");
            }
         }

         // Displayed RGB band lines
         if (mpRgbBandCollection == NULL)
         {
            mpRgbBandCollection = static_cast<CurveCollection*>(pPlotView->addObject(CURVE_COLLECTION, false));
            if (mpRgbBandCollection != NULL)
            {
               mpRgbBandCollection->setObjectName("RGB Bands");
            }
         }

         // Bad band regions
         RasterElement* pRaster = NULL;
         if (mpRasterLayer.get() != NULL)
         {
            pRaster = dynamic_cast<RasterElement*>(mpRasterLayer->getDataElement());
         }

         if (pRaster != NULL)
         {
            vector<DimensionDescriptor> allBands;
            vector<DimensionDescriptor> activeBands;

            const RasterDataDescriptor* pDescriptor =
               dynamic_cast<const RasterDataDescriptor*>(pRaster->getDataDescriptor());
            if (pDescriptor != NULL)
            {
               activeBands = pDescriptor->getBands();

               const RasterFileDescriptor* pFileDescriptor =
                  dynamic_cast<const RasterFileDescriptor*>(pDescriptor->getFileDescriptor());
               if (pFileDescriptor != NULL)
               {
                  allBands = pFileDescriptor->getBands();
               }
            }

            if (activeBands.size() < allBands.size())
            {
               bool bLoaded = true;
               double regionStart = 1.0;
               double regionEnd = 1.0;
               RegionObject* pRegion = NULL;

               for (unsigned int i = 0; i <= allBands.size(); i++)
               {
                  unsigned int bandNumber = i;
                  bool bCurrentLoaded = true;
                  if (i < allBands.size())
                  {
                     DimensionDescriptor bandDim = allBands[i];
                     if (bandDim.isOriginalNumberValid() == true)
                     {
                        bandNumber = bandDim.getOriginalNumber();
                     }

                     vector<DimensionDescriptor>::iterator iter = find(activeBands.begin(), activeBands.end(), bandDim);
                     bCurrentLoaded = (iter != activeBands.end());
                  }

                  bool startRegion = false;
                  bool endRegion = false;
                  if ((bLoaded != bCurrentLoaded) || (i == allBands.size()))
                  {
                     if (i == allBands.size())
                     {
                        bLoaded = !bLoaded;
                     }
                     else
                     {
                        bLoaded = bCurrentLoaded;
                     }

                     if (bLoaded == false)
                     {
                        startRegion = true;
                     }
                     else
                     {
                        endRegion = true;
                     }
                  }
                  else if ((bLoaded == bCurrentLoaded) && (regionEnd != (bandNumber + 0.5)))
                  {
                     if (bLoaded == false)
                     {
                        endRegion = true;
                        startRegion = true;
                     }
                  }

                  if (endRegion == true)
                  {
                     VERIFYNRV(pRegion != NULL);

                     LocationType llCorner, ulCorner, urCorner, lrCorner;
                     pPlotView->getVisibleCorners(llCorner, ulCorner, urCorner, lrCorner);

                     pRegion->setRegion(regionStart, llCorner.mY, regionEnd, ulCorner.mY);
                     pRegion = NULL;
                  }

                  if (startRegion == true)
                  {
                     VERIFYNRV(pRegion == NULL);

                     pRegion = static_cast<RegionObject*>(pPlotView->addObject(REGION, false));
                     if (pRegion != NULL)
                     {
                        regionStart = bandNumber + 0.5;
                        ColorType regionColor(mRegionColor.red(), mRegionColor.green(), mRegionColor.blue());

                        pRegion->setVisible(mDisplayRegions);
                        pRegion->setColor(regionColor);
                        pRegion->setTransparency(mRegionOpacity);
                     }
                  }

                  regionEnd = bandNumber + 1.5;
               }
            }
         }
      }
   }

   if (mWaveUnits == MICRONS)
   {
      mpMicronsAction->setChecked(true);
   }
   else if (mWaveUnits == NANOMETERS)
   {
      mpNanometersAction->setChecked(true);
   }
   else if (mWaveUnits == INVERSE_CENTIMETERS)
   {
      mpCentimetersAction->setChecked(true);
   }

   if (pPlotView != NULL)
   {
      const MouseMode* pMouseMode = pPlotView->getMouseMode("LocatorMode");
      if (pMouseMode != NULL)
      {
         pPlotView->enableMouseMode(pMouseMode, bEnable);
      }
   }

   mpGrayscaleAction->setEnabled(bEnable);
   mpRgbAction->setEnabled(bEnable);
   mpWavelengthAction->setEnabled(bEnable);
   mpBandDisplayAction->setEnabled(bEnable);
}

void SignaturePlotObject::displayBandNumbers(bool bDisplay)
{
   if (mpRasterLayer.get() == NULL)
   {
      return;
   }

   if (bDisplay == true)
   {
      mpBandDisplayAction->activate(QAction::Trigger);
   }
   else
   {
      mpWavelengthAction->activate(QAction::Trigger);
   }
}

void SignaturePlotObject::displayBandNumbers()
{
   if (mpRasterLayer.get() == NULL)
   {
      return;
   }

   QMap<Signature*, CurveCollection*>::Iterator iter = mSignatures.begin();
   while (iter != mSignatures.end())
   {
      CurveCollection* pCollection = iter.value();
      if (pCollection != NULL)
      {
         setSignaturePlotValues(pCollection, iter.key());
      }

      iter++;
   }

   setXAxisTitle();

   if (mpPlotWidget != NULL)
   {
      PlotView* pPlotView = mpPlotWidget->getPlot();
      if (pPlotView != NULL)
      {
         pPlotView->zoomExtents();
      }
   }

   updateBandCharacteristics();
   refresh();
}

bool SignaturePlotObject::areBandNumbersDisplayed() const
{
   bool bDisplayed = mpBandDisplayAction->isChecked();
   return bDisplayed;
}

void SignaturePlotObject::displayRegions(bool bDisplay)
{
   if (bDisplay != mDisplayRegions)
   {
      if (mpPlotWidget == NULL)
      {
         return;
      }

      PlotView* pPlotView = mpPlotWidget->getPlot();
      if (pPlotView == NULL)
      {
         return;
      }

      list<PlotObject*> regions;
      pPlotView->getObjects(REGION, regions);

      for (list<PlotObject*>::iterator iter = regions.begin(); iter != regions.end(); ++iter)
      {
         RegionObject* pRegion = dynamic_cast<RegionObject*>(*iter);
         if ((pRegion != NULL) && (pRegion->isPrimary() == false))
         {
            pRegion->setVisible(bDisplay);
         }
      }

      mDisplayRegions = bDisplay;
      refresh();
   }
}

bool SignaturePlotObject::areRegionsDisplayed() const
{
   return mDisplayRegions;
}

void SignaturePlotObject::setRegionColor(const QColor& clrRegion)
{
   if (clrRegion.isValid() == true)
   {
      if (clrRegion != mRegionColor)
      {
         if (mpPlotWidget == NULL)
         {
            return;
         }

         PlotView* pPlotView = mpPlotWidget->getPlot();
         if (pPlotView == NULL)
         {
            return;
         }

         list<PlotObject*> regions;
         pPlotView->getObjects(REGION, regions);

         for (list<PlotObject*>::iterator iter = regions.begin(); iter != regions.end(); ++iter)
         {
            RegionObject* pRegion = dynamic_cast<RegionObject*>(*iter);
            if ((pRegion != NULL) && (pRegion->isPrimary() == false))
            {
               ColorType regionColor(clrRegion.red(), clrRegion.green(), clrRegion.blue());
               pRegion->setColor(regionColor);
            }
         }

         mRegionColor = clrRegion;
         refresh();
      }
   }
}

QColor SignaturePlotObject::getRegionColor() const
{
   return mRegionColor;
}

void SignaturePlotObject::setRegionOpacity(int iOpacity)
{
   if (iOpacity != mRegionOpacity)
   {
      if (mpPlotWidget == NULL)
      {
         return;
      }

      PlotView* pPlotView = mpPlotWidget->getPlot();
      if (pPlotView == NULL)
      {
         return;
      }

      list<PlotObject*> regions;
      pPlotView->getObjects(REGION, regions);

      for (list<PlotObject*>::iterator iter = regions.begin(); iter != regions.end(); ++iter)
      {
         RegionObject* pRegion = dynamic_cast<RegionObject*>(*iter);
         if ((pRegion != NULL) && (pRegion->isPrimary() == false))
         {
            pRegion->setTransparency(iOpacity);
         }
      }

      mRegionOpacity = iOpacity;
      refresh();
   }
}

int SignaturePlotObject::getRegionOpacity() const
{
   return mRegionOpacity;
}

void SignaturePlotObject::setRasterLayer(RasterLayer* pRasterLayer)
{
   if (pRasterLayer == mpRasterLayer.get())
   {
      return;
   }

   mpRasterLayer.reset(pRasterLayer);

   mpRasterLayer.addSignal(SIGNAL_NAME(RasterLayer, DisplayModeChanged),
      Slot(this, &SignaturePlotObject::displayModeChanged));
   mpRasterLayer.addSignal(SIGNAL_NAME(RasterLayer, DisplayedBandChanged),
      Slot(this, &SignaturePlotObject::displayedBandChanged));

   // Enable spectral bands functionality
   enableBandCharacteristics(mpRasterLayer.get() != NULL);

   // Update the plot
   if (mpRasterLayer.get() != NULL)
   {
      // Update the display mode
      updateDisplayMode();

      // Update the displayed bands
      updateDisplayedBands();

      // Update the classification markings
      RasterElement* pRaster = dynamic_cast<RasterElement*> (mpRasterLayer->getDataElement());
      if (pRaster != NULL)
      {
         const Classification* pClass = pRaster->getClassification();
         if ((pClass != NULL) && (mpClassification.get() != NULL))
         {
            if (pClass->hasGreaterLevel(mpClassification.get()) == true)
            {
               mpClassification->setClassification(pClass);

               if (mpPlotWidget != NULL)
               {
                  mpPlotWidget->setClassification(mpClassification.get());
               }
            }
         }
      }
   }
}

RasterLayer* SignaturePlotObject::getRasterLayer() const
{
   return const_cast<RasterLayer*>(mpRasterLayer.get());
}

void SignaturePlotObject::setSignaturePlotValues(CurveCollection* pCollection, Signature* pSignature)
{
   if (pCollection == NULL)
   {
      return;
   }

   if (pSignature == NULL)
   {
      return;
   }

   pCollection->clear();

   const DataVariant& wavelengthVariant = pSignature->getData("Wavelength");
   vector<double> wavelengthData;
   wavelengthVariant.getValue(wavelengthData);
   vector<double> reflectanceData;
   const DataVariant& reflectanceVariant = pSignature->getData("Reflectance");
   reflectanceVariant.getValue(reflectanceData);

   double dScale = 1.0;

   const Units* pUnits = pSignature->getUnits("Reflectance");
   if (pUnits != NULL)
   {
      dScale = pUnits->getScaleFromStandard();
   }

   bool bDatasetSignature = isDatasetSignature(pSignature);
   if ((bDatasetSignature == false) && (mpBandDisplayAction->isChecked() == true))
   {
      return;
   }

   Curve* pCurve = NULL;
   vector<LocationType> signaturesPoints;

   // have to get statistics now if needed - can't wait for following loop which saves
   // subsets of points for bad band breaks
   double minValue(0.0);
   double range(0.0);
   if (mpFirstSignature == NULL || mpScaleToFirst->isChecked())
   {
      getMinAndRange(reflectanceData, dScale, minValue, range);
   }

   unsigned int originalNumber = 0;
   for (unsigned int i = 0; i < reflectanceData.size(); i++)
   {
      if (bDatasetSignature == true)
      {
         bool bBadBandSection = false;
         if (mpRasterLayer.get() != NULL)
         {
            DataElement* pElement = mpRasterLayer->getDataElement();
            if (pElement != NULL)
            {
               const RasterDataDescriptor* pDescriptor =
                  dynamic_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
               if (pDescriptor != NULL)
               {
                  DimensionDescriptor band = pDescriptor->getActiveBand(i);
                  if (band.isOriginalNumberValid() == true)
                  {
                     unsigned int currentOriginalNumber = band.getOriginalNumber();
                     if (currentOriginalNumber != ++originalNumber)
                     {
                        bBadBandSection = true;
                        originalNumber = currentOriginalNumber;
                     }
                  }
               }
            }
         }

         if (bBadBandSection == true)
         {
            if (pCurve != NULL)
            {
               if (signaturesPoints.size() > 0)
               {
                  if (mpFirstSignature != NULL && mpScaleToFirst->isChecked())
                  {
                     scalePoints(signaturesPoints, minValue, range);
                  }
                  pCurve->setPoints(signaturesPoints);
                  pCurve = NULL;

                  signaturesPoints.clear();
               }
            }
         }
      }

      if (pCurve == NULL)
      {
         pCurve = pCollection->addCurve();
         if (pCurve == NULL)
         {
            return;
         }

         bool bSelected = pCollection->isSelected();
         pCurve->setSelected(bSelected);
      }

      double dXValue = 0.0;
      double dYValue = 0.0;

      if (mpBandDisplayAction->isChecked() == true)
      {
         if (i < wavelengthData.size())
         {
            double dWavelength = Wavelengths::convertValue(wavelengthData[i], MICRONS, mWaveUnits);

            DimensionDescriptor bandDim = getBandFromWavelength(dWavelength);
            if (bandDim.isOriginalNumberValid())
            {
               dXValue = bandDim.getOriginalNumber() + 1.0;
            }
         }
         else
         {
            dXValue = static_cast<double>(getOriginalBandNumber(i) + 1);
         }
      }
      else if (i < wavelengthData.size())
      {
         dXValue = Wavelengths::convertValue(wavelengthData[i], MICRONS, mWaveUnits);
      }

      if (i < reflectanceData.size())
      {
         dYValue = reflectanceData[i];
      }

      if (dScale != 0.0)
      {
         dYValue *= dScale;
      }

      signaturesPoints.push_back(LocationType(dXValue, dYValue));
   }

   if (pCurve != NULL)
   {
      if (signaturesPoints.size() > 0)
      {
         if (mpFirstSignature == NULL)
         {
            mpFirstSignature = pSignature;
            mMinValue = minValue;
            mRange = range;
         }
         else if (mpScaleToFirst->isChecked())
         {
            scalePoints(signaturesPoints, minValue, range);
         }
         pCurve->setPoints(signaturesPoints);
      }
   }
}

void SignaturePlotObject::setXAxisTitle()
{
   if (mpPlotWidget == NULL)
   {
      return;
   }

   string axisTitle;
   if (mpBandDisplayAction->isChecked() == true)
   {
      axisTitle = "Band Numbers";
   }
   else
   {
      axisTitle = "Wavelengths";

      if (mWaveUnits == MICRONS)
      {
         axisTitle += " (" + MICRON + "m)";
      }
      else if (mWaveUnits == NANOMETERS)
      {
         axisTitle += " (nm)";
      }
      else if (mWaveUnits == INVERSE_CENTIMETERS)
      {
         axisTitle += " (1/cm)";
      }
   }

   Axis* pAxis = mpPlotWidget->getAxis(AXIS_BOTTOM);
   if (pAxis != NULL)
   {
      pAxis->setTitle(axisTitle);
   }
}

void SignaturePlotObject::setYAxisTitle()
{
   if (mpPlotWidget == NULL)
   {
      return;
   }

   string axisTitle = "Values";
   if (mSpectralUnits.empty() == false)
   {
      axisTitle = mSpectralUnits;
   }

   if (mpScaleToFirst->isChecked())
   {
      axisTitle = "Scaled Values";
   }

   Axis* pAxis = mpPlotWidget->getAxis(AXIS_LEFT);
   if (pAxis != NULL)
   {
      pAxis->setTitle(axisTitle);
   }
}

void SignaturePlotObject::setClearOnAdd(bool bClear)
{
   mbClearOnAdd = bClear;
}

bool SignaturePlotObject::isClearOnAdd() const
{
   return mbClearOnAdd;
}

void SignaturePlotObject::addSignature()
{
   if (mpSigSelector == NULL)
   {
      QWidget* pParent = NULL;
      if (mpPlotWidget != NULL)
      {
         pParent = mpPlotWidget->getWidget();
      }

      mpSigSelector = new SignatureSelector(mpProgress, pParent);
      VERIFYNRV(mpSigSelector != NULL);
   }

   if (mpSigSelector->exec() == QDialog::Accepted)
   {
      vector<Signature*> signatures = mpSigSelector->getSignatures();
      signatures = SpectralUtilities::extractSignatures(signatures);
      if (signatures.empty() == false)
      {
         if (mbClearOnAdd == true)
         {
            clearSignatures();
         }

         string message = "Adding the signatures to the plot...";
         updateProgress(message, 0, NORMAL);

         unsigned int numSigs = signatures.size();
         bool firstAdd = mSignatures.empty();
         vector<Signature*>::size_type i = 0;
         for (i = 0; i < numSigs; ++i)
         {
            if (mbAbort)
            {
               message = "Add signatures aborted.";
               updateProgress(message, 0, ABORT);
               break;
            }
            Signature* pSignature = signatures[i];
            if (pSignature != NULL)
            {
               insertSignature(pSignature);
            }

            updateProgress(message, i * 100 / numSigs, NORMAL);
         }

         if (i == numSigs)
         {
            message = "Add signatures complete.";
            updateProgress(message, 100, NORMAL);
         }

         if (mpPlotWidget != NULL)
         {
            PlotView* pPlotView = mpPlotWidget->getPlot();
            if (pPlotView != NULL)
            {
               if (firstAdd || mpRescaleOnAdd->isChecked())
               {
                  pPlotView->zoomExtents();
               }
               pPlotView->refresh();
            }
         }
      }
   }

   delete mpSigSelector;
   mpSigSelector = NULL;
   mbAbort = false;  // reset in case add was aborted
}

void SignaturePlotObject::saveSignatures()
{
   QWidget* pWidget = NULL;
   if (mpPlotWidget != NULL)
   {
      pWidget = mpPlotWidget->getWidget();
   }

   // Get the selected signatures
   vector<Signature*> saveSigs = getSelectedSignatures();
   if (saveSigs.empty() == true)
   {
      // No signature are selected so save all signatures in the plot
      saveSigs = getSignatures();
   }

   if (saveSigs.empty() == true)
   {
      QMessageBox::critical(pWidget, getPlotName(), "No signatures are available to save.");
      return;
   }

   // Save the signatures
   vector<SessionItem*> items;
   for (vector<Signature*>::iterator iter = saveSigs.begin(); iter != saveSigs.end(); ++iter)
   {
      SessionItem* pItem = *iter;
      if (pItem != NULL)
      {
         items.push_back(pItem);
      }
   }

   Service<DesktopServices> pDesktop;
   pDesktop->exportSessionItems(items);
}

void SignaturePlotObject::saveSignatureLibrary()
{
   QWidget* pWidget = NULL;
   if (mpPlotWidget != NULL)
   {
      pWidget = mpPlotWidget->getWidget();
   }

   // Get the selected signatures
   vector<Signature*> saveSigs = getSelectedSignatures();
   if (saveSigs.empty() == true)
   {
      // No signature are selected so save all signatures in the plot
      saveSigs = getSignatures();
   }

   if (saveSigs.empty() == true)
   {
      QMessageBox::critical(pWidget, getPlotName(), "At least one signature must be present "
         "to save as a signature library.");
      return;
   }

   // Create a signature set
   SignatureSet* pSignatureSet = NULL;

   bool accepted;
   while (pSignatureSet == NULL)
   {
      QString text = QInputDialog::getText(pWidget, "Create New Spectral Library", "Please enter a valid name for the "
         "new library:", QLineEdit::Normal, "", &accepted);
      if (accepted && !text.isEmpty())
      {
         Service<ModelServices> pModel;
         pSignatureSet = dynamic_cast<SignatureSet*>(pModel->createElement(text.toStdString(),
            TypeConverter::toString<SignatureSet>(), NULL));
      }
      else if (!accepted)
      {
         return;
      }
   }

   // Add the signatures to the signature set
   for (unsigned int i = 0; i < saveSigs.size(); i++)
   {
      Signature* pSignature = saveSigs[i];
      if (pSignature != NULL)
      {
         pSignatureSet->insertSignature(pSignature);
      }
   }

   // Save the signature set
   Service<DesktopServices> pDesktop;
   pDesktop->exportSessionItem(pSignatureSet);
}

bool SignaturePlotObject::isDatasetSignature(Signature* pSignature) const
{
   const double tolerance = 1e-12;
   if ((mpRasterLayer.get() == NULL) || (pSignature == NULL))
   {
      return false;
   }

   DataElement* pElement = mpRasterLayer->getDataElement();
   if (pElement == NULL)
   {
      return false;
   }

   const DataVariant& variant = pSignature->getData("Wavelength");
   const vector<double>* pSigWavelengthData = variant.getPointerToValue<vector<double> >();
   if (pSigWavelengthData == NULL)
   {
      return false;
   }
   const vector<double>& sigWavelengthData = *pSigWavelengthData;

   DynamicObject* pMetadata = pElement->getMetadata();
   if (pMetadata == NULL)
   {
      return false;
   }

   FactoryResource<Wavelengths> pRasterWavelengths;
   pRasterWavelengths->initializeFromDynamicObject(pMetadata, false);

   const vector<double>& rasterWavelengthData = pRasterWavelengths->getCenterValues();

   //verify that the center wavelengths from the raster element are entirely
   //contained within the signature center wavelengths
   vector<double>::size_type rasterCounter = 0, sigCounter = 0;
   for (; (sigCounter < sigWavelengthData.size()) && (rasterCounter < rasterWavelengthData.size()); ++sigCounter)
   {
      if (fabs(rasterWavelengthData[rasterCounter] - sigWavelengthData[sigCounter]) < tolerance)
      {
         rasterCounter++;
      }
   }

   return (rasterCounter == rasterWavelengthData.size());
}

void SignaturePlotObject::setDisplayMode(DisplayMode eMode)
{
   DisplayMode eCurrentMode = GRAYSCALE_MODE;
   if (mpRgbAction->isChecked() == true)
   {
      eCurrentMode = RGB_MODE;
   }

   if ((eCurrentMode == eMode) && ((mpGrayscaleAction->isChecked() == true) || (mpRgbAction->isChecked() == true)))
   {
      return;
   }

   if (eMode == GRAYSCALE_MODE)
   {
      mpGrayscaleAction->activate(QAction::Trigger);
   }
   else if (eMode == RGB_MODE)
   {
      mpRgbAction->activate(QAction::Trigger);
   }
}

void SignaturePlotObject::setDisplayMode(QAction* pAction)
{
   if (pAction == mpGrayscaleAction)
   {
      if (mpGrayscaleBandCollection != NULL)
      {
         mpGrayscaleBandCollection->setVisible(true);
      }

      if (mpRgbBandCollection != NULL)
      {
         mpRgbBandCollection->setVisible(false);
      }

      if (mpRasterLayer.get() != NULL)
      {
         mpRasterLayer->setDisplayMode(GRAYSCALE_MODE);
      }
   }
   else if (pAction == mpRgbAction)
   {
      if (mpGrayscaleBandCollection != NULL)
      {
         mpGrayscaleBandCollection->setVisible(false);
      }

      if (mpRgbBandCollection != NULL)
      {
         mpRgbBandCollection->setVisible(true);
      }

      if (mpRasterLayer.get() != NULL)
      {
         mpRasterLayer->setDisplayMode(RGB_MODE);
      }
   }

   refresh();
}

void SignaturePlotObject::setDisplayBand(RasterChannelType eColor, DimensionDescriptor band)
{
   if (mpRasterLayer.get() == NULL)
   {
      return;
   }

   mpRasterLayer->setDisplayedBand(eColor, band);

   updateBandCharacteristics();
   refresh();
}

void SignaturePlotObject::updateBandCharacteristicsFromPlot()
{
   PlotView* pPlotView = NULL;
   if (mpPlotWidget != NULL)
   {
      pPlotView = mpPlotWidget->getPlot();
   }

   if (pPlotView == NULL)
   {
      return;
   }

   LocationType llCorner, ulCorner, urCorner, lrCorner;
   pPlotView->getVisibleCorners(llCorner, ulCorner, urCorner, lrCorner);

   double dMinX = llCorner.mX;
   double dMinY = llCorner.mY;
   double dMaxX = urCorner.mX;
   double dMaxY = urCorner.mY;

   // Displayed grayscale band lines
   if (mpGrayscaleBandCollection != NULL)
   {
      const vector<Curve*>& curves = mpGrayscaleBandCollection->getCurves();
      for (unsigned int i = 0; i < curves.size(); i++)
      {
         Curve* pCurve = curves[i];
         if (pCurve != NULL)
         {
            vector<LocationType> points = pCurve->getPoints();
            if (points.size() > 1)
            {
               LocationType minPoint = points[0];
               minPoint.mY = dMinY;

               LocationType maxPoint = points[1];
               maxPoint.mY = dMaxY;

               points.clear();
               points.push_back(minPoint);
               points.push_back(maxPoint);

               pCurve->setPoints(points);
            }
         }
      }
   }

   // Displayed RGB band lines
   if (mpRgbBandCollection != NULL)
   {
      const vector<Curve*>& curves = mpRgbBandCollection->getCurves();
      for (unsigned int i = 0; i < curves.size(); i++)
      {
         Curve* pCurve = curves[i];
         if (pCurve != NULL)
         {
            vector<LocationType> points = pCurve->getPoints();
            if (points.size() > 1)
            {
               LocationType minPoint = points[0];
               LocationType maxPoint = points[1];

               QColor clrCurve;

               ColorType curveColor = pCurve->getColor();
               if (curveColor.isValid() == true)
               {
                  clrCurve.setRgb(curveColor.mRed, curveColor.mGreen, curveColor.mBlue);
               }

               if (clrCurve == Qt::red)
               {
                  minPoint.mY = dMaxY - ((dMaxY - dMinY) / 3);
                  maxPoint.mY = dMaxY;
               }
               else if (clrCurve == Qt::green)
               {
                  minPoint.mY = dMinY + ((dMaxY - dMinY) / 3);
                  maxPoint.mY = dMaxY - ((dMaxY - dMinY) / 3);
               }
               else if (clrCurve == Qt::blue)
               {
                  minPoint.mY = dMinY;
                  maxPoint.mY = dMinY + ((dMaxY - dMinY) / 3);
               }

               points.clear();
               points.push_back(minPoint);
               points.push_back(maxPoint);

               pCurve->setPoints(points);
            }
         }
      }
   }

   // Bad band regions
   updateRegions();
}

void SignaturePlotObject::updateBandCharacteristics()
{
   if ((mpPlotWidget == NULL) || (mpRasterLayer.get() == NULL))
   {
      return;
   }

   PlotView* pPlotView = mpPlotWidget->getPlot();
   if (pPlotView == NULL)
   {
      return;
   }

   LocationType llCorner, ulCorner, urCorner, lrCorner;
   pPlotView->getVisibleCorners(llCorner, ulCorner, urCorner, lrCorner);

   double dMinY = llCorner.mY;
   double dMaxY = ulCorner.mY;

   // Displayed grayscale band lines
   if (mpGrayscaleBandCollection != NULL)
   {
      mpGrayscaleBandCollection->clear();

      DimensionDescriptor bandDim = mpRasterLayer->getDisplayedBand(GRAY);
      if (bandDim.isValid())
      {
         Curve* pCurve = mpGrayscaleBandCollection->addCurve();
         if (pCurve != NULL)
         {
            double dValue = 0.0;
            if (mpBandDisplayAction->isChecked() == true)
            {
               dValue = static_cast<double>(bandDim.getOriginalNumber() + 1);
            }
            else if (mpWavelengthAction->isChecked() == true)
            {
               dValue = getWavelengthFromBand(bandDim);
            }

            vector<LocationType> points;
            points.push_back(LocationType(dValue, dMinY));
            points.push_back(LocationType(dValue, dMaxY));

            pCurve->setPoints(points);
            pCurve->setColor(ColorType(128, 128, 128));
         }
      }
   }

   // Displayed RGB band lines
   if (mpRgbBandCollection != NULL)
   {
      mpRgbBandCollection->clear();

      // Red band
      DimensionDescriptor bandDim = mpRasterLayer->getDisplayedBand(RED);
      if (bandDim.isValid())
      {
         Curve* pCurve = mpRgbBandCollection->addCurve();
         if (pCurve != NULL)
         {
            double dValue = 0.0;
            if (mpBandDisplayAction->isChecked() == true)
            {
               dValue = static_cast<double>(bandDim.getOriginalNumber() + 1);
            }
            else if (mpWavelengthAction->isChecked() == true)
            {
               dValue = getWavelengthFromBand(bandDim);
            }

            vector<LocationType> points;
            points.push_back(LocationType(dValue, dMaxY - ((dMaxY - dMinY) / 3)));
            points.push_back(LocationType(dValue, dMaxY));

            pCurve->setPoints(points);
            pCurve->setColor(ColorType(255, 0, 0));
         }
      }

      // Green band
      bandDim = mpRasterLayer->getDisplayedBand(GREEN);
      if (bandDim.isValid())
      {
         Curve* pCurve = mpRgbBandCollection->addCurve();
         if (pCurve != NULL)
         {
            double dValue = 0.0;
            if (mpBandDisplayAction->isChecked() == true)
            {
               dValue = static_cast<double>(bandDim.getOriginalNumber() + 1);
            }
            else if (mpWavelengthAction->isChecked() == true)
            {
               dValue = getWavelengthFromBand(bandDim);
            }

            vector<LocationType> points;
            points.push_back(LocationType(dValue, dMinY + ((dMaxY - dMinY) / 3)));
            points.push_back(LocationType(dValue, dMaxY - ((dMaxY - dMinY) / 3)));

            pCurve->setPoints(points);
            pCurve->setColor(ColorType(0, 255, 0));
         }
      }

      // Blue band
      bandDim = mpRasterLayer->getDisplayedBand(BLUE);
      if (bandDim.isValid())
      {
         Curve* pCurve = mpRgbBandCollection->addCurve();
         if (pCurve != NULL)
         {
            double dValue = 0.0;
            if (mpBandDisplayAction->isChecked() == true)
            {
               dValue = static_cast<double>(bandDim.getOriginalNumber() + 1);
            }
            else if (mpWavelengthAction->isChecked() == true)
            {
               dValue = getWavelengthFromBand(bandDim);
            }

            vector<LocationType> points;
            points.push_back(LocationType(dValue, dMinY));
            points.push_back(LocationType(dValue, dMinY + ((dMaxY - dMinY) / 3)));

            pCurve->setPoints(points);
            pCurve->setColor(ColorType(0, 0, 255));
         }
      }
   }

   // Bad band regions
   updateRegions();
}

void SignaturePlotObject::updateDisplayMode()
{
   if (mpRasterLayer.get() != NULL)
   {
      DisplayMode eMode = mpRasterLayer->getDisplayMode();
      setDisplayMode(eMode);
   }
}

void SignaturePlotObject::updateDisplayedBands()
{
   if (mpRasterLayer.get() != NULL)
   {
      DimensionDescriptor grayBand = mpRasterLayer->getDisplayedBand(GRAY);
      DimensionDescriptor redBand = mpRasterLayer->getDisplayedBand(RED);
      DimensionDescriptor greenBand = mpRasterLayer->getDisplayedBand(GREEN);
      DimensionDescriptor blueBand = mpRasterLayer->getDisplayedBand(BLUE);

      setDisplayBand(GRAY, grayBand);
      setDisplayBand(RED, redBand);
      setDisplayBand(GREEN, greenBand);
      setDisplayBand(BLUE, blueBand);
   }
}

unsigned int SignaturePlotObject::getOriginalBandNumber(unsigned int activeBand) const
{
   if (mpRasterLayer.get() == NULL)
   {
      return 0;
   }

   DataElement* pElement = mpRasterLayer->getDataElement();
   if (pElement == NULL)
   {
      return 0;
   }

   const RasterDataDescriptor* pDescriptor = dynamic_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
   if (pDescriptor != NULL)
   {
      DimensionDescriptor bandDim = pDescriptor->getActiveBand(activeBand);
      if (bandDim.isValid())
      {
         unsigned int originalBand = bandDim.getOriginalNumber();
         return originalBand;
      }
   }

   return 0;
}

double SignaturePlotObject::getWavelengthFromBand(DimensionDescriptor bandDim) const
{
   if (mpRasterLayer.get() == NULL)
   {
      return 0.0;
   }

   DataElement* pElement = mpRasterLayer->getDataElement();
   if (pElement == NULL)
   {
      return 0.0;
   }

   const DynamicObject* pMetadata = pElement->getMetadata();
   if (pMetadata == NULL)
   {
      return 0.0;
   }

   if (bandDim.isActiveNumberValid())
   {
      double wavelength = 0.0;

      const std::vector<double>* pCenterValues =
         dv_cast<std::vector<double> >(&pMetadata->getAttributeByPath(CENTER_WAVELENGTHS_METADATA_PATH));
      if (pCenterValues != NULL)
      {
         unsigned int bandNumber = bandDim.getActiveNumber();
         if (bandNumber < pCenterValues->size())
         {
            wavelength = pCenterValues->at(bandNumber);
         }
      }

      return Wavelengths::convertValue(wavelength, MICRONS, mWaveUnits);
   }

   return 0.0;
}

DimensionDescriptor SignaturePlotObject::getBandFromWavelength(double dWavelength) const
{
   if (mpRasterLayer.get() == NULL)
   {
      return DimensionDescriptor();
   }

   DataElement* pElement = mpRasterLayer->getDataElement();
   if (pElement == NULL)
   {
      return DimensionDescriptor();
   }

   const RasterDataDescriptor* pDescriptor = dynamic_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());
   if (pDescriptor == NULL)
   {
      return DimensionDescriptor();
   }

   DynamicObject* pMetadata = pElement->getMetadata();
   if (pMetadata == NULL)
   {
      return DimensionDescriptor();
   }

   // Convert wavelength value to raster wavelength units for comparison
   FactoryResource<Wavelengths> pRasterWavelengths;
   pRasterWavelengths->initializeFromDynamicObject(pMetadata, false);

   WavelengthUnitsType units = pRasterWavelengths->getUnits();
   dWavelength = Wavelengths::convertValue(dWavelength, mWaveUnits, units);

   double dOldDist = numeric_limits<double>::max();
   DimensionDescriptor foundBand;

   const vector<double>& wavelengthValues = pRasterWavelengths->getCenterValues();
   for (vector<double>::size_type i = 0; i < wavelengthValues.size(); i++)
   {
      double dDist = fabs(dWavelength - wavelengthValues[i]);
      if (dDist < dOldDist)
      {
         foundBand = pDescriptor->getActiveBand(i);
         dOldDist = dDist;
      }
   }

   return foundBand;
}

DimensionDescriptor SignaturePlotObject::getClosestActiveBand(const QPoint& screenCoord) const
{
   if (mpPlotWidget == NULL)
   {
      return DimensionDescriptor();
   }

   PlotView* pPlotView = mpPlotWidget->getPlot();
   if (pPlotView == NULL)
   {
      return DimensionDescriptor();
   }

   double dataX = 0.0;
   double dataY = 0.0;
   pPlotView->translateScreenToData(screenCoord.x(), screenCoord.y(), dataX, dataY);

   DimensionDescriptor closestBand;
   if (mpBandDisplayAction->isChecked() == true)
   {
      if (mpRasterLayer.get() != NULL)
      {
         DataElement* pElement = mpRasterLayer->getDataElement();
         if (pElement != NULL)
         {
            const RasterDataDescriptor* pDescriptor =
               dynamic_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
            if (pDescriptor == NULL)
            {
               return DimensionDescriptor();
            }

            unsigned int originalNumber = 0;
            if (dataX > 0.5)
            {
               originalNumber = static_cast<unsigned int>(dataX + 0.5) - 1;
            }

            unsigned int oldDist = numeric_limits<unsigned int>::max();

            const vector<DimensionDescriptor>& activeBands = pDescriptor->getBands();
            for (unsigned int i = 0; i < activeBands.size(); i++)
            {
               DimensionDescriptor currentBand = activeBands[i];
               if (currentBand.isOriginalNumberValid() == true)
               {
                  unsigned int currentOriginalNumber = currentBand.getOriginalNumber();
                  unsigned int dist = (currentOriginalNumber > originalNumber) ?
                     currentOriginalNumber - originalNumber : originalNumber - currentOriginalNumber;

                  if (dist < oldDist)
                  {
                     closestBand = currentBand;
                     oldDist = dist;
                  }
               }
            }
         }
      }
   }
   else
   {
      closestBand = getBandFromWavelength(dataX);
   }

   return closestBand;
}

LocationType SignaturePlotObject::getClosestActiveBandLocation(const QPoint& screenCoord) const
{
   DimensionDescriptor closestBand = getClosestActiveBand(screenCoord);
   if (closestBand.isValid() == false)
   {
      return LocationType();
   }

   if (mpPlotWidget == NULL)
   {
      return LocationType();
   }

   PlotView* pPlotView = mpPlotWidget->getPlot();
   if (pPlotView == NULL)
   {
      return LocationType();
   }

   double dataX = 0.0;
   double dataY = 0.0;
   pPlotView->translateScreenToData(screenCoord.x(), screenCoord.y(), dataX, dataY);

   LocationType dataCoord(dataX, dataY);
   if (mpBandDisplayAction->isChecked() == true)
   {
      dataCoord.mX = closestBand.getOriginalNumber() + 1;
   }
   else
   {
      dataCoord.mX = getWavelengthFromBand(closestBand);
   }

   return dataCoord;
}

void SignaturePlotObject::updateRegions()
{
   PlotView* pPlotView = NULL;
   if (mpPlotWidget != NULL)
   {
      pPlotView = mpPlotWidget->getPlot();
   }

   if (pPlotView == NULL)
   {
      return;
   }

   LocationType llCorner, ulCorner, urCorner, lrCorner;
   pPlotView->getVisibleCorners(llCorner, ulCorner, urCorner, lrCorner);

   double dMinY = llCorner.mY;
   double dMaxY = ulCorner.mY;

   list<PlotObject*> regions;
   pPlotView->getObjects(REGION, regions);

   for (list<PlotObject*>::iterator iter = regions.begin(); iter != regions.end(); ++iter)
   {
      RegionObject* pRegion = dynamic_cast<RegionObject*>(*iter);
      if ((pRegion != NULL) && (pRegion->isPrimary() == false))
      {
         // Location
         double dLeft = 0.0;
         double dBottom = 0.0;
         double dRight = 0.0;
         double dTop = 0.0;
         if (pRegion->getRegion(dLeft, dBottom, dRight, dTop) == true)
         {
            pRegion->setRegion(dLeft, dMinY, dRight, dMaxY);
         }

         // Do not display the regions when displaying wavelengths
         if (mpBandDisplayAction->isChecked() == true)
         {
            pRegion->setTransparency(mRegionOpacity);
         }
         else if (mpWavelengthAction->isChecked() == true)
         {
            pRegion->setTransparency(0);
         }
      }
   }
}

void SignaturePlotObject::refresh()
{
   if (mpPlotWidget != NULL)
   {
      PlotView* pPlotView = mpPlotWidget->getPlot();
      if (pPlotView != NULL)
      {
         pPlotView->refresh();
      }
   }
}

void SignaturePlotObject::setRescaleOnAdd(bool enabled)
{
   mpRescaleOnAdd->setChecked(enabled);
}

bool SignaturePlotObject::getRescaleOnAdd()const
{
   return mpRescaleOnAdd->isChecked();
}

void SignaturePlotObject::setScaleToFirst(bool enabled)
{
   mpScaleToFirst->setChecked(enabled);
}

bool SignaturePlotObject::getScaleToFirst()const
{
   return mpScaleToFirst->isChecked();
}

bool SignaturePlotObject::isValidAddition(Signature* pSignature)
{
   if (pSignature == NULL)
   {
      return false;
   }

   VERIFY(mpPlotWidget != NULL);
   QWidget* pWidget = mpPlotWidget->getWidget();

   // don't add if already in plot
   if (containsSignature(pSignature))
   {
      return false;
   }

   // check for wavelengths if band display is disabled - plot can only display in wavelengths
   if (mpBandDisplayAction->isEnabled() == false)
   {
      bool warnNoWavelengths(true);
      const DataVariant& variant = pSignature->getData("Wavelength");
      if (variant.isValid())
      {
         const vector<double>* pSigWavelengths = variant.getPointerToValue<vector<double> >();
         if (pSigWavelengths != NULL && pSigWavelengths->empty() == false)
         {
            warnNoWavelengths = false;
         }
      }
      if (warnNoWavelengths)
      {
         QMessageBox::critical(pWidget, getPlotName(), "This signature does not have "
            "any wavelength information.  It will not be added to the plot.");
         return false;
      }
   }

   // check for valid units
   const Units* pUnits = pSignature->getUnits("Reflectance");
   VERIFY(pUnits != NULL);
   UnitType eUnits = pUnits->getUnitType();
   if ((eUnits != RADIANCE) && (eUnits != REFLECTANCE) && (eUnits != EMISSIVITY) &&
      (eUnits != DIGITAL_NO) && (eUnits != CUSTOM_UNIT) && (eUnits != REFLECTANCE_FACTOR) &&
      (eUnits != TRANSMITTANCE) && (eUnits != ABSORPTANCE) && (eUnits != ABSORBANCE))
   {
      QMessageBox::critical(pWidget, getPlotName(), "This signature does not have "
         "known data units.  It will not be added to the plot.");
      return false;
   }

   // check that signature has same units as other signatures in the plot unless plot will be cleared before add
   const string& unitName = pUnits->getUnitName();
   if (mSignatures.count() > 0 && mbClearOnAdd == false)
   {
      if (unitName != mSpectralUnits)
      {
         QMessageBox::StandardButton button = QMessageBox::warning(pWidget, getPlotName(),
            "The data units of the signature being added do not match the current data units of this plot.  "
            "The plot will have to be cleared to add this signature.  Do you want to clear the plot and "
            "add the signature or cancel adding this signature?", QMessageBox::Yes | QMessageBox::Cancel);
         if (button == QMessageBox::Cancel)
         {
            return false;
         }
         clearSignatures();
      }
   }

   return true;
}

void SignaturePlotObject::updateProgress(const string& msg, int percent, ReportingLevel level)
{
   if (mpProgress != NULL)
   {
      mpProgress->updateProgress(msg, percent, level);
   }
}

void SignaturePlotObject::updatePlotForScaleToFirst(bool enableScaling)
{
   setYAxisTitle();
   if (mSignatures.empty())
   {
      return;
   }

   for (QMap<Signature*, CurveCollection*>::const_iterator it = mSignatures.constBegin();
      it != mSignatures.constEnd(); ++it)
   {
      setSignaturePlotValues(it.value(), it.key());
   }

   if (mpPlotWidget != NULL)
   {
      PlotView* pView = mpPlotWidget->getPlot();
      if (pView != NULL)
      {
         if (getRescaleOnAdd())
         {
            pView->zoomExtents();
         }
         pView->refresh();
      }
   }
}

void SignaturePlotObject::getMinAndRange(const std::vector<double>& values, const double scaleFactor,
                                         double& minValue, double& range) const
{
   minValue = numeric_limits<double>::max();
   range = 0.0;
   double maxValue = -minValue;
   for (std::vector<double>::const_iterator it = values.begin(); it != values.end(); ++it)
   {
      double value = *it;
      if (scaleFactor != 0.0)
      {
         value *= scaleFactor;
      }
      if (value < minValue)
      {
         minValue = value;
      }
      if (value > maxValue)
      {
         maxValue = value;
      }
   }
   range = maxValue - minValue;
}

void SignaturePlotObject::scalePoints(std::vector<LocationType>& points, double minValue, double range) const
{
   if (range == 0.0)
   {
      return;   // don't want any divide by zero problems
   }

   for (std::vector<LocationType>::iterator it = points.begin(); it != points.end(); ++it)
   {
      it->mY = ((it->mY - minValue) / range) * mRange + mMinValue;
   }
}
/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SIGNATUREPLOTOBJECT_H
#define SIGNATUREPLOTOBJECT_H

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtGui/QAction>

#include "AttachmentPtr.h"
#include "ColorType.h"
#include "DimensionDescriptor.h"
#include "LocationType.h"
#include "ObjectResource.h"
#include "Observer.h"
#include "Progress.h"
#include "RasterLayer.h"
#include "SessionExplorer.h"
#include "TypesFile.h"

#include <boost/any.hpp>
#include <map>
#include <string>
#include <vector>

class Classification;
class CurveCollection;
class PlotWidget;
class RegionObject;
class Signature;
class SignatureSelector;
class Subject;

/**
 *  A plot widget to display signatures.
 *
 *  The signature plot displays one or more signatures.  A plot can be added with addSignature().
 *  Each signature can have its own color, which
 *  is set using setSignatureColor().  The plot displays signature values according to
 *  wavelength.  Three display units are available for the wavelengths: microns, nanometers,
 *  and inverse centimeters.  The units may be set with setWavelengthUnits().
 *
 *  Some signatures may originate from sources containing spectral band information.  The plot
 *  may be set to display spectral band information with setBandCharacteristics().  For a plot
 *  with spectral band information, the values may be displayed according to band number
 *  instead of wavelength.  Also, the current display mode is indicated with vertical lines
 *  at the appropriate band or wavelength location.  The display mode may be changed using
 *  setDisplayMode(), and the bands may be changed with setDisplayBand().
 *
 *  The plot has five mouse modes indicating an action that will occur when the user clicks
 *  and drags the mouse.  The modes include signature selection, pan, zoom, band selection, and
 *  annotation.  The band selection mode is only available if the plot contains band information.
 *
 *  The signature selection mode allows users to highlight one or more signatures with selection
 *  nodes.  Selected signatures may be removed, have their color changed, or copied to another
 *  plot.  The pan mode allows the user to move the position of the signatures according to the
 *  axes.  The zoom mode provides the means to set the axes boundaries by clicking and dragging
 *  a rectangle in the plot.  The band selection mode allows users to change the displayed
 *  bands by dragging the band lines to a new location.  The annotation mode allows users to
 *  add annotation objects to the plot.
 */
class SignaturePlotObject : public QObject, public Observer
{
   Q_OBJECT

public:
   /**
    *  Creates the signature plot.
    *
    *  The constructor creates the plot object and initializes the right-click menu, axes,
    *  and plot settings.
    *
    *  @param    pPlotWidget
    *            The plot widget to which this object will be attached.
    *  @param    pProgress
    *            The progress object to use to report status.
    *  @param    pParent
    *            The parent object.
    */
   SignaturePlotObject(PlotWidget* pPlotWidget, Progress* pProgress, QObject* pParent = NULL);

   /**
    *  Destroys the signature plot.
    *
    *  The destructor destroys the plot object.  The signatures are not deleted.
    */
   ~SignaturePlotObject();

   /**
    *  Updates the internal data values from existing plot objects.
    *
    *  This method sets the internal mappings from Signature to plot object
    *  based on the existing objects in the plot and the given sigantures.
    *
    *  @param   signatures
    *           The signatures that correspond to the plot objects.
    */
   void initializeFromPlot(const std::vector<Signature*>& signatures);

   /**
    *  Returns the plot widget to which this object is attached.
    *
    *  @return  A pointer to the attached plot widget.
    */
   PlotWidget* getPlotWidget() const;

   //
   // Slot methods
   //

   // Signature
   void signatureModified(Subject& subject, const std::string& signal, const boost::any& value);
   void signatureDeleted(Subject& subject, const std::string& signal, const boost::any& value);
   void signatureAttached(Subject& subject, const std::string& signal, const boost::any& value);
   void attached(Subject& subject, const std::string& signal, const Slot& slot);
   void detached(Subject& subject, const std::string& signal, const Slot& slot);

   // RasterLayer
   void displayModeChanged(Subject& subject, const std::string& signal, const boost::any& value);
   void displayedBandChanged(Subject& subject, const std::string& signal, const boost::any& value);

   // PlotView
   void plotModified(Subject& subject, const std::string& signal, const boost::any& value);

   // SessionExplorer and PlotWidget
   void updateContextMenu(Subject& subject, const std::string& signal, const boost::any& value);

   // DesktopServices
   void updatePropertiesDialog(Subject& subject, const std::string& signal, const boost::any& value);

   //
   // Signatures
   //

   /**
    *  Adds a signature to the plot.
    *
    *  This method adds a signature to a plot, rescales to the new plot
    *  extents, and redraws.  To add a signature without rescaling or
    *  redrawing, call insertSignature() instead.
    *
    *  @param   pSignature
    *           The Signature object to add.  This method does nothing if
    *           \c NULL is passed in.
    *  @param   color
    *           The color to use for the added signature.
    */
   void addSignature(Signature* pSignature, ColorType color = ColorType(0, 0, 0));

   /**
    *  Adds a signature to the plot without updating the scales or redrawing.
    *
    *  To add a signature, rescale the plot to the new extents, and redraw,
    *  call addSignature() instead.
    *
    *  @warning This method ignores the value returned from isClearOnAdd().
    *
    *  @param   pSignature
    *           The Signature object to add.  This method does nothing if
    *           \c NULL is passed in.
    *  @param   color
    *           The color to use for the signature.
   */
   void insertSignature(Signature* pSignature, ColorType color = ColorType(0, 0, 0));

   /**
    *  Adds multiple signatures to the plot.
    *
    *  This method adds each signature to a plot, rescales to the new plot
    *  extents, and redraws.
    *
    *  @param   signatures
    *           The signatures to add.  If a \c NULL pointer exists in the
    *           vector, it is ignored.
    *  @param   color
    *           The color to use for the signatures being added.
    */
   void addSignatures(const std::vector<Signature*>& signatures, ColorType color = ColorType(0, 0, 0));

   /**
    *  Removes a signature from the plot.
    *
    *  @param    pSignature
    *            The signature to remove.
    *  @param    bDelete
    *            Set \c true if the signature should be deleted from the data model.  Otherwise,
    *            set \c false.
    */
   void removeSignature(Signature* pSignature, bool bDelete);

   /**
    *  Queries whether the given signature is displayed.
    *
    *  @param    pSignature
    *            The signature to query its display state.  Cannot be NULL.
    *
    *  @return   Returns \c true if the signature is displayed, otherwise returns \c false.
    */
   bool containsSignature(Signature* pSignature) const;

   /**
    *  Returns a vector of all signatures in the plot.
    *
    *  @return   A vector containing all signatures displayed in the plot.
    *
    *  @see      getSelectedSignatures()
    */
   std::vector<Signature*> getSignatures() const;

   /**
    *  Selects or deselects a signature.
    *
    *  A selected signature is drawn with black nodes at each of the data points.
    *
    *  @param    pSignature
    *            The signature to select or deselect
    *  @param    bSelect
    *            Set \c true to select the signature.  Set \c false to deselect the signature.
    */
   void selectSignature(Signature* pSignature, bool bSelect);

   /**
    *  Selects multiple signatures in the plot.
    *
    *  @param    signatures
    *            A vector containing the signatures to select.
    *  @param    bSelect
    *            Set \c true if the given signatures should be selected, set \c false if the given signatures
    *            should be deselected.
    *
    *  @see      getSelectedSignatures(), isSignatureSelected()
    */
   void selectSignatures(const std::vector<Signature*>& signatures, bool bSelect);

   /**
    *  Selects or deselects all signatures in the plot.
    *
    *  @param    bSelect
    *            Set \c true if the signatures should be selected, set \c false if the signatures
    *            should be deselected.
    *
    *  @see      selectSignatures(), isSignatureSelected()
    */
   void selectAllSignatures(bool bSelect);

   /**
    *  Queries whether a signature is selected.
    *
    *  @param    pSignature
    *            The signature.
    *
    *  @return   Returns \c true if the signature is selected.  Returns \c false if the signature is not selected
    *            or if the signature is not displayed.
    */
   bool isSignatureSelected(Signature* pSignature) const;

   /**
    *  Returns a vector of the selected signatures.
    *
    *  @return   A vector to contain the selected signatures.  The returned vector is
    *            empty if no signatures are selected.
    *
    *  @see      getSelectedSignatures(), isSignatureSelected()
    */
   std::vector<Signature*> getSelectedSignatures() const;

   /**
    *  Returns the total number of signatures displayed in the plot.
    *
    *  @return   The total number of signatures.
    *
    *  @see      getNumSelectedSignatures()
    */
   unsigned int getNumSignatures() const;

   /**
    *  Returns the number of selected signatures.
    *
    *  @return   The number of selected signatures.
    *
    *  @see      getNumSignatures()
    */
   unsigned int getNumSelectedSignatures() const;

   /**
    *  Sets the line color of a signature.
    *
    *  @param    pSignature
    *            The signature on which to change the color.
    *  @param    clrSignature
    *            The new color for the signature.
    *  @param    bRedraw
    *            Set \c true if the plot should be redrawn after the new color has been set.
    *            Set \c false if the plot should not be redrawn.
    */
   void setSignatureColor(Signature* pSignature, const QColor& clrSignature, bool bRedraw);

   /**
    *  Returns the line color of a signature.
    *
    *  @param    pSignature
    *            The signature of which to retrieve the color.
    *
    *  @return   The signature color.  An invalid color indicates that the signature is not
    *            displayed.
    */
   QColor getSignatureColor(Signature* pSignature) const;

   //
   // Plot
   //

   /**
    *  Sets the plot to rescale or not rescale when a signature is added.
    *
    *  @param   enabled
    *           \c true to enable auto rescaling.
    */
   void setRescaleOnAdd(bool enabled);

   /**
    *  Returns whether or not the plot will be rescaled when a signature is added.
    *
    *  @return   \c true if the plot will be rescaled when a signature is added.
    */
   bool getRescaleOnAdd() const;

   /**
    *  Sets the plot to scale signatures to the first signature added.
    *
    *  @param   enabled
    *           \c true to enable scaling of signatures to the first signature.
    */
   void setScaleToFirst(bool enabled);

   /**
    *  Returns whether or not signatures will be scaled to the first added signature.
    *
    *  @return   \c true if the plot will scale signatures to the first signature.
    */
   bool getScaleToFirst() const;

   /**
    *  Sets the units of the wavelength values on the X-axis.
    *
    *  @param    units
    *            The unit type to display.
    */
   void setWavelengthUnits(WavelengthUnitsType units);

   /**
    *  Returns the units of the wavelength values on the X-axis.
    *
    *  @return   The current wavelength unit type.
    */
   WavelengthUnitsType getWavelengthUnits() const;

   /**
    *  Returns the units of the signature values on the Y-axis.
    *
    *  @return   The current units of the signature values.
    */
   QString getSpectralUnits() const;

   /**
    *  Sets the plot to clear when a new signature is added.
    *
    *  @param    bClear
    *            \c true to clear the plot when a new signature is added.
    */
   void setClearOnAdd(bool bClear);

   /**
    *  Queries whether the plot is cleared when a new signature is added.
    *
    *  @return   \c true if the plot is set to clear when a new signature is added.
    */
   bool isClearOnAdd() const;

   //
   // Spectral bands
   //

   /**
    *  Sets the X-axis to display spectral band numbers or wavelengths.
    *
    *  This method sets the plot to contain spectral band information.  The band selection
    *  mode, display mode, and band number axis values are enabled.
    *
    *  @param    bDisplay
    *            Set \c true to display spectral band numbers on the X-axis.  Set \c false to display
    *            wavelengths.
    */
   void displayBandNumbers(bool bDisplay);

   /**
    *  Queries whether spectral band numbers are displayed on the X-axis.
    *
    *  @return   Returns \c true if spectral band numbers are displayed.  Returns \c false if wavelengths are
    *            displayed.
    */
   bool areBandNumbersDisplayed() const;

   /**
    *  Toggles the display of shaded regions indicating the location of spectral bands
    *  that were not loaded with the data cube.
    *
    *  @param    bDisplay
    *            Set \c true if the non-loaded band regions should be displayed, otherwise set \c false.
    */
   void displayRegions(bool bDisplay);

   /**
    *  Queries whether regions of the plot where spectral bands have not been loaded with
    *  the data cube are shaded.
    *
    *  @return   Returns \c true if non-loaded band regions are displayed, otherwise returns \c false.
    */
   bool areRegionsDisplayed() const;

   /**
    *  Sets the color of the shaded region.
    *
    *  This method may still be called if the regions are not displayed.  The new color
    *  will appear when the regions are next displayed.
    *
    *  @param    clrRegion
    *            The region color.  It must be a valid QColor.
    */
   void setRegionColor(const QColor& clrRegion);

   /**
    *  Returns the region color.
    *
    *  @return   The region color.  A valid color is still returned if the regions are
    *            not displayed.
    */
   QColor getRegionColor() const;

   /**
    *  Sets the opacity of the shaded region.
    *
    *  The opacity value describes how the region color is blended with the background
    *  color.  Valid opacity values range from 0 to 255, where the zero value is completely
    *  transparent, and the 255 value is complete opaque.
    *
    *  @param    iOpacity
    *            The region opacity value, ranging from 0 to 255.
    */
   void setRegionOpacity(int iOpacity);

   /**
    *  Returns the opacity for the regions.
    *
    *  @return   The opacity value, ranging from 0 to 255.  A valid value is still returned
    *            even if the regions are not displayed.
    */
   int getRegionOpacity() const;

   /**
    *  Sets the raster layer associated with the plot.
    *
    *  This method associates a raster layer with the plot, which will display spectral band
    *  information on the plot.  The raster layer is used to convert spectral band numbers to
    *  wavelengths and vice versa.
    *
    *  @param    pRasterLayer
    *            The raster layer to associate with the plot.
    */
   void setRasterLayer(RasterLayer* pRasterLayer);

   /**
    *  Returns the raster layer associated with the plot.
    *
    *  @return  A pointer to the associated raster layer.  NULL is returned if the plot does
    *           not have an associated raster layer.
    *
    *  @see     setRasterLayer()
    */
   RasterLayer* getRasterLayer() const;

public slots:
   /**
    *  Sets the display mode on the plot.
    *
    *  This method sets the display mode of the plot.  The displayed band vertical lines
    *  change to correspond with the new display mode.
    *
    *  @param    eMode
    *            The new display mode.
    */
   void setDisplayMode(DisplayMode eMode);

   /**
    *  Sets the displayed spectral bands on the plot.
    *
    *  This method sets the vertical displayed band lines on the plot to correspond with the
    *  displayed band numbers.
    *
    *  @param    eColor
    *            The color corresponding to the displayed band.
    *  @param    band
    *            The band to display.
    */
   void setDisplayBand(RasterChannelType eColor, DimensionDescriptor band);

   /**
    *  Removes all signatures from the plot.
    */
   void clearSignatures();

   /**
    *  Cancels the active signature search or signature exporter.
    */
   void abort();

   /**
    *  Add a loaded signature to the plot.
    *
    *  The method invokes a signature selection dialog for the user to select signatures.
    *  The selected signatures are added to the plot.
    */
   void addSignature();

protected:
   /**
    *  Intercepts event notification for registered objects.
    *
    *  This method is used to intercept messages to the axis widgets and the graph widget.
    *  The event is queried for a mouse press with the right mouse button to display
    *  the popup menu.
    *
    *  @param    pObject
    *            The object sending the event message.
    *  @param    pEvent
    *            The event message.
    *
    *  @return   Returns \c true if the event should not be passed to the respective object.  Returns \c false
    *            if the event should be processed normally.
    */
   bool eventFilter(QObject* pObject, QEvent* pEvent);

   /**
    *  Returns the name of the plot.
    *
    *  @return   The plot name.
    */
   QString getPlotName() const;

   /**
    *  Returns the original band number of the given band.
    *
    *  @param   activeBand
    *           The zero-based active band number for which to get its
    *           corresponding original band number.
    *
    *  @return  The zero-based original band number.
    */
   unsigned int getOriginalBandNumber(unsigned int activeBand) const;

   /**
    *  Returns the wavelength value for a spectral band.
    *
    *  @param    band
    *            The band for which to get its wavelength.
    *
    *  @return   The wavelength value in the current wavelength units of the plot.
    */
   double getWavelengthFromBand(DimensionDescriptor band) const;

   /**
    *  Returns the spectral band number for a given wavelength value.
    *
    *  @param   dWavelength
    *           The wavelength value in the units returned by
    *           getWavelengthUnits().
    *
    *  @return  The band with the given wavelength.  An invalid
    *           DimensionDescriptor is returned if band information is not
    *           present.
    */
   DimensionDescriptor getBandFromWavelength(double dWavelength) const;

   /**
    *  Queries whether a signature is from the associated data set.
    *
    *  This method compares the given signature wavelengths with the wavelengths of the
    *  associated data set.  If the number of wavelengths and their values are the
    *  same, the signature is considered a data set signature.
    *
    *  @param    pSignature
    *            The signature from which to compare its wavelengths with the data set.
    *
    *  @return   Returns \c true if the signature is considered a data set signature, otherwise returns \c false.
    */
   bool isDatasetSignature(Signature* pSignature) const;

   /**
    *  Perform checks to determine if a signature can be added to the plot.
    *
    *  This method checks if the signature is already in the plot, has same data units as other signatures in the plot,
    *  and if the signature contains wavelength information if band display is disabled for the plot.
    *
    *  @param    pSignature
    *            The signature to check.
    *
    *  @return   Returns \c true if the signature can be added to the plot, otherwise returns \c false.
    */
   bool isValidAddition(Signature* pSignature);

   /**
    *  Find the minimum value and range for the reflectance values in a signature.
    *
    *  This method finds the minimum value and range for a signature.
    *
    *  @param   values
    *           The reflectance values for the signature.
    *  @param   scaleFactor
    *           The scaling factor for the values.
    *  @param   minValue
    *           The returned minimum value.
    *  @param   range
    *           The returned range.
    */
   void getMinAndRange(const std::vector<double>& values, const double scaleFactor,
      double& minValue, double& range) const;

   /**
    *  Scale set of points to first signature.
    *
    *  This method scales the reflectance values for a set of points to the range and min value for the first signature.
    *
    *  @param   points
    *           The subset of points to be scaled.
    *  @param   minValue
    *           The minimum value for all the reflectance values in the signature being scaled.
    *  @param   range
    *           The range of all the reflectance values in the signature being scaled.
    */
   void scalePoints(std::vector<LocationType>& points, double minValue, double range) const;

protected slots:
   /**
    *  Sets the plot to contain spectral band information.
    *
    *  This method sets the plot to contain spectral band information.  The band selection
    *  mode, display mode, and band number axis values are enabled and bad band regions can
    *  be displayed.
    *
    *  @param    bEnable
    *            Set \c true to enable spectral band information in the plot.  Set \ false to disable
    *            band information.
    */
   void enableBandCharacteristics(bool bEnable);

   /**
    *  Updates the location of the displayed band lines and bad band regions based on the
    *  current plot settings.
    */
   void updateBandCharacteristics();

   /**
    *  Updates the location of the displayed band lines and bad band regions from the current
    *  drawing matrices in the plot.
    */
   void updateBandCharacteristicsFromPlot();

   /**
    *  Saves selected signatures to disk.
    *
    *  This method saves the selected signatures to files on disk.  If one signature is selected,
    *  an import/export dialog is displayed for the user to select a filename and an exporter.
    *  if multiple signatures are selected, a plug-in select dialog is displayed for the user to
    *  select an exporter.  Default filenames will be assigned to each signature file.  The
    *  user is prompted before overwriting a file.
    */
   void saveSignatures();

   /**
    *  Saves selected signatures to disk in a spectral library.
    *
    *  Similar to the saveSignatures() method, this method saves the selected signatures to files
    *  on disk.  A spectral library is created to store all of the selected signatures, and the
    *  library is also saved to disk.
    */
   void saveSignatureLibrary();

   /**
    *  Updates the location of the displayed band lines based on the current
    *  raster layer settings.
    */
   void updateDisplayedBands();

   /**
    *  Sets the current display mode.
    *
    *  @param    pAction
    *            The action indicating the new display mode.
    */
   void setDisplayMode(QAction* pAction);

   /**
    *  Updates the display mode based on the current raster layer settings.
    */
   void updateDisplayMode();

   /**
    *  Sets the current wavelength units type.
    *
    *  @param    pAction
    *            The action indicating the new wavelength units.
    */
   void setWavelengthUnits(QAction* pAction);

   /**
    *  Sets the X-axis to display band numbers
    */
   void displayBandNumbers();

   /**
    *  Selects all displayed signatures.
    */
   void selectAllSignatures();

   /**
    *  Deselects all displayed signatures.
    */
   void deselectAllSignatures();

   /**
    *  Removes all selected signatures.
    *
    *  This method removes the selected signatures from the plot, but does not delete
    *  the Signature objects in the data model.
    */
   void removeSelectedSignatures();

   /**
    *  Changes the color of the selected signatures.
    *
    *  This method invokes a color selection dialog for the user to select a new color.
    *  The colors of all selected signatures will be changed to this color.
    */
   void changeSignaturesColor();

   /**
    *  Updates the bad band region plot objects.
    *
    *  This method updates the location of the bad band region plot objects to
    *  extend to the visible minimum and maximum y-coordinates.  It also sets
    *  the regions to not display if wavelengths are displayed on the x-axis.
    */
   void updateRegions();

   /**
    *  Redraws the signature plot.
    */
   void refresh();

   /**
    *  Updates the progress.
    *
    *  This method updates the progress instance if it exists.
    *
    *  @param   msg
    *           The progress message to display.
    *  @param   percent
    *           The percentage completion of the task.
    *  @param   level
    *           The reporting level for the message.
    */
   void updateProgress(const std::string& msg, int percent, ReportingLevel level);

   /**
    *  Update the plot for scaling the signatures in plot to first signature.
    *  
    *  This method will scale all the signatures in the plot to the first signature or
    *  just plot the signatures depending on value of \em enableScaling.
    *
    *  @param   enableScaling
    *           If \c true, the values of the signatures in the plot will be scaled to first signature, otherwise
    *           the signature values will not be scaled.
    */
   void updatePlotForScaleToFirst(bool enableScaling);

private:
   AttachmentPtr<SessionExplorer> mpExplorer;

   // Plot widget
   PlotWidget* mpPlotWidget;

   // Progress
   Progress* mpProgress;
   bool mbAbort;

   // Classification
   FactoryResource<Classification> mpClassification;

   // Active signature selector and signature exporter
   SignatureSelector* mpSigSelector;

   // Signatures
   QMap<Signature*, CurveCollection*> mSignatures;

   // Plot
   WavelengthUnitsType mWaveUnits;
   std::string mSpectralUnits;
   RasterChannelType meActiveBandColor;
   bool mbClearOnAdd;

   // Spectral bands
   AttachmentPtr<RasterLayer> mpRasterLayer;
   CurveCollection* mpGrayscaleBandCollection;
   CurveCollection* mpRgbBandCollection;

   // Regions
   bool mDisplayRegions;
   QColor mRegionColor;
   int mRegionOpacity;

   // Stats for first signature
   Signature* mpFirstSignature;
   double mMinValue;
   double mRange;

   // Context menu
   QMenu* mpSignatureUnitsMenu;
   QAction* mpWavelengthAction;
   QAction* mpBandDisplayAction;
   QMenu* mpWaveUnitsMenu;
   QAction* mpMicronsAction;
   QAction* mpNanometersAction;
   QAction* mpCentimetersAction;
   QMenu* mpDisplayModeMenu;
   QAction* mpGrayscaleAction;
   QAction* mpRgbAction;
   QAction* mpRescaleOnAdd;
   QAction* mpScaleToFirst;

   // Convenience methods
   void setXAxisTitle();
   void setYAxisTitle();
   void setSignaturePlotValues(CurveCollection* pCollection, Signature* pSignature);
   DimensionDescriptor getClosestActiveBand(const QPoint& screenCoord) const;
   LocationType getClosestActiveBandLocation(const QPoint& screenCoord) const;
};

#endif

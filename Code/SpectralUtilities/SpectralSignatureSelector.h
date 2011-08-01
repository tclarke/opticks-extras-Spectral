/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SPECTRALSIGNATURESELECTOR_H__
#define SPECTRALSIGNATURESELECTOR_H__

#include "SignatureSelector.h"

class AlgorithmRunner;
class AoiElement;
class Progress;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class RasterElement;

/**
 *  A dialog to import and select signatures.
 *
 *  In addition to the capabilities described in the SignatureSelector
 *  documentation, the SpectralSignatureSelector adds an AOI filter type that
 *  displays child AOIs for a given raster element.  If selected, the averaged
 *  signature of the AOI is returned in getSignatures().
 *
 *  The SpectralSignatureSelector can also be created with a valid algorithm
 *  that will be executed when the user clicks the Apply button.  When the
 *  dialog is created with a valid algorithm, additional widgets are added to
 *  the dialog to provide further inputs to the algorithm.  See
 *  SpectralSignatureSelector(RasterElement*, AlgorithmRunner*, Progress*, const std::string&, bool, bool, QWidget*, const std::string&)
 *  for details on the additional dialog widgets.
 */
class SpectralSignatureSelector : public SignatureSelector
{
   Q_OBJECT

public:
   /**
    *  Creates the spectral signature selector dialog containing a signature
    *  list and an AOI list.
    *
    *  The signature list is initialized from all loaded signatures in the data
    *  model, and the AOI list is initialized with all AOIs that have \em pCube
    *  as a parent raster element.
    *
    *  @param   pCube
    *           Specifies the parent raster element for all AOIs that should be
    *           added to the AOI list.
    *  @param   pProgress
    *           An optional Progress object that is used when searching for
    *           signatures to import.
    *  @param   pParent
    *           The dialog's parent widget.
    *  @param   mode
    *           The selection mode to be used for the signature list and the AOI
    *           list.
    *  @param   addApply
    *           If set to \c true, an Apply button will be added to the dialog.
    *  @param   customButtonLabel
    *           Adds a custom button (separate from the Apply button) to the
    *           dialog with the given text.  If empty, a custom button is not
    *           added.
    */
   SpectralSignatureSelector(RasterElement* pCube, Progress* pProgress, QWidget* pParent = NULL,
      QAbstractItemView::SelectionMode mode = QAbstractItemView::ExtendedSelection, bool addApply = false,
      const std::string& customButtonLabel = std::string());

   /**
    *  Creates the spectral signature selector dialog containing a signature
    *  list, an AOI list, and additional widgets for algorithm execution.
    *
    *  The signature list is initialized from all loaded signatures in the data
    *  model, and the AOI list is initialized with all AOIs that have \em pCube
    *  as a parent raster element.
    *
    *  In addition to the widgets to select a signature or AOI, this constructor
    *  also creates the following widgets:
    *  - A QLineEdit to set a default name for the output threshold layer or
    *    pseudocolor layer created from the algorithm.
    *  - A QDoubleSpinBox to set a default threshold for the output threshold
    *    layer from the algorithm.  The default threshold value is 5.0.
    *  - A QCheckBox combined with a QComboBox containing AOIs with \em pCube
    *    as their parent for the user to optionally select an AOI as a subset
    *    for algorithm execution.  By default, an AOI is not selected.
    *  - A QCheckBox allowing users to combine algorithm results from multiple
    *    signatures into a single pseudocolor layer instead of creating
    *    multiple threshold layers.  The default state of the check box is set
    *    by the value of \em pseudocolor.
    *
    *  @param   pCube
    *           Specifies the parent raster element for all AOIs that should be
    *           added to the AOI selection list and algorithm AOI subset combo
    *           box.
    *  @param   pRunner
    *           The algorithm that should be executed when the user clicks the
    *           Apply button.  If \em addApply is \c false, this parameter is
    *           ignored.
    *  @param   pProgress
    *           An optional Progress object that is used when searching for
    *           signatures to import.
    *  @param   resultsName
    *           An initial name for the output results layer created by the
    *           algorithm.
    *  @param   pseudocolor
    *           If set to \c true, indicates that the algorithm should create a
    *           single pseudocolor layer for all selected signatures or AOIs.
    *           If set to \c false, indicates that the algorithm should create a
    *           threshold layer for each selected signature or AOI.
    *  @param   addApply
    *           If set to \c true, an Apply button will be added to the dialog.
    *  @param   pParent
    *           The dialog's parent widget.
    *  @param   customButtonLabel
    *           Adds a custom button (separate from the Apply button) to the
    *           dialog with the given text.  If empty, a custom button is not
    *           added.
    *
    *  @see     setThreshold(), usePseudocolorLayer()
    */
   SpectralSignatureSelector(RasterElement* pCube, AlgorithmRunner* pRunner, Progress* pProgress,
      const std::string& resultsName = std::string(), bool pseudocolor = true, bool addApply = false,
      QWidget* pParent = NULL, const std::string& customButtonLabel = std::string());

   /**
    *  Destroys the spectral signature selector dialog.
    */
   virtual ~SpectralSignatureSelector();

   /**
    *  Returns the name for the output results layer from the algorithm.
    *
    *  If an algorithm is available, the user is required to enter a valid
    *  (i.e. non-empty) name before accepting the dialog.
    *
    *  @return  Returns the name that should be used for the output results
    *           layer created by the algorithm.  Returns an empty string if the
    *           dialog was created without a valid algorithm.
    */
   std::string getResultsName() const;

   /**
    *  Returns the initial threshold value for the output threshold layer from
    *  the algorithm.
    *
    *  @return  Returns the threshold value that should be used as the initial
    *           threshold for the output threshold layer created by the
    *           algorithm.  Returns a value of 0.0 if the dialog was created
    *           without a valid algorithm.
    */
   double getThreshold() const;

   /**
    *  Sets the initial threshold value for the output threshold layer from the
    *  algorithm.
    *
    *  If the dialog was created without a valid algorithm, this method does
    *  nothing.
    *
    *  @param   val
    *           The initial threshold value for the output threshold layer that
    *           can be modified by the user.
    */
   void setThreshold(double val);

   /**
    *  Queries whether values in the dialog widgets have been modified by the
    *  user.
    *
    *  This method queries whether widgets in the dialog have been modified by
    *  checking the enabled state of the Apply button.
    *
    *  @return  Returns \c true if widgets in the dialog have been modified that
    *           cause the Apply button to become enabled, or if the
    *           SignatureSelector::enableApplyButton() method has been called
    *           with a value of \c true.  Returns \c false if the dialog was
    *           created without an Apply button or if the dialog is modal, which
    *           by default does not contain an Apply button.
    *
    *  @see     setModified()
    */
   bool getModified() const;

   /**
    *  Returns the AOI to use as a subset during algorithm execution.
    *
    *  @return  Returns the AOI that should be used as a subset during algorithm
    *           execution.  If the user accepts the dialog, the AOI is
    *           guaranteed to have at least one selected pixel.  Returns \c NULL
    *           if user has not selected an AOI subset, or if the dialog was
    *           created without a valid algorithm.
    */
   AoiElement* getAoi() const;

   /**
    *  Queries whether a single pseudocolor layer should be created for all
    *  selected signatures or AOIs.
    *
    *  @return  Returns \c true if a single pseudocolor layer should be created
    *           for multiple selected signatures or AOIs instead of multiple
    *           threshold layers.  Returns \c false if the dialog was created
    *           without a valid algorithm.
    */
   bool isPseudocolorLayerUsed() const;

   /**
    *  Sets whether a single pseudocolor layer should be created for all
    *  selected signatures or AOIs.
    *
    *  If the dialog was created without a valid algorithm, this method does
    *  nothing.
    *
    *  @param   bPseudocolor
    *           If set to \c true, indicates that a single pseudocolor layer
    *           should be created for multiple selected signatures or AOIs
    *           instead of multiple threshold layers.
    */
   void usePseudocolorLayer(bool bPseudocolor);

   /**
    *  Returns a vector of currently selected signatures.
    *
    *  This method returns a vector of all selected signatures in the list view.
    *  If a signature set is selected, only the SignatureSet object is added to
    *  the vector.  Any selected signatures contained in the selected signature
    *  set are not added to the vector.
    *
    *  To obtain a vector that includes Signature objects inside a selected
    *  SignatureSet object, call SignatureSelector::getExtractedSignatures()
    *  instead.
    *
    *  @return  A vector of the selected Signature objects in the signature
    *           list.  If the AOI list is active, the averaged signature for
    *           each selected AOI is returned.
    */
   std::vector<Signature*> getSignatures() const;

protected slots:
   /**
    *  Enables or disables the check box to combine results into a single
    *  pseudocolor layer.
    *
    *  This method enables the check box based on the number of selected
    *  signatures or AOIs.  If two or more signatures or AOIs are selected, then
    *  the check box is enabled; otherwise it is disabled.
    *
    *  This method is called automatically when the signature or AOI selection
    *  changes and should not need to be called directly.
    *
    *  If the dialog is created without a valid algorithm, this method does
    *  nothing.
    */
   void enablePseudocolorCheckBox();

   /**
    *  Accepts the dialog if current widget values are valid.
    *
    *  This method validates the current widget values by calling
    *  validateInputs() before accepting the dialog.
    *
    *  This method is called automatically when the user clicks the OK button
    *  and should not need to be called directly.
    */
   virtual void accept();

   /**
    *  Validates widget values and executes the algorithm.
    *
    *  This method validates the current widget values by calling
    *  validateInputs() and then executes the algorithm, if available, by
    *  calling AlgorithmRunner::runAlgorithmFromGuiInputs().
    *
    *  This method is called automatically when the user clicks the Apply button
    *  and should not need to be called directly.
    */
   virtual void apply();

   /**
    *  Sets the dialog to indicate that widget values have been modified.
    *
    *  This method indicates a modified state by enabling the Apply button.  If
    *  the dialog was created without an Apply button or if the dialog is modal,
    *  which by default does not contain an Apply button, then this method does
    *  nothing.
    *
    *  This method is called automatically when the selected signature or AOI
    *  changes.  If a valid algorithm is available, this method is called
    *  automatically when the user changes the results layer name, threshold
    *  value, AOI subset, and pseudocolor layer output widgets.
    */
   void setModified();

   /**
    *  Updates the list of AOIs in the AOI subset combo box.
    *
    *  This method updates the list of AOIs in the combo box by calling
    *  ModelServices::getElements() using the RasterElement passed into the
    *  constructor as the AOI parent.
    *
    *  This method is called automatically only from the constructor when the
    *  dialog is created.
    *
    *  If the dialog is created without a valid algorithm, this method does
    *  nothing.
    */
   void refreshAoiList();

   /**
    *  Updates the main list view with the available signatures.
    *
    *  If the AOI list is active, this method updates the displayed AOIs by
    *  calling ModelServices::getElements() using the RasterElement passed into
    *  the constructor as the AOI parent.
    */
   virtual void updateSignatureList();

protected:
   /**
    *  Validates the values of the dialog widgets.
    *
    *  This method always performs the following checks on the widget values:
    *  - At least one signature or AOI is selected.
    *
    *  The following checks are performed if a valid algorithm is available:
    *  - The output layer name is not empty.
    *  - If an AOI subset is selected, the AOI is valid and contains at least
    *    one selected pixel.
    *
    *  @return  Returns \c true if all widgets validate successfully; otherwise
    *           returns \c false.
    */
   virtual bool validateInputs();

   /**
    *  The QLineEdit to set a default name for the output threshold layer or
    *  pseudocolor layer created from the algorithm.
    */
   QLineEdit* mpNameEdit;

   /**
    *  The QDoubleSpinBox to set a default threshold for the output threshold
    *  layer from the algorithm.  The default threshold value is 5.0.
    */
   QDoubleSpinBox* mpThreshold;

   /**
    *  The QCheckBox that toggles whether an AOI subset should be used during
    *  algorithm execution.
    */
   QCheckBox* mpAoiCheck;

   /**
    *  The QComboBox containing the AOI to select as a subset during alorithm
    *  execution.
    */
   QComboBox* mpAoiCombo;

   /**
    *  The QCheckBox allowing users to combine algorithm results from multiple
    *  signatures into a single pseudocolor layer instead of creating
    *  multiple threshold layers.  The default state of the check box is set
    *  by a parameter in the constructor.
    */
   QCheckBox* mpPseudocolorCheck;

   /**
    *  The algorithm to execute when the Apply button is clicked.
    */
   AlgorithmRunner* mpRunner;

   /**
    *  The parent element of the AOIs to display in the AOI list and subset
    *  combo box.
    */
   RasterElement* mpCube;
};

#endif

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SPECTRALCONTEXTMENUACTIONS_H
#define SPECTRALCONTEXTMENUACTIONS_H

/**
 *  Clears the results on the current results page.
 */
#define SPECTRAL_LIBRARY_MATCH_RESULTS_CLEAR_RESULTS_ACTION "SPECTRAL_LIBRARY_MATCH_RESULTS_CLEAR_RESULTS_ACTION"

/**
 *  Collapse all the nodes on the current results page.
 */
#define SPECTRAL_LIBRARY_MATCH_RESULTS_COLLAPSE_ALL_ACTION "SPECTRAL_LIBRARY_MATCH_RESULTS_COLLAPSE_ALL_ACTION"

/**
 *  Create an average signature for the user-selected matched signatures on the current results page.
 *  The average will be computed using signature values that have been resampled to the raster element
 *  associated with the current page.
 */
#define SPECTRAL_LIBRARY_MATCH_RESULTS_CREATE_AVERAGE_ACTION "SPECTRAL_LIBRARY_MATCH_RESULTS_CREATE_AVERAGE_ACTION"

/**
*  Delete the current results page.
 */
#define SPECTRAL_LIBRARY_MATCH_RESULTS_DELETE_PAGE_ACTION "SPECTRAL_LIBRARY_MATCH_RESULTS_DELETE_PAGE_ACTION"

/**
*  Expand all the nodes on the current results page.
 */
#define SPECTRAL_LIBRARY_MATCH_RESULTS_EXPAND_ALL_ACTION "SPECTRAL_LIBRARY_MATCH_RESULTS_EXPAND_ALL_ACTION"

/**
*  Locate the selected signatures in the scene.
 */
#define SPECTRAL_LIBRARY_MATCH_RESULTS_LOCATE_ACTION "SPECTRAL_LIBRARY_MATCH_RESULTS_LOCATE_ACTION"

/**
 *  Tool button separator for spectral library match.
 */
#define SPECTRAL_LIBRARY_MATCH_RESULTS_SEPARATOR_ACTION "SPECTRAL_LIBRARY_MATCH_RESULTS_SEPARATOR_ACTION"

/**
 *  Adds a new signature plot to the current plot set in the Signature Window.
 */
#define SPECTRAL_SIGNATUREWINDOW_ADD_PLOT_ACTION "SPECTRAL_SIGNATUREWINDOW_ADD_PLOT_ACTION"

/**
 *  Adds one or more signatures to the signature plot.
 */
#define SPECTRAL_SIGNATUREPLOT_ADD_SIG_ACTION "SPECTRAL_SIGNATUREPLOT_ADD_SIG_ACTION"

/**
 *  Saves the selected signatures in the signature plot to separate files.
 */
#define SPECTRAL_SIGNATUREPLOT_SAVE_SIG_ACTION "SPECTRAL_SIGNATUREPLOT_SAVE_SIG_ACTION"

/**
 *  Saves the selected signatures in the signature plot as a spectral library
 *  in a single file.
 */
#define SPECTRAL_SIGNATUREPLOT_SAVE_LIBRARY_ACTION "SPECTRAL_SIGNATUREPLOT_SAVE_LIBRARY_ACTION"

/**
 *  Selects all signatures in the signature plot.
 */
#define SPECTRAL_SIGNATUREPLOT_SELECT_ALL_ACTION "SPECTRAL_SIGNATUREPLOT_SELECT_ALL_ACTION"

/**
 *  Deselects all signatures in the signature plot.
 */
#define SPECTRAL_SIGNATUREPLOT_DESELECT_ALL_ACTION "SPECTRAL_SIGNATUREPLOT_DESELECT_ALL_ACTION"

/**
 *  Deletes all selected signatures in the signature plot.
 */
#define SPECTRAL_SIGNATUREPLOT_DELETE_SELECTED_ACTION "SPECTRAL_SIGNATUREPLOT_DELETE_SELECTED_ACTION"

/**
 *  Changes the color of the selected signatures in the signature plot.
 */
#define SPECTRAL_SIGNATUREPLOT_CHANGE_COLOR_ACTION "SPECTRAL_SIGNATUREPLOT_CHANGE_COLOR_ACTION"

/**
 *  Deletes all signatures in the signature plot.
 */
#define SPECTRAL_SIGNATUREPLOT_CLEAR_ACTION "SPECTRAL_SIGNATUREPLOT_CLEAR_ACTION"

/**
 *  Enable/disable auto rescaling when a signature is added to the plot.
 */
#define SPECTRAL_SIGNATUREPLOT_RESCALE_ON_ADD_ACTION "SPECTRAL_SIGNATUREPLOT_RESCALE_ON_ADD_ACTION"

/**
 *  Enable/disable plot scaling signatures to first signature.
 */
#define SPECTRAL_SIGNATUREPLOT_SCALE_TO_FIRST_ACTION "SPECTRAL_SIGNATUREPLOT_SCALE_TO_FIRST_ACTION"

/**
 *  Default menu separator.
 */
#define SPECTRAL_SIGNATUREPLOT_SEPARATOR_ACTION "SPECTRAL_SIGNATUREPLOT_SEPARATOR_ACTION"

/**
 *  Toggles the displayed units in the signature plot.
 */
#define SPECTRAL_SIGNATUREPLOT_SIG_UNITS_ACTION "SPECTRAL_SIGNATUREPLOT_SIG_UNITS_ACTION"

/**
 *  Sets the displayed wavelength units in the signature plot.
 */
#define SPECTRAL_SIGNATUREPLOT_WAVE_UNITS_ACTION "SPECTRAL_SIGNATUREPLOT_WAVE_UNITS_ACTION"

/**
 *  Default menu separator.
 */
#define SPECTRAL_SIGNATUREPLOT_SEPARATOR2_ACTION "SPECTRAL_SIGNATUREPLOT_SEPARATOR2_ACTION"

/**
 *  Sets the display mode in the signature plot displaying data set signatures.
 */
#define SPECTRAL_SIGNATUREPLOT_DISPLAY_MODE_ACTION "SPECTRAL_SIGNATUREPLOT_DISPLAY_MODE_ACTION"

/**
 *  Default menu separator.
 */
#define SPECTRAL_SIGNATUREPLOT_SEPARATOR3_ACTION "SPECTRAL_SIGNATUREPLOT_SEPARATOR3_ACTION"

#endif

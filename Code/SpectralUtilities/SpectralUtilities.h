/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SPECTRALUTILITIES_H
#define SPECTRALUTILITIES_H

#include "Location.h"
#include "ProgressTracker.h"

#include <QtCore/qglobal.h>
#include <string>
#include <vector>

class AoiElement;
class BitMaskIterator;
class DataRequest;
class Progress;
class RasterElement;
class Signature;

/**
 * This namespace contains a number of convenience functions
 * for dealing with Signature objects and other spectral
 * related functionality.
 */
namespace SpectralUtilities
{
   /**
    *  Creates a vector of signatures from another vector containing signatures
    *  and/or signature sets.
    *
    *  This method extracts all signatures from any signature sets and creates
    *  a single vector of signatures.
    *
    *  @param   signatures
    *           The signatures, including any signature sets that should be
    *           extracted.
    *
    *  @return  A vector containing signatures with no signature sets.
    */
   std::vector<Signature*> extractSignatures(const std::vector<Signature*>& signatures);

   /**
    *  Creates a signature from a single pixel in a data set.
    *
    *  This method creates a Signature object with wavelength and reflectance
    *  data components based on the data values in each band of the given data
    *  set at the given pixel location.  If the data set does not contain
    *  wavelength data, an empty vector is set into the Signature object for
    *  the wavelength data component.
    *
    *  @param   pDataset
    *           The data set from which the signature should be created.
    *  @param   pixel
    *           The pixel location for which to create the signature from the
    *           data values in each band.
    *
    *  @return  The signature containing wavelength and reflectance data
    *           components.  \c NULL is returned if an error occurred accessing
    *           the data set data values.
    */
   Signature* getPixelSignature(RasterElement* pDataset, const Opticks::PixelLocation& pixel);

   /**
    *  Generate a signature from values in a raster element over an AOI.
    *
    *  Data points in \em pElement where \em pAoi is active are spatially averaged and used
    *  to populate \em pSignature. The number of signature values will correspond to
    *  the number of bands in \em pElement. If \em pElement does not have wavelength data
    *  nothing will happen to \em pSignature. If the number of bands in \em pElement is not
    *  equal to the number of wavelength points in the metadata, the longer will be
    *  truncated to ensure the wavelength and reflectance vectors in \em pSignature are the
    *  same length.
    *
    *  @param   pAoi
    *           Use this AOI to determine which points in \em pElement will be used to generate the signature.
    *           This must be non-\c NULL.
    *  @param   pSignature
    *           The destination signature. This must be non-\c NULL. The "Reflectance" and "Wavelength" data
    *           will be replaced with the new data from the AOI.
    *  @param   pElement
    *           The raster element used to generate the signature. If this is \c NULL and \em pAoi has a parent
    *           raster element, the parent raster element will be used to generate the signature. This
    *           must have wavelength metadata.
    *  @param   pProgress
    *           The Progress object to update.
    *  @param   pAbort
    *           The abort flag set by the progress object's Cancel button. This method will query the state
    *           of this flag during computations and will abort if the flag is \c true. If \em pAbort is \c NULL,
    *           the method will run to completion.
    *
    * @return   \c true if the signature data were calculated and set; \c false otherwise.
    */
   bool convertAoiToSignature(AoiElement* pAoi, Signature* pSignature, RasterElement* pElement,
                              Progress* pProgress = NULL, bool* pAbort = NULL);

   /**
    *  Generate signatures for all the selected pixels in an Area of Interest (AOI).
    *
    *  This method creates Signature objects with wavelength and reflectance
    *  data components based on the data values in each band of the given raster element
    *  for each selected pixel in the AOI.  If the raster element does not contain
    *  wavelength data, no signature objects are created.
    *
    *  @param   pAoi
    *           The AOI from which the signatures should be created.
    *  @param   pElement
    *           The raster element from which to create the signatures.
    *  @param   pProgress
    *           The Progress object to update.
    *  @param   pAbort
    *           The abort flag set by the progress object's Cancel button. This method will query the state
    *           of this flag during computations and will abort if the flag is \c true. If \em pAbort is \c NULL,
    *           the method will run to completion.
    *
    *  @return  The vector containing the signatures with wavelength and reflectance data components.
    *           The vector will be empty if the raster element does not contain wavelength information.
    */
    std::vector<Signature*> getAoiSignatures(const AoiElement* pAoi, RasterElement* pElement,
                                             Progress* pProgress = NULL, bool* pAbort = NULL);

   /**
    *  Generate an error message for a failed DataRequest.
    *
    *  @warning This method must be called before RasterElement::getDataAccessor().
    *
    *  @param   pRequest
    *           The failed DataRequest.
    *
    *  @param   pElement
    *           The RasterElement which reported a failure.
    *
    *  @return  A string containing suspected reasons for the failure.
    *           This string will be empty if no common errors were detected.
    */
   std::string getFailedDataRequestErrorMessage(const DataRequest* pRequest, const RasterElement* pElement);

#ifndef QT_NO_CONCURRENT
   /**
    *  Calculates the band means of a RasterElement using QtConcurrent.
    *
    *  @param   pElement
    *           The RasterElement on which the band means calculations will be performed.
    *  @param   iter
    *           A BitMaskIterator to note which pixels should be included in the mean calculation.
    *  @param   progress
    *           The ProgressTracker object to update.
    *  @param   pAbort
    *           This method will query the state of this flag during computations and will abort if the 
    *           flag is \c true. If \em pAbort is \c NULL, the method will run to completion.
    *
    *  @return  The output vector of doubles, one value per band holding the average
    *           of all pixels selected with the \em iter parameter.
    */
   std::vector<double> calculateMeans(const RasterElement* pElement, 
      BitMaskIterator& iter, ProgressTracker& progress, bool* pAbort = NULL);
#endif

   /**
    * Calculates the reflectance factor using the following equation:
    *   earthSunDistance = calculated using SpectralUtilities::determineEarthSunDistance
    *   solarZenithAngleInDegrees = 90 - solarElevationAngleInDegrees
    *   reflectance factor =                  (earthSunDistance)^2 * PI
    *                        -------------------------------------------------------------
    *                        solarIrradiance * cos(solarZenithAngleInDegrees * PI / 180.0)
    *
    * @param solarElevationAngleInDegrees
    *        The solar elevation angle in degrees, not radians
    * @param solarIrradiance
    *        The solar irradiance value, generally in W/m^2*um.  Must be the
    *        irradiance values for a Earth-Sun distance of 1 Astronomical Unit (AU).
    * @param date
    *        The date and time, in order to remove differences in solar illumination
    *        based upon the distance from the sun.
    *
    * @return The reflectance factor.  A value of 1.0 will be returned in the case that
    *         a divide-by-zero would otherwise occur.
    */
   double determineReflectanceConversionFactor(double solarElevationAngleInDegrees,
      double solarIrradiance, const DateTime& date);

   /**
    * Calculates the julian day using both the date and time information in pDate.
    *
    * @param dateTime
    *        The date and time.
    *
    * @return The resulting julian day.
    */
   double determineJulianDay(const DateTime& dateTime);

   /**
    * Calculates the earth-sun distance for the given date and time.
    *
    * @param date
    *        The date and time.
    *
    * @return The earth-sun distance in Astronomical Units (AU).
    */
   double determineEarthSunDistance(const DateTime& date);
}

#endif

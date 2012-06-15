/*
 * The information in this file is
 * Copyright(c) 2012 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef COMMONSIGNATUREMETADATAKEYS_H
#define COMMONSIGNATUREMETADATAKEYS_H

#include <string>

/**
 *  Provides methods to obtain the metadata key for common information about a spectral signature.
 */
class CommonSignatureMetadataKeys
{
public:
   /**
    *  A standard metadata key for resample information about a spectral signature.
    *
    *  Plug-ins that resample a spectral signature to the wavelengths of a dataset or
    *  need to know what dataset wavelengths a signature was resampled to can use this
    *  method to obtain a common metadata key for this information.
    *
    *  The key will not be present for spectral signatures that have not been resampled.
    *
    *  @return  Returns "ResampledTo".
    */
   static std::string ResampledTo();

   /**
    *  A standard metadata key for resample information about a spectral signature.
    *
    *  Plug-ins that resample a spectral signature or need to know if a resampled signature is
    *  padded with fill values (like pixel bad value it is a wavelength value that indicates
    *  no resampled value was calculated for a wavelength) can use this method to obtain a
    *  common metadata key for this information.
    *
    *  The key will not be present for spectral signatures that have not been resampled.
    *
    *  @return  Returns "FillValue".
    */
   static std::string FillValue();
};

#endif

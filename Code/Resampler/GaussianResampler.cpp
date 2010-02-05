/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppConfig.h"
#include "GaussianResampler.h"

#include <math.h>

double GaussianResampler::resamplePoint(IndexPair indices, double toWavelength, double toFwhm)
{
   unsigned int i;
   double value = 0.0;
   double scale = 0.0;
   double sigma = toFwhm / (2.0*sqrt(2.0*log(2.0)));
   
   for (i = 0; i < mFromData.size(); ++i)
   {
      double ratio = (toWavelength-mFromWavelengths[i])/sigma;
      double exponent = -ratio*ratio*0.5;
      double probability = 1.0 / (sigma * sqrt(2.0*PI)) * exp(exponent);
      scale += probability;
      value += mFromData[i] * probability;
   }

   value /= scale;

   return value;
}

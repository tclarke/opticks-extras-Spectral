/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "LinearInterpolator.h"

LinearInterpolator::LinearInterpolator(const std::vector<double>& fromWavelengths,
   const std::vector<double>& fromData, double dropOutWindow) :
   Interpolator(fromWavelengths, fromData, dropOutWindow)
{
   // Do nothing
}

double LinearInterpolator::resamplePoint(IndexPair indices, double toWavelength, double toFwhm)
{
   return mFromData[indices.mLeftIndex] +
      (toWavelength-mFromWavelengths[indices.mLeftIndex]) *
      (mFromData[indices.mRightIndex]-mFromData[indices.mLeftIndex]) /
      (mFromWavelengths[indices.mRightIndex]-mFromWavelengths[indices.mLeftIndex]);
}

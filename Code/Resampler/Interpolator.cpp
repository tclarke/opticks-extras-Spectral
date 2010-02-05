/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

#include "AppConfig.h"
#include "Interpolator.h"
#include "ResamplerOptions.h"

using namespace std;

Interpolator::Interpolator(const std::vector<double>& fromWavelengths,
   const std::vector<double>& fromData, double dropOutWindow) :
   mFromWavelengths(fromWavelengths), mFromData(fromData), mDropOutWindow(dropOutWindow) 
{
   // Do nothing
}

bool Interpolator::canExtrapolate(double sourceWavelength1,
   double sourceWavelength2, double destWavelength)
{
   if (sourceWavelength2-sourceWavelength1 < mDropOutWindow)
   {
      if (destWavelength < sourceWavelength1 && destWavelength + mDropOutWindow/5.0 > sourceWavelength1)
      {
         return true;
      }
      if (destWavelength > sourceWavelength2 && destWavelength - mDropOutWindow/5.0 < sourceWavelength2)
      {
         return true;
      }
   }

   return false;
}

bool Interpolator::constructorInputsAreValid(string& errorMessage)
{
   if (mDropOutWindow < 0.0)
   {
      errorMessage = "Drop Out Window must be at least 0.0.";
      return false;
   }

   if (mFromWavelengths.size() == 0)
   {
      errorMessage = "Signature has no wavelengths.";
      return false;
   }

   if (hasDuplicateValues(mFromWavelengths))
   {
      errorMessage = "Signature wavelengths have duplicate values.";
      return false;
   }
   
   if (mFromData.size() == 0)
   {
      errorMessage = "Signature has no data.";
      return false;
   }

   return true;
}

bool Interpolator::hasDuplicateValues(const std::vector<double>& values)
{
   std::vector<double>::const_iterator prev_iter;
   for (std::vector<double>::const_iterator iter=values.begin(); iter != values.end(); ++iter)
   {
      if (iter != values.begin())
      {
         if (*iter == *prev_iter)
         {
            return true;
         }
      }
      prev_iter = iter;
   }
   return false;
}

bool Interpolator::run(const std::vector<double>& toWavelengths, const std::vector<double>& toFwhm, 
                       std::vector<double>& toData, std::vector<int>& toBands, string& errorMessage)
{
   if (constructorInputsAreValid(errorMessage) == false)
   {
      return false;
   }

   toData.clear();
   toBands.clear();
   toData.reserve(toWavelengths.size());
   toBands.reserve(toWavelengths.size());

   double defaultFwhm = ResamplerOptions::getSettingFullWidthHalfMax();
   unsigned int i;
   for (i = 0; i < toWavelengths.size(); ++i)
   {
      IndexPair indices;
      if (getSourceIndices(toWavelengths[i], indices, errorMessage) == false)
      {
         return false;
      }

      if (indices.mLeftIndex != -1)
      {
         double value = 0.0;
         if (indices.mLeftIndex == indices.mRightIndex)
         {
            value = mFromData[indices.mLeftIndex];
         }
         else
         {
            double fwhm=toFwhm.size() == 0? defaultFwhm : toFwhm[i];
            value = resamplePoint(indices, toWavelengths[i], fwhm);
         }
         toData.push_back(value);
         toBands.push_back(i);
      }
   }

   if (toBands.size() == 0)
   {
      errorMessage = "No bands could be resampled.";
      return false;
   }

   return true;
}

/*
1) if the point is outside the wavelength coverage, but there are two good points at the end and the point is within window/2 from the end, extrapolate
2) find point on either side
2a) if no point to the left, see if we can simply use it (ie we are w/in window/20 from it)
2b) if no point to the right, se if we can simply use it
2c) we have found a point to the left and right of the point to resample to so,
2d) if we are not in a drop-out, linearly interpolate
2e) if we are in a drop-out, see if we can extrapolate from the two points to the left, or the two points to the right
2f) see if we can simply use either of the two points (ie we are w/in window/20 from it)
3) if none of the above succeeds, we can't resample
*/
bool Interpolator::getSourceIndices(double toWavelength, IndexPair& pair, string& errorMessage)
{
   pair.mLeftIndex = pair.mRightIndex = -1;

   if (mFromWavelengths.size() == 0)
   {
      errorMessage = "Cannot get Wavelengths";
      return false;
   }

   if (toWavelength < mFromWavelengths[0] || toWavelength > mFromWavelengths[mFromWavelengths.size()-1])
   {
      if (canExtrapolate(mFromWavelengths[0], mFromWavelengths[1], toWavelength))
      {
         pair.mLeftIndex = 0;
         pair.mRightIndex = 1;
      }
      else if (canExtrapolate(mFromWavelengths[mFromWavelengths.size()-2], mFromWavelengths[mFromWavelengths.size()-1], toWavelength))
      {
         pair.mLeftIndex = mFromWavelengths.size()-2;
         pair.mRightIndex = mFromWavelengths.size()-1;
      }
   }
   else
   {
      unsigned int i;
      for (i = 0; i < mFromWavelengths.size(); ++i)
      {
         if (mFromWavelengths[i]>toWavelength)
         {
            if (i == 0)
            {
               if (canUseSinglePoint(mFromWavelengths[i], toWavelength))
               {
                  pair.mLeftIndex = pair.mRightIndex = i;
                  break;
               }
            }
            else if (mFromWavelengths[i]-mFromWavelengths[i-1] < mDropOutWindow)
            {
               pair.mLeftIndex = i-1;
               pair.mRightIndex = i;
               break;
            }
            else if (i>=2 && canExtrapolate(mFromWavelengths[i-2],mFromWavelengths[i-1],toWavelength))
            {
               pair.mLeftIndex = i-2;
               pair.mRightIndex = i-1;
               break;
            }
            else if (i<=mFromWavelengths.size()-2 && canExtrapolate(mFromWavelengths[i],mFromWavelengths[i+1],toWavelength))
            {
               pair.mLeftIndex = i;
               pair.mRightIndex = i+1;
               break;
            }
            else if (canUseSinglePoint(mFromWavelengths[i], toWavelength))
            {
               pair.mLeftIndex = pair.mRightIndex = i;
               break;
            }
            else if (i>0 && canUseSinglePoint(mFromWavelengths[i-1], toWavelength))
            {
               pair.mLeftIndex = pair.mRightIndex = i-1;
               break;
            }
            else
            {
               break;
            }
         }
      }
      if (i == mFromWavelengths.size())
      {
         if (canUseSinglePoint(mFromWavelengths[i-1], toWavelength))
         {
            pair.mLeftIndex = pair.mRightIndex = i-1;
         }
      }
   }

   return true;
}

bool Interpolator::canUseSinglePoint(double fromWavelength, double toWavelength)
{
   return fabs(fromWavelength-toWavelength) < mDropOutWindow/20.0;
}

bool Interpolator::noResamplingNecessary(const std::vector<double>& toWavelengths)
{
   if (mFromWavelengths.size() == toWavelengths.size())
   {
      std::vector<double>::const_iterator fromIter, toIter;
      for (fromIter=mFromWavelengths.begin(),toIter=toWavelengths.begin(); fromIter != mFromWavelengths.end(); ++fromIter, ++toIter)
      {
         if (!canUseSinglePoint(*fromIter, *toIter))
         {
            return false;
         }
      }
      return true;
   }
   return false;
}

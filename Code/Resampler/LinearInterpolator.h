/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef LINEARINTERPOLATOR_H
#define LINEARINTERPOLATOR_H

#include "Interpolator.h"

class LinearInterpolator : public Interpolator
{
public:
   LinearInterpolator(const std::vector<double>& fromWavelengths,
      const std::vector<double>& fromData, double dropOutWindow);

private:
   double resamplePoint(IndexPair indices, double toWavelength, double toFwhm);
};

#endif

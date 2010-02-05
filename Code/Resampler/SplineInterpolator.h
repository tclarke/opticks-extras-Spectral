/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SPLINEINTERPOLATOR_H
#define SPLINEINTERPOLATOR_H

#include "Interpolator.h"

class SplineInterpolator : public Interpolator
{
public:
   SplineInterpolator(const std::vector<double>& fromWavelengths,
      const std::vector<double>& fromData, double dropOutWindow);
private:
   double resamplePoint(IndexPair indices, double toWavelength, double toFwhm);

   static void spline(const std::vector<double>& x, const std::vector<double>& y, int n,
      double yp1, double ypn, std::vector<double>& y2);

   static double splint(int n, const std::vector<double>& xorgn,
      const std::vector<double>& yorgn, const std::vector<double>& y2, double x);

   std::vector<double> y2;
};


#endif

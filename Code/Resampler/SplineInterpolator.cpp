/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "SplineInterpolator.h"

using namespace std;
SplineInterpolator::SplineInterpolator(const vector<double>& fromWavelengths,
   const vector<double>& fromData, double dropOutWindow) :
   Interpolator(fromWavelengths, fromData, dropOutWindow), y2(fromWavelengths.size())
{
   const double endPointDerivative = 2.0e30; // signals endpoint second derivative = 0.0

   spline(fromWavelengths, fromData, fromData.size(), endPointDerivative, endPointDerivative, y2);
}

double SplineInterpolator::resamplePoint(IndexPair indices, double toWavelength, double toFwhm)
{
   return splint(mFromData.size(), mFromWavelengths, mFromData, y2, toWavelength);
}

void SplineInterpolator::spline(const vector<double>& x, const vector<double>& y,
   int n, double yp1, double ypn, vector<double>& y2)
{
   int i, k;
   double p, qn, sig, un;
   vector<double> u(n);

   if (yp1 > 0.99e30)
   {
      y2[0] = u[0] = 0.0;
   }
   else
   {
      y2[0] = -0.5;
      u[0]  = (3.0/(x[1]-x[0]))*((y[1]-y[0])/(x[1]-x[0])-yp1);
   }

   for (i=1; i<n-1; i++)
   {
      sig=(x[i]-x[i-1])/(x[i+1]-x[i-1]);
      p = sig*y2[i-1]+2.0;
      y2[i]=(sig-1.0)/p;
      u[i]=(y[i+1]-y[i])/(x[i+1]-x[i]) - (y[i]-y[i-1])/(x[i]-x[i-1]);
      u[i]=(6.0*u[i]/(x[i+1]-x[i-1])-sig*u[i-1])/p;
   }

   if (ypn > 0.99e30)
   {
      qn = un = 0.0;
   }
   else
   {
      qn = 0.5;
      un = (3.0/(x[n-1]-x[n-2]))*(ypn-(y[n-1]-y[n-2])/(x[n-1]-x[n-2]));
   }

   y2[n-1] = (un-qn*u[n-2])/(qn*y2[n-2]+1.0);
   for (k=n-2; k>=0; k--)
   {
      y2[k] = y2[k]*y2[k+1]+u[k];
   }
}

double SplineInterpolator::splint(int n, const vector<double>& xorgn,
   const vector<double>& yorgn, const vector<double>& y2, double x)
{
   int k, klo, khi;
   double h, a, b;

   klo = 0;
   khi = n-1;
   while (khi-klo > 1) 
   {
      k=(khi+klo) >> 1;
      if (xorgn[k] > x)
      {
         khi=k;
      }
      else
      {
         klo=k;
      }
   }
   h = xorgn[khi]-xorgn[klo];
   a = (xorgn[khi]-x)/h;
   b = (x-xorgn[klo])/h;
   return a*yorgn[klo]+b*yorgn[khi]+((a*a*a-a)*y2[klo]+(b*b*b-b)*y2[khi])*(h*h)/6.0;
}

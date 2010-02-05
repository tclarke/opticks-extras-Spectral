/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef INTERPOLATOR_H
#define INTERPOLATOR_H

#include <string>
#include <vector>

struct IndexPair
{
   int mLeftIndex, mRightIndex;
};

class Interpolator
{
public:
   Interpolator(const std::vector<double>& fromWavelengths,
      const std::vector<double>& fromData, double dropOutWindow);

   bool run(const std::vector<double>& toWavelengths, const std::vector<double>& toFwhm, 
      std::vector<double>& toData, std::vector<int>& toBands, std::string& errorMessage);

   bool noResamplingNecessary(const std::vector<double>& toWavelengths);

   const std::vector<double>& mFromWavelengths;
   const std::vector<double>& mFromData;
   const double mDropOutWindow;

protected:
   virtual double resamplePoint(IndexPair indices, double toWavelength, double toFwhm) = 0;

private:
   bool constructorInputsAreValid(std::string& errorMessage);


   bool getSourceIndices(double toWavelength, IndexPair& pair, std::string& errorMessage);

   inline bool canUseSinglePoint(double fromWavelength, double toWavelength);

   inline bool canExtrapolate(double sourceWavelength1, double sourceWavelength2, double destWavelength);
   static bool hasDuplicateValues(const std::vector<double>& values);
};

#endif

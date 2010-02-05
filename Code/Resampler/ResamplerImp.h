/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RESAMPLERIMP_H
#define RESAMPLERIMP_H

#include "Resampler.h"
#include "PlugInShell.h"
#include "Testable.h"

class ResamplerImp : public PlugInShell, public Resampler, public Testable
{
public:
   ResamplerImp();
   ~ResamplerImp();

   bool execute(const std::vector<double>& fromData, std::vector<double>& toData,
      const std::vector<double>& fromWavelengths, const std::vector<double>& toWavelengths, 
      const std::vector<double>& toFwhm, std::vector<int>& toBands, std::string& errorMessage);

   bool execute(const std::vector<double>& fromData, std::vector<double>& toData,
      const std::vector<double>& fromWavelengths, const std::vector<double>& toWavelengths, 
      const std::vector<double>& toFwhm, std::vector<int>& toBands, std::string& errorMessage,
      const std::string& resamplerMethod);

   bool runOperationalTests(Progress* pProgress, std::ostream& failure) ;
   bool runAllTests(Progress* pProgress, std::ostream& failure) ;

private:
   bool runTest1(std::ostream& failure);
   bool runTest2(std::ostream& failure);
   bool runTest3(std::ostream& failure);
   bool runTest4(std::ostream& failure);
   bool runTest5(std::ostream& failure);
   bool runTest6(std::ostream& failure);
   bool runTest7(std::ostream& failure);
   bool runTest8(std::ostream& failure);
   bool runTest9(std::ostream& failure);
   bool runTest10(std::ostream& failure);
   bool runTest11(std::ostream& failure);
   bool runTest12(std::ostream& failure);
   bool runTest13(std::ostream& failure);
   bool runTest14(std::ostream& failure);
   bool runTest15(std::ostream& failure);
   bool runTest16(std::ostream& failure);

   bool runPositiveTest(const std::string& testName, std::ostream& failure, const std::vector<double>& expectedData,
      const std::vector<int>& expectedBands, const std::vector<double>& fromData, std::vector<double>& toData,
      const std::vector<double>& fromWavelengths, const std::vector<double>& toWavelengths, 
      const std::vector<double>& toFwhm, std::vector<int>& toBands, std::string& errorMessage,
      const std::string& resamplerMethod, const double& tolerance = 1e-6);

   bool runNegativeTest(const std::string& testName, std::ostream& failure, const std::string& expectedError,
      const std::vector<double>& fromData, std::vector<double>& toData,
      const std::vector<double>& fromWavelengths, const std::vector<double>& toWavelengths, 
      const std::vector<double>& toFwhm, std::vector<int>& toBands, std::string& errorMessage,
      const std::string& resamplerMethod);

   bool sortInputData(const std::vector<double>& fromWavelengths, const std::vector<double>& fromData, 
                 const std::vector<double>& toWavelengths, const std::vector<double>& toFwhm,
                 std::vector<double>& sortedFromWavelengths, std::vector<double>& sortedFromData, 
                 std::vector<double>& sortedToWavelengths, std::vector<double>& sortedToFwhm,
                 std::vector<int>& sortedToBands, std::string& errorMessage);

   void sortOutputData(const std::vector<int>& sortedToBands, std::vector<int>& toBands, std::vector<double>& toData);

   // construct vector of wavelength/fwhm/index triplets
   struct Triplet
   {
      double mWavelength, mFwhm;
      int mBand;
      bool operator<(const Triplet&  other) const { return mWavelength < other.mWavelength; }
   };
};

#endif

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "GaussianResampler.h"
#include "LinearInterpolator.h"
#include "PlugInRegistration.h"
#include "Progress.h"
#include "ResamplerImp.h"
#include "ResamplerOptions.h"
#include "SpectralVersion.h"
#include "SplineInterpolator.h"

using namespace std;

REGISTER_PLUGIN_BASIC(SpectralResampler, ResamplerImp);

ResamplerImp::ResamplerImp()
{
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setName("Resampler");
   setType("Resampler");
   setDescription("Resample data from one set of wavelengths to another.");
   setShortDescription("Resample data from one set of wavelengths to another.");
   setDescriptorId("{3DDBCE64-12A8-4a84-B795-E83ED08F9010}");
   allowMultipleInstances(true);
}

ResamplerImp::~ResamplerImp()
{
   // Do nothing
}

bool ResamplerImp::runOperationalTests(Progress* pProgress, ostream& failure) 
{
   return runAllTests(pProgress, failure);
}

bool ResamplerImp::runAllTests(Progress* pProgress, ostream& failure) 
{
   double percent = 0.0;
   const int numTests = 16;
   const double step = 100.0 / numTests;

   if (runTest1(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest2(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest3(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest4(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest5(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest6(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest7(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest8(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest9(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest10(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest11(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest12(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest13(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest14(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest15(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      percent += step;
      pProgress->updateProgress("Running Resampler Tests...", static_cast<int>(percent), NORMAL);
   }

   if (runTest16(failure) == false)
   {
      return false;
   }

   if (pProgress != NULL)
   {
      pProgress->updateProgress("Resampler Tests Complete", percent, NORMAL);
   }

   return true;
}

bool ResamplerImp::runTest1(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(1);
   fromData.push_back(4);
   fromData.push_back(6);
   fromData.push_back(5);
   fromData.push_back(3);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(.40);
   fromWavelengths.push_back(.41);
   fromWavelengths.push_back(.42);
   fromWavelengths.push_back(.43);
   fromWavelengths.push_back(.44);
   fromWavelengths.push_back(.45);

   vector<double> toWavelengths;
   toWavelengths.push_back(.4075);
   toWavelengths.push_back(.4150);
   toWavelengths.push_back(.4250);
   toWavelengths.push_back(.4350);
   toWavelengths.push_back(.4450);

   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Expected output
   vector<double> expectedData;
   expectedData.push_back(0.75);
   expectedData.push_back(2.50);
   expectedData.push_back(5.00);
   expectedData.push_back(5.50);
   expectedData.push_back(4.00);

   vector<int> expectedBands;
   expectedBands.push_back(0);
   expectedBands.push_back(1);
   expectedBands.push_back(2);
   expectedBands.push_back(3);
   expectedBands.push_back(4);

   // Run the test
   if (runPositiveTest("ResamplerTestCase1", failure, expectedData, expectedBands, fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::LinearMethod()) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest2(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(1);
   fromData.push_back(4);
   fromData.push_back(6);
   fromData.push_back(5);
   fromData.push_back(3);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(.40);
   fromWavelengths.push_back(.41);
   fromWavelengths.push_back(.42);
   fromWavelengths.push_back(.43);
   fromWavelengths.push_back(.44);
   fromWavelengths.push_back(.45);

   vector<double> toWavelengths;
   toWavelengths.push_back(.385);
   toWavelengths.push_back(.395);
   toWavelengths.push_back(.405);
   toWavelengths.push_back(.415);
   toWavelengths.push_back(.425);
   toWavelengths.push_back(.435);
   toWavelengths.push_back(.445);
   toWavelengths.push_back(.455);
   toWavelengths.push_back(.465);

   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Expected output
   vector<double> expectedData;
   expectedData.push_back(-0.5);
   expectedData.push_back( 0.5);
   expectedData.push_back( 2.5);
   expectedData.push_back( 5.0);
   expectedData.push_back( 5.5);
   expectedData.push_back( 4.0);
   expectedData.push_back( 2.0);

   vector<int> expectedBands;
   expectedBands.push_back(1);
   expectedBands.push_back(2);
   expectedBands.push_back(3);
   expectedBands.push_back(4);
   expectedBands.push_back(5);
   expectedBands.push_back(6);
   expectedBands.push_back(7);

   // Run the test
   if (runPositiveTest("ResamplerTestCase2", failure, expectedData, expectedBands, fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::LinearMethod()) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest3(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(1);
   fromData.push_back(4);
   fromData.push_back(6);
   fromData.push_back(5);
   fromData.push_back(3);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(.40);
   fromWavelengths.push_back(.41);
   fromWavelengths.push_back(.42);
   fromWavelengths.push_back(.53);
   fromWavelengths.push_back(.54);
   fromWavelengths.push_back(.55);

   vector<double> toWavelengths;
   toWavelengths.push_back(.405);
   toWavelengths.push_back(.415);
   toWavelengths.push_back(.425);
   toWavelengths.push_back(.435);
   toWavelengths.push_back(.460);
   toWavelengths.push_back(.515);
   toWavelengths.push_back(.525);
   toWavelengths.push_back(.535);
   toWavelengths.push_back(.545);

   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Expected output
   vector<double> expectedData;
   expectedData.push_back(0.50);
   expectedData.push_back(2.50);
   expectedData.push_back(5.50);
   expectedData.push_back(6.50);
   expectedData.push_back(5.50);
   expectedData.push_back(4.00);

   vector<int> expectedBands;
   expectedBands.push_back(0);
   expectedBands.push_back(1);
   expectedBands.push_back(2);
   expectedBands.push_back(6);
   expectedBands.push_back(7);
   expectedBands.push_back(8);

   // Run the test
   if (runPositiveTest("ResamplerTestCase3", failure, expectedData, expectedBands, fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::LinearMethod()) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest4(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(1);
   fromData.push_back(4);
   fromData.push_back(6);
   fromData.push_back(5);
   fromData.push_back(3);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(.40);
   fromWavelengths.push_back(.51);
   fromWavelengths.push_back(.52);
   fromWavelengths.push_back(.53);
   fromWavelengths.push_back(.54);
   fromWavelengths.push_back(.55);

   vector<double> toWavelengths;
   toWavelengths.push_back(.385);
   toWavelengths.push_back(.395);
   toWavelengths.push_back(.405);
   toWavelengths.push_back(.415);
   toWavelengths.push_back(.495);
   toWavelengths.push_back(.505);
   toWavelengths.push_back(.515);
   toWavelengths.push_back(.525);
   toWavelengths.push_back(.535);
   toWavelengths.push_back(.545);
   toWavelengths.push_back(.555);
   toWavelengths.push_back(.565);
   toWavelengths.push_back(.635);
   toWavelengths.push_back(.645);
   toWavelengths.push_back(.655);
   toWavelengths.push_back(.665);


   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Expected output
   vector<double> expectedData;
   expectedData.push_back(-0.5);
   expectedData.push_back( 2.5);
   expectedData.push_back( 5.0);
   expectedData.push_back( 5.5);
   expectedData.push_back( 4.0);
   expectedData.push_back( 2.0);

   vector<int> expectedBands;
   expectedBands.push_back(5);
   expectedBands.push_back(6);
   expectedBands.push_back(7);
   expectedBands.push_back(8);
   expectedBands.push_back(9);
   expectedBands.push_back(10);

   // Run the test
   if (runPositiveTest("ResamplerTestCase4", failure, expectedData, expectedBands, fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::LinearMethod()) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest5(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(1);
   fromData.push_back(4);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(.4);
   fromWavelengths.push_back(.6);
   fromWavelengths.push_back(.8);

   vector<double> toWavelengths;
   toWavelengths.push_back(.4);
   toWavelengths.push_back(.6);
   toWavelengths.push_back(.8);

   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Expected output
   vector<double> expectedData;
   expectedData.push_back(0);
   expectedData.push_back(1);
   expectedData.push_back(4);

   vector<int> expectedBands;
   expectedBands.push_back(0);
   expectedBands.push_back(1);
   expectedBands.push_back(2);

   // Run the test
   if (runPositiveTest("ResamplerTestCase5", failure, expectedData, expectedBands, fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::LinearMethod()) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest6(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(1);
   fromData.push_back(4);
      
   vector<double> fromWavelengths;
   fromWavelengths.push_back(.40);
   fromWavelengths.push_back(.41);
   fromWavelengths.push_back(.42);

   vector<double> toWavelengths;
   toWavelengths.push_back(.50);
   toWavelengths.push_back(.51);
   toWavelengths.push_back(.52);

   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Run the test
   if (runNegativeTest("ResamplerTestCase6", failure, "No bands could be resampled.", fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::LinearMethod()) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest7(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(1);
   fromData.push_back(4);
   fromData.push_back(6);
   fromData.push_back(5);
   fromData.push_back(3);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(.40);
   fromWavelengths.push_back(.41);
   fromWavelengths.push_back(.42);
   fromWavelengths.push_back(.43);
   fromWavelengths.push_back(.44);
   fromWavelengths.push_back(.45);
      
   vector<double> toWavelengths;
   toWavelengths.push_back(.385);
   toWavelengths.push_back(.395);
   toWavelengths.push_back(.405);
   toWavelengths.push_back(.415);
   toWavelengths.push_back(.425);
   toWavelengths.push_back(.435);
   toWavelengths.push_back(.445);
   toWavelengths.push_back(.455);
   toWavelengths.push_back(.465);

   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Expected output
   vector<double> expectedData;
   expectedData.push_back(-0.291866028708);
   expectedData.push_back( 0.291866028708);
   expectedData.push_back( 2.374401913876);
   expectedData.push_back( 5.335526315789);
   expectedData.push_back( 5.783492822967);
   expectedData.push_back( 4.030502392344);
   expectedData.push_back( 1.969497607656);

   vector<int> expectedBands;
   expectedBands.push_back(1);
   expectedBands.push_back(2);
   expectedBands.push_back(3);
   expectedBands.push_back(4);
   expectedBands.push_back(5);
   expectedBands.push_back(6);
   expectedBands.push_back(7);

   // Run the test
   if (runPositiveTest("ResamplerTestCase7", failure, expectedData, expectedBands, fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::CubicSplineMethod()) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest8(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(1);
   fromData.push_back(4);
   fromData.push_back(5);
   fromData.push_back(4);
   fromData.push_back(2);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(.40);
   fromWavelengths.push_back(.41);
   fromWavelengths.push_back(.42);
   fromWavelengths.push_back(.41);
   fromWavelengths.push_back(.43);
   fromWavelengths.push_back(.44);

   vector<double> toWavelengths;
   toWavelengths.push_back(.50);
   toWavelengths.push_back(.51);
   toWavelengths.push_back(.52);

   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Run the test
   if (runNegativeTest("ResamplerTestCase8", failure, "Signature wavelengths have duplicate values.", fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::LinearMethod()) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest9(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(1);
   fromData.push_back(4);
   fromData.push_back(5);
   fromData.push_back(4);
   fromData.push_back(2);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(.40);
   fromWavelengths.push_back(.41);
   fromWavelengths.push_back(.42);
   fromWavelengths.push_back(.43);
   fromWavelengths.push_back(.44);
      
   vector<double> toWavelengths;
   toWavelengths.push_back(.50);
   toWavelengths.push_back(.51);
   toWavelengths.push_back(.52);

   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Run the test
   if (runNegativeTest("ResamplerTestCase9", failure,
      "Number of input data values differs from number of input wavelengths.", fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::LinearMethod()) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest10(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(1);
   fromData.push_back(4);
   fromData.push_back(6);
   fromData.push_back(5);
   fromData.push_back(3);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(.40);
   fromWavelengths.push_back(.41);
   fromWavelengths.push_back(.42);
   fromWavelengths.push_back(.43);
   fromWavelengths.push_back(.44);
   fromWavelengths.push_back(.45);

   vector<double> toWavelengths;
   toWavelengths.push_back(.4075);
   toWavelengths.push_back(.4150);
   toWavelengths.push_back(.4250);
   toWavelengths.push_back(.4350);
   toWavelengths.push_back(.4450);

   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Expected output
   vector<double> expectedData;
   expectedData.push_back(0.839510061);
   expectedData.push_back(2.501945599);
   expectedData.push_back(4.992217691);
   expectedData.push_back(5.492217765);
   expectedData.push_back(4.003898635);

   vector<int> expectedBands;
   expectedBands.push_back(0);
   expectedBands.push_back(1);
   expectedBands.push_back(2);
   expectedBands.push_back(3);
   expectedBands.push_back(4);

   // Run the test
   if (runPositiveTest("ResamplerTestCase10", failure, expectedData, expectedBands, fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::GaussianMethod()) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest11(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(1);
   fromData.push_back(4);
   fromData.push_back(6);
   fromData.push_back(5);
   fromData.push_back(3);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(.40);
   fromWavelengths.push_back(.41);
   fromWavelengths.push_back(.42);
   fromWavelengths.push_back(.43);
   fromWavelengths.push_back(.44);
   fromWavelengths.push_back(.45);

   vector<double> toWavelengths;
   toWavelengths.push_back(.4075);
   toWavelengths.push_back(.4150);
   toWavelengths.push_back(.4250);
   toWavelengths.push_back(.4350);
   toWavelengths.push_back(.4450);

   vector<double> toFwhm;
   toFwhm.push_back(0.020);
   toFwhm.push_back(0.015);
   toFwhm.push_back(0.010);
   toFwhm.push_back(0.015);
   toFwhm.push_back(0.025);

   vector<double> toData;
   vector<int> toBands;
   string errorMessage;

   // Expected output
   vector<double> expectedData;
   expectedData.push_back(1.244945068);
   expectedData.push_back(2.539888968);
   expectedData.push_back(4.992217691);
   expectedData.push_back(5.342002258);
   expectedData.push_back(4.325097502);

   vector<int> expectedBands;
   expectedBands.push_back(0);
   expectedBands.push_back(1);
   expectedBands.push_back(2);
   expectedBands.push_back(3);
   expectedBands.push_back(4);

   // Run the test
   if (runPositiveTest("ResamplerTestCase11", failure, expectedData, expectedBands, fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::GaussianMethod()) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest12(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(5);
   fromData.push_back(1);
   fromData.push_back(4);
   fromData.push_back(6);
   fromData.push_back(3);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(.40);
   fromWavelengths.push_back(.44);
   fromWavelengths.push_back(.41);
   fromWavelengths.push_back(.42);
   fromWavelengths.push_back(.43);
   fromWavelengths.push_back(.45);

   vector<double> toWavelengths;
   toWavelengths.push_back(.4075);
   toWavelengths.push_back(.4450);
   toWavelengths.push_back(.4150);
   toWavelengths.push_back(.4250);
   toWavelengths.push_back(.4350);

   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Expected output
   vector<double> expectedData;
   expectedData.push_back(0.75);
   expectedData.push_back(4.00);
   expectedData.push_back(2.50);
   expectedData.push_back(5.00);
   expectedData.push_back(5.50);

   vector<int> expectedBands;
   expectedBands.push_back(0);
   expectedBands.push_back(1);
   expectedBands.push_back(2);
   expectedBands.push_back(3);
   expectedBands.push_back(4);

   // Run the test
   if (runPositiveTest("ResamplerTestCase12", failure, expectedData, expectedBands, fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::LinearMethod()) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest13(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(5);
   fromData.push_back(1);
   fromData.push_back(4);
   fromData.push_back(6);
   fromData.push_back(3);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(0.0);
   fromWavelengths.push_back(0.0);
   fromWavelengths.push_back(0.0);
   fromWavelengths.push_back(0.0);
   fromWavelengths.push_back(0.0);
   fromWavelengths.push_back(0.0);

   vector<double> toWavelengths;
   toWavelengths.push_back(0.0);
   toWavelengths.push_back(0.0);
   toWavelengths.push_back(0.0);
   toWavelengths.push_back(0.0);
   toWavelengths.push_back(0.0);
   toWavelengths.push_back(0.0);

   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Expected output
   vector<double> expectedData;
   expectedData.push_back(0);
   expectedData.push_back(5);
   expectedData.push_back(1);
   expectedData.push_back(4);
   expectedData.push_back(6);
   expectedData.push_back(3);

   vector<int> expectedBands;
   expectedBands.push_back(0);
   expectedBands.push_back(1);
   expectedBands.push_back(2);
   expectedBands.push_back(3);
   expectedBands.push_back(4);
   expectedBands.push_back(5);

   // Run the test
   if (runPositiveTest("ResamplerTestCase13", failure, expectedData, expectedBands, fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::LinearMethod(), 0.0) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest14(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(5);
   fromData.push_back(1);
   fromData.push_back(4);
   fromData.push_back(6);
   fromData.push_back(3);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(.40);
   fromWavelengths.push_back(.50);
   fromWavelengths.push_back(.60);
   fromWavelengths.push_back(.70);
   fromWavelengths.push_back(.90);
   fromWavelengths.push_back(.80);

   vector<double> toWavelengths;
   toWavelengths.push_back(.40);
   toWavelengths.push_back(.50);
   toWavelengths.push_back(.60);
   toWavelengths.push_back(.70);
   toWavelengths.push_back(.90);
   toWavelengths.push_back(.80);

   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Expected output
   vector<double> expectedData;
   expectedData.push_back(0);
   expectedData.push_back(5);
   expectedData.push_back(1);
   expectedData.push_back(4);
   expectedData.push_back(6);
   expectedData.push_back(3);

   vector<int> expectedBands;
   expectedBands.push_back(0);
   expectedBands.push_back(1);
   expectedBands.push_back(2);
   expectedBands.push_back(3);
   expectedBands.push_back(4);
   expectedBands.push_back(5);

   // Run the test
   if (runPositiveTest("ResamplerTestCase14", failure, expectedData, expectedBands, fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::LinearMethod(), 0.0) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest15(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(0);
   fromData.push_back(5);
   fromData.push_back(1);
   fromData.push_back(4);
   fromData.push_back(6);
   fromData.push_back(3);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(.40);
   fromWavelengths.push_back(.50);
   fromWavelengths.push_back(.60);
   fromWavelengths.push_back(.70);
   fromWavelengths.push_back(.90);
   fromWavelengths.push_back(.80);

   vector<double> toWavelengths;
   toWavelengths.push_back(.40);
   toWavelengths.push_back(.60);
   toWavelengths.push_back(.50);
   toWavelengths.push_back(.70);
   toWavelengths.push_back(.90);
   toWavelengths.push_back(.80);

   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Expected output
   vector<double> expectedData;
   expectedData.push_back(0);
   expectedData.push_back(1);
   expectedData.push_back(5);
   expectedData.push_back(4);
   expectedData.push_back(6);
   expectedData.push_back(3);

   vector<int> expectedBands;
   expectedBands.push_back(0);
   expectedBands.push_back(1);
   expectedBands.push_back(2);
   expectedBands.push_back(3);
   expectedBands.push_back(4);
   expectedBands.push_back(5);

   // Run the test
   if (runPositiveTest("ResamplerTestCase15", failure, expectedData, expectedBands, fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::LinearMethod()) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runTest16(ostream& failure)
{
   // Input
   vector<double> fromData;
   fromData.push_back(10);
   fromData.push_back(11);
   fromData.push_back(12);
   fromData.push_back(13);
   fromData.push_back(14);
   fromData.push_back(15);
   fromData.push_back(16);

   vector<double> fromWavelengths;
   fromWavelengths.push_back(1);
   fromWavelengths.push_back(2);
   fromWavelengths.push_back(5);
   fromWavelengths.push_back(3);
   fromWavelengths.push_back(6);
   fromWavelengths.push_back(7);
   fromWavelengths.push_back(8);

   vector<double> toWavelengths;
   toWavelengths.push_back(1);
   toWavelengths.push_back(12);
   toWavelengths.push_back(3);
   toWavelengths.push_back(4);
   toWavelengths.push_back(8);
   toWavelengths.push_back(5);
   toWavelengths.push_back(6);
   toWavelengths.push_back(7);

   vector<double> toData;
   vector<double> toFwhm;
   vector<int> toBands;
   string errorMessage;

   // Expected output
   vector<double> expectedData;
   expectedData.push_back(10);
   expectedData.push_back(13);
   expectedData.push_back(16);
   expectedData.push_back(12);
   expectedData.push_back(14);
   expectedData.push_back(15);

   vector<int> expectedBands;
   expectedBands.push_back(0);
   expectedBands.push_back(2);
   expectedBands.push_back(4);
   expectedBands.push_back(5);
   expectedBands.push_back(6);
   expectedBands.push_back(7);

   // Run the test
   if (runPositiveTest("ResamplerTestCase16", failure, expectedData, expectedBands, fromData, toData,
      fromWavelengths, toWavelengths, toFwhm, toBands, errorMessage, ResamplerOptions::LinearMethod(), 0.0) == false)
   {
      return false;
   }

   return true;
}

bool ResamplerImp::runPositiveTest(const string& testName, ostream& failure, const vector<double>& expectedData,
   const vector<int>& expectedBands, const vector<double>& fromData, vector<double>& toData,
   const vector<double>& fromWavelengths, const vector<double>& toWavelengths, 
   const vector<double>& toFwhm, vector<int>& toBands, string& errorMessage,
   const string& resamplerMethod, const double& tolerance)
{
   if (execute(fromData, toData, fromWavelengths, toWavelengths,
      toFwhm, toBands, errorMessage, resamplerMethod) == false)
   {
      failure << testName << " failed. Resampler reported \"" << errorMessage << "\".";
      return false;
   }

   if (errorMessage.empty() == false)
   {
      failure << testName << " returned true, but Resampler reported \"" << errorMessage << "\".";
      return false;
   }

   if (toData.size() != expectedData.size())
   {
      failure << testName << " failed. Output data is not of the expected size.";
      return false;
   }

   for (unsigned int i = 0; i < expectedData.size(); i++)
   {
      if (fabs(expectedData[i] - toData[i]) > tolerance)
      {
         failure << testName << " failed. Output data does not match expected data.";
         return false;
      }
   }

   if (toBands.size() != expectedBands.size())
   {
      failure << testName << " failed. Output bands are not of the expected size.";
      return false;
   }

   for (unsigned int i = 0; i < expectedBands.size(); i++)
   {
      if (expectedBands[i] != toBands[i])
      {
         failure << testName << " failed. Output bands do not match expected bands.";
         return false;
      }
   }

   return true;
}

bool ResamplerImp::runNegativeTest(const string& testName, ostream& failure, const string& expectedError,
   const vector<double>& fromData, vector<double>& toData,
   const vector<double>& fromWavelengths, const vector<double>& toWavelengths, 
   const vector<double>& toFwhm, vector<int>& toBands, string& errorMessage,
   const string& resamplerMethod)
{
   if (execute(fromData, toData, fromWavelengths, toWavelengths,
      toFwhm, toBands, errorMessage, ResamplerOptions::LinearMethod()) == true)
   {
      failure << testName << " passed a negative test.";
      return false;
   }

   if (toData.size() != 0 || toBands.size() != 0)
   {
      failure << testName << " returned valid data on a negative test.";
      return false;
   }

   if (errorMessage != expectedError)
   {
      failure << testName << " returned reported error \"" << errorMessage <<
         ", but should have reported error \"" << expectedError << "\".";
      return false;
   }

   return true;
}

bool ResamplerImp::execute(const vector<double>& fromData,
   vector<double>& toData, const vector<double>& fromWavelengths, const vector<double>& toWavelengths, 
   const vector<double>& toFwhm, vector<int>& toBands, string& errorMessage)
{
   return execute(fromData, toData, fromWavelengths, toWavelengths,
      toFwhm, toBands, errorMessage, ResamplerOptions::getSettingResamplerMethod());
}

bool ResamplerImp::execute(const vector<double>& fromData,
   vector<double>& toData, const vector<double>& fromWavelengths, const vector<double>& toWavelengths, 
   const vector<double>& toFwhm, vector<int>& toBands, string& errorMessage, const string& resamplerMethod)
{
   vector<double> sortedFromWavelengths, sortedFromData, 
      sortedToWavelengths, sortedToData, sortedToFwhm;
   vector<int> sortedToBands;

   if (sortInputData(fromWavelengths, fromData, toWavelengths, toFwhm,
         sortedFromWavelengths, sortedFromData, sortedToWavelengths, 
         sortedToFwhm, sortedToBands, errorMessage) == false)
   {
      return false;
   }

   auto_ptr<Interpolator> pInterpolator;
   const double dropOutWindow = ResamplerOptions::getSettingDropOutWindow();
   if (resamplerMethod == ResamplerOptions::LinearMethod())
   {
      pInterpolator = auto_ptr<Interpolator>
         (new LinearInterpolator(sortedFromWavelengths, sortedFromData, dropOutWindow));
   }
   else if (resamplerMethod == ResamplerOptions::CubicSplineMethod())
   {
      pInterpolator = auto_ptr<Interpolator>
         (new SplineInterpolator(sortedFromWavelengths, sortedFromData,  dropOutWindow));
   }
   else if (resamplerMethod == ResamplerOptions::GaussianMethod())
   {
      pInterpolator = auto_ptr<Interpolator>
         (new GaussianResampler(sortedFromWavelengths, sortedFromData,  dropOutWindow));
   }

   if (pInterpolator.get() == NULL)
   {
      errorMessage = "Unable to create interpolator for resampling.";
      return false;
   }

   if (pInterpolator->noResamplingNecessary(toWavelengths))
   {
      copy(fromData.begin(), fromData.end(), back_inserter(toData));
      for (unsigned int i = 0; i < fromData.size(); ++i)
      {
         toBands.push_back(i);
      }
   }
   else
   {
      if (pInterpolator->run(sortedToWavelengths, sortedToFwhm, toData, toBands, errorMessage) == false)
      {
         return false;
      }

      sortOutputData(sortedToBands, toBands, toData);
   }      

   return true;
}

bool ResamplerImp::sortInputData(const vector<double>& fromWavelengths, const vector<double>& fromData, 
              const vector<double>& toWavelengths, const vector<double>& toFwhm,
              vector<double>& sortedFromWavelengths, vector<double>& sortedFromData, 
              vector<double>& sortedToWavelengths, vector<double>& sortedToFwhm,
              vector<int>& sortedToBands, string& errorMessage)
{
   if (fromData.size() != fromWavelengths.size())
   {
      errorMessage = "Number of input data values differs from number of input wavelengths.";
      return false;
   }   
   
   // for efficiency, reserve space for results
   sortedFromWavelengths.reserve(fromWavelengths.size());
   sortedFromData.reserve(fromData.size());
   sortedToWavelengths.reserve(toWavelengths.size());
   sortedToFwhm.reserve(toFwhm.size());
   sortedToBands.reserve(toWavelengths.size());

   // construct vector of wavelength/data pairs
   typedef vector<pair<double,double> > VecPair;
   VecPair fromPairs;
   fromPairs.reserve(fromWavelengths.size());
   for (int i=0; i<(int)fromWavelengths.size(); ++i)
   {
      pair<double,double> temp; 
      temp.first = fromWavelengths[i];
      temp.second = fromData[i];
      fromPairs.push_back(temp);
   }
   sort(fromPairs.begin(), fromPairs.end());

   // extract the wavelengths and data from the sorted pairs
   for (VecPair::const_iterator iter=fromPairs.begin(); iter!=fromPairs.end(); ++iter)
   {
      sortedFromWavelengths.push_back(iter->first);
      sortedFromData.push_back(iter->second);
   }

   typedef vector<Triplet> VecTrip;
   VecTrip toTriplets;
   toTriplets.reserve(toWavelengths.size());
   for (int i=0; i<(int)toWavelengths.size(); ++i)
   {
      Triplet temp; 
      temp.mWavelength = toWavelengths[i];
      temp.mFwhm =  (toFwhm.size() != 0) ? toFwhm[i] : 0.0;
      temp.mBand = i;
      toTriplets.push_back(temp);
   }
   sort(toTriplets.begin(), toTriplets.end());

   // extract the wavelengths, fwhm and indices from the sorted triplets
   for (VecTrip::const_iterator iter=toTriplets.begin(); iter != toTriplets.end(); ++iter)
   {
      sortedToWavelengths.push_back(iter->mWavelength);
      if (toFwhm.size() != 0) sortedToFwhm.push_back(iter->mFwhm);
      sortedToBands.push_back(iter->mBand);
   }

   return true;
}

void ResamplerImp::sortOutputData(const vector<int>& sortedToBands,
   vector<int>& toBands, vector<double>& toData)
{
   vector<int> sortedOriginalBands;
   sortedOriginalBands.reserve(toData.size());

   typedef vector<pair<int,double> > VecPair;
   VecPair pairs;
   pairs.reserve(toData.size());

   for (unsigned int i=0; i<toBands.size(); ++i)
   {
      pair<int,double> temp;
      temp.first = sortedToBands[toBands[i]];
      temp.second = toData.at(i);
      pairs.push_back(temp);
   }

   sort(pairs.begin(), pairs.end());

   for (unsigned int j=0; j<toData.size(); ++j)
   {
      toBands[j] = pairs[j].first;
      toData[j] = pairs[j].second;
   }
}

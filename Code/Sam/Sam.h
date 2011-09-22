/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SAM_H
#define SAM_H

#include "AlgorithmPattern.h"
#include "AlgorithmShell.h"
#include "MultiThreadedAlgorithm.h"
#include "ProgressTracker.h"

#include <math.h>
#include <string>
#include <vector>

class AoiElement;
class BitMaskIterator;
class Progress;
class Signature;
class SamDlg;
class Wavelengths;

struct SamInputs
{
   SamInputs() : mThreshold(5.0),
                 mbDisplayResults(false),
                 mResultsName("SAM Results"),
                 mpAoi(NULL),
                 mbCreatePseudocolor(true) {}
   std::vector<Signature*> mSignatures;
   double mThreshold;
   bool mbDisplayResults;
   std::string mResultsName;
   AoiElement* mpAoi;
   bool mbCreatePseudocolor;
};

class SamAlgorithm : public AlgorithmPattern
{
   bool preprocess();
   bool processAll();
   bool postprocess();
   bool initialize(void* pAlgorithmData);
   RasterElement* createResults(int numRows, int numColumns, const std::string& sigName);
   bool resampleSpectrum(Signature* pSignature, std::vector<double>& resampledAmplitude, 
      Wavelengths* pWavelengths, std::vector<int>& resampledBands);
   bool canAbort() const;
   bool doAbort();

   RasterElement* mpResults;
   SamInputs mInputs;
   bool mAbortFlag;

public:
   SamAlgorithm(RasterElement* pElement, Progress* pProgress, bool interactive, const BitMask* pAoi);
   RasterElement* getResults() const;
};

struct SamAlgInput
{
   SamAlgInput(const RasterElement* pCube,
      RasterElement* pResultsMatrix,
      const std::vector<double>& spectrum,
      const bool* pAbortFlag, 
      const BitMaskIterator& iterCheck,
      const std::vector<int>& resampledBands) : mpCube(pCube),
      mpResultsMatrix(pResultsMatrix),
      mSpectrum(spectrum),
      mpAbortFlag(pAbortFlag),
      mIterCheck(iterCheck),
      mResampledBands(resampledBands)
   {
   }

   ~SamAlgInput()
   {
   }

   const RasterElement* mpCube;
   RasterElement* mpResultsMatrix;
   const std::vector<double>& mSpectrum;
   const bool* mpAbortFlag;
   const BitMaskIterator& mIterCheck;
   const std::vector<int>& mResampledBands;
};

class SamThread : public mta::AlgorithmThread
{
public:
   SamThread(const SamAlgInput& input, 
      int threadCount, 
      int threadIndex, 
      mta::ThreadReporter& reporter);

   void run();
   template<class T> void ComputeSam(const T* pDummyData);

private:
   const SamAlgInput& mInput;
   mta::AlgorithmThread::Range mRowRange;
};

struct SamAlgOutput
{
   bool compileOverallResults(const std::vector<SamThread*>& threads) 
   { 
      return true; 
   }
};

class Sam : public AlgorithmPlugIn
{
public:
   Sam();
   ~Sam();
   SETTING(SamHelp, SpectralContextSensitiveHelp, std::string, "");

private:
   bool canRunBatch() const { return true; }
   bool canRunInteractive() const { return true; }
   bool populateBatchInputArgList(PlugInArgList* pInArgList);
   bool populateInteractiveInputArgList(PlugInArgList* pInArgList);
   bool populateDefaultOutputArgList(PlugInArgList* pOutArgList);
   bool setActualValuesInOutputArgList(PlugInArgList* pOutArgList);
   bool parseInputArgList(PlugInArgList* pInArgList);
   QDialog* getGui(void* pAlgData);
   void propagateAbort();
   bool extractFromGui();

   ProgressTracker mProgress;
   Progress* mpProgress;
   SamInputs mInputs;
   SamDlg* mpSamGui;
   SamAlgorithm* mpSamAlg;
};

#endif

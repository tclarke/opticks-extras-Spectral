/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef CEM_H
#define CEM_H

#include "AlgorithmPattern.h"
#include "AlgorithmShell.h"
#include "MultiThreadedAlgorithm.h"
#include "ProgressTracker.h"

#include <math.h>
#include <string>
#include <vector>

class AoiElement;
class BitMaskIterator;
class CemDlg;
class Progress;
class Signature;
class Wavelengths;

struct CemInputs
{
   CemInputs() : mThreshold(0.5),
                 mbDisplayResults(false),
                 mResultsName("CEM Results"),
                 mpAoi(NULL),
                 mbCreatePseudocolor(true) {}
   std::vector<Signature*> mSignatures;
   double mThreshold;
   bool mbDisplayResults;
   std::string mResultsName;
   AoiElement* mpAoi;
   bool mbCreatePseudocolor;
};

class CemAlgorithm : public AlgorithmPattern
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
   void computeWoper(std::vector<double>& pSpectrum, double* pSmm,
      int numBands, std::vector<double>& pWoper, const std::vector<int>& resampledBands);

   RasterElement* mpResults;
   CemInputs mInputs;
   bool mAbortFlag;
   
public:
   CemAlgorithm(RasterElement* pElement, Progress* pProgress, bool interactive, const BitMask* pAoi);
   RasterElement* getResults() const;
};

struct CemAlgInput
{
   CemAlgInput(const RasterElement* pCube,
      RasterElement* pResultsMatrix,
      const std::vector<double>& woper,
      const bool* pAbortFlag,
      const BitMaskIterator& iterCheck,
      const std::vector<int>& resampledBands) :
               mpCube(pCube),
               mpResultsMatrix(pResultsMatrix),
               mWoper(woper),
               mCheck(iterCheck),
               mpAbortFlag(pAbortFlag),
               mResampledBands(resampledBands)
   {
   }

   ~CemAlgInput()
   {
   }

   const RasterElement* mpCube;
   RasterElement* mpResultsMatrix;  
   const std::vector<double>& mWoper;
   const bool* mpAbortFlag;
   const BitMaskIterator& mCheck;
   const std::vector<int>& mResampledBands;
};

class CemThread : public mta::AlgorithmThread
{
public:
   CemThread(const CemAlgInput& input,
             int threadCount,
             int threadIndex,
             mta::ThreadReporter& reporter);

   void run();
   template<class T> void ComputeCem(const T* pDummyData);

private:
   const CemAlgInput& mInput;
   mta::AlgorithmThread::Range mRowRange;
};

struct CemAlgOutput
{
   bool compileOverallResults(const std::vector<CemThread*>& threads)
   {
      return true;
   }
};

class Cem : public AlgorithmPlugIn
{
public:
   Cem();
   ~Cem();
   SETTING(CemHelp, SpectralContextSensitiveHelp, std::string, "");

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
   CemInputs mInputs;
   CemDlg* mpCemGui;
   CemAlgorithm* mpCemAlg;
};

#endif

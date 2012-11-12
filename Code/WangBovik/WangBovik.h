/*
 * The information in this file is
 * Copyright(c) 2012 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef WANGBOVIK_H
#define WANGBOVIK_H

#include "AlgorithmPattern.h"
#include "AlgorithmShell.h"
#include "MultiThreadedAlgorithm.h"
#include "ProgressTracker.h"

#include <string>
#include <vector>
#include <opencv/cv.h>

class AoiElement;
class BitMaskIterator;
class Progress;
class Signature;
class WangBovikDlg;
class Wavelengths;

struct WangBovikInputs
{
   WangBovikInputs() :
      mThreshold(0.5),
      mbDisplayResults(false),
      mResultsName("WBI Results"),
      mpAoi(NULL),
      mbCreatePseudocolor(true)
   {}

   std::vector<Signature*> mSignatures;
   double mThreshold;
   bool mbDisplayResults;
   std::string mResultsName;
   AoiElement* mpAoi;
   bool mbCreatePseudocolor;
};

class WangBovikAlgorithm : public AlgorithmPattern
{
   bool preprocess();
   bool processAll();
   bool postprocess();
   bool initialize(void* pAlgorithmData);
   RasterElement* createResults(int numRows, int numColumns, int numBands, const std::string& sigName);
   bool resampleSpectrum(Signature* pSignature, std::vector<double>& resampledAmplitude,
      Wavelengths* pWavelengths, std::vector<int>& resampledBands);
   bool canAbort() const;
   bool doAbort();

   RasterElement* mpResults;
   WangBovikInputs mInputs;
   bool mAbortFlag;

public:
   WangBovikAlgorithm(RasterElement* pElement, Progress* pProgress, bool interactive, const BitMask* pAoi);
   RasterElement* getResults() const;
};

struct WangBovikAlgInput
{
   WangBovikAlgInput(const RasterElement* pCube, RasterElement* pResultsMatrix, const cv::Mat& spectrum,
      const bool* pAbortFlag, const BitMaskIterator& iterCheck, const std::vector<int>& resampledBands,
      const double& spectrumMean, const double& spectrumVariance) :
      mpCube(pCube),
      mpResultsMatrix(pResultsMatrix),
      mSpectrum(spectrum),
      mpAbortFlag(pAbortFlag),
      mIterCheck(iterCheck),
      mResampledBands(resampledBands),
      mSpectrumMean(spectrumMean),
      mSpectrumVariance(spectrumVariance)
   {}

   virtual ~WangBovikAlgInput()
   {}

   const RasterElement* mpCube;
   RasterElement* mpResultsMatrix;
   const cv::Mat& mSpectrum;                  // mean subtracted values
   const bool* mpAbortFlag;
   const BitMaskIterator& mIterCheck;
   const std::vector<int>& mResampledBands;
   const double& mSpectrumMean;
   const double& mSpectrumVariance;
};

class WangBovikThread : public mta::AlgorithmThread
{
public:
   WangBovikThread(const WangBovikAlgInput& input, int threadCount, int threadIndex, mta::ThreadReporter& reporter);

   void run();
   template<class T> void ComputeWangBovik(const T* pDummyData);

private:
   const WangBovikAlgInput& mInput;
   mta::AlgorithmThread::Range mRowRange;
};

struct WangBovikAlgOutput
{
   bool compileOverallResults(const std::vector<WangBovikThread*>& threads)
   {
      return true;
   }
};

class WangBovik : public AlgorithmPlugIn
{
public:
   WangBovik();
   virtual ~WangBovik();
   SETTING(WangBovikHelp, SpectralContextSensitiveHelp, std::string, "");

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
   WangBovikInputs mInputs;
   WangBovikDlg* mpWangBovikGui;
   WangBovikAlgorithm* mpWangBovikAlg;
};

#endif

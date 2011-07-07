/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef ACE_H
#define ACE_H

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
class AceDlg;
class Wavelengths;

struct AceInputs
{
   AceInputs() : mThreshold(0.4225),
                 mbDisplayResults(false),
                 mResultsName("ACE Results"),
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

class AceAlgorithm : public AlgorithmPattern
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
   AceInputs mInputs;
   bool mAbortFlag;

public:
   AceAlgorithm(RasterElement* pElement, Progress* pProgress, bool interactive, const BitMask* pAoi);
   RasterElement* getResults() const;
};

struct AceAlgInput
{
   AceAlgInput(const RasterElement* pCube,
      RasterElement* pResultsMatrix,
      const cv::Mat& spectrum,
      const bool* pAbortFlag, 
      const BitMaskIterator& iterCheck,
      const std::vector<int>& resampledBands,
      const cv::Mat& muMat, 
      const cv::Mat& covMat,
      const cv::Mat& sigCovMat,
      const cv::Mat& sigCovSigMat) : mpCube(pCube),
      mpResultsMatrix(pResultsMatrix),
      mSpectrum(spectrum),
      mpAbortFlag(pAbortFlag),
      mIterCheck(iterCheck),
      mResampledBands(resampledBands),
      mMuMat(muMat),
      mCovMat(covMat),
      mSigCovMat(sigCovMat),
      mSigCovSigMat(sigCovSigMat)
   {}

   virtual ~AceAlgInput()
   {}

   const RasterElement* mpCube;
   RasterElement* mpResultsMatrix;
   const cv::Mat& mSpectrum;
   const bool* mpAbortFlag;
   const BitMaskIterator& mIterCheck;
   const std::vector<int>& mResampledBands;
   const cv::Mat& mMuMat;
   const cv::Mat& mCovMat;
   const cv::Mat& mSigCovMat;
   const cv::Mat& mSigCovSigMat;};

class AceThread : public mta::AlgorithmThread
{
public:
   AceThread(const AceAlgInput& input, 
      int threadCount, 
      int threadIndex, 
      mta::ThreadReporter& reporter);

   void run();
   template<class T> void ComputeAce(const T* pDummyData);

private:
   const AceAlgInput& mInput;
   mta::AlgorithmThread::Range mRowRange;
};

struct AceAlgOutput
{
   bool compileOverallResults(const std::vector<AceThread*>& threads) 
   { 
      return true; 
   }
};

class Ace : public AlgorithmPlugIn
{
public:
   Ace();
   ~Ace();
   SETTING(AceHelp, SpectralContextSensitiveHelp, std::string, "");

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
   AceInputs mInputs;
   AceDlg* mpAceGui;
   AceAlgorithm* mpAceAlg;
};

#endif

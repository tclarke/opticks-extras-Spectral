/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef MNF_H
#define MNF_H

#include <QtCore/QString>
#include <QtCore/QStringList>

#include "AlgorithmShell.h"
#include "ApplicationServices.h"
#include "BitMask.h"
#include "DesktopServices.h"
#include "EnumWrapper.h"
#include "MessageLogMgr.h"
#include "ModelServices.h"
#include "ObjectFactory.h"
#include "PlugInManagerServices.h"
#include "PlugInResource.h"
#include "Progress.h"
#include "RasterElement.h"
#include "UtilityServices.h"

#include <vector>
#include <string>

class AoiElement;
class ApplicationServices;
class SpatialDataView;
class Step;

class Mnf : public AlgorithmShell
{
public:
   Mnf();
   virtual ~Mnf();

   bool getInputSpecification(PlugInArgList*& pArgList);
   bool getOutputSpecification(PlugInArgList*& pArgList);
   bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

protected:
   virtual bool extractInputArgs(const PlugInArgList* pArgList);
   bool computeCovarianceMatrix(RasterElement* pRaster, double** pMatrix,
      std::string info = std::string(), AoiElement* pAoi = NULL, int rowSkip = 1, int colSkip = 1);
   bool calculateEigenValues();
   bool createMnfCube();
   bool computeMnfValues();
   bool createMnfView();
   void initializeNoiseMethods();
   AoiElement* getAoiElement(const std::string& aoiName, RasterElement* pRaster);
   bool writeOutMnfTransform(const std::string& filename);
   bool readInMnfTransform(const std::string& filename);
   AoiElement* generateAutoSelectionMask(float bandFractionThreshold);
   RasterElement* createDifferenceRaster(AoiElement* pAoi);
   AoiElement* createDifferenceAoi(AoiElement* pAoi, RasterElement* pParent);
   bool readMatrixFromFile(QString filename, double **pData, int numBands, const std::string &caption);
   bool writeMatrixToFile(QString filename, const double **pData, int numBands, const std::string &caption);
   bool generateNoiseStatistics();
   bool performCholeskyDecomp(double** pMatrix, double* pVector, int numRows, int numCols);

private:
   Service<PlugInManagerServices> mpPlugInMgr;
   Service<ModelServices> mpModel;
   Service<ObjectFactory> mpObjFact;
   Service<DesktopServices> mpDesktop;
   Service<UtilityServices> mpUtilities;
   Service<ApplicationServices> mpAppSvcs;

   QStringList mNoiseEstimationMethods;
   unsigned int mNumRows;
   unsigned int mNumColumns;
   unsigned int mNumBands;
   Progress* mpProgress;
   SpatialDataView* mpView;
   RasterElement* mpRaster;
   ModelResource<RasterElement> mpMnfRaster;
   ModelResource<RasterElement> mpNoiseRaster;
   Step* mpStep;
   double** mpMnfTransformMatrix;
   double** mpNoiseCovarMatrix;
   std::vector<double> mSignalBandMeans;
   bool mbUseTransformFile;
   std::string mTransformFilename;
   std::string mSaveCoefficientsFilename;
   bool mbUseAoi;
   AoiElement* mpProcessingAoi;
   AoiElement* mpNoiseAoi;
   std::string mPreviousNoiseFilename;
   unsigned int mNumComponentsToUse;
   bool mbUseSnrValPlot;
   bool mbDisplayResults;
   std::string mMessage;


   // These values are in same order as method strings in mNoiseEstimationMethods
   enum NoiseEstimateTypeEnum { DIFFDATA = 0, DARKCURRENT = 1, PREVIOUS = 2 };
   typedef EnumWrapper<NoiseEstimateTypeEnum> NoiseEstimateType;

   NoiseEstimateType mNoiseStatisticsMethod;
   std::string getNoiseEstimationMethodString(NoiseEstimateType noiseType);
   NoiseEstimateType getNoiseEstimationMethodType(std::string noiseStr);
};

#endif
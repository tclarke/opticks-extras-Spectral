/*
 * The information in this file is
 * Copyright(c) 2010 Trevor R.H. Clarke
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

// Keep this include here..it uses an OpenCV macro X.
// Moving this after Opticks includes will incorrectly use the Xerces X macro.
#include <opencv/cv.h>

#include "AoiElement.h"
#include "AppVerify.h"
#include "BitMaskIterator.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DesktopServices.h"
#include "LayerList.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "ProgressTracker.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "Rx.h"
#include "RxDialog.h"
#include "SpatialDataView.h"
#include "SpectralUtilities.h"
#include "SpectralVersion.h"
#include "ThresholdLayer.h"
#include <memory>
#include <QtCore/QtConcurrentMap>

REGISTER_PLUGIN_BASIC(RxModule, Rx);

namespace
{
   template<typename T>
   void readBandData(T* pPtr, std::vector<double>& output)
   {
      for (std::vector<double>::size_type band = 0; band < output.size(); ++band)
      {
         output[band] = static_cast<double>(pPtr[band]);
      }
   }
 
   struct PcaMap
   {
      typedef unsigned int input_type;
      typedef QPair<LocationType, QPair<DataAccessor*, cv::Mat > > result_type;
      DataAccessor* mpResAcc;
      cv::Mat* mpInputMat;
      cv::PCA* mpPcaAlgorithm;
      int mCols;
      std::vector<LocationType>& mLocations;

      PcaMap(cv::Mat* inputMat, cv::PCA* pPcaAlgorithm, DataAccessor* pResAcc, std::vector<LocationType>& locations) :
               mpInputMat(inputMat),
               mpPcaAlgorithm(pPcaAlgorithm),
               mpResAcc(pResAcc),
               mLocations(locations)
      {
      }

      //Perform the transform into PCA space and back in parallel
      result_type operator()(const input_type& loc)
      {
         //initialize the variables
         LocationType location(0, 0);
         cv::Mat reconstructed;
         if (loc < mLocations.size())
         {
            //retrieve the location of the data relative to the output
            location.mY = mLocations[loc].mY;
            location.mX = mLocations[loc].mX;
            if (mpInputMat != NULL)
            {
               //retrieve the set of bands for the specified pixels
               cv::Mat vec = mpInputMat->row(loc);
               if (mpPcaAlgorithm != NULL)
               {
                  //project into PCA space
                  cv::Mat coeffs = mpPcaAlgorithm->project(vec);

                  //project back into the original space, the first eigen vectors were
                  //zeroed at an earlier step to make projecting back create a less noisy image
                  reconstructed = mpPcaAlgorithm->backProject(coeffs);
               }
            }
         }
         return QPair<LocationType, QPair<DataAccessor*, cv::Mat> >(location, QPair<DataAccessor*, cv::Mat>(
            mpResAcc, reconstructed));
      }

   };

   //write the results to the data accessor after each transform
   void pcaReduce(QPair<LocationType, QPair<DataAccessor*, cv::Mat> >& final, 
      const QPair<LocationType, QPair<DataAccessor*, cv::Mat> >& intermediate)
   {
      int row = intermediate.first.mY;
      int col = intermediate.first.mX;
      if (intermediate.second.first != NULL)
      {
         DataAccessor acc = *intermediate.second.first;
         if (acc.isValid())
         {
            acc->toPixel(row, col);
            if (intermediate.second.second.empty() == false)
            {
               //the data is assumed to be retrieved with a BIP accessor
               int size = intermediate.second.second.cols;
               memcpy(acc->getColumn(), intermediate.second.second.data, size * sizeof(double));
            }
         }
      }
   }

   struct RxMap
   {
      typedef QPair<int, QList<int> > input_type;
      typedef const QPair<int, QList<int> >& result_type;

      RasterElement* mpElement;
      RasterElement* mpResult;
      const RasterDataDescriptor* mpDesc;
      const RasterDataDescriptor* mpResDesc;
      LocationType mStart;
      int mBands;
      EncodingType mEncoding;
      cv::Mat& mCovMat;
      cv::Mat& mMuMat;
      unsigned int mLocalWidthOffset;
      unsigned int mLocalHeightOffset;
      bool mLocal;

      RxMap(RasterElement* pElement, RasterElement* pResult, LocationType start,
            cv::Mat& covMat, cv::Mat& muMat, unsigned int localWidthOffset, unsigned int localHeightOffset) :
               mpElement(pElement),
               mpResult(pResult),
               mStart(start),
               mCovMat(covMat),
               mMuMat(muMat),
               mLocalWidthOffset(localWidthOffset),
               mLocalHeightOffset(localHeightOffset),
               mLocal(localWidthOffset > 0 && localHeightOffset > 0)
      {
         mpDesc = static_cast<const RasterDataDescriptor*>(mpElement->getDataDescriptor());
         mpResDesc = static_cast<const RasterDataDescriptor*>(mpResult->getDataDescriptor());
         mBands = mpDesc->getBandCount();
         mEncoding = mpDesc->getDataType();
      }

      result_type operator()(const input_type& locList)
      {
         int startRow = locList.first;
         int endRow = locList.first;

         if (mLocal)
         {
            startRow = std::max<int>(0, startRow - mLocalHeightOffset);
            endRow = std::min<int>(mpDesc->getRowCount() - 1, endRow + mLocalHeightOffset);
         }
         FactoryResource<DataRequest> pReq;
         pReq->setInterleaveFormat(BIP);
         pReq->setRows(mpDesc->getActiveRow(startRow), mpDesc->getActiveRow(endRow));
         DataAccessor acc(mpElement->getDataAccessor(pReq.release()));
         ENSURE(acc.isValid());

         DimensionDescriptor resRowDesc = mpResDesc->getActiveRow(locList.first - mStart.mY);
         FactoryResource<DataRequest> pResReq;
         pResReq->setRows(resRowDesc, resRowDesc);
         DataAccessor resacc(mpResult->getDataAccessor(pResReq.release()));
         ENSURE(resacc.isValid());

         cv::Mat pixelMat(mBands, 1, CV_64F);
         foreach(int col, locList.second)
         {
            cv::Mat localCovMat;
            cv::Mat localMuMat;
            if (mLocal)
            {
               // calculate local statistics
               int startCol = std::max<int>(0, col - mLocalWidthOffset);
               int endCol = std::min<int>(mpDesc->getColumnCount() - 1, col + mLocalWidthOffset);
               cv::Mat samples(mBands, (endRow - startRow + 1) * (endCol - startCol + 1) - 1, CV_64F);
               int curSample = 0;
               for (int subRow = startRow; subRow <= endRow; ++subRow)
               {
                  for (int subCol = startCol; subCol <= endCol; ++subCol)
                  {
                     if (subRow == locList.first && subCol == col)
                     {
                        continue;
                     }
                     acc->toPixel(subRow, subCol);
                     for (int band = 0; band < mBands; ++band)
                     {
                        double val = Service<ModelServices>()->getDataValue(mEncoding, acc->getColumn(), band);
                        samples.at<double>(band, curSample) = val;
                     }
                     curSample++;
                  }
               }
               localCovMat = cv::Mat(mBands, mBands, CV_64F);
               localMuMat = cv::Mat(mBands, 1, CV_64F);
               cv::calcCovarMatrix(samples, localCovMat, localMuMat, CV_COVAR_NORMAL | CV_COVAR_COLS);
            }
            acc->toPixel(locList.first, col);
            resacc->toPixel(locList.first - mStart.mY, col - mStart.mX);
            for (int band = 0; band < mBands; ++band)
            {
               double val = Service<ModelServices>()->getDataValue(mEncoding, acc->getColumn(), band);
               pixelMat.at<double>(band, 0) = val;
            }

            pixelMat -= mLocal ? localMuMat : mMuMat;
            cv::Mat tempMat(1, mBands, CV_64F);
            cv::Mat resMat(1, 1, CV_64F);
            cv::gemm(pixelMat, mLocal ? localCovMat : mCovMat, 1.0, cv::Mat(), 0.0, tempMat, cv::GEMM_1_T);
            cv::gemm(tempMat, pixelMat, 1.0, cv::Mat(), 0.0, resMat);
            *reinterpret_cast<double*>(resacc->getColumn()) = resMat.at<double>(0,0);
         }
         return locList;
      }
   };
}

Rx::Rx()
{
   setName("Rx");
   setDescriptorId("{127341c5-9eb4-40e1-8036-fd234ea5fdd0}");
   setSubtype("Anomaly Detection");
   setMenuLocation("[Spectral]/Anomaly Detection/RX");
   setAbortSupported(true);
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   addDependencyCopyright("OpenCV",
"IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. \n"
"\n"
" By downloading, copying, installing or using the software you agree to this license.\n"
" If you do not agree to this license, do not download, install,\n"
" copy or use the software.\n"
"\n"
"\n"
"                          License Agreement\n"
"               For Open Source Computer Vision Library\n"
"\n"
"Copyright (C) 2000-2008, Intel Corporation, all rights reserved.\n"
"Copyright (C) 2008-2010, Willow Garage Inc., all rights reserved.\n"
"Third party copyrights are property of their respective owners.\n"
"\n"
"Redistribution and use in source and binary forms, with or without modification,\n"
"are permitted provided that the following conditions are met:\n"
"\n"
"  * Redistribution's of source code must retain the above copyright notice,\n"
"    this list of conditions and the following disclaimer.\n"
"\n"
"  * Redistribution's in binary form must reproduce the above copyright notice,\n"
"    this list of conditions and the following disclaimer in the documentation\n"
"    and/or other materials provided with the distribution.\n"
"\n"
"  * The name of the copyright holders may not be used to endorse or promote products\n"
"    derived from this software without specific prior written permission.\n"
"\n"
"This software is provided by the copyright holders and contributors \"as is\" and\n"
"any express or implied warranties, including, but not limited to, the implied\n"
"warranties of merchantability and fitness for a particular purpose are disclaimed.\n"
"In no event shall the Intel Corporation or contributors be liable for any direct,\n"
"indirect, incidental, special, exemplary, or consequential damages\n"
"(including, but not limited to, procurement of substitute goods or services;\n"
"loss of use, data, or profits; or business interruption) however caused\n"
"and on any theory of liability, whether in contract, strict liability,\n"
"or tort (including negligence or otherwise) arising in any way out of\n"
"the use of this software, even if advised of the possibility of such damage.");
}

Rx::~Rx()
{
}

bool Rx::getInputSpecification(PlugInArgList*& pArgList)
{
   VERIFY(pArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL, Executable::ProgressArgDescription()));
   VERIFY(pArgList->addArg<RasterElement>(Executable::DataElementArg(), NULL, "Raster element on which RX will be "
      "performed."));
   VERIFY(pArgList->addArg<SpatialDataView>(Executable::ViewArg(), NULL, "View to be used with RX from "
      "which AOI layers can be selected. Additionally, the result of RX will be attached to this view as a new layer."));
   VERIFY(pArgList->addArg<AoiElement>("AOI", NULL, "Execute over this AOI only."));
   VERIFY(pArgList->addArg<double>("Threshold", 2.0, "Default result threshold in stddev."));
   VERIFY(pArgList->addArg<unsigned int>("Local Width", 
                                    "Width of the local neighborhood used to calculate statistics. "
                                    "If this or \"Local Height\" is not set or is set to 0, use global statistics."));
   VERIFY(pArgList->addArg<unsigned int>("Local Height", 
                                    "Height of the local neighborhood used to calculate statistics. "
                                    "If this or \"Local Width\" is not set or is set to 0, use global statistics."));
   VERIFY(pArgList->addArg<unsigned int>("Subspace Components", 
                                    "Number of components to strip for subspace RX. "
                                    "If this is not set or is set to 0, use standard RX."));
   return true;
}

bool Rx::getOutputSpecification(PlugInArgList*& pArgList)
{
   VERIFY(pArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pArgList->addArg<RasterElement>("Results", NULL, "Raster element resulting from the RX operation."));
   return true;
}

bool Rx::execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList)
{
   VERIFY(pInArgList);
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(ProgressArg()), "Executing RX.",
      "spectral", "{f5a21b68-013b-4d32-9923-b266e5311752}");

   RasterElement* pElement = pInArgList->getPlugInArgValue<RasterElement>(DataElementArg());
   if (pElement == NULL)
   {
      progress.report("Invalid raster element.", 0, ERRORS, true);
      return false;
   }
   const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
   VERIFY(pDesc);
   SpatialDataView* pView = pInArgList->getPlugInArgValue<SpatialDataView>(ViewArg());
   AoiElement* pAoi = pInArgList->getPlugInArgValue<AoiElement>("AOI");
   double threshold = 0.0;
   pInArgList->getPlugInArgValue("Threshold", threshold);
   unsigned int localWidth = 0;
   unsigned int localHeight = 0;
   bool useLocal = pInArgList->getPlugInArgValue("Local Width", localWidth);
   useLocal = useLocal && pInArgList->getPlugInArgValue("Local Height", localHeight);
   unsigned int components = 0;
   bool useSubspace = pInArgList->getPlugInArgValue("Subspace Components", components);

   // display options dialog
   if (!isBatch())
   {
      RxDialog dlg;
      std::vector<Layer*> layers;
      pView->getLayerList()->getLayers(AOI_LAYER, layers);
      QList<QPair<QString, QString> > aoiIds;
      for (std::vector<Layer*>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
      {
         aoiIds.push_back(qMakePair(
                     QString::fromStdString((*layer)->getDisplayName(true)),
                     QString::fromStdString((*layer)->getId())));
      }
      dlg.setAoiList(aoiIds);
      dlg.setThreshold(threshold);
      if (pAoi != NULL)
      {
         dlg.setAoi(QString::fromStdString(pAoi->getId()));
      }
      dlg.setLocal(useLocal);
      dlg.setLocalSize(localWidth, localHeight);
      dlg.setSubspace(useSubspace);
      dlg.setSubspaceComponents(components);
      if (dlg.exec() == QDialog::Rejected)
      {
         progress.report("Canceled by user", 100, ABORT, true);
         return false;
      }
      threshold = dlg.getThreshold();
      QString aoiId = dlg.getAoi();
      pAoi = aoiId.isEmpty() ? NULL : dynamic_cast<AoiElement*>(
               static_cast<Layer*>(Service<SessionManager>()->getSessionItem(
                        aoiId.toStdString()))->getDataElement());
      useLocal = dlg.isLocal();
      dlg.getLocalSize(localWidth, localHeight);
      useSubspace = dlg.isSubspace();
      components = dlg.getSubspaceComponents();
   }

   if (useLocal && (localWidth < 3 || localHeight < 3 || localWidth % 2 == 0 || localHeight % 2 == 0))
   {
      progress.report("Invalid local neighborhood size. Width and height must be at least 3 and odd.", 
         0, ERRORS, true);
      return false;
   }
   if (useSubspace && (components <= 0 || components >= pDesc->getBandCount()))
   {
      progress.report("Invalid number of subspace components. Must be 1 or more and less than the number of bands.", 
         0, ERRORS, true);
      return false;
   }

   //set up extents
   unsigned int startCol = 0;
   unsigned int startRow = 0;
   ModelResource<RasterElement> pRaster(static_cast<RasterElement*>(NULL));
   AoiElement* pLocalAoi = NULL;

   //clear out any filtered input when rerunning the tool
   string filterInputName = "RX Filtered Input";
   string resultsName = "RX Results";

   // Calculate PCA, remove "components" and invert the PCA...the result will be pRaster
   if (useSubspace)
   {
      try
      {
         //retrieve the input bitmask iterator, a separate one is created because
         //we'll have to output a new AOI relative to the selected area
         const BitMask* pBitmaskSS = (pAoi == NULL) ? NULL : pAoi->getSelectedPoints();
         BitMaskIterator iterSS(pBitmaskSS, pElement);
         if (!*iterSS)
         {
            progress.report("No pixels selected for processing.", 0, ERRORS, true);
            return false;
         }
         unsigned int bands = pDesc->getBandCount();
         cv::Mat muMat = cv::Mat(1, bands, CV_64F, 0.0);
         unsigned int numCols = iterSS.getNumSelectedColumns();
         unsigned int numRows = iterSS.getNumSelectedRows();
         startRow = iterSS.getRowOffset();
         startCol = iterSS.getColumnOffset();

         // generate location index map from the bitmask iterator
         std::vector<double> meansVector = SpectralUtilities::calculateMeans(pElement,
            iterSS, progress, &mAborted);

         if (mAborted == true)
         {
            //user canceled
            return false;
         }
         //store the mean of the dataset we processing
         muMat = cv::Mat(1, bands, CV_64F, &meansVector[0]);
         muMat.rows = 1;
         muMat.cols = bands;

         //create the output dataset
         pRaster = ModelResource<RasterElement>(createResults(numRows, numCols, bands, filterInputName, 
            FLT8BYTES, pElement));
         if (pRaster.get() == NULL)
         {
            progress.report("Unable to create results.", 0, ERRORS, true);
            return false;
         }
         const RasterDataDescriptor* pResDesc = static_cast<const RasterDataDescriptor*>(pRaster->getDataDescriptor());
         VERIFY(pResDesc);
         bool bCancel = false;
         std::vector<double> pixelValues(bands);
         unsigned int blockSize = 50;
         unsigned int numRowBlocks = numRows / blockSize;
         if (numRows%blockSize > 0)
         {
            numRowBlocks++;
         }

         //perform the operation on blocks of rows
         for (unsigned int rowBlocks = 0; rowBlocks < numRowBlocks; rowBlocks++)
         {
            unsigned int localStartRow = rowBlocks * blockSize;
            unsigned int endRow = localStartRow + blockSize;
            if (endRow > numRows)
            {
               endRow = numRows;
            }

            //set up the result data accessor
            DimensionDescriptor rowDesc = pResDesc->getActiveRow(localStartRow);
            DimensionDescriptor rowDesc2 = pResDesc->getActiveRow(endRow - 1);
            FactoryResource<DataRequest> pReq;
            pReq->setInterleaveFormat(BIP);
            pReq->setRows(rowDesc, rowDesc2);
            DataAccessor resacc(pRaster->getDataAccessor(pReq.release()));
            VERIFY(resacc.isValid());

            //set up the input data accessor
            DimensionDescriptor inputRowDesc = pDesc->getActiveRow(localStartRow + startRow);
            DimensionDescriptor inputRowDesc2 = pDesc->getActiveRow(startRow+endRow - 1);
            FactoryResource<DataRequest> pInputReq;
            pInputReq->setInterleaveFormat(BIP);
            pInputReq->setRows(inputRowDesc, inputRowDesc2);
            DataAccessor acc(pElement->getDataAccessor(pInputReq.release()));
            VERIFY(acc.isValid());

            resacc->toPixel(localStartRow, 0);
            acc->toPixel(localStartRow+startRow, startCol);
            for (unsigned int row = localStartRow; row < endRow; row++)
            {
               for (unsigned int col = 0; col < numCols; col++)
               {
                  acc->toPixel(startRow+row, startCol + col);
                  resacc->toPixel(row, col);

                  //get the value from the raster element
                  switchOnEncoding(pDesc->getDataType(), readBandData, acc->getColumn(), pixelValues);

                  //subtract the average
                  cv::Mat subtracted(1, bands, CV_64F, &pixelValues[0]);
                  subtracted -= muMat;
                  memcpy(resacc->getColumn(), subtracted.data, bands * sizeof(double));
               }
               if (bCancel)
               {
                  progress.report("User canceled operation.", 100, ABORT, true);
                  return false;
               }
               else
               {
                  progress.report("Initializing PCA Variables",
                        (row - startRow) * 45 /
                              (numRows - startRow), NORMAL);
                  if (isAborted())
                  {
                     bCancel = true;
                     setAbortSupported(false);
                  }
               }
            }
         }

         //calculate the covariance
         bool success = true;
         ExecutableResource covar("Covariance", std::string(), progress.getCurrentProgress(), true);
         success &= covar->getInArgList().setPlugInArgValue(DataElementArg(), pRaster.get());
         bool bInverse = false;
         success &= covar->getInArgList().setPlugInArgValue("ComputeInverse", &bInverse);
         success &= covar->execute();
         RasterElement* pCov = static_cast<RasterElement*>(
            Service<ModelServices>()->getElement("Covariance Matrix", 
                                                  TypeConverter::toString<RasterElement>(), pRaster.get()));
         success &= pCov != NULL;
         if (!success)
         {
            progress.report("Unable to calculate covariance.", 0, ERRORS, true);
            return false;
         }

         cv::Mat covMat = cv::Mat(bands, bands, CV_64F, pCov->getRawData());

         //calculate the Eigens
         cv::Mat unsortedEigenVectors;
         cv::Mat unsortedEigenValues;
         if (eigen(covMat, unsortedEigenValues, unsortedEigenVectors) == false)
         {
            progress.report("Unable to calculate eigen vectors.", 0, ERRORS, true);
            return false;
         }

         //delete the covariance matrix so it doesn't accidentally get used at the later step
         Service<ModelServices>()->destroyElement(pCov);

         //sort the eigens
         cv::Mat sortedIndices;
         cv::sortIdx(unsortedEigenValues, sortedIndices, CV_SORT_DESCENDING | CV_SORT_EVERY_COLUMN);
         cv::Mat eigenValues(bands, 1, CV_64F);
         cv::Mat eigenVectors(bands, bands, CV_64F);
         for (unsigned int i = 0; i < bands; i++)
         {
            eigenValues.at<double>(i) = static_cast<double>(unsortedEigenValues.at<double>(sortedIndices.at<int>(i)));
            for (unsigned int j = 0; j < bands; j++)
            {
               eigenVectors.at<double>(i, j) = static_cast<double>(unsortedEigenVectors.at<double>(
                  sortedIndices.at<int>(i), j));
            }
         }
         bCancel = false;
         int pixelCount = 0;

         std::auto_ptr<cv::PCA> pPcaAlgorithm(new cv::PCA());
         pPcaAlgorithm->eigenvectors = eigenVectors;
         pPcaAlgorithm->eigenvalues = eigenValues;
         pPcaAlgorithm->mean = muMat;

         //knock off the first components of the eigen vectors and values
         double zero = 0.0;
         for (unsigned int i = 0; i < components; i++)
         {
            pPcaAlgorithm->eigenvalues.at<double>(i) = zero;
            for (unsigned int j = 0; j < bands; j++)
            {
               double val = pPcaAlgorithm->eigenvectors.at<double>(i,j);
               pPcaAlgorithm->eigenvectors.at<double>(i,j) = zero;
            }
         }

         //make an AOI relative to the subset we are running SSRX on
         if (pAoi != NULL)  
         {
            Service<ModelServices> pModel;
            pLocalAoi = dynamic_cast<AoiElement*>(pModel->createElement("SSRX AOI", "AoiElement", pRaster.get()));
         }

         // setup write data accessor
         FactoryResource<DataRequest> pResReq;
         pResReq->setWritable(true);
         DataAccessor resacc(pRaster->getDataAccessor(pResReq.release()));
         if (!resacc.isValid())
         {
            progress.report("Unable to access data.", 0, ERRORS, true);
            return false;
         }

         //restart the iterator so we can put the values back in the same spot
         iterSS.begin();

         for (unsigned int rowBlocks = 0; rowBlocks < numRowBlocks; rowBlocks++)
         {
            std::vector<LocationType> aoiLocations;
            unsigned int localStartRow = rowBlocks * blockSize;
            unsigned int endRow = localStartRow + blockSize;
            if (endRow > numRows)
            {
               endRow = numRows;
            }

            //set up the result data accessor
            DimensionDescriptor rowDesc = pResDesc->getActiveRow(localStartRow);
            DimensionDescriptor rowDesc2 = pResDesc->getActiveRow(endRow - 1);
            FactoryResource<DataRequest> pReq;
            pReq->setInterleaveFormat(BIP);
            pReq->setRows(rowDesc, rowDesc2);
            DataAccessor resacc(pRaster->getDataAccessor(pReq.release()));
            VERIFY(resacc.isValid());

            //set up the input data accessor
            DimensionDescriptor inputRowDesc = pDesc->getActiveRow(localStartRow + startRow);
            DimensionDescriptor inputRowDesc2 = pDesc->getActiveRow(startRow+endRow - 1);
            FactoryResource<DataRequest> pInputReq;
            pInputReq->setInterleaveFormat(BIP);
            pInputReq->setRows(inputRowDesc, inputRowDesc2);
            DataAccessor acc(pElement->getDataAccessor(pInputReq.release()));
            VERIFY(acc.isValid());

            resacc->toPixel(localStartRow, 0);
            acc->toPixel(localStartRow + startRow, startCol);
            QList<int> indices;
            std::vector<double> pixelValues(bands);
            cv::Mat inputMat(numCols * (endRow - localStartRow), pDesc->getBandCount(), CV_64F);
            inputMat = cv::Scalar(0);
            pixelCount = 0;
            for (unsigned int row = localStartRow; row < endRow; row++)
            {
               for (unsigned int col = 0; col < numCols; col++)
               {
                  acc->toPixel(startRow + row, startCol + col);

                  //record which indices to put into the multithreaded processing function
                  if (iterSS.getPixel(startCol + col, startRow + row))
                  {
                     switchOnEncoding(pDesc->getDataType(), readBandData, acc->getColumn(), pixelValues);

                     //store the data in the input matrix with information per band stored as a column to
                     //each pixels row
                     for (unsigned int band = 0; band < bands; ++band)
                     {
                        inputMat.at<double>(pixelCount, band) = pixelValues[band];
                     }
                     indices.push_back(pixelCount);
                     aoiLocations.push_back(LocationType(col, row));
                     pixelCount++;
                  }
                  else
                  {
                     //if not within the AOI, then set the RasterElement band set to average
                     resacc->toPixel(row, col);
                     memcpy(resacc->getColumn(), pPcaAlgorithm->mean.data, bands * sizeof(double));
                  }
               }
               if (bCancel)
               {
                  progress.report("User canceled operation.", 100, ABORT, true);
                  return false;
               }
               else
               {
                  if (isAborted())
                  {
                     bCancel = true;
                     setAbortSupported(false);
                  }
               }
            }

            //project bands to PCA space and back
            PcaMap pcaMap(&inputMat, pPcaAlgorithm.get(), &resacc, aoiLocations);
            QFuture<QPair<LocationType, QPair<DataAccessor*, cv::Mat> > > pcaResults;
            pcaResults = QtConcurrent::mappedReduced(
               indices.begin(), indices.end(), pcaMap, pcaReduce, QtConcurrent::UnorderedReduce);
            bool isCancelling = false;
            float rowBlocksPercent = 100 / numRowBlocks;
            while (pcaResults.isRunning())
            {
               if (isCancelling)
               {
                  progress.report("Cleaning up processing threads. Please wait.", 99, NORMAL);
               }
               else
               {
                  progress.report("Applying PCs",
                        rowBlocksPercent*rowBlocks + 
                        (pcaResults.progressValue() - pcaResults.progressMinimum()) * (rowBlocksPercent) /
                        (pcaResults.progressMaximum() - pcaResults.progressMinimum()), NORMAL);
                  if (isAborted())
                  {
                     pcaResults.cancel();
                     isCancelling = true;
                     setAbortSupported(false);
                  }
               }
               QThread::yieldCurrentThread();
            }
            if (pcaResults.isCanceled())
            {
               progress.report("User canceled operation.", 100, ABORT, true);
               return false;
            }

            //add all of the pixel locations to the AOI
            if (pLocalAoi != NULL)
            {
               pLocalAoi->addPoints(aoiLocations);
            }
         }
      }
      catch (const cv::Exception& exc)
      {
         progress.report("OpenCV error: " + std::string(exc.what()), 0, ERRORS, true);
         return false;
      }
   }
   else
   {
      //clear any previous run of the subspace Rx
      clearPreviousResults(filterInputName, pElement);
   }

   if (pRaster.get() != NULL)
   {
      //clear any previous run of the Rx
      clearPreviousResults(resultsName, pElement);

      //set the inputs to the rest of the Rx algorithm to the outputs of the 
      //subspace function
      pElement = pRaster.get();
      pAoi = pLocalAoi;
      pDesc = static_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
   }

   // calculate global covariance matrix
   RasterElement* pCov = NULL;
   if (!useLocal)
   {
      bool success = true;
      ExecutableResource covar("Covariance", std::string(), progress.getCurrentProgress(), isBatch());
      success &= covar->getInArgList().setPlugInArgValue(DataElementArg(), pElement);

      if (isBatch())
      {
         success &= covar->getInArgList().setPlugInArgValue("AOI", pAoi);
      }
      success &= covar->execute();
      pCov = static_cast<RasterElement*>(
         Service<ModelServices>()->getElement("Inverse Covariance Matrix", 
         TypeConverter::toString<RasterElement>(), pElement));
      success &= pCov != NULL;
      if (!success)
      {
         progress.report("Unable to calculate covariance.", 0, ERRORS, true);
         return false;
      }
   }

   // setup read data accessor
   const BitMask* pBitmask = (pAoi == NULL) ? NULL : pAoi->getSelectedPoints();
   BitMaskIterator iter(pBitmask, pElement);
   if (!*iter)
   {
      progress.report("No pixels selected for processing.", 0, ERRORS, true);
      return false;
   }

   FactoryResource<DataRequest> pReq;
   pReq->setInterleaveFormat(BIP);
   pReq->setRows(pDesc->getActiveRow(iter.getBoundingBoxStartRow()), pDesc->getActiveRow(iter.getBoundingBoxEndRow()));
   pReq->setColumns(pDesc->getActiveColumn(iter.getBoundingBoxStartColumn()), 
                    pDesc->getActiveColumn(iter.getBoundingBoxEndColumn()));
   DataAccessor acc(pElement->getDataAccessor(pReq.release()));

   // create results element
   ModelResource<RasterElement> pResult(createResults(iter.getNumSelectedRows(), iter.getNumSelectedColumns(), 1, 
      resultsName, FLT8BYTES, pElement));
   if (pResult.get() == NULL)
   {
      progress.report("Unable to create results.", 0, ERRORS, true);
      return false;
   }

   // setup write data accessor
   FactoryResource<DataRequest> pResReq;
   pResReq->setWritable(true);
   DataAccessor resacc(pResult->getDataAccessor(pResReq.release()));
   if (!acc.isValid() || !resacc.isValid())
   {
      progress.report("Unable to access data.", 0, ERRORS, true);
      return false;
   }

   // execute Rx
   { // scope temp matrices
      int bands = pDesc->getBandCount();
      EncodingType encoding = pDesc->getDataType();
      cv::Mat covMat;
      cv::Mat muMat;
      if (!useLocal)
      {
         covMat = cv::Mat(bands, bands, CV_64F, pCov->getRawData());
         muMat = cv::Mat(bands, 1, CV_64F, 0.0);
      }

      if (!useLocal)
      {
         std::vector<double> meansVector = SpectralUtilities::calculateMeans(pElement, iter,
            progress, &mAborted);
         // setup the map-reduce and execute with progress reporting
         if (mAborted == true)
         {
            //user canceled
            return false;
         }
         muMat = cv::Mat(bands, 1, CV_64F, &meansVector[0]);
      }

      // generate location index map from the bitmask iterator
      QMap<int, QList<int> > locationMap;
      for (; iter != iter.end(); ++iter)
      {
         LocationType loc;
         iter.getPixelLocation(loc);
         locationMap[loc.mY].push_back(loc.mX);
      }

      // arrange the location index map by row with a list of columns
      QList<QPair<int, QList<int> > > locations;
      foreach(int key, locationMap.keys())
      {
         locations.push_back(qMakePair(key, locationMap[key]));
      }
      // setup and run the Rx map-reduce
      int localWidthOffset = useLocal ? ((localWidth - 1) / 2) : 0;
      int localHeightOffset = useLocal ? ((localHeight - 1) / 2) : 0;
      RxMap rxMap(pElement, pResult.get(),
         LocationType(iter.getBoundingBoxStartColumn(), iter.getBoundingBoxStartRow()), covMat, muMat,
         localWidthOffset, localHeightOffset);
      QFuture<void> rx;
      rx = QtConcurrent::map(locations, rxMap);
      bool isCancelling = false;
      while (rx.isRunning())
      {
         if (isCancelling)
         {
            progress.report("Cleaning up processing threads. Please wait.", 99, NORMAL);
         }
         else
         {
            if (useLocal)
            {
               progress.report("Calculating RX",
                     (rx.progressValue() - rx.progressMinimum()) * 99 /
                           (rx.progressMaximum() - rx.progressMinimum()), NORMAL);
            }
            else
            {
               progress.report("Calculating RX",
                     (rx.progressValue() - rx.progressMinimum()) * 49 /
                           (rx.progressMaximum() - rx.progressMinimum()) + 50, NORMAL);
            }
            if (isAborted())
            {
               rx.cancel();
               isCancelling = true;
               setAbortSupported(false);
            }
         }
         QThread::yieldCurrentThread();
      }

      if (rx.isCanceled())
      {
         progress.report("User canceled operation.", 100, ABORT, true);
         return false;
      }
   }

   // display results
   if (!isBatch())
   {
      ThresholdLayer* pLayer = static_cast<ThresholdLayer*>(pView->createLayer(THRESHOLD, pResult.get()));
      pLayer->setXOffset(iter.getBoundingBoxStartColumn() + startCol);
      pLayer->setYOffset(iter.getBoundingBoxStartRow() + startRow);
      pLayer->setPassArea(UPPER);
      pLayer->setRegionUnits(STD_DEV);
      pLayer->setFirstThreshold(pLayer->convertThreshold(STD_DEV, threshold, RAW_VALUE));
   }
   if (pOutArgList != NULL)
   {
      pOutArgList->setPlugInArgValue<RasterElement>("Results", pResult.get());
   }
   pRaster.release();
   pResult.release();

   progress.report("Complete", 100, NORMAL);
   progress.upALevel();
   return true;
}

RasterElement* Rx::createResults(int numRows, int numColumns, int numBands, const string& sigName, 
   EncodingType eType, RasterElement* pElement)
{
   clearPreviousResults(sigName, pElement);

   // create results element
   ModelResource<RasterElement> pResult(
      RasterUtilities::createRasterElement(sigName, numRows, numColumns, numBands, eType, BIP, true, pElement));

   if (pResult.get() == NULL)
   {
      //create the dataset on disk
      pResult = ModelResource<RasterElement>(
         RasterUtilities::createRasterElement(sigName, numRows, numColumns, numBands, eType, BIP, false, pElement));
   }
   return pResult.release();
}

void Rx::clearPreviousResults(const string& sigName, RasterElement* pElement)
{
   ModelResource<RasterElement> pResult(static_cast<RasterElement*>(
      Service<ModelServices>()->getElement(sigName, TypeConverter::toString<RasterElement>(), pElement)));
   if (pResult.get() != NULL && !isBatch())
   {
      Service<DesktopServices>()->showSuppressibleMsgDlg(sigName + " Exists",
         "The results data element already exists and will be replaced.",
         MESSAGE_WARNING, "Rx/ReplaceResults");
      Service<ModelServices>()->destroyElement(pResult.release());
   }
}

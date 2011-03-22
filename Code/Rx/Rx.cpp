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
#include "ThresholdLayer.h"
#include <QtCore/QtConcurrentMap>

REGISTER_PLUGIN_BASIC(RxModule, Rx);

namespace
{
   struct GlobalMeansMap
   {
      typedef QPair<int, QList<int> > input_type;
      typedef cv::Mat result_type;

      RasterElement* mpElement;
      const RasterDataDescriptor* mpDesc;
      int mBands;
      EncodingType mEncoding;

      GlobalMeansMap(RasterElement* pElement) : mpElement(pElement)
      {
         mpDesc = static_cast<const RasterDataDescriptor*>(mpElement->getDataDescriptor());
         mBands = mpDesc->getBandCount();
         mEncoding = mpDesc->getDataType();
      }

      result_type operator()(const input_type& locList)
      {
         int row = locList.first;
         DimensionDescriptor rowDesc = mpDesc->getActiveRow(row);
         FactoryResource<DataRequest> pReq;
         pReq->setInterleaveFormat(BIP);
         pReq->setRows(rowDesc, rowDesc);
         DataAccessor acc(mpElement->getDataAccessor(pReq.release()));
         ENSURE(acc.isValid());
         cv::Mat pixelMat(mBands, 1, CV_64F, 0.0);
         foreach(int col, locList.second)
         {
            acc->toPixel(row, col);
            for (int band = 0; band < mBands; ++band)
            {
               double val = Service<ModelServices>()->getDataValue(mEncoding, acc->getColumn(), band);
               pixelMat.at<double>(band, 0) += val;
            }
         }
         return pixelMat;
      }
   };

   void meansReduce(cv::Mat& final, const cv::Mat& intermediate)
   {
      if (final.empty())
      {
         final = intermediate;
      }
      else
      {
         final += intermediate;
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
   VERIFY(pArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pArgList->addArg<RasterElement>(DataElementArg()));
   VERIFY(pArgList->addArg<SpatialDataView>(ViewArg()));
   VERIFY(pArgList->addArg<AoiElement>("AOI", NULL, "Execute over this AOI only."));
   VERIFY(pArgList->addArg<double>("Threshold", 2.0, "Default result threshold in stddev."));
   VERIFY(pArgList->addArg<unsigned int>("Local Width", "Width of the local neighborhood used to calculate statistics. "
                                                        "If this or \"Local Height\" is not set or is set to 0, use global statistics."));
   VERIFY(pArgList->addArg<unsigned int>("Local Height", "Height of the local neighborhood used to calculate statistics. "
                                                         "If this or \"Local Width\" is not set or is set to 0, use global statistics."));
   VERIFY(pArgList->addArg<unsigned int>("Subspace Components", "Number of components to strip for subspace RX. "
                                                                "If this is not set or is set to 0, use standard RX."));
   return true;
}

bool Rx::getOutputSpecification(PlugInArgList*& pArgList)
{
   VERIFY(pArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pArgList->addArg<RasterElement>("Results"));
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
         progress.report("Cancelled by user", 100, ABORT, true);
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
      progress.report("Invalid local neighborhood size. Width and height must be at least 3 and odd.", 0, ERRORS, true);
      return false;
   }
   if (useSubspace && (components <= 0 || components >= pDesc->getBandCount()))
   {
      progress.report("Invalid number of subspace components. Must be 1 or more and less than the number of bands.", 0, ERRORS, true);
      return false;
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
         Service<ModelServices>()->getElement("Inverse Covariance Matrix", TypeConverter::toString<RasterElement>(), pElement));
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
   pReq->setColumns(pDesc->getActiveColumn(iter.getBoundingBoxStartColumn()), pDesc->getActiveColumn(iter.getBoundingBoxEndColumn()));
   DataAccessor acc(pElement->getDataAccessor(pReq.release()));

   // create results element
   ModelResource<RasterElement> pResult(static_cast<RasterElement*>(
      Service<ModelServices>()->getElement("RX Results", TypeConverter::toString<RasterElement>(), pElement)));
   if (pResult.get() != NULL && !isBatch())
   {
      Service<DesktopServices>()->showSuppressibleMsgDlg("RX Results Exists",
         "The results data element already exists and will be replaced.",
         MESSAGE_WARNING, "Rx/ReplaceResults");
      Service<ModelServices>()->destroyElement(pResult.release());
   }
   pResult = ModelResource<RasterElement>(
      RasterUtilities::createRasterElement("RX Results", iter.getNumSelectedRows(), iter.getNumSelectedColumns(), FLT8BYTES, true, pElement));
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

   // execure Rx
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

      if (!useLocal)
      {
         // setup the map-reduce and execute with progress reporting
         GlobalMeansMap meansMap(pElement);
         QFuture<cv::Mat> means;
         means = QtConcurrent::mappedReduced(locations.begin(), locations.end(), meansMap, meansReduce, QtConcurrent::UnorderedReduce);
         bool isCancelling = false;
         while (means.isRunning())
         {
            if (isCancelling)
            {
               progress.report("Cleaning up processing threads. Please wait.", 99, NORMAL);
            }
            else
            {
               progress.report("Calculating means",
                     (means.progressValue() - means.progressMinimum()) * 50 /
                           (means.progressMaximum() - means.progressMinimum()), NORMAL);
               if (isAborted())
               {
                  means.cancel();
                  isCancelling = true;
                  setAbortSupported(false);
               }
            }
            QThread::yieldCurrentThread();
         }

         if (means.isCanceled())
         {
            progress.report("User cancelled operation.", 100, ABORT, true);
            return false;
         }

         // complete the mean calculation
         muMat = means.result() / iter.getCount();
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
         progress.report("User cancelled operation.", 100, ABORT, true);
         return false;
      }
   }

   // display results
   if (!isBatch())
   {
      ThresholdLayer* pLayer = static_cast<ThresholdLayer*>(pView->createLayer(THRESHOLD, pResult.get()));
      pLayer->setXOffset(iter.getBoundingBoxStartColumn());
      pLayer->setYOffset(iter.getBoundingBoxStartRow());
      pLayer->setPassArea(UPPER);
      pLayer->setRegionUnits(STD_DEV);
      pLayer->setFirstThreshold(pLayer->convertThreshold(STD_DEV, threshold, RAW_VALUE));
   }
   if (pOutArgList != NULL)
   {
      pOutArgList->setPlugInArgValue<RasterElement>("Results", pResult.get());
   }
   pResult.release();

   progress.report("Complete", 100, NORMAL);
   progress.upALevel();
   return true;
}

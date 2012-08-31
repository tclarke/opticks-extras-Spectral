/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

// Keep this include here..it uses an OpenCV macro X.
// Moving this after Opticks includes will incorrectly use the Xerces X macro.
#include <cstddef>
#include <opencv/cv.h>
#include <memory>
#include <limits>

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
#include "Tad.h"
#include "TadDialog.h"
#include "SpatialDataView.h"
#include "SpectralVersion.h"
#include "ThresholdLayer.h"
#include "UtilityServices.h"
#include <QtCore/QtConcurrentMap>

REGISTER_PLUGIN_BASIC(TadModule, Tad);

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
 
   struct DistCalcMap
   {
      typedef unsigned int input_type;
      typedef std::vector<float> result_type;

      cv::Mat& mInputMat;

      DistCalcMap(cv::Mat& inputMat) : mInputMat(inputMat)
      {
      }

      result_type operator()(const input_type& index)
      {
         std::vector<float> pixelDistances;
         unsigned int bands = mInputMat.cols;
         unsigned int size = mInputMat.rows;

         unsigned int count = 0;
         std::vector<float> spectrum(bands);
         for (unsigned int j = 0; j < bands; j++)
         {
            spectrum[j] = mInputMat.at<float>(index, j);
         }

         //calculate the euclidian distance between the spectrum this and every other pixel
         for (unsigned int k = 0; k < size; k++)
         {
            double dist = 0.0;
            for (unsigned int l = 0; l < bands; l++)
            {
               double diff = spectrum[l] - mInputMat.at<float>(k, l);
               dist += diff * diff;
            }
            if (dist > std::numeric_limits<float>::epsilon()) 
            {
               pixelDistances.push_back(dist);
            }
         }
         return pixelDistances;
      }
   };

   //write the results to the data accessor after each calculation
   void distCalcReduce(std::vector<float>& final, 
      const std::vector<float>& intermediate)
   {
      unsigned int size = intermediate.size();
      for (unsigned int i = 0; i < size; i++)
      {

         final.push_back(intermediate[i]);
      }
   }

   struct BackCalcMap
   {
      typedef unsigned int input_type;
      typedef std::vector<unsigned int> result_type;

      cv::Mat& mInputMat;
      cv::flann::Index& mIndex;
      float mThreshold;
      float mRadius;

      BackCalcMap(cv::Mat& inputMat, cv::flann::Index& index, float threshold, float radius) : 
         mInputMat(inputMat), mIndex(index), mThreshold(threshold), mRadius(radius)
      {
      }

      result_type operator()(const input_type& index)
      {
         std::vector<unsigned int> backLocations;
         unsigned int bands = mInputMat.cols;
         unsigned int size = mInputMat.rows;
         std::vector<float> spectrum(bands);
         std::vector<int> indicesRadius(size);
         std::vector<float> distsRadius(size);

         for (unsigned int j = 0; j < bands; j++)
         {
            spectrum[j] = mInputMat.at<float>(index, j);
         }
         unsigned int paramsNum = static_cast<unsigned int>(floor(mThreshold / 100.0 * size)) + 1;
         if (paramsNum < 0.1 * size)
         {
            paramsNum = 0.1 * size;
         }
         //search for pixels close in value to the current pixel's spectrum
         unsigned int count = mIndex.radiusSearch(spectrum, indicesRadius, distsRadius, 
            mRadius *2.0 *PI, cv::flann::SearchParams(paramsNum));

         if (static_cast<float>(count) / size * 100.0 >= mThreshold)
         {
            //keep in the list of background pixels
            backLocations.push_back(index);
         }
         return backLocations;
      }
   };

   //write the results to the data accessor after each calculation
   void backCalcReduce(std::vector<unsigned int>& final, 
      const std::vector<unsigned int>& intermediate)
   {
      unsigned int size = intermediate.size();
      for (unsigned int i = 0; i < size; i++)
      {
         final.push_back(intermediate[i]);
      }
   }

   struct TadMap
   {
      typedef unsigned int input_type;
      typedef QPair<LocationType, QPair<DataAccessor*, double > > result_type;
      DataAccessor* mpResAcc;
      cv::Mat& mInputMat;
      cv::Mat& mBackground;
      cv::flann::Index& mFlannIndex;
      unsigned int mCols;
      unsigned int mStartRow;
      std::vector<LocationType>& mLocations;

      TadMap(cv::Mat& inputMat, cv::flann::Index& flannIndex, cv::Mat& background, 
         DataAccessor* pResAcc, std::vector<LocationType>& locations, unsigned int nCols,
         unsigned int startRow) :
               mInputMat(inputMat),
               mFlannIndex(flannIndex),
               mBackground(background),
               mpResAcc(pResAcc),
               mLocations(locations),
               mCols(nCols),
               mStartRow(startRow)
      {
      }

      //Perform the TAD calculation for each pixel in parallel
      result_type operator()(const input_type& loc)
      {
         //initialize the variables
         LocationType location(0, 0);
         double tadValue = 0.0;
         if (loc < mLocations.size())
         {
            //retrieve the location of the data relative to the output
            location.mY = mLocations[loc].mY;
            location.mX = mLocations[loc].mX;

            unsigned int inputMatLoc = (location.mY-mStartRow) * mCols + location.mX;
            //retrieve the set of bands for the specified pixels
            cv::Mat vec = mInputMat.row(inputMatLoc);
            std::vector<float> nnInput(mInputMat.cols);
            for (unsigned int i = 0; i < static_cast<unsigned int>(mInputMat.cols); i++)
            {
               nnInput[i] = static_cast<float>(vec.at<double>(i));
            }

            std::vector<int> indices(5);
            std::vector<float> dists(5);
            mFlannIndex.knnSearch(nnInput, indices, dists, 5, cv::flann::SearchParams(mBackground.cols));

            tadValue = sqrt(dists[2]);
            tadValue += sqrt(dists[3]);
            tadValue += sqrt(dists[4]);
         }
         return QPair<LocationType, QPair<DataAccessor*, double> >(location, QPair<DataAccessor*, double>(
            mpResAcc, tadValue));
      }
   };

   //write the results to the data accessor after each calculation
   void tadReduce(QPair<LocationType, QPair<DataAccessor*, double> >& final, 
      const QPair<LocationType, QPair<DataAccessor*, double> >& intermediate)
   {
      int row = intermediate.first.mY;
      int col = intermediate.first.mX;
      if (intermediate.second.first != NULL)
      {
         DataAccessor acc = *intermediate.second.first;
         if (acc.isValid())
         {
            acc->toPixel(row, col);

            //the data is assumed to be retrieved with a BIP accessor
            memcpy(acc->getColumn(), &intermediate.second.second, sizeof(double));
         }
      }
   }
}

Tad::Tad()
{
   setName("Tad");
   setDescriptorId("{6570919B-25D5-4305-8AE6-DD66C8E1DB72}");
   setSubtype("Anomaly Detection");
   setMenuLocation("[Spectral]/Anomaly Detection/TAD");
   setAbortSupported(true);
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   addDependencyCopyright("OpenCV", Service<UtilityServices>()->getTextFromFile(":/licenses/opencv"));
}

Tad::~Tad()
{
}

bool Tad::getInputSpecification(PlugInArgList*& pArgList)
{
   VERIFY(pArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pArgList->addArg<RasterElement>(DataElementArg()));
   VERIFY(pArgList->addArg<SpatialDataView>(ViewArg()));
   VERIFY(pArgList->addArg<AoiElement>("AOI", NULL, "Execute over this AOI only."));
   VERIFY(pArgList->addArg<double>("Component Threshold", 2.0, "if a region covers more than "
            "'Componet Size %' of the image, the region is declared background."));
   VERIFY(pArgList->addArg<double>("Background Threshold", 10.0, "the minimum distance for two spectrums "
                                    "to be considered different."));
   VERIFY(pArgList->addArg<unsigned int>("Sample Size", 10000,
                                    "The number of samples to use when calculating the background components. "));
   return true;
}

bool Tad::getOutputSpecification(PlugInArgList*& pArgList)
{
   VERIFY(pArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pArgList->addArg<RasterElement>("Results"));
   VERIFY(pArgList->addArg<double>("Threshold"));
   return true;
}

bool Tad::execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList)
{
   VERIFY(pInArgList);
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(ProgressArg()), "Executing TAD.",
      "spectral", "{3F6F80D6-F384-4E1C-8BB6-706FAAA87AC1}");

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
   double componentThreshold = 0.0;
   pInArgList->getPlugInArgValue("Component Threshold", componentThreshold);
   double backgroundThreshold = 0.0;
   pInArgList->getPlugInArgValue("Background Threshold", backgroundThreshold);
   unsigned int sampleSize = 0;
   bool useSubspace = pInArgList->getPlugInArgValue("Sample Size", sampleSize);

   // display options dialog
   if (!isBatch())
   {
      TadDialog dlg;
      std::vector<Layer*> layers;
      VERIFY(pView != NULL);
      pView->getLayerList()->getLayers(AOI_LAYER, layers);
      QList<QPair<QString, QString> > aoiIds;
      for (std::vector<Layer*>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
      {
         aoiIds.push_back(qMakePair(
                     QString::fromStdString((*layer)->getDisplayName(true)),
                     QString::fromStdString((*layer)->getId())));
      }
      dlg.setAoiList(aoiIds);
      dlg.setPercentBackground(backgroundThreshold);
      if (pAoi != NULL)
      {
         dlg.setAoi(QString::fromStdString(pAoi->getId()));
      }
      dlg.setComponentSize(componentThreshold);
      dlg.setSampleSize(sampleSize);
      if (dlg.exec() == QDialog::Rejected)
      {
         progress.report("Canceled by user", 100, ABORT, true);
         return false;
      }
      backgroundThreshold = dlg.getPercentBackground();
      componentThreshold = dlg.getComponentSize();
      sampleSize = dlg.getSampleSize();
      QString aoiId = dlg.getAoi();
      pAoi = aoiId.isEmpty() ? NULL : dynamic_cast<AoiElement*>(
               static_cast<Layer*>(Service<SessionManager>()->getSessionItem(
                        aoiId.toStdString()))->getDataElement());
   }

   //set up extents
   int selectedPixels = 0;
   ModelResource<RasterElement> pResult(static_cast<RasterElement*>(NULL));
   AoiElement* pLocalAoi = NULL;

   //clear out any filtered input when rerunning the tool
   string resultsName = "TAD Results";

   //retrieve the input bitmask iterator, a separate one is created because
   //we'll have to output a new AOI relative to the selected area
   const BitMask* pBitmask = (pAoi == NULL) ? NULL : pAoi->getSelectedPoints();
   BitMaskIterator iter(pBitmask, pElement);
   if (!*iter)
   {
      progress.report("No pixels selected for processing.", 0, ERRORS, true);
      return false;
   }
   unsigned int bands = pDesc->getBandCount();
   unsigned int numCols = iter.getNumSelectedColumns();
   unsigned int numRows = iter.getNumSelectedRows();
   unsigned int pixelCount = iter.getCount();
   unsigned int startCol = iter.getColumnOffset();
   unsigned int startRow = iter.getRowOffset();
   double resultBackgroundFraction = 0.0;
   double threshold = 0.0;
   bool bCancel = false;
   
   if (sampleSize > pixelCount)
   {
      progress.report("Invalid sample size. Cannot select more samples than there are pixels.", 
         0, ERRORS, true);
      return false;
   }
   // Calculate values using Topographical Anomaly Detector
   try
   {
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

      //first populate the samples vector
      unsigned int radiusSampleSize = sampleSize;
      if (radiusSampleSize > 2500)
      {
         radiusSampleSize = 2500;
      }
      progress.report("Generating Radius for background",
         1, NORMAL);
      std::auto_ptr<cv::Mat> pLocationsMat(getSampleOfPixels(radiusSampleSize, *pElement, iter));
      VERIFY(pLocationsMat.get() != NULL);

      //create the index to perform the searches on
      progress.report("Generating Radius for background",
         25, NORMAL);

      //do a search on every pixel in the samples, searching for the distance to every other pixel in the samples
      float radius = 0.0;
      progress.report("Generating Radius for background",
         50, NORMAL);

      std::vector<float> pixelDistances;
      // setup the map-reduce and execute with progress reporting
      DistCalcMap distMap(*pLocationsMat);
      QFuture<std::vector<float> > distances;
      QList<int> inputIndices;
      for (int i = 0; i < static_cast<int>(radiusSampleSize); i++)
      {
         inputIndices.push_back(i);
      }
      distances = QtConcurrent::mappedReduced(inputIndices.begin(), inputIndices.end(), distMap, distCalcReduce, 
         QtConcurrent::UnorderedReduce);
      bool isCancelling = false;
      while (distances.isRunning())
      {
         if (isCancelling)
         {
            progress.report("Cleaning up processing threads. Please wait.", 99, NORMAL);
         }
         else
         {
            //Handle fringe case of max=min to avoid divide by zero
            if (distances.progressMinimum() == distances.progressMaximum())
            {
               progress.report("Calculating distances in sample", 0, NORMAL);
            }
            else
            {
               progress.report("Calculating distances in sample",
                  (distances.progressValue() - distances.progressMinimum()) * 75 /
                  (distances.progressMaximum() - distances.progressMinimum()), NORMAL);
            }
            if (isAborted())
            {
               distances.cancel();
               isCancelling = true;
               setAbortSupported(false);
            }
         }
         QThread::yieldCurrentThread();
      }

      if (distances.isCanceled())
      {
         progress.report("User canceled operation.", 100, ABORT, true);
         return false;
      }

      // save the results from the distances calculation
      pixelDistances = distances.result() ;
      if (pixelDistances.empty())
      {
         progress.report("Could not generate pixel distances. Try larger data set.", 0, ERRORS, true);
         return false;
      }
      progress.report("Generating Radius for background",
         75, NORMAL);

      std::sort(pixelDistances.begin(), pixelDistances.end());
      radius = pixelDistances.at(static_cast<unsigned int>(ceil(backgroundThreshold/100.0*pixelDistances.size())));

      if (sampleSize != radiusSampleSize)
      {
         //we need to re-get the sample, as it is of a different size from the one used to get the radius
         pLocationsMat.reset(getSampleOfPixels(sampleSize, *pElement, iter));
         VERIFY(pLocationsMat.get() != NULL);
         sampleSize = pLocationsMat->rows;
      }
      std::auto_ptr<cv::flann::Index> pFlannIndex(new cv::flann::Index(*pLocationsMat, 
         cv::flann::KDTreeIndexParams(4)));

      //go through each pixel in the locations vector and determine if it makes up enough of the image
      //to be considered a background pixel
      std::vector<unsigned int> validBackgroundIndices;
      BackCalcMap backMap(*pLocationsMat, *pFlannIndex, componentThreshold, radius);
      QFuture<std::vector<unsigned int> > backgrounds;
      inputIndices.clear();
      for (int i = 0; i < static_cast<int>(sampleSize); i++)
      {
         inputIndices.push_back(i);
      }
      backgrounds = QtConcurrent::mappedReduced(inputIndices.begin(), inputIndices.end(), backMap, backCalcReduce, 
         QtConcurrent::UnorderedReduce);
      isCancelling = false;
      while (backgrounds.isRunning())
      {
         if (isCancelling)
         {
            progress.report("Cleaning up processing threads. Please wait.", 99, NORMAL);
         }
         else
         {
            progress.report("Calculating background values",
                  (backgrounds.progressValue() - backgrounds.progressMinimum()) * 100 /
                        (backgrounds.progressMaximum() - backgrounds.progressMinimum()), NORMAL);
            if (isAborted())
            {
               backgrounds.cancel();
               isCancelling = true;
               setAbortSupported(false);
            }
         }
         QThread::yieldCurrentThread();
      }

      if (backgrounds.isCanceled())
      {
         progress.report("User canceled operation.", 100, ABORT, true);
         return false;
      }

      // save the results of the background calculation
      validBackgroundIndices = backgrounds.result() ;
      unsigned int backgroundCount = validBackgroundIndices.size();
      resultBackgroundFraction = static_cast<double>(backgroundCount)/sampleSize;      
      threshold = resultBackgroundFraction*100.0;

      if (resultBackgroundFraction < 1.0 && resultBackgroundFraction > 0.0)
      {
         cv::Mat backLocationsMat(backgroundCount, bands, CV_32F);
         for (unsigned int i = 0; i < backgroundCount; i++)
         {
            for (unsigned int j = 0; j < bands; j++)
            {
               backLocationsMat.at<float>(i,j) = pLocationsMat->at<float>(validBackgroundIndices[i], j);
            }
         }
         std::auto_ptr<cv::flann::Index> backFlannIndex(new cv::flann::Index(backLocationsMat, 
            cv::flann::KDTreeIndexParams(4)));
      
         //create the output dataset
         pResult = ModelResource<RasterElement>(createResults(numRows, numCols, 1, resultsName, 
            FLT8BYTES, pElement));
         if (pResult.get() == NULL)
         {
            progress.report("Unable to create results.", 0, ERRORS, true);
            return false;
         }
         const RasterDataDescriptor* pResDesc = 
            static_cast<const RasterDataDescriptor*>(pResult->getDataDescriptor());
         VERIFY(pResDesc);
         unsigned int blockSize = 50;
         unsigned int numRowBlocks = numRows / blockSize;
         if (numRows%blockSize > 0)
         {
            numRowBlocks++;
         }
         int pixelCount = 0;

         // setup write data accessor
         FactoryResource<DataRequest> pResReq;
         pResReq->setWritable(true);
         DataAccessor resacc(pResult->getDataAccessor(pResReq.release()));
         if (!resacc.isValid())
         {
            progress.report("Unable to access data.", 0, ERRORS, true);
            return false;
         }

         //restart the iterator so we can put the values back in the same spot
         iter.begin();

         //now loop through each pixel of the actual image, searching for the 5 closest
         //neighbors and computing the sam distance between them to get a TAD value
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
            DataAccessor resacc(pResult->getDataAccessor(pReq.release()));
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
                  if (iter.getPixel(startCol + col, startRow + row))
                  {
                     switchOnEncoding(pDesc->getDataType(), readBandData, acc->getColumn(), pixelValues);

                     //store the data in the input matrix with information per band stored as a column to
                     //each pixels row
                     for (unsigned int band = 0; band < bands; ++band)
                     {
                        inputMat.at<double>((row - localStartRow) * numCols + col, band) = pixelValues[band];
                     }
                     indices.push_back(pixelCount);
                     aoiLocations.push_back(LocationType(col, row));
                     pixelCount++;
                  }
                  else
                  {
                     //if not within the AOI, then set the RasterElement value to 0
                     resacc->toPixel(row, col);
                     double zero = 0.0;
                     memcpy(resacc->getColumn(), &zero, sizeof(double));
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

            //create structure to calculate TAD on each pixel
            TadMap tadMap(inputMat, *backFlannIndex, backLocationsMat, &resacc, aoiLocations, numCols, localStartRow);
            QFuture<QPair<LocationType, QPair<DataAccessor*, double> > > tadResults;
            tadResults = QtConcurrent::mappedReduced(
               indices.begin(), indices.end(), tadMap, tadReduce, QtConcurrent::UnorderedReduce);
            bool isCancelling = false;
            float rowBlocksPercent = 100 / numRowBlocks;
            while (tadResults.isRunning())
            {
               if (isCancelling)
               {
                  progress.report("Cleaning up processing threads. Please wait.", 99, NORMAL);
               }
               else
               {
                  progress.report("Calculating Topographical Anomaly Detector result",
                        rowBlocksPercent*rowBlocks + 
                        (tadResults.progressValue() - tadResults.progressMinimum())*(rowBlocksPercent)/
                        (tadResults.progressMaximum() - tadResults.progressMinimum()), NORMAL);
                  if (isAborted())
                  {
                     tadResults.cancel();
                     isCancelling = true;
                     setAbortSupported(false);
                  }
               }
               QThread::yieldCurrentThread();
            }
            if (tadResults.isCanceled())
            {
               progress.report("User canceled operation.", 100, ABORT, true);
               return false;
            }
         }
         cv::Mat result(numRows, numCols, CV_64F, pResult->getRawData());
         double minVal = 0.0;
         double maxVal = 0.0;
         cv::minMaxLoc(result, &minVal, &maxVal);
         if (maxVal > 0)
         {
            result /= maxVal;
         }
      }
      else
      {
         progress.report("Could not distinguish background.", 100, ABORT, true);
         return false;
      }
   }
   catch (const cv::Exception& exc)
   {
      progress.report("OpenCV error: " + std::string(exc.what()), 0, ERRORS, true);
      return false;
   }
   
   // display results
   if (!isBatch())
   {
      ThresholdLayer* pLayer = static_cast<ThresholdLayer*>(pView->createLayer(THRESHOLD, pResult.get()));
      pLayer->setXOffset(iter.getBoundingBoxStartColumn());
      pLayer->setYOffset(iter.getBoundingBoxStartRow());
      pLayer->setPassArea(UPPER);
      pLayer->setRegionUnits(PERCENTILE);
      pLayer->setFirstThreshold(pLayer->convertThreshold(PERCENTILE, threshold, RAW_VALUE));
   }
   if (pOutArgList != NULL)
   {
      pOutArgList->setPlugInArgValue<RasterElement>("Results", pResult.get());
      pOutArgList->setPlugInArgValue<double>("Threshold", &threshold);
   }
   pResult.release();

   progress.report("Complete", 100, NORMAL);
   progress.upALevel();
   return true;
}

RasterElement* Tad::createResults(int numRows, int numColumns, int numBands, const string& sigName, 
   EncodingType eType, RasterElement* pElement)
{
   ModelResource<RasterElement> pResult(static_cast<RasterElement*>(
      Service<ModelServices>()->getElement(sigName, TypeConverter::toString<RasterElement>(), pElement)));
   if (pResult.get() != NULL && !isBatch())
   {
      Service<DesktopServices>()->showSuppressibleMsgDlg(sigName + " Exists",
         "The results data element already exists and will be replaced.",
         MESSAGE_WARNING, "Tad/ReplaceResults");
      Service<ModelServices>()->destroyElement(pResult.release());
   }

   // create results element
   pResult = ModelResource<RasterElement>(
      RasterUtilities::createRasterElement(sigName, numRows, numColumns, numBands, eType, BIP, true, pElement));

   if (pResult.get() == NULL)
   {
      //create the dataset on disk
      pResult = ModelResource<RasterElement>(
         RasterUtilities::createRasterElement(sigName, numRows, numColumns, numBands, eType, BIP, false, pElement));
   }
   return pResult.release();
}

cv::Mat* Tad::getSampleOfPixels(unsigned int& sampleSize, RasterElement& element, BitMaskIterator& iter)
{
   unsigned int numCols = iter.getNumSelectedColumns();
   unsigned int numRows = iter.getNumSelectedRows();
   unsigned int startCol = iter.getColumnOffset();
   unsigned int startRow = iter.getRowOffset();
   RasterDataDescriptor* pDesc = dynamic_cast<RasterDataDescriptor*>(element.getDataDescriptor());

   VERIFYRV(pDesc != NULL, NULL);

   unsigned int bands = pDesc->getBandCount();

   cv::Mat fltIndices(sampleSize, 1, CV_64F);
   for (unsigned int i = 0; i < sampleSize; i++)
   {
      fltIndices.at<double>(i,0) = i;
   }
   double multiplier = (static_cast<double>(numCols * numRows - 1) / static_cast<double>(sampleSize - 1));

   //make sure all of the sample data is not from the same row or column
   if ((static_cast<int>(ceil(multiplier)) % numCols == 0) ||
      (static_cast<int>(ceil(multiplier)) % numRows == 0) ||
      (static_cast<int>(floor(multiplier)) % numCols == 0) ||
      (static_cast<int>(floor(multiplier)) % numRows == 0))
   {
      multiplier -= 2.0;
   }
   fltIndices *= multiplier;

   //populate a location vector with points we will use to determine background
   std::vector<cv::Point> locationsVector;
   bool bEqual = true;
      
   for (unsigned int i = 0; i < sampleSize; i++)
   {
      int indexPos = floor(fltIndices.at<double>(i));
      int r = indexPos / numCols + startRow;
      int c = indexPos % numCols + startCol;
      if (r != c)
      {
         bEqual = false;
      }

      //only add to the vector if it is inside the AOI
      if (iter.getPixel(c, r))
      {
         cv::Point point;
         point.x = c;
         point.y = r;
         locationsVector.push_back(point);
      }
   }
   if (bEqual && locationsVector.size() > 0)
   {
      locationsVector.at(0).x += 1;
   }

   //set up the input data accessor
   DimensionDescriptor inputRowDesc = pDesc->getActiveRow(startRow);
   DimensionDescriptor inputRowDesc2 = pDesc->getActiveRow(startRow + numRows - 1);
   FactoryResource<DataRequest> pInputReq;
   pInputReq->setInterleaveFormat(BIP);
   pInputReq->setRows(inputRowDesc, inputRowDesc2);
   DataAccessor acc(element.getDataAccessor(pInputReq.release()));
   VERIFYRV(acc.isValid(), NULL);

   std::vector<double> pixelValues(bands);
   sampleSize = locationsVector.size();
   std::auto_ptr<cv::Mat> pLocationsMat( new cv::Mat(sampleSize, bands, CV_32F) );

   for (unsigned int i = 0; i < sampleSize; i++)
   {
      int r = locationsVector.at(i).y;
      int c = locationsVector.at(i).x; 
      acc->toPixel(r-startRow, c-startCol);

      //get the value from the raster element
      switchOnEncoding(pDesc->getDataType(), readBandData, acc->getColumn(), pixelValues);
      for (unsigned int j = 0; j < bands; j++)
      {
         pLocationsMat->at<float>(i,j) = pixelValues[j];
      }
   }
   return pLocationsMat.release();
}

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AoiElement.h"
#include "AppVerify.h"
#include "BitMask.h"
#include "BitMaskIterator.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DataVariant.h"
#include "MessageLogResource.h"
#include "ObjectResource.h"
#include "Progress.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "Signature.h"
#include "SignatureSet.h"
#include "SpectralUtilities.h"
#include "StringUtilities.h"
#include "switchOnEncoding.h"
#include "TypesFile.h"
#include "Wavelengths.h"
#include <QtCore/QtConcurrentMap>

#include <vector>
#include <string>

namespace
{
   template<typename T>
   void averageSignatureAccum(T* pPtr, std::vector<double>& accum)
   {
      for (std::vector<double>::size_type band = 0; band < accum.size(); ++band)
      {
         accum[band] += pPtr[band];
      }
   }

   struct GlobalMeansMap
   {
      typedef unsigned int input_type;
      typedef std::vector<double> result_type;

      const RasterElement* mpElement;
      const RasterDataDescriptor* mpDesc;
      int mBands;
      EncodingType mEncoding;
      BitMaskIterator& mIter;
      unsigned int mStartCol;
      unsigned int mEndCol;

      GlobalMeansMap(const RasterElement* pElement, BitMaskIterator& iter) : mpElement(pElement),
         mIter(iter), mBands(0), mEncoding(INT1SBYTE), mStartCol(0), mEndCol(0)
      {
         mpDesc = dynamic_cast<const RasterDataDescriptor*>(mpElement->getDataDescriptor());
         VERIFYNRV(mpDesc != NULL);
         mBands = mpDesc->getBandCount();
         mEncoding = mpDesc->getDataType();
         mStartCol = mIter.getBoundingBoxStartColumn();
         mEndCol = mIter.getBoundingBoxEndColumn();
      }

      result_type operator()(const input_type& row)
      {
         std::vector<double> pixelVec(mBands, 0.0);
         VERIFYRV(mpDesc != NULL, pixelVec);
         DimensionDescriptor rowDesc = mpDesc->getActiveRow(row);
         FactoryResource<DataRequest> pReq;
         pReq->setInterleaveFormat(BIP);
         pReq->setRows(rowDesc, rowDesc);
         DataAccessor acc(mpElement->getDataAccessor(pReq.release()));
         ENSURE(acc.isValid());
         Service<ModelServices> pModel;

         for (unsigned int col = mStartCol; col <= mEndCol; col++)
         {
            if (mIter.getPixel(col, row))
            {
               acc->toPixel(row, col);
               for (int band = 0; band < mBands; ++band)
               {
                  double val = pModel->getDataValue(mEncoding, acc->getColumn(), band);
                  pixelVec.at(band) += val;
               }
            }
         }
         return pixelVec;
      }
   };

   void meansReduce(std::vector<double>& final, const std::vector<double>& intermediate)
   {
      if (final.empty())
      {
         final = intermediate;
      }
      else
      {
         for (unsigned int i = 0; i < final.size(); i++)
         {
            final[i] += intermediate[i];
         }
      }
   }
}

std::vector<Signature*> SpectralUtilities::extractSignatures(const std::vector<Signature*>& signatures)
{
   std::vector<Signature*> extractedSignatures;
   for (std::vector<Signature*>::const_iterator iter = signatures.begin(); iter != signatures.end(); ++iter)
   {
      Signature* pSignature = *iter;
      if (pSignature != NULL)
      {
         SignatureSet* pSignatureSet = dynamic_cast<SignatureSet*>(pSignature);
         if (pSignatureSet != NULL)
         {
            const std::vector<Signature*>& setSignatures = pSignatureSet->getSignatures();

            std::vector<Signature*> extractedSetSignatures = extractSignatures(setSignatures);
            for (std::vector<Signature*>::size_type i = 0; i < extractedSetSignatures.size(); ++i)
            {
               Signature* pSetSignature = extractedSetSignatures[i];
               if (pSetSignature != NULL)
               {
                  extractedSignatures.push_back(pSetSignature);
               }
            }
         }
         else
         {
            extractedSignatures.push_back(pSignature);
         }
      }
   }

   return extractedSignatures;
}

Signature* SpectralUtilities::getPixelSignature(RasterElement* pDataset, const Opticks::PixelLocation& pixel)
{
   if (pDataset == NULL)
   {
      return NULL;
   }

   RasterDataDescriptor* pDescriptor = static_cast<RasterDataDescriptor*>(pDataset->getDataDescriptor());
   VERIFYRV(pDescriptor != NULL, NULL);

   // Check for an invalid coordinate
   if ((pixel.mY >= static_cast<int>(pDescriptor->getRowCount())) ||
      (pixel.mX >= static_cast<int>(pDescriptor->getColumnCount())) ||
      (pixel.mY < 0 || pixel.mX < 0))
   {
      return NULL;
   }

   // Get the wavelength data
   std::vector<double> centerWaves;

   DynamicObject* pMetadata = pDescriptor->getMetadata();
   if (pMetadata != NULL)
   {
      FactoryResource<Wavelengths> pWavelengths;
      pWavelengths->initializeFromDynamicObject(pMetadata, false);
      centerWaves = pWavelengths->getCenterValues();
   }

   // Get the reflectance data
   unsigned int row = static_cast<unsigned int>(pixel.mY);
   unsigned int column = static_cast<unsigned int>(pixel.mX);
   DimensionDescriptor rowDim = pDescriptor->getActiveRow(row);
   DimensionDescriptor columnDim = pDescriptor->getActiveColumn(column);

   FactoryResource<DataRequest> pRequest;
   pRequest->setInterleaveFormat(BIP);
   pRequest->setRows(rowDim, rowDim);
   pRequest->setColumns(columnDim, columnDim);
   DataAccessor accessor = pDataset->getDataAccessor(pRequest.release());
   if (accessor.isValid() == false)
   {
      return NULL;
   }

   EncodingType dataType = pDescriptor->getDataType();
   if (!dataType.isValid())
   {
      return NULL;
   }

   std::vector<double> reflectanceData(pDescriptor->getBandCount(), 0.0);

   void* pData = accessor->getRow();

   const std::vector<DimensionDescriptor>& activeBands = pDescriptor->getBands();
   for (unsigned int i = 0; i < activeBands.size(); i++)
   {
      DimensionDescriptor bandDim = activeBands[i];
      if (bandDim.isActiveNumberValid() == true)
      {
         unsigned int activeBand = bandDim.getActiveNumber();
         double dValue = 0.0;
         switchOnComplexEncoding(dataType, ModelServices::getDataValue, pData, COMPLEX_MAGNITUDE, i, dValue);

         if (activeBand < reflectanceData.size())
         {
            reflectanceData[activeBand] = dValue;
         }
      }
   }

   // Get the signature element
   Service<ModelServices> pModel;

   char spectrumName[32];

   // use original row and column numbers instead of the active numbers (data may be a subset)
   Opticks::PixelLocation display;
   display.mX = static_cast<int>(pDescriptor->getActiveColumn(pixel.mX).getOriginalNumber());
   display.mY = static_cast<int>(pDescriptor->getActiveRow(pixel.mY).getOriginalNumber());

   sprintf(spectrumName, "Pixel (%d, %d)", display.mX + 1, display.mY + 1);

   Signature* pSignature = static_cast<Signature*>(pModel->getElement(std::string(spectrumName), "Signature", pDataset));
   if (pSignature == NULL)
   {
      DataDescriptor* pSignatureDescriptor = pModel->createDataDescriptor(std::string(spectrumName), "Signature", pDataset);
      if (pSignatureDescriptor != NULL)
      {
         pSignatureDescriptor->setClassification(pDataset->getClassification());

         pSignature = static_cast<Signature*>(pModel->createElement(pSignatureDescriptor));
      }
   }

   if (pSignature != NULL)
   {
      // Set the spectrum data
      pSignature->setData("Wavelength", DataVariant(centerWaves));
      pSignature->setData("Reflectance", DataVariant(reflectanceData));

      // Set the units
      const Units* pUnits = pDescriptor->getUnits();
      if (pUnits != NULL)
      {
         pSignature->setUnits("Reflectance", pUnits);
      }
   }

   return pSignature;
}

bool SpectralUtilities::convertAoiToSignature(AoiElement* pAoi, Signature* pSignature, RasterElement* pElement,
                                              Progress* pProgress, bool* pAbort)
{
   VERIFY(pAoi != NULL && pSignature != NULL);

   if (pElement == NULL)
   {
      pElement = dynamic_cast<RasterElement*>(pAoi->getParent());
   }
   if (pElement == NULL)
   {
      return false;
   }

   const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
   VERIFY(pDesc != NULL);

   const BitMask* pPoints = pAoi->getSelectedPoints();
   VERIFY(pPoints != NULL);
   BitMaskIterator it(pPoints, pElement);

   // check for empty AOI
   if (it == it.end())
   {
      if (pProgress != NULL)
      {
         pProgress->updateProgress("There are no selected pixels in the AOI", 0, ERRORS);
      }
      return false;
   }
   FactoryResource<DataRequest> request;
   request->setInterleaveFormat(BIP);
   request->setRows(pDesc->getActiveRow(it.getBoundingBoxStartRow()),
      pDesc->getActiveRow(it.getBoundingBoxEndRow()));
   request->setColumns(pDesc->getActiveColumn(it.getBoundingBoxStartColumn()),
      pDesc->getActiveColumn(it.getBoundingBoxEndColumn()));
   DataAccessor accessor = pElement->getDataAccessor(request.release());
   VERIFY(accessor.isValid());

   std::vector<double> reflectances(pDesc->getBandCount());
   int cnt = 0;
   int numRows = it.getNumSelectedRows();
   int rowIndex(0);
   std::string msg = "Computing average signature for AOI...";
   if (pProgress != NULL)
   {
      pProgress->updateProgress(msg, 0, NORMAL);
   }
   for (int y = it.getBoundingBoxStartRow(); y <= it.getBoundingBoxEndRow(); ++y)
   {
      if (pAbort != NULL && *pAbort)
      {
         if (pProgress != NULL)
         {
            pProgress->updateProgress("Compute AOI average signature aborted", 0, ABORT);
         }
         return false;
      }
      for (int x = it.getBoundingBoxStartColumn(); x <= it.getBoundingBoxEndColumn(); ++x)
      {
         accessor->toPixel(y, x);
         VERIFY(accessor.isValid());
         if (pPoints->getPixel(x, y))
         {
            switchOnEncoding(pDesc->getDataType(), averageSignatureAccum, accessor->getColumn(), reflectances);
            cnt++;
         }
      }
      ++rowIndex;
      if (pProgress != NULL)
      {
         pProgress->updateProgress(msg, rowIndex * 100 / numRows, NORMAL);
      }
   }
   if (cnt != 0)
   {
      for (std::vector<double>::iterator reflectance = reflectances.begin(); reflectance != reflectances.end(); ++reflectance)
      {
         *reflectance /= cnt;
      }
   }

   FactoryResource<Wavelengths> pWavelengths;
   pWavelengths->initializeFromDynamicObject(pElement->getMetadata(), false);

   std::vector<double> wavelengthData = pWavelengths->getCenterValues();
   unsigned int sz = reflectances.size();
   if (!wavelengthData.empty() && wavelengthData.size() < sz)
   {
      sz = wavelengthData.size();
   }
   if (reflectances.size() != sz)
   {
      MessageResource msg("Reflectance data is too long and will be truncated.",
         "spectral", "B6C2AD5C-6B7B-4C03-8633-632A8BE6284D");
      msg->addProperty("Old size", static_cast<int>(reflectances.size()));
      msg->addProperty("New size", sz);
      reflectances.resize(sz);
   }
   if (!wavelengthData.empty() && wavelengthData.size() != sz)
   {
      MessageResource msg("Wavelength data is too long and will be truncated.",
         "spectral", "A0C90436-C7CF-4E74-8E87-E72BE47AE7F2");
      msg->addProperty("Old size", static_cast<int>(wavelengthData.size()));
      msg->addProperty("New size", sz);
      wavelengthData.resize(sz);
   }
   const std::vector<DimensionDescriptor>& bands = pDesc->getBands();
   std::vector<unsigned int> bandNumbers;
   for (std::vector<DimensionDescriptor>::const_iterator band = bands.begin(); band != bands.end(); ++band)
   {
      if (band->isActiveNumberValid())
      {
         bandNumbers.push_back(band->getActiveNumber());
      }
   }
   pSignature->setData("BandNumber", bandNumbers);
   pSignature->setData("Reflectance", reflectances);
   pSignature->setData("Wavelength", wavelengthData);
   pSignature->setUnits("Reflectance", pDesc->getUnits());

   if (pProgress != NULL)
   {
      pProgress->updateProgress("Finished computing AOI average signature", 100, NORMAL);
   }
   return true;
}

std::string SpectralUtilities::getFailedDataRequestErrorMessage(const DataRequest* pRequest,
   const RasterElement* pElement)
{
#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : This should be changed to query for a \"real\" error message (dadkins)")
   if (pRequest == NULL)
   {
      return "Data Request cannot be NULL.\n";
   }

   if (pElement == NULL)
   {
      return "Raster Element cannot be NULL.\n";
   }

   const RasterDataDescriptor* pDescriptor =
      dynamic_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
   if (pDescriptor == NULL)
   {
      return "Unable to obtain a Raster Data Descriptor.\n";
   }

   if (pDescriptor->getInterleaveFormat() == BSQ && pRequest->getConcurrentBands() != 1)
   {
      return "Cannot request more than one concurrent band when interleave is BSQ";
   }

   std::string errorMessage;
   if (pRequest->getWritable() == true)
   {
      if (pDescriptor->getProcessingLocation() == ON_DISK_READ_ONLY)
      {
         errorMessage += "Unable to obtain a writable Data Accessor for a dataset which has been loaded " +
            StringUtilities::toDisplayString(ON_DISK_READ_ONLY) + ".\n";
      }

      const InterleaveFormatType requestedInterleave = pRequest->getInterleaveFormat();
      const InterleaveFormatType defaultInterleave = pDescriptor->getInterleaveFormat();
      if (requestedInterleave.isValid() && requestedInterleave != defaultInterleave)
      {
         errorMessage += "Unable to obtain a writable " +
            StringUtilities::toDisplayString(requestedInterleave) + " Data Accessor for a " +
            StringUtilities::toDisplayString(defaultInterleave) + " dataset.\n";
      }
   }

   return errorMessage;
}

std::vector<Signature*> SpectralUtilities::getAoiSignatures(const AoiElement* pAoi, RasterElement* pElement,
                                                            Progress* pProgress, bool* pAbort)
{
   std::vector<Signature*> signatures;

   if (pAoi == NULL || pElement == NULL)
   {
      return signatures;
   }

   // Get each selected pixel signature
   if (pProgress != NULL)
   {
      pProgress->updateProgress("Generating AOI pixel signatures...", 0, NORMAL);
   }

   BitMaskIterator bit(pAoi->getSelectedPoints(), pElement);

   // check for empty AOI
   if (bit == bit.end())
   {
      if (pProgress != NULL)
      {
         pProgress->updateProgress("There are no selected pixels in the AOI", 0, ERRORS);
      }
      return signatures;
   }
   int startRow(0);
   int endRow(0);
   int startCol(0);
   int endCol(0);
   bit.getBoundingBox(startCol, startRow, endCol, endRow);
   int numRows = endRow - startRow + 1;
   int rowCounter(0);
   int lastRow(startRow);
   for (; bit != bit.end(); ++bit)
   {
      if (pAbort != NULL && *pAbort)
      {
         if (pProgress != NULL)
         {
            pProgress->updateProgress("Generating AOI pixel signatures aborted", 0, ABORT);
         }
         signatures.clear();
         return signatures;
      }
      Opticks::PixelLocation pixLoc(bit.getPixelColumnLocation(), bit.getPixelRowLocation());
      Signature* pSignature = SpectralUtilities::getPixelSignature(pElement, pixLoc);
      if (pSignature != NULL)
      {
         signatures.push_back(pSignature);
      }
      if (pProgress != NULL && pixLoc.mY > lastRow)
      {
         ++rowCounter;
         lastRow = pixLoc.mY;
         pProgress->updateProgress("Generating AOI pixel signatures...", rowCounter * 100 / numRows, NORMAL);
      }

   }
   if (pProgress != NULL)
   {
      pProgress->updateProgress("Finished generating AOI pixel signatures", 100, NORMAL);
   }

   return signatures;
}

std::vector<double> SpectralUtilities::calculateMeans(const RasterElement* pElement,
   BitMaskIterator& iter, ProgressTracker& progress, bool* pAbort)
{
   std::vector<double> muMat;
   VERIFYRV(pElement != NULL, muMat);

   unsigned int count = iter.getCount();
   if (count == 0)
   {
      progress.report("Need to calculate means on at least one pixel.", 100, ERRORS, true);
      return muMat;
   }
   QList<int> rows;
   unsigned int startRow = iter.getBoundingBoxStartRow();
   unsigned int endRow = iter.getBoundingBoxEndRow();
   for (unsigned int row = startRow; row <= endRow; row++)
   {
      rows.push_back(row);
   }
   // setup the map-reduce and execute with progress reporting
   GlobalMeansMap meansMap(pElement, iter);
   QFuture<std::vector<double> > means;
   means = QtConcurrent::mappedReduced(rows.begin(), rows.end(), meansMap, meansReduce, 
      QtConcurrent::UnorderedReduce);
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
         if (pAbort != NULL && *pAbort)
         {
            means.cancel();
            isCancelling = true;
         }
      }
      QThread::yieldCurrentThread();
   }

   if (means.isCanceled())
   {
      progress.report("User canceled operation.", 100, ABORT, true);
      muMat.clear();
      return muMat;
   }

   // complete the mean calculation
   for (unsigned int i = 0; i < means.result().size(); i++)
   {
      muMat.push_back(means.result().at(i) / count);
   }
   iter.firstPixel();
   return muMat;
}
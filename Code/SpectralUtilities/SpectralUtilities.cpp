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
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "Signature.h"
#include "SignatureSet.h"
#include "SpectralUtilities.h"
#include "StringUtilities.h"
#include "switchOnEncoding.h"
#include "TypesFile.h"
#include "Wavelengths.h"

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
      Wavelengths wavelengths(pMetadata);
      centerWaves = wavelengths.getCenterValues();
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
   sprintf(spectrumName, "Pixel (%d, %d)", static_cast<int>(pixel.mX) + 1, static_cast<int>(pixel.mY) + 1);

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

bool SpectralUtilities::convertAoiToSignature(AoiElement* pAoi, Signature* pSignature, RasterElement* pElement)
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
   Wavelengths wavelengths(pElement->getMetadata());

   const BitMask* pPoints = pAoi->getSelectedPoints();
   VERIFY(pPoints != NULL);
   BitMaskIterator it(pPoints, pElement);
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
   for (int y = it.getBoundingBoxStartRow(); y <= it.getBoundingBoxEndRow(); ++y)
   {
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
   }
   if (cnt != 0)
   {
      for (std::vector<double>::iterator reflectance = reflectances.begin(); reflectance != reflectances.end(); ++reflectance)
      {
         *reflectance /= cnt;
      }
   }
   std::vector<double> wavelengthData;
   wavelengthData = wavelengths.getCenterValues();
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

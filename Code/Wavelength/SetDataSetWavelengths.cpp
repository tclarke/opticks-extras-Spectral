/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "DynamicObject.h"
#include "LayerList.h"
#include "MessageLogResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "Progress.h"
#include "RasterElement.h"
#include "SetDataSetWavelengths.h"
#include "SpatialDataView.h"
#include "SpectralVersion.h"
#include "Wavelengths.h"

using namespace std;

REGISTER_PLUGIN_BASIC(SpectralWavelength, SetDataSetWavelengths);

SetDataSetWavelengths::SetDataSetWavelengths()
{
   setName("Set Data Set Wavelengths");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setType(Wavelengths::WavelengthType());
   setDescription("Sets wavelengths into an existing data set");
   setDescriptorId("{D4C4B967-B7A2-4F1A-8052-567330632BA2}");
   allowMultipleInstances(true);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

SetDataSetWavelengths::~SetDataSetWavelengths()
{
}

bool SetDataSetWavelengths::getInputSpecification(PlugInArgList*& pArgList)
{
   Service<PlugInManagerServices> pManager;
   pArgList = pManager->getPlugInArgList();
   VERIFY(pArgList != NULL);

   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   VERIFY(pArgList->addArg<RasterElement>(Executable::DataElementArg(), NULL));

   if (isBatch() == false)
   {
      VERIFY(pArgList->addArg<SpatialDataView>(Executable::ViewArg(), NULL));
   }

   VERIFY(pArgList->addArg<DynamicObject>(Wavelengths::WavelengthsArg()));
   return true;
}

bool SetDataSetWavelengths::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

bool SetDataSetWavelengths::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (pInArgList == NULL)
   {
      return false;
   }

   StepResource pStep(string("Execute ") + getName(), "Spectral", "863CB0EE-5BC0-4A49-8FCB-FBC385F1AD2D");

   // Extract the input args
   Progress* pProgress = pInArgList->getPlugInArgValue<Progress>(ProgressArg());

   RasterElement* pDataset = pInArgList->getPlugInArgValue<RasterElement>(DataElementArg());
   if ((pDataset == NULL) && (isBatch() == false))
   {
      SpatialDataView* pView = pInArgList->getPlugInArgValue<SpatialDataView>(ViewArg());
      if (pView != NULL)
      {
         LayerList* pLayerList = pView->getLayerList();
         if (pLayerList != NULL)
         {
            pDataset = pLayerList->getPrimaryRasterElement();
         }
      }
   }

   if (pDataset == NULL)
   {
      string message = "The data set input value is invalid.";
      if (pProgress != NULL)
      {
         pProgress->updateProgress(message, 0, ERRORS);
      }

      pStep->finalize(Message::Failure, message);
      return false;
   }

   DynamicObject* pWavelengthData = pInArgList->getPlugInArgValue<DynamicObject>(Wavelengths::WavelengthsArg());
   if (pWavelengthData == NULL)
   {
      string message = "The " + Wavelengths::WavelengthsArg() + " input value is invalid.";
      if (pProgress != NULL)
      {
         pProgress->updateProgress(message, 0, ERRORS);
      }

      pStep->finalize(Message::Failure, message);
      return false;
   }

   // Apply the wavelength data to the data set
   Wavelengths wavelengths(pWavelengthData);
   if (wavelengths.applyToDataset(pDataset) == false)
   {
      string message = "The wavelengths could not be applied to the data set.  The number of wavelength values "
         "may not match the number of bands in the data set.";
      if (pProgress != NULL)
      {
         pProgress->updateProgress(message, 0, ERRORS);
      }

      pStep->finalize(Message::Failure, message);
      return false;
   }

   pStep->finalize(Message::Success);
   return true;
}

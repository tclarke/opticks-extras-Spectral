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
#include "GetDataSetWavelengths.h"
#include "LayerList.h"
#include "MessageLogResource.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "Progress.h"
#include "RasterElement.h"
#include "SpatialDataView.h"
#include "SpectralVersion.h"
#include "StringUtilities.h"
#include "Wavelengths.h"

#include <vector>
using namespace std;

REGISTER_PLUGIN_BASIC(SpectralWavelength, GetDataSetWavelengths);

GetDataSetWavelengths::GetDataSetWavelengths()
{
   setName("Get Data Set Wavelengths");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setType(Wavelengths::WavelengthType());
   setDescription("Retrieves the wavelengths from an existing data set");
   setDescriptorId("{6CB722F9-8BAF-48DD-87BC-606C5A54AE2E}");
   allowMultipleInstances(true);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

GetDataSetWavelengths::~GetDataSetWavelengths()
{
}

bool GetDataSetWavelengths::getInputSpecification(PlugInArgList*& pArgList)
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

   return true;
}

bool GetDataSetWavelengths::getOutputSpecification(PlugInArgList*& pArgList)
{
   Service<PlugInManagerServices> pManager;
   pArgList = pManager->getPlugInArgList();
   VERIFY(pArgList != NULL);

   VERIFY(pArgList->addArg<DynamicObject>(Wavelengths::WavelengthsArg()));
   return true;
}

bool GetDataSetWavelengths::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (pInArgList == NULL)
   {
      return false;
   }

   StepResource pStep(string("Execute ") + getName(), "Spectral", "9AD895AB-F5BB-4CBB-9351-179B19238B13");

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

   // Get the wavelengths from the data set
   const DynamicObject* pMetadata = pDataset->getMetadata();
   VERIFY(pMetadata != NULL);

   // Populate the output arg list
   if (pOutArgList != NULL)
   {
      FactoryResource<DynamicObject> pWavelengthData;

      Wavelengths wavelengths(pWavelengthData.get());
      if (wavelengths.initializeFromDynamicObject(pMetadata) == true)
      {
         pOutArgList->setPlugInArgValue<DynamicObject>(Wavelengths::WavelengthsArg(), pWavelengthData.get());
      }
   }

   pStep->finalize(Message::Success);
   return true;
}

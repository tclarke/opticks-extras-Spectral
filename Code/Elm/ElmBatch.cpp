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
#include "ElmBatch.h"
#include "LayerList.h"
#include "MessageLogResource.h"
#include "ModelServices.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "Progress.h"
#include "RasterElement.h"
#include "Signature.h"
#include "SpatialDataView.h"
#include "SpectralVersion.h"

using namespace std;

REGISTER_PLUGIN_BASIC(SpectralElm, ElmBatch);

ElmBatch::ElmBatch()
{
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setName("ELM Batch");
   setDescription("ELM Batch");
   setShortDescription("ELM Batch");
   setDescriptorId("{88FB2862-C24E-4415-BADF-FE1D943C1E6E}");
}

ElmBatch::~ElmBatch()
{
}

bool ElmBatch::setInteractive()
{
   return false;
}

bool ElmBatch::getInputSpecification(PlugInArgList*& pArgList)
{
   if (ElmCore::getInputSpecification(pArgList) == false)
   {
      return false;
   }

   // Batch mode: Use Gains/Offsets Flag, Gains Offsets Filename, Reflectance Filenames, and AOI File/Layer names
   VERIFY(pArgList->addArg<bool>(UseGainsOffsetsArg(), true, "Flag for whether gains/offsets should be loaded "
      "from an existing file."));
   VERIFY(pArgList->addArg<Filename>(GainsOffsetsFilenameArg(), NULL, "Name of the file containing gains/offsets, "
      "if they are to be loaded from a file."));

   string description = "Filenames of signatures for ELM to use. The number of signatures specified must match the "
      "number of AOIs specified by " + AoiFilenamesArg() + ".";
   VERIFY(pArgList->addArg<vector<Filename*> >(SignatureFilenamesArg(), NULL, description));

   description = "Filenames for AOIs over which ELM will be performed. The number of AOIs specified must match "
      "the number of signatures specified by " + SignatureFilenamesArg() + ".";
   VERIFY(pArgList->addArg<vector<Filename*> >(AoiFilenamesArg(), NULL, description));

   return true;
}

bool ElmBatch::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = Service<PlugInManagerServices>()->getPlugInArgList();
   VERIFY(pArgList != NULL);

   // Batch mode: RasterElement
   VERIFY(pArgList->addArg<RasterElement>(Executable::DataElementArg(), NULL, "Raster element containing reflectance "
      "data resulting from the ELM operation."));
   return true;
}

bool ElmBatch::execute(PlugInArgList* pInputArgList, PlugInArgList* pOutputArgList)
{
   StepResource pStep("Execute " + getName(), "app", "FA402F8E-3CD4-408f-9E6F-200C2AC6814B");
   VERIFY(pStep.get() != NULL);

   if (extractInputArgs(pInputArgList) == false)
   {
      pStep->finalize(Message::Failure, "extractInputArgs() returned false.");
      return false;
   }

   bool success = executeElm(mGainsOffsetsFilename, mpSignatures, mpAoiElements);

   Service<ModelServices> pModel;
   for (vector<Signature*>::iterator iter =  mpSignaturesToDestroy.begin();
      iter != mpSignaturesToDestroy.end(); ++iter)
   {
      pModel->destroyElement(dynamic_cast<DataElement*>(*iter));
   }

   for (vector<AoiElement*>::iterator iter =  mpAoiElementsToDestroy.begin();
      iter != mpAoiElementsToDestroy.end(); ++iter)
   {
      pModel->destroyElement(dynamic_cast<DataElement*>(*iter));
   }

   if (success == false)
   {
      pStep->finalize(Message::Failure, "ElmCore::executeElm() returned false");
      return false;
   }

   if (pOutputArgList == NULL)
   {
      pStep->finalize(Message::Failure, "No output argument list defined.");
      return false;
   }

   if (pOutputArgList->setPlugInArgValue(DataElementArg(), mpRasterElement) == false)
   {
      pStep->finalize(Message::Failure, "Unable to set output argument.");
      return false;
   }

   pStep->finalize();
   return true;
}

bool ElmBatch::extractInputArgs(PlugInArgList* pInputArgList)
{
   if (ElmCore::extractInputArgs(pInputArgList) == false)
   {
      return false;
   }

   StepResource pStep("Extract Batch Input Args", "app", "32A136BE-8531-42ca-8B22-086293B5A925");
   VERIFY(pStep.get() != NULL);

   // Get the Use Gains/Offsets Flag.
   if (pInputArgList->getPlugInArgValue<bool>(UseGainsOffsetsArg(), mUseGainsOffsets) == false)
   {
      pStep->finalize(Message::Failure, "The \"" + UseGainsOffsetsArg() + "\" input arg is invalid.");
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
      }

      return false;
   }

   if (mUseGainsOffsets == true)
   {
      // If the Use Gains/Offsets Flag is set to true, get the Gains/Offsets Filename.
      Filename* pFilename = pInputArgList->getPlugInArgValue<Filename>(GainsOffsetsFilenameArg());
      if (pFilename == NULL || pFilename->getFullPathAndName().empty() == true)
      {
         // If the Gains/Offsets Filename is not set, use the default.
         mGainsOffsetsFilename = getDefaultGainsOffsetsFilename();
      }
      else if (pFilename->isDirectory() == true)
      {
         pStep->finalize(Message::Failure, "The \"" + GainsOffsetsFilenameArg() + "\" cannot be a directory.");
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
         }

         return false;
      }
      else
      {
         mGainsOffsetsFilename = pFilename->getFullPathAndName();
      }
   }
   else
   {
      // If the Use Gains/Offsets Flag is set to false, get the Signature Filenames and AOI Filenames.
      vector<Filename*> pSignatureFilenames;
      if (pInputArgList->getPlugInArgValue<vector<Filename*> >
         (SignatureFilenamesArg(), pSignatureFilenames) == false)
      {
         pStep->finalize(Message::Failure, "The \"" + SignatureFilenamesArg() + "\" input arg is invalid.");
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
         }

         return false;
      }

      for (vector<Filename*>::iterator iter = pSignatureFilenames.begin(); iter != pSignatureFilenames.end(); ++iter)
      {
         bool previouslyLoaded;
         Signature* pSignature = dynamic_cast<Signature*>
            (getElement(*iter, "Signature", NULL, previouslyLoaded));
         if (pSignature == NULL)
         {
            pStep->finalize(Message::Failure, "The \"" + SignatureFilenamesArg() +
               "\" input arg contains an invalid value.");
            if (mpProgress != NULL)
            {
               mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
            }

            return false;
         }

         mpSignatures.push_back(pSignature);
         if (previouslyLoaded == false)
         {
            mpSignaturesToDestroy.push_back(pSignature);
         }
      }

      // Get the AOI names.
      vector<Filename*> pAoiFilenames;
      if (pInputArgList->getPlugInArgValue<vector<Filename*> >(AoiFilenamesArg(), pAoiFilenames) == false)
      {
         pStep->finalize(Message::Failure, "The \"" + AoiFilenamesArg() + "\" input arg is invalid.");
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
         }

         return false;
      }

      for (vector<Filename*>::iterator iter = pAoiFilenames.begin(); iter != pAoiFilenames.end(); ++iter)
      {
         bool previouslyLoaded;
         AoiElement* pAoiElement = dynamic_cast<AoiElement*>
            (getElement(*iter, "AoiElement", mpRasterElement, previouslyLoaded));
         if (pAoiElement == NULL)
         {
            pStep->finalize(Message::Failure, "The \"" + AoiFilenamesArg() +
               "\" input arg contains an invalid value.");
            if (mpProgress != NULL)
            {
               mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
            }

            return false;
         }

         mpAoiElements.push_back(pAoiElement);
         if (previouslyLoaded == false)
         {
            mpAoiElementsToDestroy.push_back(pAoiElement);
         }
      }
   }

   pStep->finalize();
   return true;
}

DataElement* ElmBatch::getElement(const Filename* pFilename,
   const string& type, DataElement* pParent, bool& previouslyLoaded)
{
   DataElement* pDataElement = NULL;
   if (pFilename != NULL)
   {
      Service<ModelServices> pModel;
      const string& filename = pFilename->getFullPathAndName();
      pDataElement = pModel->getElement(filename, type, pParent);
      if (pDataElement != NULL)
      {
         previouslyLoaded = true;
      }
      else
      {
         previouslyLoaded = false;

         const string importerName = "Auto Importer";
         ImporterResource importer(importerName, filename, mpProgress);
         if (importer->getPlugIn() != NULL && importer->execute() == true)
         {
            const vector<DataElement*>& pImportedElements = importer->getImportedElements();
            if (pImportedElements.empty() == false)
            {
               pDataElement = pImportedElements.front();
            }
         }
      }
   }

   return pDataElement;
}

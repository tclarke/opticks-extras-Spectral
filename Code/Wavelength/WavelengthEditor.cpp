/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "DesktopServices.h"
#include "DynamicObject.h"
#include "MessageLogResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "Progress.h"
#include "SpectralVersion.h"
#include "WavelengthEditor.h"
#include "WavelengthEditorDlg.h"
#include "Wavelengths.h"

#include <string>
using namespace std;

REGISTER_PLUGIN_BASIC(SpectralWavelength, WavelengthEditor);

WavelengthEditor::WavelengthEditor()
{
   setName("Wavelength Editor");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setType(Wavelengths::WavelengthType());
   setDescription("Allows editing of wavelength files");
   setDescriptorId("{2115FE27-94EF-4DC1-A513-3FD417872328}");
   setMenuLocation("[Spectral]\\Support Tools\\Wavelength Editor...");
   allowMultipleInstances(true);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

WavelengthEditor::~WavelengthEditor()
{
}

bool WavelengthEditor::getInputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   if (isBatch() == true)
   {
      Service<PlugInManagerServices> pManager;
      pArgList = pManager->getPlugInArgList();
      VERIFY(pArgList != NULL);

      bool bApplyScale = false;
      double dScaleFactor = 1.0;
      bool bCalculateFwhm = false;
      double dFwhmConstant = 1.0;

      VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
      VERIFY(pArgList->addArg<DynamicObject>("Original Wavelengths"));
      VERIFY(pArgList->addArg<bool>("Apply Scale", &bApplyScale));
      VERIFY(pArgList->addArg<double>("Scale Factor", &dScaleFactor));
      VERIFY(pArgList->addArg<bool>("Calculate FWHM", &bCalculateFwhm));
      VERIFY(pArgList->addArg<double>("FWHM Constant", &dFwhmConstant));
   }

   return true;
}

bool WavelengthEditor::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   if (isBatch() == true)
   {
      Service<PlugInManagerServices> pManager;
      pArgList = pManager->getPlugInArgList();
      VERIFY(pArgList != NULL);

      VERIFY(pArgList->addArg<DynamicObject>("Edited Wavelengths"));
   }

   return true;
}

bool WavelengthEditor::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (isBatch() == false)
   {
      // Display the wavelength editor dialog
      Service<DesktopServices> pDesktop;

      WavelengthEditorDlg dialog(pDesktop->getMainWidget());
      if (dialog.exec() == QDialog::Accepted)
      {
         return true;
      }
   }
   else if (pInArgList != NULL)
   {
      StepResource pStep(string("Execute ") + getName(), "Spectral", "8C08A698-AF45-4C13-A2EA-16DB7CE3B369");

      // Extract the input args
      Progress* pProgress = pInArgList->getPlugInArgValue<Progress>(ProgressArg());

      const DynamicObject* pWavelengthData = pInArgList->getPlugInArgValue<DynamicObject>("Original Wavelengths");
      if (pWavelengthData == NULL)
      {
         string message = "The Original Wavelengths input value is invalid.";
         if (pProgress != NULL)
         {
            pProgress->updateProgress(message, 0, ERRORS);
         }

         pStep->finalize(Message::Failure, message);
         return false;
      }

      bool bApplyScale = false;
      double dScaleFactor = 1.0;
      bool bCalculateFwhm = false;
      double dFwhmConstant = 1.0;

      pInArgList->getPlugInArgValue<bool>("Apply Scale", bApplyScale);
      pInArgList->getPlugInArgValue<double>("Scale Factor", dScaleFactor);
      pInArgList->getPlugInArgValue<bool>("Calculate FWHM", bCalculateFwhm);
      pInArgList->getPlugInArgValue<double>("FWHM Constant", dFwhmConstant);

      // Create the edited wavelengths
      FactoryResource<DynamicObject> pEditedWavelengthData;

      Wavelengths editedWavelengths(pEditedWavelengthData.get());
      if (editedWavelengths.initializeFromDynamicObject(pWavelengthData) == false)
      {
         string message = "Could not create the edited wavelengths.";
         if (pProgress != NULL)
         {
            pProgress->updateProgress(message, 0, ERRORS);
         }

         pStep->finalize(Message::Failure, message);
         return false;
      }

      // Edit the wavelength values
      if (pProgress != NULL)
      {
         pProgress->updateProgress("Editing wavelengths...", 0, NORMAL);
      }

      if (bApplyScale == true)
      {
         editedWavelengths.scaleValues(dScaleFactor);
      }

      if (bCalculateFwhm == true)
      {
         editedWavelengths.calculateFwhm(dFwhmConstant);
      }

      if (pProgress != NULL)
      {
         pProgress->updateProgress("Editing wavelengths completed successfully.", 100, NORMAL);
      }

      // Populate the output arg list
      if (pOutArgList != NULL)
      {
         pOutArgList->setPlugInArgValue<DynamicObject>("Edited Wavelengths", pEditedWavelengthData.get());
      }

      pStep->finalize(Message::Success);
      return true;
   }

   return false;
}

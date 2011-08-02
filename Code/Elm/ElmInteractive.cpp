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
#include "ElmDlg.h"
#include "ElmInteractive.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "Progress.h"
#include "SpatialDataView.h"
#include "SpectralVersion.h"

using namespace std;

REGISTER_PLUGIN_BASIC(SpectralElm, ElmInteractive);

ElmInteractive::ElmInteractive() :
   mpView(NULL),
   mpDialog(NULL),
   mpStep(NULL)
{
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setName("ELM Interactive");
   setType(PlugInManagerServices::AlgorithmType());
   setDescription("ELM Interactive");
   setShortDescription("ELM Interactive");
   setDescriptorId("{5760F07A-FFCA-47bf-907C-4DBBEB7BD969}");
   setMenuLocation("[Spectral]\\Preprocessing\\ELM");
   setWizardSupported(false);
}

ElmInteractive::~ElmInteractive()
{}

bool ElmInteractive::setBatch()
{
   ViewerShell::setBatch();
   return false;
}

bool ElmInteractive::getInputSpecification(PlugInArgList*& pArgList)
{
   if (isBatch() == true)
   {
      return false;
   }

   if (ElmCore::getInputSpecification(pArgList) == false)
   {
      return false;
   }

   // Interactive mode: View
   VERIFY(pArgList->addArg<SpatialDataView>(Executable::ViewArg(), NULL, "View containing the primary raster element "
      "on which ELM will be performed."));
   return true;
}

bool ElmInteractive::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return !isBatch();
}

Step* ElmInteractive::getLogStep()
{
   return mpStep.get();
}

bool ElmInteractive::extractInputArgs(PlugInArgList* pInputArgList)
{
   if (isBatch() == true)
   {
      return false;
   }

   if (ElmCore::extractInputArgs(pInputArgList) == false)
   {
      return false;
   }

   StepResource pStep("Extract Interactive Input Args", "app", "DE529F43-D255-47a5-AE38-2B3E91443446");
   VERIFY(pStep.get() != NULL);

   // Get the View.
   mpView = pInputArgList->getPlugInArgValue<SpatialDataView>(Executable::ViewArg());
   if (mpView == NULL)
   {
      pStep->finalize(Message::Failure, "The \"" + Executable::ViewArg() + "\" input arg is invalid.");
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(pStep->getFailureMessage(), 100, ERRORS);
      }

      return false;
   }

   pStep->finalize();
   return true;
}

bool ElmInteractive::execute(PlugInArgList* pInputArgList, PlugInArgList* pOutputArgList)
{
   mpStep = StepResource("Execute " + getName(), "app", "BE15A9D5-A085-43de-B980-781063270033");
   VERIFY(mpStep.get() != NULL);

   if (extractInputArgs(pInputArgList) == false)
   {
      mpStep->finalize(Message::Failure, "extractInputArgs() returned false");
      return false;
   }

   // Invoke the dialog as a modeless dialog
   if (mpDialog == NULL)
   {
      Service<DesktopServices> pDesktopServices;
      mpDialog = new ElmDlg(mpView, this, pDesktopServices->getMainWidget());
   }

   mpDialog->show();
   return true;
}

QWidget* ElmInteractive::getWidget() const
{
   return mpDialog;
}

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
#include "LibraryBuilder.h"
#include "MessageLogResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "Progress.h"
#include "SpectralLibraryDlg.h"
#include "SpectralVersion.h"

#include <string>
using namespace std;

REGISTER_PLUGIN_BASIC(SpectralSpectralLibrary, LibraryBuilder);

LibraryBuilder::LibraryBuilder() :
   mpLibraryDlg(NULL)
{
   setName("Spectral Library Builder");
   setCreator("Ball Aerospace & Technologies, Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setType("Viewer");
   setDescription("Tool for creating and editing spectral libraries.");
   setDescriptorId("{75910853-A516-44FA-BEDA-595F14E9A496}");
   setMenuLocation("[Spectral]\\Support Tools\\Spectral Library Builder");
   allowMultipleInstances(false);
   executeOnStartup(false);
   destroyAfterExecute(true);
   setAbortSupported(true);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

LibraryBuilder::~LibraryBuilder()
{
}

bool LibraryBuilder::setBatch()
{
   ExecutableShell::setBatch();
   return false;
}

bool LibraryBuilder::getInputSpecification(PlugInArgList*& pArgList)
{
   if (isBatch() == true)
   {
      return false;
   }

   Service<PlugInManagerServices> pManager;
   pArgList = pManager->getPlugInArgList();
   VERIFY(pArgList != NULL);

   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   return true;
}

bool LibraryBuilder::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return (isBatch() == false);
}

bool LibraryBuilder::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (isBatch() == true)
   {
      return false;
   }

   StepResource pStep("Execute the Spectral Library Builder", "spectral", "3FCC8624-ED34-41BF-892E-F75FA3B8E7D5");

   // Extract the progress input arg
   Progress* pProgress = NULL;
   if (pInArgList != NULL)
   {
      pProgress = pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg());
   }

   // Create the library dialog
   if (mpLibraryDlg == NULL)
   {
      Service<DesktopServices> pDesktop;
      mpLibraryDlg = new SpectralLibraryDlg(pProgress, pStep.get(), pDesktop->getMainWidget());
   }

   if (mpLibraryDlg == NULL)
   {
      string message = "Could not create the Spectral Library dialog.";
      if (pProgress != NULL)
      {
         pProgress->updateProgress(message, 0, ERRORS);
      }

      pStep->finalize(Message::Failure, message);
      return false;
   }

   // Invoke the dialog
   int iReturn = mpLibraryDlg->exec();

   // Destroy the dialog
   if (mpLibraryDlg != NULL)
   {
      delete mpLibraryDlg;
      mpLibraryDlg = NULL;
   }

   if (iReturn == QDialog::Rejected)
   {
      pStep->finalize(Message::Abort);
      return false;
   }

   pStep->finalize(Message::Success);
   return true;
}

bool LibraryBuilder::abort()
{
   if (mpLibraryDlg != NULL)
   {
      mpLibraryDlg->abortSearch();
      return true;
   }

   return false;
}

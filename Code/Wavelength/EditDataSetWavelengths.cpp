/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtGui/QAction>

#include "ContextMenu.h"
#include "ContextMenuActions.h"
#include "DesktopServices.h"
#include "EditDataSetWavelengths.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "RasterElement.h"
#include "SpectralVersion.h"
#include "WavelengthEditorDlg.h"

#include <boost/any.hpp>

#include <vector>
using namespace std;

#define SPECTRAL_EDITDATASETWAVELENGTHS_SEPARATOR_ACTION "SPECTRAL_EDITDATASETWAVELENGTHS_SEPARATOR_ACTION"
#define SPECTRAL_EDITDATASETWAVELENGTHS_WAVELENGTHS_ACTION "SPECTRAL_EDITDATASETWAVELENGTHS_WAVELENGTHS_ACTION"

REGISTER_PLUGIN_BASIC(SpectralWavelength, EditDataSetWavelengths);

EditDataSetWavelengths::EditDataSetWavelengths() :
   mpExplorer(SIGNAL_NAME(SessionExplorer, AboutToShowSessionItemContextMenu),
      Slot(this, &EditDataSetWavelengths::updateContextMenu))
{
   ExecutableShell::setName("Edit Data Set Wavelengths");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setType(Wavelengths::WavelengthType());
   setDescription("Allow editing of wavelengths in an existing data set");
   setDescriptorId("{0228CDDB-D3AC-43A8-A53F-9FDA9D6CEC7B}");
   executeOnStartup(true);
   destroyAfterExecute(false);
   allowMultipleInstances(false);
   setWizardSupported(false);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

EditDataSetWavelengths::~EditDataSetWavelengths()
{
}

bool EditDataSetWavelengths::setBatch()
{
   ExecutableShell::setBatch();
   return false;
}

bool EditDataSetWavelengths::getInputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return (isBatch() == false);
}

bool EditDataSetWavelengths::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return (isBatch() == false);
}

bool EditDataSetWavelengths::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (isBatch() == true)
   {
      return false;
   }

   Service<SessionExplorer> pExplorer;
   mpExplorer.reset(pExplorer.get());

   return true;
}

void EditDataSetWavelengths::updateContextMenu(Subject& subject, const string& signal, const boost::any& value)
{
   ContextMenu* pMenu = boost::any_cast<ContextMenu*>(value);
   if (pMenu == NULL)
   {
      return;
   }

   if ((pMenu->getSessionItems().size() != 1) || (pMenu->getSessionItems<RasterElement>().size() != 1))
   {
      return;
   }

   // Separator
   QAction* pSeparatorAction = new QAction(pMenu->getActionParent());
   pSeparatorAction->setSeparator(true);
   pMenu->addActionBefore(pSeparatorAction, SPECTRAL_EDITDATASETWAVELENGTHS_SEPARATOR_ACTION,
      APP_APPLICATIONWINDOW_DATAELEMENT_DELETE_ACTION);

   // Wavelengths
   QAction* pWavelengthsAction = new QAction("Wavelengths...", pMenu->getActionParent());
   pWavelengthsAction->setAutoRepeat(false);
   VERIFYNR(connect(pWavelengthsAction, SIGNAL(triggered()), this, SLOT(editWavelengths())));
   pMenu->addActionBefore(pWavelengthsAction, SPECTRAL_EDITDATASETWAVELENGTHS_WAVELENGTHS_ACTION,
      SPECTRAL_EDITDATASETWAVELENGTHS_SEPARATOR_ACTION);
}

void EditDataSetWavelengths::editWavelengths()
{
   // Get the selected data set
   Service<SessionExplorer> pExplorer;

   vector<RasterElement*> datasets = pExplorer->getSelectedSessionItems<RasterElement>();
   if (datasets.size() != 1)
   {
      return;
   }

   RasterElement* pDataset = datasets.front();
   VERIFYNR(pDataset != NULL);

   // Invoke the wavelength editor dialog
   Service<DesktopServices> pDesktop;

   WavelengthEditorDlg dialog(pDataset, pDesktop->getMainWidget());
   dialog.exec();
}

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "ApplicationServices.h"
#include "AppVerify.h"
#include "DesktopServices.h"
#include "Ndvi.h"
#include "NdviDlg.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "ProgressTracker.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "SpectralVersion.h"
#include "StringUtilities.h"
#include "Wavelengths.h"
#include <vector>

REGISTER_PLUGIN_BASIC(NdviModule, Ndvi);

namespace
{
   // Wavelength range definitions for red and NIR in micrometers
   const double redBandLow = 0.630;
   const double redBandHigh = 0.690;
   const double nirBandLow = 0.760;
   const double nirBandHigh = 1.000;
}

Ndvi::Ndvi() :
   mbDisplayResults(Service<ApplicationServices>()->isInteractive()),
   mbOverlayResults(false)
{
   setName("NDVI");
   setDescriptorId("{c7b85850-874a-4a22-ae1d-53cfbe5511b4}");
   setDescription("Calculate NDVI using wavelength information to determine which bands to process.");
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setMenuLocation("[Spectral]\\Transforms\\NDVI");
   setWizardSupported(true);
}

Ndvi::~Ndvi()
{}

bool Ndvi::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pInArgList->addArg<Progress>(Executable::ProgressArg(), NULL, Executable::ProgressArgDescription()));
   VERIFY(pInArgList->addArg<RasterElement>(Executable::DataElementArg(), NULL, "Raster element on which NDVI will be "
      "performed."));

   if (isBatch())
   {
      VERIFY(pInArgList->addArg<unsigned int>("Red Band Number", "Optional argument: Band number of red band. "
         "If no band is specified, will attempt wavelength match to find red band."));
      VERIFY(pInArgList->addArg<unsigned int>("NIR Band Number", "Optional argument: Band number of NIR band. "
         "If no band is specified, will attempt wavelength match to find NIR band."));
      VERIFY(pInArgList->addArg<bool>("Display Results", mbDisplayResults, "Optional Argument: Whether or not "
         "to display the result of the NDVI operation. Default is true in interactive application mode, false "
         "in batch application mode."));
      VERIFY(pInArgList->addArg<bool>("Overlay Results", mbOverlayResults, "Optional Argument: Flag for whether "
         "the results should be added to the original view or a new view.  A new view is created "
         "by default if results are displayed."));
   }
   return true;
}

bool Ndvi::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   VERIFY(pOutArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pOutArgList->addArg<RasterElement>("NDVI Result", NULL, "Raster element resulting from the NDVI operation."));
   return true;
}

bool Ndvi::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   VERIFY(pInArgList);
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg()),
      "Calculating NDVI", "spectral", "{500ae505-9080-4e24-8b56-beeab06787a5}");
   RasterElement* pElement = pInArgList->getPlugInArgValue<RasterElement>(Executable::DataElementArg());
   if (pElement == NULL)
   {
      progress.report("No RasterElement specified.", 0, ERRORS, true);
      return false;
   }

   RasterDataDescriptor* pDesc = static_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());

   FactoryResource<Wavelengths> pWavelengthResource;
   pWavelengthResource->initializeFromDynamicObject(pDesc->getMetadata(), true);

   if (pWavelengthResource->isEmpty())
   {
      progress.report("No wavelength data available for processing.", 0, ERRORS, true);
      return false;
   }

   Service<DesktopServices> pDesktopServices;

   // Filter wavelength data and select appropriate red band.
   DimensionDescriptor redBandDD = RasterUtilities::findBandWavelengthMatch(redBandLow, redBandHigh, pDesc);
   // Filter wavelength data and select appropriate NIR band
   DimensionDescriptor nirBandDD = RasterUtilities::findBandWavelengthMatch(nirBandLow, nirBandHigh, pDesc);
   if (!isBatch())
   {
      NdviDlg bandDlg(pDesc, redBandLow, redBandHigh, nirBandLow, nirBandHigh,
         redBandDD, nirBandDD, pDesktopServices->getMainWidget());
      if (bandDlg.exec() == QDialog::Rejected)
      {
         return false;
      }
      //Note: Dialog returns zero based active band index
      redBandDD = pDesc->getActiveBand(bandDlg.getRedBand());
      nirBandDD = pDesc->getActiveBand(bandDlg.getNirBand());
      mbOverlayResults = bandDlg.getOverlay();
   }
   else
   {
      //If values were provided in input arguments, they are ORIGINAL band numbers.
      //Translate to active band numbers and get dimension descriptor for requested band.
      //Note: Convert to zero based, user entered 1 based band index!! If zero is entered
      //for a bandNumber, this becomes int_max, which will be an invalid band number anyways,
      //so don't worry about decrementing zero in unsigned!
      unsigned int redBandNumber;
      if (pInArgList->getPlugInArgValue<unsigned int>("Red Band Number", redBandNumber))
      {
         redBandDD = pDesc->getOriginalBand(redBandNumber - 1);
         if (!redBandDD.isValid())
         {
            progress.report("Specified red band not available.", 0, ERRORS, true);
            return false;
         }
      }
      else if (!redBandDD.isValid())
      {
         progress.report("No bands fall in the red wavelength range.", 0, ERRORS, true);
         return false;
      }

      unsigned int nirBandNumber;
      if (pInArgList->getPlugInArgValue<unsigned int>("NIR Band Number", nirBandNumber))
      {
         nirBandDD = pDesc->getOriginalBand(nirBandNumber - 1);
         if (!nirBandDD.isValid())
         {
            progress.report("Specified NIR band not available.", 0, ERRORS, true);
            return false;
         }
      }
      else if (!nirBandDD.isValid())
      {
         progress.report("No bands fall in the NIR wavelength range.", 0, ERRORS, true);
         return false;
      }
      VERIFY(pInArgList->getPlugInArgValue<bool>("Display Results", mbDisplayResults));
      VERIFY(pInArgList->getPlugInArgValue<bool>("Overlay Results", mbOverlayResults));
   }

   // Execute Band Math with the appropriate expression
   progress.report("Executing NDVI calculation", 15, NORMAL);
   //Note: Band math wants 1 based band index, so add 1 to active band number from dimension descriptor
   std::string redBand =
      "b" + StringUtilities::toDisplayString(redBandDD.getActiveNumber()+1);
   std::string nirBand =
      "b" + StringUtilities::toDisplayString(nirBandDD.getActiveNumber()+1);
   std::string expression = "(" + nirBand + "-" + redBand + ")/(" + nirBand + "+" + redBand + ")";
   ExecutableResource bandMath("Band Math", std::string(), progress.getCurrentProgress());
   bool success = bandMath->getInArgList().setPlugInArgValue(Executable::DataElementArg(), pElement);
   success &= bandMath->getInArgList().setPlugInArgValue("Display Results", &mbDisplayResults);
   success &= bandMath->getInArgList().setPlugInArgValue("Input Expression", &expression);
   success &= bandMath->getInArgList().setPlugInArgValue("Overlay Results", &mbOverlayResults);
   success &= bandMath->execute();
   if (!success)
   {
      // Error already reported by Band Math
      return false;
   }
   if (pOutArgList != NULL)
   {
      RasterElement* pResult = bandMath->getOutArgList().getPlugInArgValue<RasterElement>("Band Math Result");
      pOutArgList->setPlugInArgValue("NDVI Result", pResult);
   }

   progress.report("NDVI Calculation Complete", 100, NORMAL);
   progress.upALevel();
   return true;
}

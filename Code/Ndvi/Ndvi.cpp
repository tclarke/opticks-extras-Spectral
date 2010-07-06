/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppConfig.h"
#include "AppVerify.h"
#include "Ndvi.h"
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

Ndvi::Ndvi()
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
{
}

bool Ndvi::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pInArgList->addArg<Progress>(Executable::ProgressArg()));
   VERIFY(pInArgList->addArg<RasterElement>(Executable::DataElementArg()));
   return true;
}

bool Ndvi::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   VERIFY(pOutArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pOutArgList->addArg<RasterElement>("NDVI Result"));
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

   progress.report("Determining which bands to use for NDVI calculation", 10, NORMAL);
   RasterDataDescriptor* pDesc = static_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());

   // Filter wavelength data and select appropriate red band.
   DimensionDescriptor redBandDD = RasterUtilities::findBandWavelengthMatch(redBandLow, redBandHigh, pDesc);
   if (!redBandDD.isValid())
   {
      progress.report("No bands fall in the red wavelength range.", 0, ERRORS, true);
      return false;
   }

   // Filter wavelength data and select appropriate NIR band
   DimensionDescriptor nirBandDD = RasterUtilities::findBandWavelengthMatch(nirBandLow, nirBandHigh, pDesc);
   if (!nirBandDD.isValid())
   {
      progress.report("No bands fall in the NIR wavelength range.", 0, ERRORS, true);
      return false;
   }

   // Execute Band Math with the appropriate expression
   progress.report("Executing NDVI calculation", 15, NORMAL);
   bool displayResults = !isBatch();
   std::string redBand =
      "b" + StringUtilities::toDisplayString(redBandDD.getActiveNumber()+1); // change to 1-based index
   std::string nirBand =
      "b" + StringUtilities::toDisplayString(nirBandDD.getActiveNumber()+1); // change to 1-based index
   std::string expression = "(" + nirBand + "-" + redBand + ")/(" + nirBand + "+" + redBand + ")";
   ExecutableResource bandMath("Band Math", std::string(), progress.getCurrentProgress());
   bool success = bandMath->getInArgList().setPlugInArgValue(Executable::DataElementArg(), pElement);
   success &= bandMath->getInArgList().setPlugInArgValue("Display Results", &displayResults);
   success &= bandMath->getInArgList().setPlugInArgValue("Input Expression", &expression);
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

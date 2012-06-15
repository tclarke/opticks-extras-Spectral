/*
 * The information in this file is
 * Copyright(c) 2012 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "CommonSignatureMetadataKeys.h"
#include "DataVariant.h"
#include "DesktopServices.h"
#include "Filename.h"
#include "LayerList.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "ProgressTracker.h"
#include "RasterElement.h"
#include "Resampler.h"
#include "ResamplerOptions.h"
#include "ResamplerPlugIn.h"
#include "ResamplerPlugInDlg.h"
#include "Signature.h"
#include "SignatureDataDescriptor.h"
#include "SignatureSet.h"
#include "SpatialDataView.h"
#include "SpectralVersion.h"
#include "StringUtilities.h"
#include "Units.h"
#include "Wavelengths.h"

#include <memory>
#include <vector>

#include <QtCore/QFile>

REGISTER_PLUGIN_BASIC(SpectralResampler, ResamplerPlugIn);

ResamplerPlugIn::ResamplerPlugIn()
{
   setName("Spectral Resampler");
   setDescriptorId("{D20D4C10-B9B8-4ADB-85FA-105446430966}");
   setSubtype("Algorithm");
   setShortDescription("Run Spectral Resampler");
   setDescription("Resample spectral signatures to a set of wavelengths.");
   setMenuLocation("[Spectral]/Support Tools/Spectral Resampler");
   setAbortSupported(true);
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setWizardSupported(true);
}

ResamplerPlugIn::~ResamplerPlugIn()
{}

bool ResamplerPlugIn::getInputSpecification(PlugInArgList*& pArgList)
{
   pArgList = Service<PlugInManagerServices>()->getPlugInArgList();
   VERIFY(pArgList != NULL);
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL, Executable::ProgressArgDescription()));
   VERIFY(pArgList->addArg<SpatialDataView>(Executable::ViewArg(), NULL,
      "If the current active view is a spatial data\n"
      "view, the wavelengths from the primary raster\n"
      "element of this view will be used as the default\n"
      "set of wavelengths for the resampling."));

   if (isBatch())
   {
      VERIFY(pArgList->addArg<std::vector<Signature*> >("Signatures to resample", NULL,
         "The signatures to be resampled"));
      VERIFY(pArgList->addArg<Signature>("Signature to resample", NULL,
         "The signature to be resampled. If arg \"Signatures to resample\" is provided, this arg will be ignored."));
      VERIFY(pArgList->addArg<DataElement>("Data element wavelength source", NULL,
         "The signatures will be resampled to the wavelengths from this data element."));
      VERIFY(pArgList->addArg<Filename>("Wavelengths Filename", NULL,
         "The signatures will be resampled to the wavelengths from this wavelengths file.\n This arg will "
         "be ignored if arg \"Data element wavelength source\" is provided"));
      VERIFY(pArgList->addArg<std::string>("Resampling Method", NULL,
         "The name of the resampling method. The accepted values are \n"
         + ResamplerOptions::LinearMethod() + ", " + ResamplerOptions::CubicSplineMethod() + " and "
         + ResamplerOptions::GaussianMethod() + ".\n"));
      VERIFY(pArgList->addArg<double>("Drop out window", NULL,
         "The drop out window to use during resampling.\n"));
      VERIFY(pArgList->addArg<double>("FWHM", NULL,
         "The full width half max to use during gaussian resampling.\n"
         "This arg is ignored for other methods.\n"));
      VERIFY(pArgList->addArg<bool>("Use fill value", NULL,
         "If true, resampled signatures will have values for all the target wavelengths regardless \n"
         "of whether or not the input signatures have spectral coverage for all the wavelengths.\n"
         "Any wavelengths that would normally not be in the resampled signature will be assigned the fill value."));
      VERIFY(pArgList->addArg<double>("Fill value", NULL,
         "The value to be assigned to wavelengths in the resampled signature for which the input signature\n"
         "does not have spectral coverage.\n"
         "This arg is ignored if arg \"Use fill value\" is false."));
   }

   return true;
}

bool ResamplerPlugIn::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = Service<PlugInManagerServices>()->getPlugInArgList();
   VERIFY(pArgList != NULL);
   VERIFY(pArgList->addArg<std::vector<Signature*> >("Resampled signatures", NULL,
      "The resampled signatures"));

   return true;
}

bool ResamplerPlugIn::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   VERIFY(pInArgList != NULL && pOutArgList != NULL);
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg()),
      "Executing Spectral Resampler.", "spectral", "{88CD3E49-A522-431A-AE2A-96A6B2EB4012}");

   Service<DesktopServices> pDesktop;

   const DataElement* pElement(NULL);
   std::string waveFilename;

   // get default resampling options from user config
   std::string resampleMethod = ResamplerOptions::getSettingResamplerMethod();
   double dropOutWindow = ResamplerOptions::getSettingDropOutWindow();
   double fwhm = ResamplerOptions::getSettingFullWidthHalfMax();
   bool useFillValue = ResamplerOptions::getSettingUseFillValue();
   double fillValue = ResamplerOptions::getSettingSignatureFillValue();

   std::vector<Signature*> originalSignatures;
   std::auto_ptr<std::vector<Signature*> > pResampledSignatures(new std::vector<Signature*>);
   std::string errorMsg;

   if (isBatch())
   {
      VERIFY(pInArgList->getPlugInArgValue("Signatures to resample", originalSignatures));
      if (originalSignatures.empty())
      {
         Signature* pSignature = pInArgList->getPlugInArgValue<Signature>("Signature to resample");
         if (pSignature != NULL)
         {
            originalSignatures.push_back(pSignature);
         }
      }
      if (originalSignatures.empty())
      {
         progress.report("No signatures are available to be resampled.", 0, ERRORS, true);
         return false;
      }
      pElement = pInArgList->getPlugInArgValue<DataElement>("Data element wavelength source");
      Filename* pWaveFilename = pInArgList->getPlugInArgValue<Filename>("Wavelengths Filename");
      if (pWaveFilename != NULL)
      {
         waveFilename = pWaveFilename->getFullPathAndName();
      }
      VERIFY(pInArgList->getPlugInArgValue("Resampling Method", resampleMethod));
      VERIFY(pInArgList->getPlugInArgValue("Drop out window", dropOutWindow));
      VERIFY(pInArgList->getPlugInArgValue("FWHM", fwhm));
      VERIFY(pInArgList->getPlugInArgValue("Use fill value", useFillValue));
      VERIFY(pInArgList->getPlugInArgValue("Fill value", fillValue));
   }
   else
   {
      ResamplerPlugInDlg dlg(pDesktop->getMainWidget());
      if (dlg.exec() == QDialog::Rejected)
      {
         progress.report("User canceled resampling.", 0, ABORT, true);
         progress.upALevel();
         return false;
      }
      originalSignatures = dlg.getSignaturesToResample();
      resampleMethod = dlg.getResamplingMethod();
      dropOutWindow = dlg.getDropOutWindow();
      fwhm = dlg.getFWHM();
      useFillValue = dlg.getUseFillValue();
      fillValue = dlg.getFillValue();
      pElement = dlg.getWavelengthsElement();
      waveFilename = dlg.getWavelengthsFilename();
   }

   std::string resampledTo;
   FactoryResource<Wavelengths> pWavelengths;
   if (pElement != NULL)  // try loading wavelengths from user specified data element
   {
      if (getWavelengthsFromElement(pElement, pWavelengths.get(), errorMsg) == false)
      {
         progress.report(errorMsg, 0, ERRORS, true);
         return false;
      }
      resampledTo = pElement->getName();
   }
   else if (waveFilename.empty() == false)  // if no user provided raster, look for a wavelengths file
   {
      if (QFile::exists(QString::fromStdString(waveFilename)))
      {
         if (getWavelengthsFromFile(waveFilename, pWavelengths.get(), errorMsg) == false)
         {
            progress.report(errorMsg, 0, ERRORS, true);
            return false;
         }
      }
      else
      {
         errorMsg = "The wavelengths file \"" + waveFilename + "\" could not be found.";
         progress.report(errorMsg, 0, ERRORS, true);
         return false;
      }
      resampledTo = waveFilename;
   }
   else  // if no wavelength source provided, look for raster in current active spatial data view
   {
      SpatialDataView* pView = dynamic_cast<SpatialDataView*>(pDesktop->getCurrentWorkspaceWindowView());
      if (pView != NULL)
      {
         LayerList* pLayerList = pView->getLayerList();
         if (pLayerList != NULL)
         {
            pElement = pLayerList->getPrimaryRasterElement();
            pWavelengths->initializeFromDynamicObject(pElement->getMetadata(), false);
            if (pWavelengths->isEmpty())
            {
               progress.report("No target wavelengths are available for resampling the signatures.", 0, ERRORS, true);
               return false;
            }
            resampledTo = pElement->getName();
         }
      }
   }

   PlugInResource pPlugIn("Resampler");
   Resampler* pResampler = dynamic_cast<Resampler*>(pPlugIn.get());
   if (pResampler == NULL)
   {
      progress.report("The \"Resampler\" plug-in is not available so the signatures can not be resampled.",
         0, ERRORS, true);
      return false;
   }
   std::string dataName("Reflectance");
   std::string wavelengthName("Wavelength");

   // save user config settings - Resampler doesn't have interface to set them separately from user config
   std::string configMethod = ResamplerOptions::getSettingResamplerMethod();
   ResamplerOptions::setSettingResamplerMethod(resampleMethod);
   double configDropout = ResamplerOptions::getSettingDropOutWindow();
   ResamplerOptions::setSettingDropOutWindow(dropOutWindow);
   double configFwhm = ResamplerOptions::getSettingFullWidthHalfMax();
   ResamplerOptions::setSettingFullWidthHalfMax(fwhm);

   std::vector<double> toWavelengths = pWavelengths->getCenterValues();
   std::vector<double> toFwhm = pWavelengths->getFwhm();
   if (toFwhm.size() != toWavelengths.size())
   {
      toFwhm.clear();  // Resampler will use the default config setting fwhm if this vector is empty
   }

   unsigned int numSigs = originalSignatures.size();
   unsigned int numSigsResampled(0);
   progress.report("Begin resampling signatures...", 0, NORMAL);
   for (unsigned int index = 0; index < numSigs; ++index)
   {
      if (isAborted())
      {
         progress.report("Resampling aborted by user", 100 * index / numSigs, ABORT, true);
         return false;
      }
      if (originalSignatures[index] == NULL)
      {
         continue;
      }

      // check if signature has target wavelength centers and doesn't need to be resampled
      if (needToResample(originalSignatures[index], pWavelengths.get()) == false)
      {
         pResampledSignatures->push_back(originalSignatures[index]);
         continue;
      }

      DataVariant var = originalSignatures[index]->getData(dataName);
      if (var.isValid() == false)
      {
         continue;
      }
      std::vector<double> fromData;
      if (!var.getValue(fromData))
      {
         continue;
      }
      var = originalSignatures[index]->getData(wavelengthName);
      if (var.isValid() == false)
      {
         continue;
      }
      std::vector<double> fromWavelengths;
      if (!var.getValue(fromWavelengths))
      {
         continue;
      }
      std::string resampledSigName = originalSignatures[index]->getName() + "_resampled";
      int suffix(2);
      ModelResource<Signature> pSignature(resampledSigName, originalSignatures[index]->getParent());

      // probably not needed but just in case resampled name already used
      while (pSignature.get() == NULL)
      {
         pSignature = ModelResource<Signature>(resampledSigName + StringUtilities::toDisplayString<int>(suffix),
            originalSignatures[index]->getParent());
         ++suffix;
      }
      if (resampledTo.empty() == false)
      {
         DynamicObject* pMetaData = pSignature->getMetadata();
         if (pMetaData != NULL)
         {
            pMetaData->setAttribute(CommonSignatureMetadataKeys::ResampledTo(), resampledTo);
         }
      }
      std::vector<double> toData;
      std::vector<int> toBands;
      if (pResampler->execute(fromData, toData, fromWavelengths, toWavelengths, toFwhm, toBands, errorMsg))
      {
         if (toWavelengths.size() != toBands.size())
         {
            if (toBands.size() < 2)  // no need to try if only one point
            {
               continue;
            }

            if (useFillValue)
            {
               std::vector<double> values(toWavelengths.size(), fillValue);
               for (unsigned int i = 0; i < toBands.size(); ++i)
               {
                  values[static_cast<unsigned int>(toBands[i])] = toData[i];
               }
               toData.swap(values);
               DynamicObject* pMetaData = pSignature->getMetadata();
               if (pMetaData != NULL)
               {
                  pMetaData->setAttribute(CommonSignatureMetadataKeys::FillValue(), fillValue);
               }
            }
            else
            {
               std::vector<double> wavelengths(toBands.size());
               for (unsigned int i = 0; i < toBands.size(); ++i)
               {
                  wavelengths[i] = toWavelengths[static_cast<unsigned int>(toBands[i])];
               }
               toWavelengths.swap(wavelengths);
            }
         }
         pSignature->setData(dataName, toData);
         pSignature->setData(wavelengthName, toWavelengths);
         SignatureDataDescriptor* pDesc = dynamic_cast<SignatureDataDescriptor*>(pSignature->getDataDescriptor());
         if (pDesc == NULL)
         {
            continue;
         }
         pDesc->setUnits(dataName, originalSignatures[index]->getUnits(dataName));
         pResampledSignatures->push_back(pSignature.release());
         ++numSigsResampled;
      }
      std::string progressStr =
         QString("Resampled signature %1 of %2 signatures").arg(index + 1).arg(numSigs).toStdString();
      progress.report(progressStr, (index + 1) * 100 / numSigs, NORMAL);
   }

   // reset config options
   ResamplerOptions::setSettingResamplerMethod(configMethod);
   ResamplerOptions::setSettingDropOutWindow(configDropout);
   ResamplerOptions::setSettingFullWidthHalfMax(configFwhm);

   if (numSigsResampled == numSigs)
   {
      progress.report("Complete", 100, NORMAL);
      progress.upALevel();
   }
   else
   {
      errorMsg = QString("Only %1 of the %2 signatures were successfully resampled.").arg(
         numSigsResampled).arg(numSigs).toStdString();
      progress.report(errorMsg, 100, WARNING, true);
   }

   VERIFY(pOutArgList->setPlugInArgValue("Resampled signatures", pResampledSignatures.release()));
   return true;
}

bool ResamplerPlugIn::getWavelengthsFromElement(const DataElement* pElement,
   Wavelengths* pWavelengths, std::string& errorMsg)
{
   errorMsg.clear();
   if (pElement == NULL || pWavelengths == NULL)
   {
      errorMsg = "Invalid input parameters.";
      return false;
   }
   pWavelengths->clear();
   if (pElement->isKindOf(TypeConverter::toString<RasterElement>()))
   {
      if (pWavelengths->initializeFromDynamicObject(pElement->getMetadata(), false) == true)
      {
            return true;
      }
   }
   else if (pElement->isKindOf(TypeConverter::toString<Signature>()))
   {
      const Signature* pSig(NULL);

      // look for signature set first - if signature set, use first sig
      const SignatureSet* pSignatureSet = dynamic_cast<const SignatureSet*>(pElement);
      if (pSignatureSet != NULL)
      {
         const std::vector<Signature*>& setSignatures = pSignatureSet->getSignatures();
         if (setSignatures.empty())
         {
            errorMsg = "Signature set \"" + pSignatureSet->getDisplayName(true) +
               "\" is empty - no wavelength information is available.";
            return false;
         }
         pSig = setSignatures.front();
      }

      // if signature not gotten from set, try as signature
      if (pSig == NULL)
      {
         pSig = dynamic_cast<const Signature*>(pElement);
         if (pSig != NULL)
         {
            DataVariant variant = pSig->getData("Wavelength");
            if (variant.isValid())
            {
               std::vector<double> wavelengths;
               if (variant.getValue(wavelengths) == true)
               {
                  pWavelengths->setCenterValues(wavelengths, MICRONS);
                  return true;
               }
            }
         }
      }
   }

   errorMsg = "Unable to obtain the wavelengths from data element \"" + pElement->getDisplayName(true) + "\".";
   return false;
}

bool ResamplerPlugIn::getWavelengthsFromFile(const std::string& filename,
   Wavelengths* pWavelengths, std::string& errorMsg)
{
   errorMsg.clear();
   if (filename.empty() || pWavelengths == NULL)
   {
      errorMsg = "Invalid input parameters.";
      return false;
   }
   pWavelengths->clear();

   FactoryResource<Filename> pFilename;
   pFilename->setFullPathAndName(filename);
   std::string importerName;
   std::string extension;
   std::vector<std::string> extensionStrs =
      StringUtilities::split(StringUtilities::toLower(pFilename->getExtension()), '.');
   if (extensionStrs.empty() == false)
   {
      extension = extensionStrs.back();
   }
   if (extension == "wmd")
   {
      importerName = "Wavelength Metadata Importer";
   }
   else
   {
      importerName = "Wavelength Text Importer";
   }
   ExecutableResource pImporter(importerName, std::string());
   if (pImporter->getInArgList().setPlugInArgValue(Wavelengths::WavelengthFileArg(), pFilename.get()) == false)
   {
      errorMsg = "Unable to set filename into plug-in \"" + importerName + "\".";
      return false;
   }

   if (pImporter->execute() == false)
   {
      errorMsg = "Unable to load file \"" + filename + "\".";
      return false;
   }

   Wavelengths* pWave = pImporter->getOutArgList().getPlugInArgValue<Wavelengths>(Wavelengths::WavelengthsArg());
   if (pWave == NULL)
   {
      errorMsg = "Unable to extract wavelengths from plug-in \"" + importerName + "\".";
      return false;
   }

   if (pWavelengths->initializeFromWavelengths(pWave) == false)
   {
      errorMsg = "Unable to retrieve the wavelengths.";
      return false;
   }

   return true;
}

bool ResamplerPlugIn::needToResample(const Signature* pSig, const Wavelengths* pWavelengths)
{
   VERIFY(pSig != NULL && pWavelengths != NULL && pWavelengths->hasCenterValues());

#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : This code to check the wavelength units needs to " \
   "be updated when EXTRAS-47 is implemented (rforehan)")
   WavelengthUnitsType waveUnits(MICRONS);  // set as default
   const Units* pUnits = pSig->getUnits("Wavelength");
   if (pUnits != NULL)
   {
      bool error(true);
      std::string unitName = pUnits->getUnitName();
      WavelengthUnitsType tmpUnits = StringUtilities::fromXmlString<WavelengthUnitsType>(unitName, &error);
      if (error == false)
      {
         waveUnits = tmpUnits;
      }
   }

   // use 0.1 nanometer or 0.001 inv cm as close enough for two wavelengths to be considered equivalent
   double closeEnough(0.0001);
   switch (waveUnits)
   {

   case NANOMETERS:
      closeEnough = 0.1;
      break;

   case INVERSE_CENTIMETERS:
      closeEnough = 0.001;
      break;

   case MICRONS:  // fall through
   default:
      closeEnough = 0.0001;
      break;
   }

   const DataVariant& sigVariant = pSig->getData("Wavelength");
   VERIFY(sigVariant.isValid());
   std::vector<double> sigWavelengths;
   sigVariant.getValue(sigWavelengths);
   const std::vector<double>& resampleWavelengths = pWavelengths->getCenterValues();
   if (sigWavelengths.size() != resampleWavelengths.size())
   {
      return true;
   }
   for (unsigned int i = 0; i < sigWavelengths.size(); ++i)
   {
      if (fabs(sigWavelengths[i] - resampleWavelengths[i]) > closeEnough)
      {
         return true;
      }
   }
   return false;
}
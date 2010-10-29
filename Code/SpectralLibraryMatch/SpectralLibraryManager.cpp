/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DataVariant.h"
#include "DesktopServices.h"
#include "DynamicObject.h"
#include "LibraryEditDlg.h"
#include "ModelServices.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "Progress.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "Resampler.h"
#include "SessionItemDeserializer.h"
#include "SessionItemSerializer.h"
#include "SessionManager.h"
#include "Signature.h"
#include "SignatureLibrary.h"
#include "Slot.h"
#include "SpectralLibraryManager.h"
#include "SpectralLibraryMatch.h"
#include "SpectralVersion.h"
#include "Subject.h"
#include "ToolBar.h"
#include "TypeConverter.h"
#include "Units.h"
#include "Wavelengths.h"
#include "XercesIncludes.h"
#include "xmlreader.h"
#include "xmlwriter.h"

#include <QtGui/QAction>
#include <QtGui/QPixmap>

XERCES_CPP_NAMESPACE_USE

REGISTER_PLUGIN_BASIC(SpectralSpectralLibraryMatch, SpectralLibraryManager);

namespace
{
   const char* const EditSpectralLibraryIcon[] =
   {
      "16 16 8 1",
      " 	c None",
      ".	c #000000",
      "+	c #800000",
      "@	c #FFFFFF",
      "#	c #FFFF00",
      "$	c #0000FF",
      "%	c #C0C0C0",
      "&	c #808080",
      "            ... ",
      ".............++ ",
      ".@@@@@@@@@@.#.. ",
      ".@$$$$$$$$$.#%. ",
      ".@@@@@@@@@.#%.  ",
      ".@&&&&&&&&.#%.  ",
      ".@&@@@@@@.#%..  ",
      ".@&@$$$@@.#%..  ",
      ".@&@&&&@@...@.  ",
      ".@&@@@@@@..&@.  ",
      ".@&@&&&&&.@&@.  ",
      ".@&@@@@@@@@&@.  ",
      ".@&&&&&&&&&&@.  ",
      ".@@@@@@@@@@@@.  ",
      ".@@@@@@@@@@@@.  ",
      "..............  "   };
}

SpectralLibraryManager::SpectralLibraryManager() :
   mpProgress(NULL),
   mpEditSpectralLibraryAction(NULL)
{
   ExecutableShell::setName(SpectralLibraryMatch::getNameLibraryManagerPlugIn());
   setType("Manager");
   setSubtype("SpectralLibrary");
   setVersion(SPECTRAL_VERSION_NUMBER);
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setShortDescription("Manages a spectral library.");
   setDescription("Controls populating and editing a spectral library for use in matching in-scene spectra.");
   setDescriptorId("{72116B2A-0A82-46b6-B0D0-CE168C73CA7E}");
   allowMultipleInstances(false);
   executeOnStartup(true);
   destroyAfterExecute(false);
   setWizardSupported(false);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
}

SpectralLibraryManager::~SpectralLibraryManager()
{
   clearLibrary();

   // Remove the toolbar button
   Service<DesktopServices> pDesktop;
   ToolBar* pToolBar = static_cast<ToolBar*>(pDesktop->getWindow("Spectral", TOOLBAR));
   if (pToolBar != NULL)
   {
      if (mpEditSpectralLibraryAction != NULL)
      {
         disconnect(mpEditSpectralLibraryAction, SIGNAL(activated()), this, SLOT(matchAoiAverageSpectrum()));
         pToolBar->removeItem(mpEditSpectralLibraryAction);
         delete mpEditSpectralLibraryAction;
      }
   }
}

bool SpectralLibraryManager::getInputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

bool SpectralLibraryManager::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

bool SpectralLibraryManager::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   mpProgress = Service<PlugInManagerServices>()->getProgress(this);
   if (mpProgress != NULL)
   {
      Service<DesktopServices>()->createProgressDialog(getName(), mpProgress);
   }

   // Create edit library action
   if (isBatch() == false)
   {
      QPixmap pixEditLib(EditSpectralLibraryIcon);
      mpEditSpectralLibraryAction = new QAction(QIcon(pixEditLib),
         "&Edit Spectral Library", this);
      mpEditSpectralLibraryAction->setAutoRepeat(false);
      mpEditSpectralLibraryAction->setStatusTip("Display the editor for adding and removing "
         "signatures used by the Spectral Library Match algorithm plug-ins.");
      VERIFYNR(connect(mpEditSpectralLibraryAction, SIGNAL(triggered()), this, SLOT(editSpectralLibrary())));

      ToolBar* pToolBar = static_cast<ToolBar*>(Service<DesktopServices>()->getWindow("Spectral", TOOLBAR));
      if (pToolBar != NULL)
      {
         pToolBar->addSeparator();
         pToolBar->addButton(mpEditSpectralLibraryAction);
      }

      return true;
   }

   return false;
}

bool SpectralLibraryManager::addSignatures(const std::vector<Signature*>& signatures)
{
   if (signatures.empty())
   {
      return false;
   }

   // set library UnitType to first signature added to the library
   if (mSignatures.empty())
   {
      mLibraryUnitType = signatures.front()->getUnits(
         SpectralLibraryMatch::getNameSignatureAmplitudeData())->getUnitType();
   }

   bool needToRebuildLibraries(false);
   mSignatures.reserve(mSignatures.size() + signatures.size());
   std::vector<Signature*> notAdded;
   unsigned int numAdded(0);
   for (std::vector<Signature*>::const_iterator it = signatures.begin(); it != signatures.end(); ++it)
   {
      std::vector<Signature*>::iterator sit = std::find(mSignatures.begin(), mSignatures.end(), *it);
      if (sit == mSignatures.end())
      {
         // check that units are same as rest of the library
         if ((*it)->getUnits(SpectralLibraryMatch::getNameSignatureAmplitudeData())->getUnitType() == mLibraryUnitType)
         {
            mSignatures.push_back(*it);
            (*it)->attach(SIGNAL_NAME(Subject, Deleted), Slot(this, &SpectralLibraryManager::signatureDeleted));
            needToRebuildLibraries = true;
            ++numAdded;
         }
         else
         {
            notAdded.push_back(*it);
         }
      }
   }

   if (needToRebuildLibraries)
   {
      invalidateLibraries();
   }

   if (notAdded.empty() == false)
   {
      std::string msg = "The following signatures are not in the same units (" +
         StringUtilities::toDisplayString<UnitType>(mLibraryUnitType) + ") as the rest of the library. "
         "They were not added to the library:\n";
      for (std::vector<Signature*>::iterator it = notAdded.begin(); it != notAdded.end(); ++it)
      {
         msg += "  " + (*it)->getName() + "\n";
      }
      if (mpProgress != NULL)
      {
         mpProgress->updateProgress(msg, 100, ERRORS);
      }
   }

   return (numAdded > 0);
}

bool SpectralLibraryManager::removeSignatures(const std::vector<Signature*>& signatures)
{
   bool needToRebuildLibraries(false);
   for (std::vector<Signature*>::const_iterator it = signatures.begin(); it != signatures.end(); ++it)
   {
      std::vector<Signature*>::iterator sit = std::find(mSignatures.begin(), mSignatures.end(), *it);
      if (sit != mSignatures.end())
      {
         (*it)->detach(SIGNAL_NAME(Subject, Deleted), Slot(this, &SpectralLibraryManager::signatureDeleted));
         mSignatures.erase(sit);
         needToRebuildLibraries = true;
      }
   }

   if (needToRebuildLibraries)
   {
      invalidateLibraries();
   }

   return true;
}

const RasterElement* SpectralLibraryManager::getLibraryData(const RasterElement* pRaster)
{
   if (mSignatures.empty())
   {
      return NULL;
   }

   std::map<const RasterElement*, RasterElement*>::iterator rit = mLibraries.find(pRaster);
   if (rit == mLibraries.end())
   {
      if (generateResampledLibrary(pRaster))
      {
         rit = mLibraries.find(pRaster);
      }
   }

   if (rit != mLibraries.end())
   {
      return rit->second;
   }

   return NULL;
}

bool SpectralLibraryManager::generateResampledLibrary(const RasterElement* pRaster)
{
   VERIFY(pRaster != NULL);

   Service<ModelServices> pModel;
   std::string libName = "SpectralLibrary";
   RasterElement* pLib =
      dynamic_cast<RasterElement*>(pModel->getElement(libName, TypeConverter::toString<RasterElement>(), pRaster));
   if (pLib != NULL)
   {
      pLib->detach(SIGNAL_NAME(Subject, Deleted), Slot(this, &SpectralLibraryManager::resampledElementDeleted));
      pModel->destroyElement(pLib);
      pLib = NULL;
   }

   // create new raster element for resampled signatures:
   // num rows = num Signatures, num cols = 1, num bands = pRaster num bands
   const RasterDataDescriptor* pDesc = dynamic_cast<const RasterDataDescriptor*>(pRaster->getDataDescriptor());
   VERIFY(pDesc != NULL);
   pLib = RasterUtilities::createRasterElement(libName, static_cast<unsigned int>(mSignatures.size()), 1,
      pDesc->getBandCount(), FLT8BYTES, BIP, true, const_cast<RasterElement*>(pRaster));
   VERIFY(pLib != NULL);
   pLib->attach(SIGNAL_NAME(Subject, Deleted), Slot(this, &SpectralLibraryManager::resampledElementDeleted));

   bool libValid = resampleSigsToLib(pLib, pRaster);   
   std::map<const RasterElement*, RasterElement*>::iterator rit = mLibraries.find(pRaster);
   if (rit != mLibraries.end())
   {
      if (libValid)
      {
         rit->second = pLib;
      }
      else
      {
         mLibraries.erase(rit);
      }
   }
   else if (libValid)
   {
      mLibraries.insert(std::pair<const RasterElement*, RasterElement*>(pRaster, pLib));
   }

   const_cast<RasterElement*>(pRaster)->attach(SIGNAL_NAME(Subject, Deleted),
      Slot(this, &SpectralLibraryManager::elementDeleted));
   return true;
}

bool SpectralLibraryManager::resampleSigsToLib(RasterElement* pLib, const RasterElement* pRaster)
{
   VERIFY(pLib != NULL && pRaster != NULL);

   // check that lib sigs are in same units as the raster element
   const RasterDataDescriptor* pDesc = dynamic_cast<const RasterDataDescriptor*>(pRaster->getDataDescriptor());
   VERIFY(pDesc != NULL);
   const Units* pUnits = pDesc->getUnits();
   if (pDesc->getUnits()->getUnitType() != mLibraryUnitType)
   {
      if (Service<DesktopServices>()->showMessageBox("Mismatched Units", "The data are not in the "
         "same units as the spectral library.\n Do you want to continue anyway?", "Yes", "No") == 1)
      {
         return false;
      }
   }

   FactoryResource<Wavelengths> pWavelengths;
   VERIFY(pWavelengths->initializeFromDynamicObject(pRaster->getMetadata(), false));

   // populate the library with the resampled signatures
   PlugInResource pPlugIn("Resampler");
   Resampler* pResampler = dynamic_cast<Resampler*>(pPlugIn.get());
   VERIFY(pResampler != NULL);
   RasterDataDescriptor* pLibDesc = dynamic_cast<RasterDataDescriptor*>(pLib->getDataDescriptor());
   VERIFY(pLibDesc != NULL);
   VERIFY(pWavelengths->getNumWavelengths() == pLibDesc->getBandCount());
   VERIFY(pLibDesc->getRowCount() == static_cast<unsigned int>(mSignatures.size()));
   FactoryResource<DataRequest> pRequest;
   pRequest->setWritable(true);
   pRequest->setRows(pLibDesc->getActiveRow(0), pLibDesc->getActiveRow(pLibDesc->getRowCount()-1), 1);
   DataAccessor acc = pLib->getDataAccessor(pRequest.release());

   std::vector<double> sigValues;
   std::vector<double> sigWaves;
   std::vector<double> rasterWaves = pWavelengths->getCenterValues();
   std::vector<double> rasterFwhm = pWavelengths->getFwhm();
   std::vector<double> resampledValues;
   std::vector<int> bandIndex;
   DataVariant data;
   for (std::vector<Signature*>::const_iterator it = mSignatures.begin(); it != mSignatures.end(); ++it)
   {
      resampledValues.clear();
      data = (*it)->getData(SpectralLibraryMatch::getNameSignatureAmplitudeData());
      VERIFY(data.isValid());
      VERIFY(data.getValue(sigValues));
      double scaleFactor = (*it)->getUnits(
         SpectralLibraryMatch::getNameSignatureAmplitudeData())->getScaleFromStandard();
      for (std::vector<double>::iterator sit = sigValues.begin(); sit != sigValues.end(); ++sit)
      {
         *sit *= scaleFactor;
      }

      data = (*it)->getData("Wavelength");
      VERIFY(data.isValid());
      VERIFY(data.getValue(sigWaves));
      VERIFY(acc.isValid());
      std::string msg;
      if (pResampler->execute(sigValues, resampledValues, sigWaves, rasterWaves,
         rasterFwhm, bandIndex, msg) == false)
      {
         if (mpProgress != NULL)
         {
            mpProgress->updateProgress(msg, 0, ERRORS);
            return false;
         }
      }
      void* pData = acc->getColumn();
      memcpy(acc->getColumn(), &resampledValues[0], pLibDesc->getBandCount() * sizeof(double));

      acc->nextRow();
   }

   // set wavelength info in resampled library
   pWavelengths->applyToDynamicObject(pLib->getMetadata());
   FactoryResource<Units> libUnits;
   libUnits->setUnitType(mLibraryUnitType);
   libUnits->setUnitName(StringUtilities::toDisplayString<UnitType>(mLibraryUnitType));
   pLibDesc->setUnits(libUnits.get());
   return true;
}

void SpectralLibraryManager::elementDeleted(Subject& subject, const std::string& signal, const boost::any& value)
{
   RasterElement* pRaster = dynamic_cast<RasterElement*>(&subject);
   if (pRaster != NULL)
   {
      std::map<const RasterElement*, RasterElement*>::iterator rit = mLibraries.find(pRaster);
      if (rit != mLibraries.end())
      {
         mLibraries.erase(rit);
      }
   }
}

void SpectralLibraryManager::resampledElementDeleted(Subject& subject, const std::string& signal,
                                                     const boost::any& value)
{
   RasterElement* pLib = dynamic_cast<RasterElement*> (&subject);
   if (pLib != NULL)
   {
      for (std::map<const RasterElement*, RasterElement*>::iterator it = mLibraries.begin();
         it != mLibraries.end(); ++it)
      {
         if (it->second == pLib)
         {
            mLibraries.erase(it);
            return;
         }
      }
   }
}

void SpectralLibraryManager::signatureDeleted(Subject& subject, const std::string& signal, const boost::any& value)
{
   Signature* pSignature = dynamic_cast<Signature*>(&subject);
   if (pSignature != NULL)
   {
      std::vector<Signature*>::iterator it = std::find(mSignatures.begin(), mSignatures.end(), pSignature);
      if (it != mSignatures.end())
      {
         mSignatures.erase(it);
      }
      invalidateLibraries();
   }
}

Signature* SpectralLibraryManager::getLibrarySignature(unsigned int index)
{
   return mSignatures[index];
}

const std::vector<Signature*>& SpectralLibraryManager::getLibrarySignatures() const
{
   return mSignatures;
}

void SpectralLibraryManager::invalidateLibraries()
{
   Service<ModelServices> pModel;
   for (std::map<const RasterElement*, RasterElement*>::iterator it = mLibraries.begin(); it != mLibraries.end(); ++it)
   {
      const_cast<RasterElement*>(it->first)->detach(SIGNAL_NAME(Subject, Deleted),
         Slot(this, &SpectralLibraryManager::elementDeleted));
      it->second->detach(SIGNAL_NAME(Subject, Deleted), Slot(this, &SpectralLibraryManager::resampledElementDeleted));
      pModel->destroyElement(it->second);
   }
   mLibraries.clear();
}

void SpectralLibraryManager::clearLibrary()
{
   // detach from signatures and raster elements and destroy resampled raster elements (libraries)
   invalidateLibraries();
   for (std::vector<Signature*>::iterator it = mSignatures.begin(); it != mSignatures.end(); ++it)
   {
      (*it)->detach(SIGNAL_NAME(Subject, Deleted), Slot(this, &SpectralLibraryManager::signatureDeleted));
   }
   mSignatures.clear();
}

bool SpectralLibraryManager::editSpectralLibrary()
{
   LibraryEditDlg dlg(mSignatures, Service<DesktopServices>()->getMainWidget());
   if (dlg.exec() == QDialog::Rejected)
   {
      return false;
   }
   
   std::vector<Signature*> editedSigs = dlg.getSignatures();

   bool libChanged(false);

   // simple check for change
   if (mSignatures.size() != editedSigs.size())
   {
      libChanged = true;
   }
   else
   {
      // detailed check for different sig in same size container
      std::vector<Signature*>::iterator origIt = mSignatures.begin();
      for (std::vector<Signature*>::iterator editIt = editedSigs.begin();
         editIt != editedSigs.end() && origIt != mSignatures.end(); ++editIt, ++origIt)
      {
         if (*editIt != *origIt)
         {
            libChanged = true;
            break;
         }
      }
   }

   if (libChanged)
   {
      clearLibrary();
      addSignatures(editedSigs);
   }

   return true;
}

bool SpectralLibraryManager::isEmpty() const
{
   return mSignatures.empty();
}

unsigned int SpectralLibraryManager::size() const
{
   return mSignatures.size();
}

bool SpectralLibraryManager::serialize(SessionItemSerializer& serializer) const
{
   XMLWriter writer("SpectralLibraryManager");

   // Save Signatures
   for (std::vector<Signature*>::const_iterator it = mSignatures.begin(); it != mSignatures.end(); ++it)
   {
      Signature* pSignature = *it;
      if (pSignature != NULL)
      {
         writer.pushAddPoint(writer.addElement("Signature"));
         writer.addAttr("signatureId", pSignature->getId());
         writer.popAddPoint();
      }
   }

   return serializer.serialize(writer);
}

bool SpectralLibraryManager::deserialize(SessionItemDeserializer& deserializer)
{
   if (isBatch() == true)
   {
      setInteractive();
   }

   bool success = execute(NULL, NULL);

   if (success)
   {
      Service<SessionManager> pSessionManager;
      XmlReader reader(NULL, false);
      DOMElement* pRootElement = deserializer.deserialize(reader, "SpectralLibraryManager");
      for (DOMNode* pChild = pRootElement->getFirstChild(); pChild != NULL; pChild = pChild->getNextSibling())
      {
         DOMElement* pElement = static_cast<DOMElement*>(pChild);
         if (XMLString::equals(pElement->getNodeName(), X("Signature")))
         {
            std::string signatureId = A(pElement->getAttribute(X("signatureId")));

            Signature* pSignature = dynamic_cast<Signature*>(pSessionManager->getSessionItem(signatureId));
            if (pSignature != NULL)
            {
               mSignatures.push_back(pSignature);
            }
         }
      }
   }

   return success;
}

bool SpectralLibraryManager::setBatch()
{
   ExecutableShell::setBatch();
   return false;
}

int SpectralLibraryManager::getSignatureIndex(const Signature* pSignature) const
{
   int index(-1);
   for (unsigned int i = 0; i < mSignatures.size(); ++i)
   {
      if (mSignatures[i] == pSignature)
      {
         index = i;
         break;
      }
   }

   return index;
}

bool SpectralLibraryManager::getResampledSignatureValues(const RasterElement* pRaster, const Signature* pSignature,
                                                         std::vector<double>& values)
{
   values.clear();
   if (pRaster == NULL || pSignature == NULL)
   {
      return false;
   }

   const RasterElement* pLibData = getLibraryData(pRaster);
   if (pLibData == NULL)
   {
      return false;
   }

   int index = getSignatureIndex(pSignature);
   if (index < 0)
   {
      return false;
   }
   const RasterDataDescriptor* pLibDesc = dynamic_cast<const RasterDataDescriptor*>(pLibData->getDataDescriptor());
   VERIFY(pLibDesc != NULL);
   unsigned int numBands = pLibDesc->getBandCount();
   values.reserve(numBands);
   FactoryResource<DataRequest> pRqt;
   unsigned int row = static_cast<unsigned int>(index);
   pRqt->setInterleaveFormat(BIP);
   pRqt->setRows(pLibDesc->getActiveRow(row), pLibDesc->getActiveRow(row), 1);
   DataAccessor acc = pLibData->getDataAccessor(pRqt.release());
   VERIFY(acc.isValid());
   double* pDbl = reinterpret_cast<double*>(acc->getColumn());
   for (unsigned int band = 0; band < numBands; ++band)
   {
      values.push_back(*pDbl);
      ++pDbl;
   }

   return true;
}

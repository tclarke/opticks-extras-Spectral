/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtCore/QString>
#include <QtCore/QFileInfo>

#include "AppVerify.h"
#include "DataDescriptor.h"
#include "DataVariant.h"
#include "DynamicObject.h"
#include "FileResource.h"
#include "ImportDescriptor.h"
#include "MessageLogMgr.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "ProgressTracker.h"
#include "SignatureFileDescriptor.h"
#include "SignatureSet.h"
#include "SignatureSetImporter.h"
#include "SpectralVersion.h"
#include "StringUtilities.h"
#include "XercesIncludes.h"

XERCES_CPP_NAMESPACE_USE
using namespace std;

namespace
{
   class ImportDescriptorFilter : public DOMNodeFilter
   {
   public:
      ImportDescriptorFilter() {}
      virtual ~ImportDescriptorFilter() {}
      virtual FilterAction acceptNode(const DOMNode* pNode) const
      {
         if (pNode == NULL)
         {
            return FILTER_REJECT;
         }
         if (XMLString::equals(pNode->getNodeName(), X("metadata")) ||
            XMLString::equals(pNode->getNodeName(), X("signature_set")))
         {
            return FILTER_ACCEPT;
         }
         return FILTER_SKIP;
      }
   };
};

REGISTER_PLUGIN_BASIC(SpectralSignature, SignatureSetImporter);

SignatureSetImporter::SignatureSetImporter() : mDatasetNumber(0)
{
   setDescriptorId("{792F86A1-AAB3-4333-A3DB-39A9B13F6CC6}");
   setName("Spectral Signature Library Importer");
   setSubtype("Signature Set");
   setCreator("Ball Aerospace & Technologies Corp.");
   setShortDescription("Import spectral signature libraries.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setExtensions("Spectral Signature Library Files (*.slb)");
   setAbortSupported(true);
}

SignatureSetImporter::~SignatureSetImporter()
{
   for (std::map<std::string, XmlReader*>::const_iterator xmlit = mXml.begin(); xmlit != mXml.end(); ++xmlit)
   {
      delete xmlit->second;
   }
}

void SignatureSetImporter::loadDoc(const std::string& filename)
{
   if (mXml.find(filename) == mXml.end())
   {
      mXml[filename] = new XmlReader(Service<MessageLogMgr>()->getLog(), false);
      mDoc[filename] = mXml[filename]->parse(filename);
   }
}

unsigned char SignatureSetImporter::getFileAffinity(const string &filename)
{
   // is this an XML file?
   // This is a double parse but ensures that we don't get parse errors on non-xml files
   loadDoc(filename);
   if (mDoc[filename] == NULL || mDoc[filename]->getDocumentElement() == NULL)
   {
      return CAN_NOT_LOAD;
   }
   return getImportDescriptors(filename).empty() ? CAN_NOT_LOAD : CAN_LOAD;
}

vector<ImportDescriptor*> SignatureSetImporter::getImportDescriptors(const string &filename)
{
   vector<ImportDescriptor*> descriptors;
   if (filename.empty())
   {
      return descriptors;
   }
   mFilename = filename;
   try
   {
      loadDoc(filename);
      if (mDoc[filename] == NULL)
      {
         return descriptors;
      }
      FactoryResource<DynamicObject> pMetadata;
      VERIFYRV(pMetadata.get() != NULL, descriptors);

      mDatasetNumber = 0;
      ImportDescriptorFilter filter;
      DOMTreeWalker* pTree = mDoc[filename]->createTreeWalker(mDoc[filename]->getDocumentElement(), DOMNodeFilter::SHOW_ELEMENT, &filter, false);
      std::vector<std::string> dummy;
      descriptors = createImportDescriptors(pTree, dummy);
   }
   catch(const DOMException &) {}
   catch(const XMLException &) {}

   return descriptors;
}

vector<ImportDescriptor*> SignatureSetImporter::createImportDescriptors(DOMTreeWalker* pTree, vector<string> &datasetPath)
{
   vector<ImportDescriptor*> descriptors;
   FactoryResource<DynamicObject> pMetadata;
   VERIFYRV(pMetadata.get() != NULL, descriptors);

   string datasetName = StringUtilities::toDisplayString(mDatasetNumber++);
   for (DOMNode* pChld = pTree->firstChild(); pChld != NULL; pChld = pTree->nextSibling())
   {
      if (XMLString::equals(pChld->getNodeName(), X("metadata")))
      {
         DOMElement* pElmnt = static_cast<DOMElement*>(pChld);
         string name = A(pElmnt->getAttribute(X("name")));
         string val = A(pElmnt->getAttribute(X("value")));
         pMetadata->setAttribute(name, val);
         if (name == "Name")
         {
            datasetName = val;
         }
      }
      else if (XMLString::equals(pChld->getNodeName(), X("signature_set")))
      {
         datasetPath.push_back(datasetName);
         vector<ImportDescriptor*> sub = createImportDescriptors(pTree, datasetPath);
         datasetPath.pop_back();
         descriptors.insert(descriptors.end(), sub.begin(), sub.end());
         pTree->parentNode();
      }
   }
   ImportDescriptorResource pImportDescriptor(datasetName, "SignatureSet", datasetPath);
   VERIFYRV(pImportDescriptor.get() != NULL, descriptors);
   DataDescriptor* pDataDescriptor = pImportDescriptor->getDataDescriptor();
   VERIFYRV(pDataDescriptor != NULL, descriptors);
   FactoryResource<SignatureFileDescriptor> pFileDescriptor;
   VERIFYRV(pFileDescriptor.get() != NULL, descriptors);
   pFileDescriptor->setFilename(mFilename);
   datasetPath.push_back(datasetName);
   string loc = "/" + StringUtilities::join(datasetPath, "/");
   datasetPath.pop_back();
   pFileDescriptor->setDatasetLocation(loc);
   pDataDescriptor->setFileDescriptor(pFileDescriptor.get());
   pDataDescriptor->setMetadata(pMetadata.get());
   descriptors.push_back(pImportDescriptor.release());
   return descriptors;
}

bool SignatureSetImporter::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY((pInArgList = Service<PlugInManagerServices>()->getPlugInArgList()) != NULL);
   VERIFY(pInArgList->addArg<Progress>(Executable::ProgressArg(), NULL, Executable::ProgressArgDescription()));
   VERIFY(pInArgList->addArg<SignatureSet>(Importer::ImportElementArg(), NULL, "Spectral library to be imported."));
   return true;
}

bool SignatureSetImporter::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   pOutArgList = NULL;
   return true;
}

bool SignatureSetImporter::execute(PlugInArgList* pInArgList, PlugInArgList* OutArgList)
{
   VERIFY(pInArgList != NULL);
   Progress* pProgress = pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg());
   ProgressTracker progress(pProgress, "Loading spectral signature library", "spectral", "7B21EE8A-D2E1-4325-BB9F-F4E521BFD5ED");

   SignatureSet* pSignatureSet = pInArgList->getPlugInArgValue<SignatureSet>(Importer::ImportElementArg());
   VERIFY(pSignatureSet != NULL);
   DataDescriptor* pDataDescriptor = pSignatureSet->getDataDescriptor();
   VERIFY(pDataDescriptor != NULL);
   FileDescriptor* pFileDescriptor = pDataDescriptor->getFileDescriptor();
   VERIFY(pFileDescriptor != NULL);
   const string& filename = pFileDescriptor->getFilename().getFullPathAndName();

   progress.getCurrentStep()->addProperty("signature set", pSignatureSet->getName());
   progress.getCurrentStep()->addProperty("dataset location", pFileDescriptor->getDatasetLocation());

   // locate the spot in the tree for this dataset
   try
   {
      string expr;
      vector<string> parts = StringUtilities::split(pFileDescriptor->getDatasetLocation(), '/');
      for (vector<string>::iterator part = parts.begin(); part != parts.end(); ++part)
      {
         if (!part->empty())
         {
            expr += "/signature_set[metadata/@name='Name' and metadata/@value='" + *part + "']";
         }
      }
      expr += "/signature";
      loadDoc(filename);
      DOMXPathResult* pResult = mXml[filename]->query(expr, DOMXPathResult::SNAPSHOT_RESULT_TYPE);
      VERIFY(pResult != NULL);
      int nodeTotal = pResult->getSnapshotLength();
      for (int nodeNum = 0; nodeNum < nodeTotal; ++nodeNum)
      {
         if (isAborted())
         {
            progress.report("Aborted file " + pFileDescriptor->getFilename().getFullPathAndName(), 0, WARNING, true);
            progress.report("User aborted the operation.", 0, ABORT, true);
            return false;
         }
         int percent = static_cast<int>(100.0 * nodeNum / nodeTotal);
         progress.report("Importing signature library", percent, NORMAL);
         if (!pResult->snapshotItem(nodeNum) || !pResult->isNode())
         {
            continue;
         }
         const DOMElement* pElmnt = static_cast<const DOMElement*>(pResult->getNodeValue());
         string filename = A(pElmnt->getAttribute(X("filename")));
         if (filename.empty() == false)
         {
            string path = pFileDescriptor->getFilename().getPath();

            QString tempFilename = QString::fromStdString(filename);
            if (tempFilename.startsWith("./") == true)
            {
               tempFilename.replace(0, 1, QString::fromStdString(path));
               filename = tempFilename.toStdString();
            }
            else
            {
               QFileInfo fileInfo(tempFilename);
               if (fileInfo.isRelative() == true)
               {
                  filename = path + SLASH + filename;
               }
            }
         }

         // don't pass progress to importer - the individual signature imports are rapid and passing progress will
         // cause isAborted() to not function properly.
         ImporterResource importer("Auto Importer", filename, NULL);
         if (importer->getPlugIn() == NULL)
         {
            progress.report("The \"Auto Importer\" is not available and is required to import signature sets.", 0, ERRORS, true);
            return false;
         }
         if (importer->execute())
         {
            vector<DataElement*> elements = importer->getImportedElements();
            vector<Signature*> sigs(elements.size(), NULL);
            for (vector<DataElement*>::iterator element = elements.begin(); element != elements.end(); ++element)
            {
               Signature* pSig = dynamic_cast<Signature*>(*element);
               if (pSig != NULL)
               {
                  pSignatureSet->insertSignature(pSig);
                  // reparent the signature
                  Service<ModelServices>()->setElementParent(pSig, pSignatureSet);
               }
            }
         }
         else
         {
            progress.report("Unable to import signature " + filename, percent, WARNING, true);
         }
      }
   }
   catch(DOMException &exc)
   {
      progress.report(A(exc.getMessage()), 0, ERRORS, true);
      return false;
   }
   SignatureSet* pParent = dynamic_cast<SignatureSet*>(pSignatureSet->getParent());
   if (pParent != NULL && pParent->getFilename() == pSignatureSet->getFilename())
   {
      pParent->insertSignature(pSignatureSet);
   }

   progress.report("Spectral signature library loaded", 100, NORMAL);
   progress.upALevel();
   return true;
}

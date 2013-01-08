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
#include "Classification.h"
#include "DesktopServices.h"
#include "Endian.h"
#include "ImportDescriptor.h"
#include "ObjectResource.h"
#include "PlotObject.h"
#include "PlotView.h"
#include "PlotWidget.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "ProgressTracker.h"
#include "RangeProfileImporter.h"
#include "RangeProfilePlotManager.h"
#include "RasterUtilities.h"
#include "Signature.h"
#include "SignatureDataDescriptor.h"
#include "SignatureFileDescriptor.h"
#include "SpectralVersion.h"
#include "TypeConverter.h"
#include "Units.h"

#include <QtCore/QFile>
#include <QtCore/QMap>
#include <QtCore/QRegExp>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>

#include <set>
REGISTER_PLUGIN_BASIC(SpectralRangeProfile, RangeProfileImporter);

RangeProfileImporter::RangeProfileImporter()
{
   setName("Range Profile Importer");
   setDescriptorId("{be9172e9-9be2-4b44-91f1-eabcc045b0a2}");
   setDescription("Imports radar range profile data from a CSV file with a metadata header.");
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setExtensions("Range Profile (*.csv *.txt)");
}

RangeProfileImporter::~RangeProfileImporter()
{}

std::vector<ImportDescriptor*> RangeProfileImporter::getImportDescriptors(const std::string& filename)
{
   std::vector<ImportDescriptor*> descriptors;
   QFile file(QString::fromStdString(filename));
   if (!file.open(QFile::ReadOnly | QFile::Text))
   {
      return descriptors;
   }
   QTextStream instream(&file);

   ImportDescriptorResource pImportDesc(filename, TypeConverter::toString<Signature>());
   SignatureDataDescriptor* pDataDesc = dynamic_cast<SignatureDataDescriptor*>(pImportDesc->getDataDescriptor());
   VERIFYRV(pDataDesc != NULL, descriptors);
   SignatureFileDescriptor* pFileDesc =
      dynamic_cast<SignatureFileDescriptor*>(RasterUtilities::generateAndSetFileDescriptor(pDataDesc, filename,
      std::string(), Endian::getSystemEndian()));
   VERIFYRV(pFileDesc != NULL, descriptors);
   DynamicObject* pMetadata = pDataDesc->getMetadata();
   VERIFYRV(pMetadata != NULL, descriptors);

   bool ok = true;
   QString line = instream.readLine();

   // Get classification
   pMetadata->setAttribute("Raw Classification", line.toStdString());

   // Get az/el
   QRegExp line2regexp("Range Profile \\(az = ([-+]?[0-9]*\\.[0-9]+(?:[eE][-+]?[0-9]+)?), "
                       "el = ([-+]?[0-9]*\\.[0-9]+(?:[eE][-+]?[0-9]+)?)\\)");
   line = instream.readLine(2048); // maxlength is arbitrary and prevents potentially long reads on binary files
   if (line2regexp.indexIn(line) == -1)
   {
      return descriptors;
   }
   double azimuth = line2regexp.cap(1).toDouble(&ok);
   if (!ok)
   {
      return descriptors;
   }
   double elevation = line2regexp.cap(2).toDouble(&ok);
   if (!ok)
   {
      return descriptors;
   }
   pMetadata->setAttribute("Azimuth", azimuth);
   pMetadata->setAttribute("Elevation", elevation);

   // column names
   line = instream.readLine();
   QStringList columnNames = line.split("\t");

   // column units
   line = instream.readLine();
   QStringList columnUnits = line.split("\t");

   for (int idx = 0; idx < columnNames.size() && idx < columnUnits.size(); ++idx)
   {
      FactoryResource<Units> pUnits;
      pUnits->setUnitName(columnUnits[idx].toStdString());
      if (columnUnits[idx] == "m")
      {
         pUnits->setUnitType(DISTANCE);
      }
      else
      {
         pUnits->setUnitType(CUSTOM_UNIT);
      }
      pDataDesc->setUnits(columnNames[idx].toStdString(), pUnits.get());
      pFileDesc->setUnits(columnNames[idx].toStdString(), pUnits.get());
   }

   descriptors.push_back(pImportDesc.release());

   return descriptors;
}

unsigned char RangeProfileImporter::getFileAffinity(const std::string& filename)
{
   if (getImportDescriptors(filename).empty())
   {
      return CAN_NOT_LOAD;
   }
   return CAN_LOAD;
}

bool RangeProfileImporter::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pInArgList->addArg<Progress>(Executable::ProgressArg(), NULL, Executable::ProgressArgDescription()));
   VERIFY(pInArgList->addArg<Signature>(Importer::ImportElementArg(), NULL, "Signature into which range profiles will "
      "be loaded."));
   return true;
}

bool RangeProfileImporter::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   pOutArgList = NULL;
   return true;
}

bool RangeProfileImporter::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (pInArgList == NULL)
   {
      return false;
   }
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(ProgressArg()), "Import range profile",
      "spectral", "{107b881b-ec81-4159-af72-e4247c4ff092}");

   Signature* pSig = pInArgList->getPlugInArgValue<Signature>(ImportElementArg());
   if (pSig == NULL)
   {
      progress.report("No signature element provided.", 0, ERRORS, true);
      return false;
   }

   QFile file(QString::fromStdString(pSig->getFilename()));
   if (!file.open(QFile::ReadOnly | QFile::Text))
   {
      progress.report("Unable to open range profile file.", 0, ERRORS, true);
      return false;
   }
   QTextStream instream(&file);
   QString line = instream.readLine(); // classification
   line = instream.readLine();         // az/el

   progress.report("Importing data", 1, NORMAL);

   // column names
   line = instream.readLine();
   QStringList columnNames = line.split("\t");
   if (columnNames.size() != 2)
   {
      progress.report("There are multiple data sets in this file; only the first will be loaded.", 0, WARNING, true);
   }

   // column units
   line = instream.readLine();
   QStringList columnUnits = line.split("\t");

   // data
   QMap<QString, std::vector<double> > dataLists;
   foreach(QString name, columnNames)
   {
      dataLists[name] = std::vector<double>();
   }
   qint64 lastPos = instream.pos();
   while(!instream.atEnd() && lastPos > 0)  // in Solaris: atEnd() doesn't work but pos() returns 0 when at end of file
   {
      foreach(QString name, columnNames)
      {
         double data;
         instream >> data;
         if (instream.status() == QTextStream::Ok)
         {
            dataLists[name].push_back(data);
         }
      }
      if (instream.pos() == lastPos)
      {
         // we haven't made any progress parsing the file...likely a bad file
         // error here or we'll get stuck in an infinite loop
         progress.report("Invalid range profile signature file.", 0, ERRORS, true);
         return false;
      }
      lastPos = instream.pos();
   }

   for (int idx = 0; idx < columnNames.size(); ++idx)
   {
      pSig->setData(columnNames[idx].toStdString(), dataLists[columnNames[idx]]);
   }

   if (!isBatch())
   {
      std::vector<PlugIn*> instances =
         Service<PlugInManagerServices>()->getPlugInInstances("Range Profile Plot Manager");
      if (instances.size() != 1)
      {
         progress.report("Unable to plot the data.", 0, ERRORS, true);
         return false;
      }
      RangeProfilePlotManager* pManager = static_cast<RangeProfilePlotManager*>(instances.front());
      VERIFY(pManager);
      if (!pManager->plotProfile(pSig))
      {
         progress.report("Unable to plot the data.", 0, ERRORS, true);
         return false;
      }
   }

   progress.report("Done importing data", 100, NORMAL);
   progress.upALevel();
   return true;
}

bool RangeProfileImporter::validate(const DataDescriptor* pDescriptor,
                                    const std::vector<const DataDescriptor*>& importedDescriptors,
                                    std::string& errorMessage) const
{
   if (ImporterShell::validate(pDescriptor, importedDescriptors, errorMessage) == false)
   {
      return false;
   }

   // check signature data descriptor for the number of Units components - there should be one for each column
   // of data and there must be at least two for a valid plot.
   const SignatureDataDescriptor* pDd = dynamic_cast<const SignatureDataDescriptor*>(pDescriptor);
   if (pDd == NULL)
   {
      errorMessage = "Unable to obtain a valid signature data descriptor for the range profile.";
      return false;
   }

   std::set<std::string> unitNames = pDd->getUnitNames();
   if (unitNames.size() < 2)
   {
      errorMessage = "Insufficient data in the file to generate a range profile. "
         "It requires a minimum of two columns of values.";
      return false;
   }

   return true;
}
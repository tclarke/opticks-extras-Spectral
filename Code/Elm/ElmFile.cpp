/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtCore/QFile>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include "ElmFile.h"
#include "MessageLogResource.h"
#include "PlugInResource.h"
#include "Resampler.h"

#include <iomanip>
#include <sstream>
#include <fstream>

using namespace std;
ElmFile::ElmFile(const string& filename, vector<double> centerWavelengths, double** pGainsOffsets) :
   mCenterWavelengths(centerWavelengths),
   mpGainsOffsets(pGainsOffsets),
   mFilename(filename)
{
      // Do nothing
}

ElmFile::~ElmFile()
{
   // Do nothing
}

bool ElmFile::saveResults()
{
   StepResource pStep("Save Gains/Offsets File", "app", "1723A695-125D-42d0-9BC7-BEFE1C52073E");
   VERIFY(pStep.get() != NULL);

   bool fileWritten = false;
   while (fileWritten == false)
   {
      const QString filename = QString::fromStdString(mFilename);
      if ((QFile::exists(filename) == true) &&
         ((QFile::permissions(filename) & QFile::WriteUser) == QFile::WriteUser))
      {
         const QString message = QString("The file \"%1\" already exists.\n"
            "Would you like to overwrite it?").arg(filename);

         if (QMessageBox::question(NULL, "Save ELM Gains/Offsets File", message,
               QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
         {
            mFilename.clear();
         }
      }

      ofstream output(mFilename.c_str());
      if (output.good() == false)
      {
         const QString message = QString("The file \"%1\" is read-only.\n"
            "Do you want to save the Gains/Offsets file with a different name?\n\n"
            "Select \"Yes\" to specify a different file.\n"
            "Select \"No\" to apply the Gains/Offsets without saving.").arg(filename);

         if (QMessageBox::question(NULL, "Save ELM Gains/Offsets File", message,
               QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
         {
            mFilename = QFileDialog::getSaveFileName(NULL, "ELM Gains/Offsets Files", filename).toStdString();
         }
         else
         {
            mFilename.clear();
         }

         if (mFilename.empty() == true)
         {
            pStep->finalize(Message::Failure, "Unable to open/create results file \"" + mFilename + "\".");
            break;
         }
      }
      else
      {
         output.setf(ios::fixed | ios::right);
         output.precision(16);

         const unsigned int numWavelengths = mCenterWavelengths.size();
         for (unsigned int i = 0; i < numWavelengths; ++i)
         {
            output << mCenterWavelengths[i] << "\t" << "\t" <<
               mpGainsOffsets[i][0] << "\t" << mpGainsOffsets[i][1] << "\n";
         }

         output.close();
         fileWritten = true;
         pStep->finalize();
      }
   }

   return fileWritten;
}

bool ElmFile::readResults()
{
   StepResource pStep("Read Gains/Offsets File", "app", "D52A2267-4D43-44c1-A772-A8C6FD130E87");
   VERIFY(pStep.get() != NULL);

   ifstream input(mFilename.c_str());
   if (input.good() == false)
   {
      pStep->finalize(Message::Failure, "Unable to open file \"" + mFilename + "\".");
      return false;
   }

   vector<double> wavelengths;
   vector<double> gains;
   vector<double> offsets;
   while (input.good() == true)
   {
      double wavelength = 0;
      double gain = 0;
      double offset = 0;

      input >> wavelength >> gain >> offset;
      if (input.good() == true)
      {
         wavelengths.push_back(wavelength);
         gains.push_back(gain);
         offsets.push_back(offset);
      }
   }

   input.close();

   PlugInResource pPlugIn("Resampler");
   Resampler* pResampler = dynamic_cast<Resampler*>(pPlugIn.get());
   if (pResampler == NULL)
   {
      pStep->finalize(Message::Failure, "The Resampler plug-in is not available");
      return false;
   }

   string errorMsg;
   vector<int> toBands;
   vector<double> toFwhm;
   vector<double> toGains;
   if (pResampler->execute(gains, toGains, wavelengths, mCenterWavelengths, toFwhm, toBands, errorMsg) == false)
   {
      pStep->finalize(Message::Failure, "Unable to compute Gains.\nResampler reported \"" + errorMsg + "\".");
      return false;
   }

   vector<double> toOffsets;
   if (pResampler->execute(offsets, toOffsets, wavelengths, mCenterWavelengths, toFwhm, toBands, errorMsg) == false)
   {
      pStep->finalize(Message::Failure, "Unable to compute Offsets.\nResampler reported \"" + errorMsg + "\".");
      return false;
   }

   if (toGains.size() == 0)
   {
      pStep->finalize(Message::Failure, "The results vector is empty.");
      return false;
   }

   int width = 1;
   for (unsigned int size = toGains.size(); (size - 1) / 10 > 0; size /= 10)
   {
      width++;
   }

   for (unsigned int bandCount = 0; bandCount < toGains.size(); ++bandCount)
   {
      stringstream name;
      name << "Band ";
      name << setw(width) << setfill('0') << (bandCount + 1);

      stringstream text;
      text << "Gain: " << setprecision(16) << toGains[bandCount];
      text << ", Offset: " << setprecision(16) << toOffsets[bandCount];
      pStep->addProperty(name.str(), text.str());

      mpGainsOffsets[bandCount][0] = toGains[bandCount];
      mpGainsOffsets[bandCount][1] = toOffsets[bandCount];
   }

   pStep->finalize();
   return true;
}

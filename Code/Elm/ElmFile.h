/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef ELMFILE_H
#define ELMFILE_H

class ElmFile
{
public:
   ElmFile(const std::string& filename, std::vector<double> centerWavelengths, double** pGainsOffsets);
   ~ElmFile();

   void setFilename(const std::string& filename);
   bool saveResults();
   bool readResults();

   static const std::string& getExt()
   {
      static std::string ext(".eog");
      return ext;
   }

private:
   std::string mFilename;
   std::vector<double> mCenterWavelengths;
   double** mpGainsOffsets;
};

#endif

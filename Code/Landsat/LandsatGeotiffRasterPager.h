/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef LANDSATGEOTIFFRASTERPAGER_H__
#define LANDSATGEOTIFFRASTERPAGER_H__

#include "LandsatUtilities.h"
#include "CachedPager.h"

#include <string>

class LandsatGeotiffRasterPager : public CachedPager
{
public:
   LandsatGeotiffRasterPager();
   virtual ~LandsatGeotiffRasterPager();

private:
   virtual bool openFile(const std::string& filename);
   virtual CachedPage::UnitPtr fetchUnit(DataRequest* pOriginalRequest);
   void convertToRadiance(unsigned int band, unsigned int numRows,
      unsigned int numCols, char* pOriginalData, char* pData);
   void convertToReflectance(unsigned int band, unsigned int numRows,
      unsigned int numCols, char* pOriginalData, char* pData);
   void convertToTemperature(unsigned int band, unsigned int numRows,
      unsigned int numCols, char* pOriginalData, char* pData);

   std::vector<RasterPager*> mBandPagers;
   Landsat::LandsatDataTypeEnum mDataType;
   Landsat::LandsatImageType mImageType;
   std::vector<std::pair<double, double> > mRadianceFactors;
   std::vector<double> mReflectanceFactors;
   double mK1;
   double mK2;
};

#endif

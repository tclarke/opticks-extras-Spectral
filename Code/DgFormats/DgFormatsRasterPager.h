/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef DGFORMATSRASTERPAGER_H__
#define DGFORMATSRASTERPAGER_H__

#include "CachedPager.h"
#include "DgFileTile.h"
#include "DgUtilities.h"

#include <string>
#include <vector>

class DgFormatsRasterPager : public CachedPager
{
public:
   DgFormatsRasterPager();
   virtual ~DgFormatsRasterPager();

private:
   virtual bool openFile(const std::string& filename);
   virtual CachedPage::UnitPtr fetchUnit(DataRequest* pOriginalRequest);

   std::vector<std::pair<DgFileTile, RasterPager*> > mTilePagers;
   DgUtilities::DgDataType mDataType;
   std::vector<double> mConversionFactors;
};

#endif

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef BANDRESAMPLEPAGER_H__
#define BANDRESAMPLEPAGER_H__

#include "RasterPage.h"
#include "RasterPagerShell.h"
#include "PlugInResource.h"
#include <vector>

class RasterElement;
class RasterFileDescriptor;

class BandResampleRasterPage : public RasterPage
{
public:
   BandResampleRasterPage(void* pData, unsigned int rows, unsigned int columns);
   void* getRawData();
   unsigned int getNumRows();
   unsigned int getNumColumns();
   unsigned int getNumBands();
   unsigned int getInterlineBytes();

protected:
   ~BandResampleRasterPage();
   friend class BandResamplePager;

private:
   void* mpData;
   unsigned int mRow;
   unsigned int mColumn;
   unsigned int mRows;
   unsigned int mColumns;
};

class BandResamplePager : public RasterPagerShell
{
public:
   BandResamplePager();
   ~BandResamplePager();

   bool getInputSpecification(PlugInArgList*& pArgList);
   bool execute(PlugInArgList* pInputArgList, PlugInArgList* pOutputArgList);
   RasterPage* getPage(DataRequest* pOriginalRequest, DimensionDescriptor startRow,
                       DimensionDescriptor startColumn, DimensionDescriptor startBand);
   void releasePage(RasterPage* pPage);
   int getSupportedRequestVersion() const;

private:
   RasterElement* mpElement;
   RasterFileDescriptor* mpFileDescriptor;
   unsigned int mBand;
   unsigned int mRows;
   unsigned int mColumns;
   ExecutableResource mMemoryMappedPagerPlugin;
   RasterPager* mpMemoryMappedPager;
   char* mpRemapData;
};

#endif
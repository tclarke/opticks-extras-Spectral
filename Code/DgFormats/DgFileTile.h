/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef DGFILETILE_H__
#define DGFILETILE_H__

#include "XercesIncludes.h"

#include <string>
#include <vector>

class DgFileTile
{
public:
   DgFileTile() : mStartRow(0), mStartCol(0), mEndRow(0), mEndCol(0) {}
   std::string mTilFilename;
   unsigned int mStartRow;
   unsigned int mStartCol;
   unsigned int mEndRow;
   unsigned int mEndCol;

   static std::vector<DgFileTile> getTiles(XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* pDocument,
      const std::string& filename,
      unsigned int& height,
      unsigned int& width);
};

#endif
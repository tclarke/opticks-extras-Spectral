/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef TAD_H__
#define TAD_H__

#include <string.h>

#include "AlgorithmShell.h"
#include "TypesFile.h"

#include <opencv/cv.h>
#include <QtCore/QList>
#include <QtCore/QPair>

class RasterElement;
class ProgressTracker;


class Tad : public AlgorithmShell
{
public:
   Tad();
   virtual ~Tad();

   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

private:
   RasterElement* createResults(int numRows, int numColumns, int numBands, const std::string& sigName, EncodingType eType, 
      RasterElement* pElement);
   cv::Mat* getSampleOfPixels(unsigned int& sampleSize, RasterElement& element, BitMaskIterator& iter);
};

#endif
/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef MNFINVERSE_H
#define MNFINVERSE_H

#include "AlgorithmShell.h"
#include "ProgressTracker.h"

#include <string>
#include <vector>

class PlugInArgList;
class RasterElement;

class MnfInverse : public AlgorithmShell
{
public:
   MnfInverse();
   ~MnfInverse();

   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

protected:
   virtual bool extractInputArgs(const PlugInArgList* pArgList);
   bool getInfoFromTransformFile(std::string& filename, unsigned int& numBands, unsigned int& numComponents);
   bool readInMnfTransform(const std::string& filename, double** pTransform, std::vector<double>& wavelengths);
   RasterElement* createInverseRaster(std::string name, unsigned int numRows,
      unsigned int numColumns, unsigned int numBands);
   bool computeInverse(RasterElement* pInvRaster, double** pInvTransform,
      unsigned int numBands, unsigned int numComponents);
   bool createInverseView(RasterElement* pInvRaster);

private:
   ProgressTracker mProgress;
   RasterElement* mpRaster;
   std::string mMessage;
   std::string mTransformFilename;
   bool mbDisplayResults;
   unsigned int mNumColumns;
   unsigned int mNumRows;
   unsigned int mNumBands;
};

#endif

/*
 * The information in this file is
 * Copyright(c) 2010 Trevor R.H. Clarke
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RX_H__
#define RX_H__

#include <string>

#include "AlgorithmShell.h"
#include "TypesFile.h"

class RasterElement;

class Rx : public AlgorithmShell
{
public:
   Rx();
   virtual ~Rx();

   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

private:
   RasterElement* createResults(int numRows, int numColumns, int numBands, const std::string& sigName, 
      EncodingType eType, RasterElement* pElement);
   void clearPreviousResults(const std::string& sigName, RasterElement* pElement);
};

#endif
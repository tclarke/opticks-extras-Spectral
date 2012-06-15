/*
 * The information in this file is
 * Copyright(c) 2012 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RESAMPLERPLUGIN_H
#define RESAMPLERPLUGIN_H

#include "AlgorithmShell.h"

#include <string>

class PlugInArgList;
class Signature;
class Wavelengths;

class ResamplerPlugIn : public AlgorithmShell
{
public:
   ResamplerPlugIn();
   virtual ~ResamplerPlugIn();

   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

protected:
   bool getWavelengthsFromElement(const DataElement* pElement, Wavelengths* pWavelengths, std::string& errorMsg);
   bool getWavelengthsFromFile(const std::string& filename, Wavelengths* pWavelengths, std::string& errorMsg);
   bool needToResample(const Signature* pSig, const Wavelengths* pWavelengths);
};

#endif
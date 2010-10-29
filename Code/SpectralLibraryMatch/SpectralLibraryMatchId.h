/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SPECTRALLIBRAYMATCHID_H
#define SPECTRALLIBRAYMATCHID_H

#include "AlgorithmShell.h"
#include "ColorType.h"
#include "Location.h"
#include "TypesFile.h"

#include <string>

class AoiElement;
class PlugInArgList;
class Progress;
class Signature;

class SpectralLibraryMatchId : public AlgorithmShell
{
public:
   SpectralLibraryMatchId();
   virtual ~SpectralLibraryMatchId();

   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

protected:
   void updateProgress(const std::string& msg, int percent, ReportingLevel level);
   bool generatePseudocolorLayer(std::vector<std::pair<Signature*, float> >& bestMatches,
      std::map<Signature*, ColorType>& colorMap, std::vector<std::string>& pixelNames,
      const std::string& layerName);
   Opticks::PixelLocation getLocationFromPixelName(const std::string& pixelName) const;
   EncodingType getSmallestType(unsigned int numClasses) const;

private:
   Progress* mpProgress;
};

#endif

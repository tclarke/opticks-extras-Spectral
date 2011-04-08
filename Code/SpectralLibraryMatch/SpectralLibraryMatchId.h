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
class DataElement;
class PlugInArgList;
class Progress;
class RasterDataDescriptor;
class Signature;
class SignatureSet;
class SpectralLibraryManager;
class SpectralLibraryMatchResults;
class Step;

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
   bool loadLibraryFromDataElement(SpectralLibraryManager* pLibMgr, const DataElement* pSignatureData);
   bool generatePseudocolorLayer(std::vector<std::pair<Signature*, float> >& bestMatches,
      std::map<Signature*, ColorType>& colorMap, std::vector<std::string>& pixelNames,
      const std::string& layerName);
   Opticks::PixelLocation getLocationFromPixelName(const std::string& pixelName,
      const RasterDataDescriptor* pDesc) const;
   EncodingType getSmallestType(unsigned int numClasses) const;
   bool outputResults(std::vector<SpectralLibraryMatch::MatchResults>& theResults,
      SpectralLibraryMatch::MatchLimits& limits, const std::map<Signature*, ColorType>& colorMap);
   bool writeResultsToFile(std::vector<SpectralLibraryMatch::MatchResults>& theResults,
      SpectralLibraryMatch::MatchLimits& limits, const std::string& filename);
   bool sendResultsToWindow(std::vector<SpectralLibraryMatch::MatchResults>& theResults,
      const std::map<Signature*, ColorType>& colorMap);

private:
   Progress* mpProgress;
   Step* mpStep;
   SpectralLibraryMatchResults* mpResultsWindow;
   std::string mMatchResultsFilename;
};

#endif

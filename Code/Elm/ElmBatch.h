/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef ELMBATCH_H
#define ELMBATCH_H

#include "AlgorithmShell.h"
#include "ElmCore.h"

class DataElement;
class Filename;

class ElmBatch : public AlgorithmShell, public ElmCore
{
public:
   ElmBatch();
   ~ElmBatch();

   bool setInteractive();
   bool getInputSpecification(PlugInArgList*& pArgList);
   bool getOutputSpecification(PlugInArgList*& pArgList);


   bool execute(PlugInArgList* pInputArgList, PlugInArgList* pOutputArgList);

protected:
   bool extractInputArgs(PlugInArgList* pInputArgList);

private:
   DataElement* getElement(const Filename* pFilename,
      const std::string& type, DataElement* pParent, bool& previouslyLoaded);
   static std::string UseGainsOffsetsArg() { return "Use Existing Gains/Offsets File"; }
   static std::string GainsOffsetsFilenameArg() { return "Existing Gains/Offsets Filename"; }
   static std::string SignatureFilenamesArg() { return "Signature Filenames"; }
   static std::string AoiFilenamesArg() { return "AOI Filenames"; }

   bool mUseGainsOffsets;
   std::string mGainsOffsetsFilename;
   std::vector<Signature*> mpSignatures;
   std::vector<Signature*> mpSignaturesToDestroy;
   std::vector<AoiElement*> mpAoiElements;
   std::vector<AoiElement*> mpAoiElementsToDestroy;
};

#endif

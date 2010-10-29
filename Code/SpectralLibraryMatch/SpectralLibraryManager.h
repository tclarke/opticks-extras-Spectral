/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SPECTRALLIBRARYMANAGER_H
#define SPECTRALLIBRARYMANAGER_H

#include "ExecutableShell.h"

#include <boost/any.hpp>
#include <map>
#include <vector>

#include <QtCore/QObject>

class PlugInArgList;
class Progress;
class QAction;
class RasterElement;
class SessionItemDeserializer;
class SessionItemSerializer;
class Signature;
class Subject;
class Wavelengths;

class SpectralLibraryManager : public QObject, public ExecutableShell
{
   Q_OBJECT

public:
   SpectralLibraryManager();
   ~SpectralLibraryManager();

   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
   virtual bool setBatch();

   bool isEmpty() const;
   unsigned int size() const;
   bool addSignatures(const std::vector<Signature*>& signatures);
   bool removeSignatures(const std::vector<Signature*>& signatures);
   void clearLibrary();
   const RasterElement* getLibraryData(const RasterElement* pRaster);
   Signature* getLibrarySignature(unsigned int index);
   int getSignatureIndex(const Signature* pSignature) const;
   bool getResampledSignatureValues(const RasterElement* pRaster, const Signature* pSignature,
      std::vector<double>& values);
   const std::vector<Signature*>& getLibrarySignatures() const;

   bool serialize(SessionItemSerializer& serializer) const;
   bool deserialize(SessionItemDeserializer& deserializer);

public slots:
   bool editSpectralLibrary();

protected:
   bool generateResampledLibrary(const RasterElement* pRaster);
   bool resampleSigsToLib(RasterElement* pLib, const RasterElement* pRaster);
   void elementDeleted(Subject& subject, const std::string& signal, const boost::any& value);
   void resampledElementDeleted(Subject& subject, const std::string& signal, const boost::any& value);
   void signatureDeleted(Subject& subject, const std::string& signal, const boost::any& value);
   void invalidateLibraries();

private:
   std::vector<Signature*> mSignatures;
   UnitType mLibraryUnitType;
   std::map<const RasterElement*, RasterElement*> mLibraries;
   Progress* mpProgress;
   QAction* mpEditSpectralLibraryAction;
};

#endif

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
#include "SubjectAdapter.h"

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

class SpectralLibraryManager : public QObject, public ExecutableShell, public SubjectAdapter
{
   Q_OBJECT

public:
   /**
    * Emitted with boost::any<\link Signature\endlink*> when a signature in the library is deleted from Model.
    */
   SIGNAL_METHOD(SpectralLibraryManager, SignatureDeleted)

   SpectralLibraryManager();
   ~SpectralLibraryManager();

   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
   virtual bool setBatch();
   virtual const std::string& getObjectType() const;
   virtual bool isKindOf(const std::string& className) const;
   bool isEmpty() const;
   unsigned int size() const;
   bool addSignatures(const std::vector<Signature*>& signatures);
   void clearLibrary();
   const RasterElement* getResampledLibraryData(const RasterElement* pRaster);
   const std::vector<Signature*>* getResampledLibrarySignatures(const RasterElement* pResampledLib) const;
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
   void elementDeleted(Subject& subject, const std::string& signal, const boost::any& value);
   void resampledElementDeleted(Subject& subject, const std::string& signal, const boost::any& value);
   void signatureDeleted(Subject& subject, const std::string& signal, const boost::any& value);
   void invalidateLibraries();

private:
   std::vector<Signature*> mSignatures;
   UnitType mLibraryUnitType;
   std::map<const RasterElement*, RasterElement*> mLibraries;
   std::map<const RasterElement*, std::vector<Signature*> > mResampledSignatures;
   Progress* mpProgress;
   QAction* mpEditSpectralLibraryAction;
};

#endif

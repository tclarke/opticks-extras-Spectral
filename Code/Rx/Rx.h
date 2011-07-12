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

#include <QtCore/QtConcurrentMap>

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

// This subclass of QtConcurrent::Exception is required to do proper exception
// propagation from within threads managed by QtConcurrent
class CvExceptionWrapper : public QtConcurrent::Exception
{
public:
   CvExceptionWrapper()
   {
   }

   CvExceptionWrapper(int errCode)
   {
      setExceptionInfo(errCode);
   }

   CvExceptionWrapper(const CvExceptionWrapper &rhs)
   {
      mErrString = rhs.errorString();
   }

   Exception* clone() const
   {
      return new CvExceptionWrapper(*this);
   }

   void raise() const
   {
      throw *this;
   }

   void setExceptionInfo(int errCode)
   {
      switch (errCode)
      {
      case -4:
         mErrString = "Out of memory.";
         break;
      default:
         mErrString = "Unknown error. ";
         break;
      }
   }

   std::string errorString() const
   {
      return mErrString;
   }

private:
   std::string mErrString;
};

#endif
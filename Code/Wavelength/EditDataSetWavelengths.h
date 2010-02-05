/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef EDITDATASETWAVELENGTHS_H
#define EDITDATASETWAVELENGTHS_H

#include <QtCore/QObject>

#include "AttachmentPtr.h"
#include "ExecutableShell.h"
#include "SessionExplorer.h"

class EditDataSetWavelengths : public QObject, public ExecutableShell
{
   Q_OBJECT

public:
   EditDataSetWavelengths();
   ~EditDataSetWavelengths();

   bool setBatch();
   bool getInputSpecification(PlugInArgList*& pArgList);
   bool getOutputSpecification(PlugInArgList*& pArgList);
   bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

protected:
   void updateContextMenu(Subject& subject, const std::string& signal, const boost::any& value);

protected slots:
   void editWavelengths();

private:
   AttachmentPtr<SessionExplorer> mpExplorer;
};

#endif

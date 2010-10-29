/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef LIBRARYEDITDLG_H
#define LIBRARYEDITDLG_H

#include "SpectralLibraryMatch.h"

#include <vector>

#include <QtCore/QMetaType>
#include <QtGui/QDialog>

class QTreeWidget;
class Signature;

Q_DECLARE_METATYPE(Signature*)

class LibraryEditDlg : public QDialog
{
   Q_OBJECT

public:
   LibraryEditDlg(const std::vector<Signature*>& signatures, QWidget* pParent = 0);
   virtual ~LibraryEditDlg();

   std::vector<Signature*> getSignatures() const;

protected:
   void addSignatures(const std::vector<Signature*>& signatures);

protected slots:
   void addSignatures();
   void removeSignatures();

private:
   QTreeWidget* mpTree;
};

#endif


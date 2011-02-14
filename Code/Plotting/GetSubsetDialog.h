/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef GETSUBSETDIALOG_H__
#define GETSUBSETDIALOG_H__

#include <QtGui/QDialog>
#include <vector>

class QComboBox;
class QListWidget;
class QStringList;

class GetSubsetDialog : public QDialog
{
   Q_OBJECT

public:
   GetSubsetDialog(const QStringList& aoiNames,
                   const QStringList& bands,
                   const std::vector<unsigned int>& defaultSelection,
                   QWidget* pParent = NULL);
   virtual ~GetSubsetDialog();

   QString getSelectedAoi() const;
   void setSelectedAoi(const std::string& aoiName);
   std::vector<unsigned int> getBandSelectionIndices() const;

private:
   QComboBox* mpAoiSelect;
   QListWidget* mpBandSelect;
};

#endif
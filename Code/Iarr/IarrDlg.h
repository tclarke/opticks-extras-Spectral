/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef IARRDLG_H
#define IARRDLG_H

#include <QtGui/QDialog>

#include "TypesFile.h"

#include <string>
#include <vector>

class FileBrowser;
class QComboBox;
class QRadioButton;
class QSpinBox;

class IarrDlg : public QDialog
{
   Q_OBJECT

public:
   IarrDlg(const std::string& defaultFilename, const std::vector<std::string>& aoiNames,
      int maxRowStepFactor, int maxColumnStepFactor, bool isDouble, bool inMemory, QWidget* pParent = NULL);
   ~IarrDlg();

   QString getInputFilename() const;
   QString getAoiName() const;
   unsigned int getRowStepFactor() const;
   unsigned int getColumnStepFactor() const;
   EncodingType getOutputDataType() const;
   ProcessingLocation getProcessingLocation() const;
   QString getOutputFilename() const;

public slots:
   void accept();

private:
   QRadioButton* mpInputFileRadio;
   FileBrowser* mpInputFileBrowser;

   QRadioButton* mpAoiRadio;
   QComboBox* mpAoiCombo;

   QRadioButton* mpFullExtentsRadio;
   QSpinBox* mpRowStepFactor;
   QSpinBox* mpColumnStepFactor;

   QComboBox* mpOutputDataTypeCombo;
   QComboBox* mpProcessingLocationCombo;
   FileBrowser* mpOutputFileBrowser;
};

#endif

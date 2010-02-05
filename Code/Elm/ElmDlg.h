/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef ELMDLG_H
#define ELMDLG_H

#include <QtGui/QDialog>

#include "AoiElement.h"
#include "AttachmentPtr.h"
#include "ElmElement.h"

class QComboBox;
class QLineEdit;
class QListWidget;
class QRadioButton;

class ElmInteractive;
class FileBrowser;
class SpatialDataView;

class ElmDlg : public QDialog
{ 
   Q_OBJECT

public:
   ElmDlg(SpatialDataView* pView, ElmInteractive* pElmInteractive = NULL, QWidget* pParent = NULL);
   virtual ~ElmDlg();

   void viewDeleted(Subject& subject, const std::string& signal, const boost::any& value);

   void aoiModified(Subject& subject, const std::string& signal, const boost::any& value);
   void aoiDeleted(Subject& subject, const std::string& signal, const boost::any& value);

private slots:
   void currentIndexChanged(int index);
   void newElement();
   void deleteElement();
   void accept();
   void deletePixels();
   void selectSignature();
   void help();

private:
   const LocationType mPixelOffset;
   const unsigned int mMaxDisplayedPixels;
   int mCurrentIndex;
   void setCurrentIndex(int index);

   QString mGainsOffsetsFilename;
   std::vector<ElmElement*> mElements;
   ElmElement* getCurrentElement();

   void updatePixelList();
   void refresh();

   QComboBox* mpElementComboBox;
   QLineEdit* mpSignature;
   QListWidget* mpPixelList;
   QRadioButton* mpUseExistingFileRadio;
   QRadioButton* mpCalculateRadio;

   ElmInteractive* const mpElmInteractive;
   FileBrowser* mpExistingFileBrowser;
   AttachmentPtr<SpatialDataView> mpView;
   AttachmentPtr<AoiElement> mpAoiElement;
};

#endif

/*
 * The information in this file is
 * Copyright(c) 2012 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RESAMPLERPLUGINDLG_H
#define RESAMPLERPLUGINDLG_H

#include <QtCore/QMetaType>
#include <QtGui/QDialog>

#include <string>
#include <vector>

class DataElement;
class FileBrowser;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QRadioButton;
class QString;
class QTreeWidget;
class Signature;

Q_DECLARE_METATYPE(DataElement*)
Q_DECLARE_METATYPE(Signature*)

class ResamplerPlugInDlg : public QDialog
{
   Q_OBJECT

public:
   ResamplerPlugInDlg(QWidget* pParent = NULL);
   virtual ~ResamplerPlugInDlg();

   std::vector<Signature*> getSignaturesToResample() const;
   const DataElement* getWavelengthsElement() const;
   std::string getWavelengthsFilename() const;
   std::string getResamplingMethod() const;
   double getDropOutWindow() const;
   double getFWHM() const;
   bool getUseFillValue() const;
   double getFillValue() const;

public slots:
   void accept();

protected:
   void initialize();

protected slots:
   void addSignatures();
   void removeSignatures();
   void clearAllSignatures();
   void checkValidWaveFile(const QString& filename);
   void methodChanged(const QString& methodName);

private:
   QTreeWidget* mpSignatures;
   QRadioButton* mpUseDataSource;
   QComboBox* mpWavelengthsElement;
   QRadioButton* mpUseFileSource;
   FileBrowser* mpWavelengthsFilename;
   QComboBox* mpResampleMethod;
   QDoubleSpinBox* mpDropOutWindow;
   QDoubleSpinBox* mpFwhm;
   QCheckBox* mpUseFillValue;
   QDoubleSpinBox* mpFillValue;
};

#endif
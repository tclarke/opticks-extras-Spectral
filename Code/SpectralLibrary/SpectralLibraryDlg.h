/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SPECTRALLIBRARYDLG_H
#define SPECTRALLIBRARYDLG_H

#include <QtCore/QStringList>
#include <QtGui/QDialog>
#include <QtGui/QPushButton>
#include <QtGui/QTreeWidgetItem>

#include <boost/any.hpp>

#include <map>
#include <vector>

class CustomTreeWidget;
class Progress;
class Signature;
class SignatureSelector;
class SignatureSet;
class Step;
class Subject;

class SpectralLibraryDlg : public QDialog
{
   Q_OBJECT

public:
   SpectralLibraryDlg(Progress* pProgress, Step* pStep, QWidget* pParent = NULL);
   ~SpectralLibraryDlg();

   void abortSearch();

public slots:
   void accept();
   void reject();

protected:
   bool addLibrary(SignatureSet* pSignatureSet);
   bool addSignature(Signature* pSignature);
   bool removeLibrary(SignatureSet* pSignatureSet);
   bool removeSignature(Signature* pSignature);
   SignatureSet* getSelectedLibrary() const;
   Signature* getSignature(QTreeWidgetItem* pItem);
   bool saveSignatures(const std::vector<Signature*>& signatures, const QStringList& sigFilenames,
      const QString& strExporter);
   QString getSignatureExporterName() const;

   void updateLibraryName(Subject& subject, const std::string& signal, const boost::any& value);
   void updateSignatureName(Subject& subject, const std::string& signal, const boost::any& value);
   void removeLibraryItem(Subject& subject, const std::string& signal, const boost::any& value);
   void removeSignatureItem(Subject& subject, const std::string& signal, const boost::any& value);

protected slots:
   void createLibrary();
   void deleteLibrary();
   void loadLibrary();
   void saveLibrary();
   void help();

   void enableSignatureButtons();
   void addSignature();
   void removeSignature();
   void saveSignature();

   void updateSignatureList();
   void updateSignatureData(QTreeWidgetItem* pItem, int iColumn);

private:
   Progress* mpProgress;
   Step* mpStep;

   CustomTreeWidget* mpLibraryTree;
   CustomTreeWidget* mpSignatureTree;
   QPushButton* mpDeleteLibButton;
   QPushButton* mpSaveLibButton;
   QPushButton* mpAddSigButton;
   QPushButton* mpRemoveSigButton;
   QPushButton* mpSaveSigButton;

   std::map<QTreeWidgetItem*, SignatureSet*> mLibraries;
   std::map<QTreeWidgetItem*, Signature*> mSignatures;

   SignatureSelector* mpSigSelector;
};

#endif

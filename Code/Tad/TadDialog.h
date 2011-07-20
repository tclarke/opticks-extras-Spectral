/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef TADDIALOG_H__
#define TADDIALOG_H__

#include "ConfigurationSettings.h"
#include "ui_TadDialog.h"
#include <QtGui/QDialog>

class TadDialog : public QDialog, private Ui_TadDialog
{
   Q_OBJECT

public:
   TadDialog(QWidget* pParent = NULL);
   virtual ~TadDialog();

   void setAoiList(const QList<QPair<QString, QString> >& aois);
   void setPercentBackground(double threshold);
   void setComponentSize(double threshold);
   void setAoi(const QString& sessionId);
   void setSampleSize(unsigned int size);

   double getPercentBackground() const;
   double getComponentSize() const;
   QString getAoi() const;
   unsigned int getSampleSize() const;
};

#endif
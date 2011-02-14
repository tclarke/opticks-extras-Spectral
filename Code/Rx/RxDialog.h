/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RXDIALOG_H__
#define RXDIALOG_H__

#include "ConfigurationSettings.h"
#include "ui_RxDialog.h"
#include <QtGui/QDialog>

class RxDialog : public QDialog, private Ui_RxDialog
{
   Q_OBJECT

public:
   RxDialog(QWidget* pParent = NULL);
   virtual ~RxDialog();

   void setAoiList(const QList<QPair<QString, QString> >& aois);
   void setThreshold(double threshold);
   void setAoi(const QString& sessionId);
   void setLocal(bool enabled);
   void setLocalSize(unsigned int width, unsigned int height);
   void setSubspace(bool enabled);
   void setSubspaceComponents(unsigned int components);

   double getThreshold() const;
   QString getAoi() const;
   bool isLocal() const;
   void getLocalSize(unsigned int& width, unsigned int& height) const;
   bool isSubspace() const;
   unsigned int getSubspaceComponents() const;
};

#endif
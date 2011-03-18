/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef NDVIDLG_H
#define NDVIDLG_H

#include <QtGui/QDialog>
#include <QtGui/QTableWidget>

class RasterDataDescriptor;

class NdviDlg : public QDialog
{
   Q_OBJECT

public:
   NdviDlg(const RasterDataDescriptor* pDataDescriptor, QWidget* pParent);
   virtual ~NdviDlg();

   int getBand() const;

public slots:
   virtual void accept();

private:
   QTableWidget* mpBandTable;
};

#endif
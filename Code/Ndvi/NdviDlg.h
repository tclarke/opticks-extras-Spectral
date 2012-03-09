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

#include <QtGui/QCheckBox>
#include <QtGui/QDialog>
#include <QtGui/QTableWidget>

class DimensionDescriptor;
class RasterDataDescriptor;
class Wavelengths;

class NdviDlg : public QDialog
{
   Q_OBJECT

public:
   NdviDlg(const RasterDataDescriptor* pDataDescriptor, const double redBandLow,
      const double redBandHigh, const double nirBandLow, const double nirBandHigh,
      DimensionDescriptor redBandDD, DimensionDescriptor nirBandDD, QWidget* pParent);
   virtual ~NdviDlg();

   unsigned int getRedBand() const;
   unsigned int getNirBand() const;
   bool getOverlay() const;

public slots:
   virtual void accept();

private:
   QTableWidget* mpRedBandTable;
   QTableWidget* mpNirBandTable;
   QCheckBox* mpOverlay;
   QTableWidget* createDataTable(const RasterDataDescriptor* pDataDescriptor, Wavelengths* pWavelengths);
};

#endif
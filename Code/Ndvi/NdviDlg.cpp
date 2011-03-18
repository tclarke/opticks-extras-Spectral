/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "NdviDlg.h"
#include "ObjectResource.h"
#include "RasterDataDescriptor.h"
#include "RasterUtilities.h"
#include "Wavelengths.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QHeaderView>
#include <QtGui/QMessageBox>
#include <QtGui/QVBoxLayout>

NdviDlg::NdviDlg(const RasterDataDescriptor* pDataDescriptor, QWidget* pParent) : QDialog(pParent)
{
   VERIFYNRV(pDataDescriptor != NULL);

   // Get all of the wavelengths from the metadata
   FactoryResource<Wavelengths> pWavelengthResource;
   pWavelengthResource->initializeFromDynamicObject(pDataDescriptor->getMetadata(), true);

   QVBoxLayout* pBox = new QVBoxLayout(this);
   pBox->setMargin(10);
   pBox->setSpacing(5);

   // Initialize the data table.
   mpBandTable = new QTableWidget(pDataDescriptor->getBandCount(), 4, this);
   mpBandTable->setSortingEnabled(false);
   mpBandTable->setSelectionBehavior(QAbstractItemView::SelectRows);
   mpBandTable->setSelectionMode(QAbstractItemView::SingleSelection);

   mpBandTable->verticalHeader()->hide();
   mpBandTable->verticalHeader()->setDefaultSectionSize(20);
   QStringList horizontalHeaderLabels(QStringList() << "Band" << "Start" << "Center" << "End");
   mpBandTable->setHorizontalHeaderLabels(horizontalHeaderLabels);
   mpBandTable->horizontalHeader()->setDefaultSectionSize(85);

   std::vector<std::string> bandNames = RasterUtilities::getBandNames(pDataDescriptor);
   VERIFYNRV(bandNames.size() == pDataDescriptor->getBandCount());

   bool hasStartValues = pWavelengthResource->hasStartValues();
   bool hasCenterValues = pWavelengthResource->hasCenterValues();
   bool hasEndValues = pWavelengthResource->hasEndValues();
   // Populate the table with band values.
   for (unsigned int i = 0; i < pDataDescriptor->getBandCount(); ++i)
   {
      QTableWidgetItem* pTableWidget;

      pTableWidget = new QTableWidgetItem(QString::fromStdString(bandNames[i]));
      pTableWidget->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
      mpBandTable->setItem(i, 0, pTableWidget);

      if (hasStartValues && i < pWavelengthResource->getStartValues().size())
      {
         pTableWidget = new QTableWidgetItem(QString::number(pWavelengthResource->getStartValues()[i]));
      }
      else
      {
         pTableWidget = new QTableWidgetItem(QString::number(0));
      }
      pTableWidget->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
      mpBandTable->setItem(i, 1, pTableWidget);

      if (hasCenterValues && i < pWavelengthResource->getCenterValues().size())
      {
         pTableWidget = new QTableWidgetItem(QString::number(pWavelengthResource->getCenterValues()[i]));
      }
      else
      {
         pTableWidget = new QTableWidgetItem(QString::number(0));
      }
      pTableWidget->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
      mpBandTable->setItem(i, 2, pTableWidget);

      if (hasEndValues && i < pWavelengthResource->getEndValues().size())
      {
         pTableWidget = new QTableWidgetItem(QString::number(pWavelengthResource->getEndValues()[i]));
      }
      else
      {
         pTableWidget = new QTableWidgetItem(QString::number(0));
      }
      pTableWidget->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
      mpBandTable->setItem(i, 3, pTableWidget);
   }
   mpBandTable->setCurrentCell(-1, -1);
   mpBandTable->setMinimumWidth(350);
   pBox->addWidget(mpBandTable);

   // OK and Cancel buttons
   QDialogButtonBox* pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, 
      Qt::Horizontal, this);
   VERIFYNR(connect(pButtonBox, SIGNAL(accepted()), this, SLOT(accept())));
   VERIFYNR(connect(pButtonBox, SIGNAL(rejected()), this, SLOT(reject())));

   pBox->addWidget(pButtonBox);
}

NdviDlg::~NdviDlg()
{}

int NdviDlg::getBand() const
{
   return mpBandTable->currentRow();
}

void NdviDlg::accept()
{
   if (mpBandTable->currentRow() >= 0)
   {
      QDialog::accept();
   }
   else
   {
      QMessageBox::warning(this, "NDVI", "No band selected.  Please select a band from the list.");
   }
}
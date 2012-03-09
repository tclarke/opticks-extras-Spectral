/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "DimensionDescriptor.h"
#include "NdviDlg.h"
#include "ObjectResource.h"
#include "RasterDataDescriptor.h"
#include "RasterUtilities.h"
#include "Wavelengths.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QGridLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QMessageBox>

NdviDlg::NdviDlg(const RasterDataDescriptor* pDataDescriptor, const double redBandLow, const double redBandHigh,
   const double nirBandLow, const double nirBandHigh, DimensionDescriptor redBandDD, DimensionDescriptor nirBandDD,
   QWidget* pParent) : QDialog(pParent)
{
   VERIFYNRV(pDataDescriptor != NULL);

   setWindowTitle("NDVI");

   // Get all of the wavelengths from the metadata
   FactoryResource<Wavelengths> pWavelengthResource;
   pWavelengthResource->initializeFromDynamicObject(pDataDescriptor->getMetadata(), true);

   QGridLayout* pBox = new QGridLayout(this);
   pBox->setMargin(10);
   pBox->setSpacing(5);

   QLabel* pRedLabel = new QLabel(QString("Select Red Band (%1 - %2):")
      .arg(Wavelengths::convertValue(redBandLow, MICRONS, pWavelengthResource->getUnits()))
      .arg(Wavelengths::convertValue(redBandHigh, MICRONS, pWavelengthResource->getUnits())),
      this);
   pBox->addWidget(pRedLabel, 0, 0);

   QLabel* pNirLabel = new QLabel(QString("Select NIR Band (%1 - %2):")
      .arg(Wavelengths::convertValue(nirBandLow, MICRONS, pWavelengthResource->getUnits()))
      .arg(Wavelengths::convertValue(nirBandHigh, MICRONS, pWavelengthResource->getUnits())),
      this);
   pBox->addWidget(pNirLabel, 0, 2);

   // Initialize the data tables
   VERIFYNRV(mpRedBandTable = createDataTable(pDataDescriptor, pWavelengthResource.get()));
   VERIFYNRV(mpNirBandTable = createDataTable(pDataDescriptor, pWavelengthResource.get()));

   if (redBandDD.isValid())
   {
      mpRedBandTable->setCurrentCell(redBandDD.getActiveNumber(), 2);
   }

   if (nirBandDD.isValid())
   {
      mpNirBandTable->setCurrentCell(nirBandDD.getActiveNumber(), 2);
   }

   pBox->addWidget(mpRedBandTable, 1, 0, 1, 2);
   pBox->addWidget(mpNirBandTable, 1, 2, 1, 2);

   //Separator line
   QFrame* pLine = new QFrame(this);
   pLine->setFrameStyle(QFrame::HLine | QFrame::Sunken);
   pBox->addWidget(pLine, 2, 0, 1, 4, Qt::AlignBottom);
   pBox->setRowMinimumHeight(2, 10);

   //Overlay checkbox
   mpOverlay = new QCheckBox("Overlay Results", this);
   pBox->addWidget(mpOverlay, 3, 0);

   // OK and Cancel buttons
   QDialogButtonBox* pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, 
      Qt::Horizontal, this);
   VERIFYNR(connect(pButtonBox, SIGNAL(accepted()), this, SLOT(accept())));
   VERIFYNR(connect(pButtonBox, SIGNAL(rejected()), this, SLOT(reject())));

   pBox->addWidget(pButtonBox, 3, 3, Qt::AlignRight);
}

NdviDlg::~NdviDlg()
{}

QTableWidget* NdviDlg::createDataTable(const RasterDataDescriptor* pDataDescriptor, Wavelengths* pWavelengths)
{
   QTableWidget* pBandTable = new QTableWidget(pDataDescriptor->getBandCount(), 4, this);
   pBandTable->setSortingEnabled(false);
   pBandTable->setSelectionBehavior(QAbstractItemView::SelectRows);
   pBandTable->setSelectionMode(QAbstractItemView::SingleSelection);

   pBandTable->verticalHeader()->hide();
   pBandTable->verticalHeader()->setDefaultSectionSize(20);
   QStringList horizontalHeaderLabels(QStringList() << "Band" << "Start" << "Center" << "End");
   pBandTable->setHorizontalHeaderLabels(horizontalHeaderLabels);
   pBandTable->horizontalHeader()->setDefaultSectionSize(85);

   std::vector<std::string> bandNames = RasterUtilities::getBandNames(pDataDescriptor);
   VERIFYRV(bandNames.size() == pDataDescriptor->getBandCount(), NULL);

   bool hasStartValues = pWavelengths->hasStartValues();
   bool hasCenterValues = pWavelengths->hasCenterValues();
   bool hasEndValues = pWavelengths->hasEndValues();
   // Populate the table with band values.
   for (unsigned int i = 0; i < pDataDescriptor->getBandCount(); ++i)
   {
      QTableWidgetItem* pTableWidget;

      pTableWidget = new QTableWidgetItem(QString::fromStdString(bandNames[i]));
      pTableWidget->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
      pBandTable->setItem(i, 0, pTableWidget);

      if (hasStartValues && i < pWavelengths->getStartValues().size())
      {
         pTableWidget = new QTableWidgetItem(QString::number(pWavelengths->getStartValues()[i]));
      }
      else
      {
         pTableWidget = new QTableWidgetItem(QString::number(0));
      }
      pTableWidget->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
      pBandTable->setItem(i, 1, pTableWidget);

      if (hasCenterValues && i < pWavelengths->getCenterValues().size())
      {
         pTableWidget = new QTableWidgetItem(QString::number(pWavelengths->getCenterValues()[i]));
      }
      else
      {
         pTableWidget = new QTableWidgetItem(QString::number(0));
      }
      pTableWidget->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
      pBandTable->setItem(i, 2, pTableWidget);

      if (hasEndValues && i < pWavelengths->getEndValues().size())
      {
         pTableWidget = new QTableWidgetItem(QString::number(pWavelengths->getEndValues()[i]));
      }
      else
      {
         pTableWidget = new QTableWidgetItem(QString::number(0));
      }
      pTableWidget->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
      pBandTable->setItem(i, 3, pTableWidget);
   }
   pBandTable->setCurrentCell(-1, -1);
   pBandTable->setMinimumWidth(359);

   return pBandTable;
}

unsigned int NdviDlg::getRedBand() const
{
   int row = mpRedBandTable->currentRow();
   VERIFYRV(row >= 0, 0);
   return static_cast<unsigned int>(row);
}

unsigned int NdviDlg::getNirBand() const
{
   int row = mpNirBandTable->currentRow();
   VERIFYRV(row >= 0, 0);
   return static_cast<unsigned int>(row);
}

bool NdviDlg::getOverlay() const
{
   return mpOverlay->isChecked();
}

void NdviDlg::accept()
{
   if ((mpRedBandTable->currentRow() >= 0) && (mpNirBandTable->currentRow() >= 0))
   {
      QDialog::accept();
   }
   else
   {
      if (mpRedBandTable->currentRow() < 0)
      {
         QMessageBox::warning(this, windowTitle(), "No red band selected.  Please select a red band from the list.");
      }
      else
      {
         QMessageBox::warning(this, windowTitle(), "No NIR band selected.  Please select a NIR band from the list.");
      }
   }
}
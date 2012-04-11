/*
 * The information in this file is
 * Copyright(c) 2012 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtCore/QString>
#include <QtGui/QCheckBox>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QFrame>
#include <QtGui/QGridLayout>
#include <QtGui/QLabel>
#include <QtGui/QMessageBox>
#include <QtGui/QPushButton>
#include <QtGui/QDoubleSpinBox>
#include <QtGui/QSpinBox>

#include "AppVerify.h"
#include "KMeansDlg.h"

#include <limits>

KMeansDlg::KMeansDlg(double threshold, double convergenceThreshold, unsigned int maxIterations,
   unsigned int clusterCount, bool selectSignatures, bool keepIntermediateResults, QWidget* pParent) :
   QDialog(pParent)
{
   setModal(true);
   setWindowTitle("K-Means");

   QLabel* pThresholdLabel = new QLabel("SAM Threshold", this);
   pThresholdLabel->setToolTip("This threshold wil be used for each run of the SAM algorithm.");
   mpThreshold = new QDoubleSpinBox(this);
   mpThreshold->setValue(threshold);
   mpThreshold->setDecimals(5);
   mpThreshold->setMinimum(0.0);
   mpThreshold->setMaximum(180.0);
   mpThreshold->setToolTip(pThresholdLabel->toolTip());

   QLabel* pConvergenceThresholdLabel = new QLabel("Convergence Threshold", this);
   pConvergenceThresholdLabel->setToolTip("This is the minimum percent of pixels which can change groups while still "
      "allowing the algorithm to converge. This setting is provided to prevent infinite looping.");
   mpConvergenceThreshold = new QDoubleSpinBox(this);
   mpConvergenceThreshold->setValue(convergenceThreshold);
   mpConvergenceThreshold->setDecimals(5);
   mpConvergenceThreshold->setMinimum(0.0);
   mpConvergenceThreshold->setMaximum(1.0);
   mpConvergenceThreshold->setSingleStep(0.01);
   mpConvergenceThreshold->setToolTip(pConvergenceThresholdLabel->toolTip());

   QLabel* pMaxIterationsLabel = new QLabel("Max Iterations", this);
   pMaxIterationsLabel->setToolTip("Determines how many iterations are allowed before terminating the algorithm. "
      "Setting this value to 0 forces the algorithm to run until convergence (which may never occur).");
   mpMaxIterations = new QSpinBox(this);
   mpMaxIterations->setValue(maxIterations);
   mpMaxIterations->setMinimum(0);
   mpMaxIterations->setMaximum(std::numeric_limits<int>::max());
   mpMaxIterations->setToolTip(pMaxIterationsLabel->toolTip());

   QLabel* pClusterCountLabel = new QLabel("Cluster Count", this);
   pClusterCountLabel->setToolTip("Determines how many clusters should be created from random points in the data. "
      "This will be in addition to selected signatures if \"Select Signatures\" is checked.");
   mpClusterCount = new QSpinBox(this);
   mpClusterCount->setValue(clusterCount);
   mpClusterCount->setMinimum(0);
   mpClusterCount->setMaximum(std::numeric_limits<int>::max());
   mpClusterCount->setToolTip(pClusterCountLabel->toolTip());

   mpSelectSignatures = new QCheckBox("Select Signatures", this);
   mpSelectSignatures->setChecked(selectSignatures);
   mpSelectSignatures->setToolTip("Determines whether to select signatures to use. "
      "Any signatures selected will be in addition to \"Cluster Count\".");

   mpKeepIntermediateResults = new QCheckBox("Keep Intermediate Results", this);
   mpKeepIntermediateResults->setChecked(keepIntermediateResults);
   mpKeepIntermediateResults->setToolTip("Determines whether to keep or discard intermediate results.");

   QFrame* pLine = new QFrame(this);
   pLine->setFrameStyle(QFrame::HLine | QFrame::Sunken);
   QDialogButtonBox* pButtonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);

   // Layout Begin
   QGridLayout* pLayout = new QGridLayout(this);
   pLayout->addWidget(pThresholdLabel, 0, 0);
   pLayout->addWidget(mpThreshold, 0, 1);
   pLayout->addWidget(pConvergenceThresholdLabel, 1, 0);
   pLayout->addWidget(mpConvergenceThreshold, 1, 1);
   pLayout->addWidget(pMaxIterationsLabel, 2, 0);
   pLayout->addWidget(mpMaxIterations, 2, 1);
   pLayout->addWidget(pClusterCountLabel, 3, 0);
   pLayout->addWidget(mpClusterCount, 3, 1);
   pLayout->addWidget(mpSelectSignatures, 4, 0, 1, 2);
   pLayout->addWidget(mpKeepIntermediateResults, 5, 0, 1, 2);
   pLayout->addWidget(pLine, 6, 0, 1, 2);
   pLayout->addWidget(pButtonBox, 7, 0, 1, 2);
   pLayout->setRowStretch(8, 10);
   pLayout->setColumnStretch(2, 10);
   pLayout->setMargin(10);
   pLayout->setSpacing(5);
   setLayout(pLayout);
   // Layout End

   // Make GUI connections
   VERIFYNRV(connect(pButtonBox, SIGNAL(accepted()), this, SLOT(accept())));
   VERIFYNRV(connect(pButtonBox, SIGNAL(rejected()), this, SLOT(reject())));
}

KMeansDlg::~KMeansDlg()
{}

double KMeansDlg::getThreshold() const
{
   return mpThreshold->value();
}

double KMeansDlg::getConvergenceThreshold() const
{
   return mpConvergenceThreshold->value();
}

bool KMeansDlg::getSelectSignatures() const
{
   return mpSelectSignatures->isChecked();
}

bool KMeansDlg::getKeepIntermediateResults() const
{
   return mpKeepIntermediateResults->isChecked();
}

unsigned int KMeansDlg::getMaxIterations() const
{
   return static_cast<unsigned int>(mpMaxIterations->value());
}

unsigned int KMeansDlg::getClusterCount() const
{
   return static_cast<unsigned int>(mpClusterCount->value());
}

void KMeansDlg::accept()
{
   if (getSelectSignatures() == false && getClusterCount() < 2)
   {
      QMessageBox::critical(this, "Error", "Unable to perform K-Means with fewer than 2 clusters.");
      return;
   }

   QDialog::accept();
}

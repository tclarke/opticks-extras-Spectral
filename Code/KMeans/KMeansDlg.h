/*
 * The information in this file is
 * Copyright(c) 2012 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef KMEANSDLG_H
#define KMEANSDLG_H

#include <QtGui/QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;

class KMeansDlg : public QDialog
{
   Q_OBJECT

public:
   KMeansDlg(double threshold, double convergenceThreshold, unsigned int maxIterations,
      unsigned int clusterCount, bool selectSignatures, bool keepIntermediateResults, QWidget* pParent = NULL);
   virtual ~KMeansDlg();

   double getThreshold() const;
   double getConvergenceThreshold() const;
   bool getSelectSignatures() const;
   bool getKeepIntermediateResults() const;
   unsigned int getMaxIterations() const;
   unsigned int getClusterCount() const;

public slots:
   void accept();

private:
   QDoubleSpinBox* mpThreshold;
   QDoubleSpinBox* mpConvergenceThreshold;
   QSpinBox* mpMaxIterations;
   QSpinBox* mpClusterCount;
   QCheckBox* mpSelectSignatures;
   QCheckBox* mpKeepIntermediateResults;
};

#endif

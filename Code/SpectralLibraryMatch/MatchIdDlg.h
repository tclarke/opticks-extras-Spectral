/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef MATCHIDDLG_H
#define MATCHIDDLG_H

#include "SpectralLibraryMatch.h"

#include <string>

#include <QtGui/QDialog>

class AoiElement;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;
class RasterElement;

class MatchIdDlg : public QDialog
{
   Q_OBJECT

public:
   MatchIdDlg(const RasterElement* pRaster, QWidget* pParent = 0);
   virtual ~MatchIdDlg();

   virtual void accept();
   AoiElement* getAoi() const;
   bool getMatchEachPixel() const;
   bool getLimitByNumber() const;
   int getMaxMatches() const;
   bool getLimitByThreshold() const;
   double getThresholdLimit() const;
   std::string getLayerName() const;
   SpectralLibraryMatch::MatchAlgorithm getMatchAlgorithm() const;

private:
   const RasterElement* mpRaster;
   QComboBox* mpAoiCombo;
   QCheckBox* mpMatchEachCheckBox;
   QCheckBox* mpLimitByMaxNum;
   QSpinBox* mpMaxMatchesSpinBox;
   QCheckBox* mpLimitByThreshold;
   QDoubleSpinBox* mpThresholdLimit;
   QLineEdit* mpEditLayerName;
   QComboBox* mpAlgorithmCombo;
   QCheckBox* mpSaveSettings;
};

#endif

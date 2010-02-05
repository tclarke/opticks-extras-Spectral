/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SPECTRALSIGNATURESELECTOR_H__
#define SPECTRALSIGNATURESELECTOR_H__

#include "SignatureSelector.h"

class AlgorithmRunner;
class AoiElement;
class Progress;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class RasterElement;

class SpectralSignatureSelector : public SignatureSelector
{
   Q_OBJECT

public:
   SpectralSignatureSelector(RasterElement* pCube, Progress* pProgress, QWidget* pParent = NULL,
      QAbstractItemView::SelectionMode mode = QAbstractItemView::ExtendedSelection, bool addApply = false,
      const std::string& customButtonLabel = std::string());
   SpectralSignatureSelector(RasterElement* pCube, AlgorithmRunner* pRunner, Progress* pProgress,
      const std::string& resultsName = std::string(), bool pseudocolor = true, bool addApply = false, QWidget* pParent = NULL,
      const std::string& customButtonLabel = std::string());
   ~SpectralSignatureSelector();

   std::string getResultsName() const;
   double getThreshold() const;
   void setThreshold(double val);
   bool getModified() const { return isApplyButtonEnabled(); }
   AoiElement* getAoi() const;
   bool isPseudocolorLayerUsed() const;
   void usePseudocolorLayer(bool bPseudocolor);
   std::vector<Signature*> getSignatures() const;

protected slots:
   void enablePseudocolorCheckBox();
   void accept();
   void apply();
   void setModified() 
   { 
      enableApplyButton(true); 
   }
   void refreshAoiList();
   void updateSignatureList();

protected:
   virtual bool validateInputs();
   QLineEdit* mpNameEdit;
   QDoubleSpinBox* mpThreshold;
   QCheckBox* mpAoiCheck;
   QComboBox* mpAoiCombo;
   QCheckBox* mpPseudocolorCheck;

   AlgorithmRunner* mpRunner;
   RasterElement* mpCube;
};

#endif

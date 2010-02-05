/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */
#ifndef CEMDLG_H
#define CEMDLG_H

#include "SpectralSignatureSelector.h"

class CemDlg : public SpectralSignatureSelector
{
   Q_OBJECT

public:
   CemDlg(RasterElement* pCube, AlgorithmRunner* pRunner, Progress* pProgress,
      const std::string& resultsName = std::string(), bool pseudocolor = true, 
      bool addApply = false, bool contextSensitiveHelp = false, QWidget* pParent = NULL);
   ~CemDlg();

protected slots:
   void customButtonClicked();
};

#endif

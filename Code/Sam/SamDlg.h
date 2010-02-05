/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SAMDLG_H
#define SAMDLG_H

#include "SpectralSignatureSelector.h"

class SamDlg : public SpectralSignatureSelector
{
   Q_OBJECT

public:
   SamDlg(RasterElement* pCube, AlgorithmRunner* pRunner, Progress* pProgress,
      const std::string& resultsName = std::string(), bool pseudocolor = true, 
      bool addApply = false, bool contextHelp = false, QWidget* pParent = NULL);
   ~SamDlg();

protected slots:
   void customButtonClicked();
};

#endif

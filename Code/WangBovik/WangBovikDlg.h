/*
 * The information in this file is
 * Copyright(c) 2012 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef WANGBOVIKDLG_H
#define WANGBOVIKDLG_H

#include "SpectralSignatureSelector.h"

class WangBovikDlg : public SpectralSignatureSelector
{
   Q_OBJECT

public:
   WangBovikDlg(RasterElement* pCube, AlgorithmRunner* pRunner, Progress* pProgress,
      const std::string& resultsName = std::string(), bool pseudocolor = true,
      bool addApply = false, bool contextHelp = false, double threshold = 0.5, QWidget* pParent = NULL);
   virtual ~WangBovikDlg();

protected slots:
   void customButtonClicked();
};

#endif

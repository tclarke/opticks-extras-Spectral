/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "Cem.h"
#include "CemDlg.h"
#include "ConfigurationSettings.h"
#include "DesktopServices.h"

using namespace std;

CemDlg::CemDlg(RasterElement* pCube, AlgorithmRunner* pRunner, Progress* pProgress,
       const string& resultsName, bool pseudocolor, bool addApply, bool contextSensitiveHelp, QWidget* pParent) :
   SpectralSignatureSelector(pCube, pRunner, pProgress, resultsName, pseudocolor,
      addApply, pParent, (contextSensitiveHelp ? "Help" : string()))
{
}

CemDlg::~CemDlg()
{
}

void CemDlg::customButtonClicked()
{
   Service<DesktopServices> pDesktop;
   Service<ConfigurationSettings> pSettings;

   string helpFile = pSettings->getHome() + Cem::getSettingCemHelp();
   pDesktop->displayHelp(helpFile);
}

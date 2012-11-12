/*
 * The information in this file is
 * Copyright(c) 2012 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "ConfigurationSettings.h"
#include "DesktopServices.h"
#include "WangBovik.h"
#include "WangBovikDlg.h"

using namespace std;

WangBovikDlg::WangBovikDlg(RasterElement* pCube, AlgorithmRunner* pRunner, Progress* pProgress,
   const string& resultsName, bool pseudocolor, bool addApply, bool contextHelp, double threshold,
   QWidget* pParent) :
      SpectralSignatureSelector(pCube, pRunner, pProgress, resultsName, pseudocolor,
         addApply, pParent, (contextHelp ? "Help" : string()))
{
   setThreshold(threshold);
}

WangBovikDlg::~WangBovikDlg()
{}

void WangBovikDlg::customButtonClicked()
{
   Service<DesktopServices> pDesktop;
   Service<ConfigurationSettings> pSettings;

   string helpFile = pSettings->getHome() + WangBovik::getSettingWangBovikHelp();
   pDesktop->displayHelp(helpFile);
}

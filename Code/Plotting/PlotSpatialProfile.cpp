/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AoiElement.h"
#include "AoiLayer.h"
#include "AppVerify.h"
#include "LayerList.h"
#include "GetSubsetDialog.h"
#include "GraphicGroup.h"
#include "GraphicObject.h"
#include "PlotSpatialProfile.h"
#include "PlottingManager.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "ProgressTracker.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "SpectralVersion.h"
#include <QtGui/QMessageBox>

REGISTER_PLUGIN_BASIC(SpectralPlotting, PlotSpatialProfile);

PlotSpatialProfile::PlotSpatialProfile()
{
   setName("Plot Spatial Profile");
   setDescriptorId("{c6ff7ce4-aefd-4e68-9b68-df305e0fbfd1}");
   setDescription("Plot rows, columns, h-lines, and v-lines in an AOI in horizontal and vertical "
                  "profile plots.");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setMenuLocation("[Spectral]/Plotting/Plot Spatial Profile");
}

PlotSpatialProfile::~PlotSpatialProfile()
{
}

bool PlotSpatialProfile::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pInArgList->addArg<Progress>(ProgressArg()));
   VERIFY(pInArgList->addArg<RasterLayer>(LayerArg()));
   VERIFY(pInArgList->addArg<AoiElement>("AOI", "This AOI must contain HLINE, VLINE, ROW and/or COLUMN objects. "
                                                "If none are found, an error will occcur."));
   VERIFY(pInArgList->addArg<std::vector<unsigned int> >("Bands", std::vector<unsigned int>(),
         "If specified, these bands (original numbers, 0 based) will be plotted. "\
         "If not specified, the displayed band(s) for the layer will be plotted."));
   return true;
}

bool PlotSpatialProfile::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   VERIFY(pInArgList);
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(ProgressArg()), "Plotting data",
      "spectral", "{4e7d71bf-a98f-41b6-9243-3d65c6fd3526}");
   RasterLayer* pLayer = pInArgList->getPlugInArgValue<RasterLayer>(LayerArg());
   SpatialDataView* pSdv = pLayer == NULL ? NULL : dynamic_cast<SpatialDataView*>(pLayer->getView());
   if (pSdv == NULL)
   {
#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : This code is only needed because a RasterLayer arg "\
"will never autopopulate. When this is fixed, remove this code. (OPTICKS-1060) (tclarke)")
      pSdv = dynamic_cast<SpatialDataView*>(Service<DesktopServices>()->getCurrentWorkspaceWindowView());
      if (pSdv != NULL)
      {
         LayerList* pLayers = pSdv->getLayerList();
         VERIFY(pLayers);
         pLayer = static_cast<RasterLayer*>(pLayers->getLayer(RASTER, pLayers->getPrimaryRasterElement()));
      }
   }
   if (pLayer == NULL)
   {
      // End OPTICKS-1060 RasterLayer code

      progress.report("No raster layer specified.", 0, ERRORS, true);
      return false;
   }

   RasterDataDescriptor* pDesc = static_cast<RasterDataDescriptor*>(pLayer->getDataElement()->getDataDescriptor());
   VERIFY(pDesc);

   AoiElement* pAoi = pInArgList->getPlugInArgValue<AoiElement>("AOI");
   std::vector<unsigned int> bands;
   pInArgList->getPlugInArgValue("Bands", bands);

   if (!isBatch())
   {
      std::vector<Layer*> aoiLayers;
      pSdv->getLayerList()->getLayers(AOI_LAYER, aoiLayers);
      std::map<std::string, AoiElement*> aoiElements;
      QStringList aoiNames;
      for (std::vector<Layer*>::const_iterator layer = aoiLayers.begin(); layer != aoiLayers.end(); ++layer)
      {
         AoiLayer* pAoiLayer = static_cast<AoiLayer*>(*layer);
         AoiElement* pElement = static_cast<AoiElement*>(pAoiLayer->getDataElement());
         VERIFY(pElement);
         const std::list<GraphicObject*> objs = pElement->getGroup()->getObjects();
         for (std::list<GraphicObject*>::const_iterator obj = objs.begin(); obj != objs.end(); ++obj)
         {
            if ((*obj)->getGraphicObjectType() == ROW_OBJECT ||
                (*obj)->getGraphicObjectType() == COLUMN_OBJECT ||
                (*obj)->getGraphicObjectType() == HLINE_OBJECT ||
                (*obj)->getGraphicObjectType() == VLINE_OBJECT)
            {
               aoiNames << QString::fromStdString(pAoiLayer->getDisplayName(true));
               aoiElements[pAoiLayer->getName()] = static_cast<AoiElement*>(pAoiLayer->getDataElement());
               break;
            }
         }
      }
      if (aoiElements.empty() && pAoi != NULL)
      {
         aoiElements[pAoi->getName()] = pAoi;
      }
      if (aoiElements.empty())
      {
         progress.report("No AOI layers with row or column objects are available. "
                         "You must have at least one AOI with a row or column object for plotting.", 0, ERRORS, true);
         return false;
      }
      std::vector<std::string> bandNamesStl = RasterUtilities::getBandNames(pDesc);
      QStringList bandNames;
      for (std::vector<std::string>::const_iterator bandName = bandNamesStl.begin();
         bandName != bandNamesStl.end(); ++bandName)
      {
         bandNames << QString::fromStdString(*bandName);
      }
      GetSubsetDialog dlg(aoiNames, bandNames, bands, Service<DesktopServices>()->getMainWidget());
      if (pAoi != NULL)
      {
         dlg.setSelectedAoi(pAoi->getName());
      }
      if (dlg.exec() == QDialog::Accepted)
      {
         bands = dlg.getBandSelectionIndices();
         pAoi = aoiElements[dlg.getSelectedAoi().toStdString()];
         if (bands.size() > 20) // somewhat arbitrary number chosen based on some testing
         {
            if (QMessageBox::warning(Service<DesktopServices>()->getMainWidget(), "Performance Warning",
               QString("Plotting %1 bands could take a while. Are you sure you want to continue?").arg(bands.size()),
               QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
            {
               progress.report("Cancelled by user.", 0, ABORT, true);
               return false;
            }
         }
      }
      else
      {
         progress.report("Cancelled by user.", 0, ABORT, true);
         return false;
      }
   }

   if (pAoi == NULL)
   {
      progress.report("No AOI specified.", 0, ERRORS, true);
      return false;
   }
   std::vector<DimensionDescriptor> bandDd;
   for (std::vector<unsigned int>::const_iterator band = bands.begin(); band != bands.end(); ++band)
   {
      bandDd.push_back(pDesc->getOriginalBand(*band));
   }

   std::vector<PlugIn*> instances = Service<PlugInManagerServices>()->getPlugInInstances("Plotting Manager");
   VERIFY(!instances.empty());
   PlottingManager* pMgr = dynamic_cast<PlottingManager*>(instances.front());
   VERIFY(pMgr);

   pMgr->getHorizontal().clearPlot(pLayer);
   pMgr->getVertical().clearPlot(pLayer);

   GraphicGroup* pGroup = pAoi->getGroup();
   VERIFY(pGroup);
   bool foundAtLeastOne = false;
   const std::list<GraphicObject*>& objs = pGroup->getObjects();
   int cnt = 0;
   for (std::list<GraphicObject*>::const_iterator obj = objs.begin(); obj != objs.end(); ++obj, ++cnt)
   {
      progress.report("Plotting data", 99 * cnt / objs.size(), NORMAL);
      int startPos = -1;
      int stopPos = -1;
      unsigned int coord = 0;
      switch((*obj)->getGraphicObjectType())
      {
      case HLINE_OBJECT:
         startPos = (*obj)->getLlCorner().mX;
         stopPos = (*obj)->getUrCorner().mX;
      case ROW_OBJECT: // fall through
         coord = (*obj)->getLlCorner().mY;
         if (bandDd.empty())
         {
            pMgr->getHorizontal().plot(pLayer, coord, startPos, stopPos);
         }
         else
         {
            pMgr->getHorizontal().plot(pLayer, coord, startPos, stopPos, bandDd);
         }
         foundAtLeastOne = true;
         break;
      case VLINE_OBJECT:
         startPos = (*obj)->getLlCorner().mY;
         stopPos = (*obj)->getUrCorner().mY;
      case COLUMN_OBJECT: // fall through
         coord = (*obj)->getLlCorner().mX;
         if (bandDd.empty())
         {
            pMgr->getVertical().plot(pLayer, coord, startPos, stopPos);
         }
         else
         {
            pMgr->getVertical().plot(pLayer, coord, startPos, stopPos, bandDd);
         }
         foundAtLeastOne = true;
         break;
      default:
         continue;
      }
   }
   if (!foundAtLeastOne)
   {
      progress.report("AOI must contain at least one hline, vline, row, or column object.", 0, ERRORS, true);
      return false;
   }

   progress.report("Finished plotting data.", 100, NORMAL);
   progress.upALevel();
   return true;
}

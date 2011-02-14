/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "Axis.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DimensionDescriptor.h"
#include "Locator.h"
#include "ObjectFactory.h"
#include "PlotSet.h"
#include "PlotView.h"
#include "PlotWidget.h"
#include "PlotWindow.h"
#include "PointSet.h"
#include "ProfilePlotUtilities.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "RasterUtilities.h"
#include "SessionManager.h"
#include "Slot.h"
#include "StringUtilities.h"
#include "Units.h"
#include "xmlreader.h"
#include "xmlwriter.h"

using XERCES_CPP_NAMESPACE_QUALIFIER DOMNode;
using XERCES_CPP_NAMESPACE_QUALIFIER DOMElement;
using XERCES_CPP_NAMESPACE_QUALIFIER XMLString;

ProfilePlotUtilities::ProfilePlotUtilities(bool isHorizontal) :
         mIsHorizontal(isHorizontal),
         mpPlotSet(NULL),
         mpPlotWindow(NULL)
{
}

ProfilePlotUtilities::~ProfilePlotUtilities()
{
   for (std::map<RasterLayer*, PlotWidget*>::iterator it = mPlotWidgets.begin();
            it != mPlotWidgets.end(); ++it)
   {
      it->first->detach(SIGNAL_NAME(Subject, Deleted), Slot(this, &ProfilePlotUtilities::cleanupObjects));
      it->second->detach(SIGNAL_NAME(Subject, Deleted), Slot(this, &ProfilePlotUtilities::cleanupObjects));
      if (mpPlotSet != NULL)
      {
         mpPlotSet->deletePlot(it->second);
      }
   }
}

void ProfilePlotUtilities::setPlotSet(PlotSet* pPlotSet, PlotWindow* pPlotWindow)
{
   mpPlotSet = pPlotSet;
   mpPlotWindow = pPlotWindow;
}

PlotView* ProfilePlotUtilities::getPlot(RasterLayer* pLayer)
{
   VERIFY(pLayer && mpPlotSet);
   PlotView* pPlot = NULL;
   std::map<RasterLayer*, PlotWidget*>::iterator it = mPlotWidgets.find(pLayer);
   if (it != mPlotWidgets.end())
   {
      pPlot = it->second->getPlot();
   }
   else
   {
      std::string name = pLayer->getDisplayName(true);
      PlotWidget* pWidget = mpPlotSet->createPlot(name, CARTESIAN_PLOT);
      if (pWidget == NULL)
      {
         return NULL;
      }
      pLayer->attach(SIGNAL_NAME(Subject, Deleted), Slot(this, &ProfilePlotUtilities::cleanupObjects));
      pWidget->attach(SIGNAL_NAME(Subject, Deleted), Slot(this, &ProfilePlotUtilities::cleanupObjects));
      mPlotWidgets[pLayer] = pWidget;
      pWidget->setTitle(name);
      pWidget->getAxis(AXIS_BOTTOM)->setTitle(mIsHorizontal ? "Column Number" : "Row Number");
      const Units* pUnits = 
         static_cast<const RasterDataDescriptor*>(pLayer->getDataElement()->getDataDescriptor())->getUnits();
      pWidget->getAxis(AXIS_LEFT)->setTitle(pUnits == NULL ? "Digital Numbers" : pUnits->getUnitName());
      pPlot = pWidget->getPlot();
      VERIFYRV(pPlot, NULL);
   }
   return pPlot;
}

RasterLayer* ProfilePlotUtilities::getActivePlotLayer() const
{
   VERIFYRV(mpPlotSet, NULL);
   PlotWidget* pWidget = mpPlotSet->getCurrentPlot();
   for (std::map<RasterLayer*, PlotWidget*>::const_iterator it = mPlotWidgets.begin();
            it != mPlotWidgets.end(); ++it)
   {
      if (it->second == pWidget)
      {
         return it->first;
      }
   }
   return NULL;
}

void ProfilePlotUtilities::clearPlot(RasterLayer* pLayer)
{
   PlotView* pPlot = getPlot(pLayer);
   if (pPlot != NULL)
   {
      // full clear
      std::list<PlotObject*> objs;
      pPlot->getObjects(objs);
      for (std::list<PlotObject*>::iterator obj = objs.begin(); obj != objs.end(); ++obj)
      {
         pPlot->deleteObject(*obj);
      }
   }
}

void ProfilePlotUtilities::cleanupObjects(Subject& subject, const std::string& signal, const boost::any& v)
{
   VERIFYNRV(mpPlotSet);
   RasterLayer* pLayer = dynamic_cast<RasterLayer*>(&subject);
   PlotWidget* pWidget = dynamic_cast<PlotWidget*>(&subject);
   if (pLayer != NULL)
   {
      std::map<RasterLayer*, PlotWidget*>::iterator it = mPlotWidgets.find(pLayer);
      if (it != mPlotWidgets.end())
      {
         it->second->detach(SIGNAL_NAME(Subject, Deleted), Slot(this, &ProfilePlotUtilities::cleanupObjects));
         mpPlotSet->deletePlot(it->second);
         mPlotWidgets.erase(it);
      }
   }
   else if (pWidget != NULL)
   {
      for (std::map<RasterLayer*, PlotWidget*>::iterator it = mPlotWidgets.begin();
               it != mPlotWidgets.end(); ++it)
      {
         if (it->second == pWidget)
         {
            it->first->detach(SIGNAL_NAME(Subject, Deleted), Slot(this, &ProfilePlotUtilities::cleanupObjects));
            mPlotWidgets.erase(it);
            break;
         }
      }
   }
}

bool ProfilePlotUtilities::plot(RasterLayer* pLayer, unsigned int coord, int tickCoord)
{
   std::vector<DimensionDescriptor> bands;
   getLayerBandList(pLayer, bands);
   return plot(pLayer, coord, -1, -1, tickCoord, bands);
}

bool ProfilePlotUtilities::plot(RasterLayer* pLayer,
                               unsigned int coord,
                               int startPos,
                               int stopPos)
{
   std::vector<DimensionDescriptor> bands;
   getLayerBandList(pLayer, bands);
   return plot(pLayer, coord, startPos, stopPos, -1, bands);
}

bool ProfilePlotUtilities::plot(RasterLayer* pLayer,
                               unsigned int coord,
                               int startPos,
                               int stopPos,
                               const std::vector<DimensionDescriptor>& bands)
{
   return plot(pLayer, coord, startPos, stopPos, -1, bands);
}

bool ProfilePlotUtilities::plot(RasterLayer* pLayer,
                               unsigned int coord,
                               int startPos,
                               int stopPos,
                               int tickCoord,
                               const std::vector<DimensionDescriptor>& bands)
{
   if (pLayer == NULL)
   {
      return false;
   }
   PlotView* pPlot = getPlot(pLayer);
   if (pPlot == NULL || bands.empty())
   {
      return false;
   }
   // ensure the window is visible
   mpPlotWindow->show();

   RasterElement* pElement = static_cast<RasterElement*>(pLayer->getDataElement());
   VERIFY(pElement);
   RasterDataDescriptor* pDesc = static_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());
   VERIFY(pDesc);

   if (startPos != -1)
   {
      startPos = std::max(startPos, 0);
   }
   if (stopPos != -1)
   {
      stopPos = std::min<int>(stopPos, (mIsHorizontal ? pDesc->getColumnCount() : pDesc->getRowCount()) - 1);
   }

   double scale = 1.0;
   const Units* pUnits = pDesc->getUnits();
   if (pUnits != NULL)
   {
      scale = pUnits->getScaleFromStandard();
   }
   std::vector<ColorType> colors;
   // colors may contain more than needed if there are invalid band DimensionDescriptors in the band
   // list...this is ok, we'll just ignore the extra colors

   // special case 1 and 3 colors which often indicate gray and RGB display, if they are arbitrary
   // the special case colors will still be fine
   if (bands.size() == 1)
   {
      colors.push_back(ColorType(0x78, 0x78, 0x78));
   }
   else if (bands.size() == 3)
   {
      colors.push_back(ColorType(0x78, 0x00, 0x00));
      colors.push_back(ColorType(0x00, 0x78, 0x00));
      colors.push_back(ColorType(0x00, 0x00, 0x78));
   }
   else
   {
      std::vector<ColorType> exclude;
      exclude.push_back(pPlot->getBackgroundColor());
      ColorType::getUniqueColors(bands.size(), colors, exclude);
   }

   // Active number and the created PointSet with attributes set
   std::vector<std::pair<unsigned int, PointSet*> > bandInfo;
   for (size_t bandidx = 0; bandidx < bands.size(); ++bandidx)
   {
      if (bands[bandidx].isActiveNumberValid())
      {
         PointSet* pPointSet = static_cast<PointSet*>(pPlot->addObject(POINT_SET, true));
         VERIFY(pPointSet);
         std::string objName;
         if (mIsHorizontal)
         {
            objName = "Row " + StringUtilities::toDisplayString(coord) + ", ";
         }
         else
         {
            objName = "Column " + StringUtilities::toDisplayString(coord) + ", ";
         }
         objName += RasterUtilities::getBandName(pDesc, bands[bandidx]);
         pPointSet->setObjectName(objName);
         pPointSet->setLineColor(colors[bandidx % colors.size()]);
         bandInfo.push_back(std::make_pair(bands[bandidx].getActiveNumber(), pPointSet));
      }
   }

   FactoryResource<DataRequest> pReq;
   pReq->setInterleaveFormat(BIP);
   size_t totalPixels = 0;
   if (mIsHorizontal)
   {
      pReq->setRows(pDesc->getActiveRow(coord), pDesc->getActiveRow(coord), 1);
      totalPixels = stopPos == -1 ? pDesc->getColumnCount() : (stopPos + 1);
      if (startPos != -1)
      {
         totalPixels -= startPos;
      }
      pReq->setColumns(startPos == -1 ? DimensionDescriptor() : pDesc->getActiveColumn(startPos),
                       stopPos == -1 ? DimensionDescriptor() : pDesc->getActiveColumn(stopPos),
                       totalPixels);
   }
   else
   {
      pReq->setColumns(pDesc->getActiveColumn(coord), pDesc->getActiveColumn(coord), 1);
      totalPixels = stopPos == -1 ? pDesc->getRowCount() : (stopPos + 1);
      if (startPos != -1)
      {
         totalPixels -= startPos;
      }
      pReq->setRows(startPos == -1 ? DimensionDescriptor() : pDesc->getActiveRow(startPos),
                    stopPos == -1 ? DimensionDescriptor() : pDesc->getActiveRow(stopPos),
                    totalPixels);
   }
   // turn off interactive processing for the PointSets
   for (std::vector<std::pair<unsigned int, PointSet*> >::const_iterator band = bandInfo.begin();
            band != bandInfo.end(); ++band)
   {
      band->second->setInteractive(false);
   }
   DataAccessor acc(pElement->getDataAccessor(pReq.release()));
   for (size_t pixel = 1; pixel <= totalPixels; ++pixel)
   {
      VERIFY(acc.isValid());

      for (std::vector<std::pair<unsigned int, PointSet*> >::const_iterator band = bandInfo.begin();
               band != bandInfo.end(); ++band)
      {
         double val = Service<ModelServices>()->getDataValue(pDesc->getDataType(), acc->getColumn(), band->first);
         band->second->addPoint(static_cast<double>((startPos == -1 ? pixel : (pixel + startPos))), val * scale);
      }
      if (mIsHorizontal)
      {
         acc->nextColumn();
      }
      else
      {
         acc->nextRow();
      }
   }
   // turn on interactive processing for the PointSets::g
   for (std::vector<std::pair<unsigned int, PointSet*> >::const_iterator band = bandInfo.begin();
            band != bandInfo.end(); ++band)
   {
      band->second->setInteractive(true);
   }
   if (tickCoord != -1)
   {
      Locator* pLocator = static_cast<Locator*>(pPlot->addObject(LOCATOR, false));
      VERIFY(pLocator);
      pLocator->setStyle(Locator::VERTICAL_LOCATOR);
      pLocator->setLocation(LocationType(tickCoord + 1, 0));
      pLocator->setColor(ColorType(0xff, 0x66, 0x00));
   }
   pPlot->zoomExtents();
   pPlot->refresh();
   return true;
}

void ProfilePlotUtilities::getLayerBandList(RasterLayer* pLayer, std::vector<DimensionDescriptor>& bands)
{
   if (pLayer != NULL)
   {
      switch(pLayer->getDisplayMode())
      {
      case GRAYSCALE_MODE:
         bands.push_back(pLayer->getDisplayedBand(GRAY));
         break;
      case RGB_MODE:
         bands.push_back(pLayer->getDisplayedBand(RED));
         bands.push_back(pLayer->getDisplayedBand(GREEN));
         bands.push_back(pLayer->getDisplayedBand(BLUE));
         break;
      default:
         break;
      }
   }
}

bool ProfilePlotUtilities::toXml(XMLWriter* pXml) const
{
   pXml->addAttr("horizontal", mIsHorizontal);
   pXml->addAttr("plotSet", mpPlotSet->getId());
   pXml->addAttr("plotWindow", mpPlotWindow->getId());
   for (std::map<RasterLayer*, PlotWidget*>::const_iterator widget = mPlotWidgets.begin();
         widget != mPlotWidgets.end(); ++widget)
   {
      pXml->pushAddPoint(pXml->addElement("PlotWidget"));
      pXml->addAttr("rasterLayer", widget->first->getId());
      pXml->addAttr("plotWidget", widget->second->getId());
      pXml->popAddPoint();
   }
   return true;
}

bool ProfilePlotUtilities::fromXml(DOMNode* pDocument, unsigned int version)
{
   DOMElement* pTopElement = static_cast<DOMElement*>(pDocument);
   mIsHorizontal =
      StringUtilities::fromXmlString<bool>(A(pTopElement->getAttribute(X("horizontal"))));
   if ((mpPlotSet = dynamic_cast<PlotSet*>(
      Service<SessionManager>()->getSessionItem(A(pTopElement->getAttribute(X("plotSet")))))) == NULL)
   {
      return false;
   }
   if ((mpPlotWindow = dynamic_cast<PlotWindow*>(
      Service<SessionManager>()->getSessionItem(A(pTopElement->getAttribute(X("plotWindow")))))) == NULL)
   {
      return false;
   }
   for (DOMNode* pChild = pTopElement->getFirstChild(); pChild != NULL; pChild = pChild->getNextSibling())
   {
      if (XMLString::equals(pChild->getNodeName(), X("PlotWidget")))
      {
         DOMElement* pElement = static_cast<DOMElement*>(pChild);
         RasterLayer* pLayer = dynamic_cast<RasterLayer*>(
            Service<SessionManager>()->getSessionItem(A(pElement->getAttribute(X("rasterLayer")))));
         PlotWidget* pWidget = dynamic_cast<PlotWidget*>(
            Service<SessionManager>()->getSessionItem(A(pElement->getAttribute(X("plotWidget")))));
         if (pLayer == NULL || pWidget == NULL)
         {
            return false;
         }
         pLayer->attach(SIGNAL_NAME(Subject, Deleted), Slot(this, &ProfilePlotUtilities::cleanupObjects));
         pWidget->attach(SIGNAL_NAME(Subject, Deleted), Slot(this, &ProfilePlotUtilities::cleanupObjects));
         mPlotWidgets[pLayer] = pWidget;
      }
   }
   return true;
}
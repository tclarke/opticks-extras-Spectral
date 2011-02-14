/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef PROFILEPLOTUTILITIES_H__
#define PROFILEPLOTUTILITIES_H__

#include "Serializable.h"
#include <boost/any.hpp>
#include <map>
#include <string>

class DimensionDescriptor;
class PlotSet;
class PlotView;
class PlotWidget;
class PlotWindow;
class RasterLayer;
class Subject;

class ProfilePlotUtilities : public Serializable
{
public:
   ProfilePlotUtilities(bool isHorizontal);
   virtual ~ProfilePlotUtilities();

   void setPlotSet(PlotSet* pPlotSet, PlotWindow* pPlotWindow);
   PlotView* getPlot(RasterLayer* pLayer);
   RasterLayer* getActivePlotLayer() const;
   void clearPlot(RasterLayer* pLayer);
   void cleanupObjects(Subject& subject, const std::string& signal, const boost::any& v);

   bool plot(RasterLayer* pLayer, unsigned int coord, int tickCoord);
   bool plot(RasterLayer* pLayer,
             unsigned int coord,
             int startPos,
             int stopPos);
   bool plot(RasterLayer* pLayer,
             unsigned int coord,
             int startPos,
             int stopPos,
             const std::vector<DimensionDescriptor>& bands);

   virtual bool toXml(XMLWriter* pXml) const;
   virtual bool fromXml(DOMNode* pDocument, unsigned int version);

private:
   bool plot(RasterLayer* pLayer,
             unsigned int coord,
             int startPos,
             int stopPos,
             int tickCoord,
             const std::vector<DimensionDescriptor>& bands);

   void getLayerBandList(RasterLayer* pLayer, std::vector<DimensionDescriptor>& bands);

   bool mIsHorizontal;
   PlotSet* mpPlotSet;
   PlotWindow* mpPlotWindow;
   std::map<RasterLayer*, PlotWidget*> mPlotWidgets;
};

#endif
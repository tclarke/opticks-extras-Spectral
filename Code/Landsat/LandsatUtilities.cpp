/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "LandsatUtilities.h"
#include <stdlib.h>

using namespace std;

namespace Landsat
{
   double LatLongConvert(string strInputLatLongData)
   {
      int iStringLength = 0;	//len
      double dblTemperature = 0.0;		//temp
      double dblDegrees = 0.0;			//deg
      double dblMinutes = 0.0;			//min
      double dblSeconds = 0.0;			//sec
      double dblValue = 0.0;			//value
      string strHemisphereCode = "";
      string strLatLongData = "";

      iStringLength = strInputLatLongData.length();							  //get length total length of the string
      strHemisphereCode = strInputLatLongData.at(iStringLength - 1);			  //pull off the directional character   
      strLatLongData.append( strInputLatLongData.c_str(), iStringLength - 1 );   //get the actual lat/long data

      dblTemperature = atof( strLatLongData.c_str() );      //convert the string to a double

      dblDegrees = (int)( dblTemperature / 10000 );
      dblMinutes = (int)(( dblTemperature - 10000 * dblDegrees ) / 100 );
      dblSeconds = ( dblTemperature - 10000 * dblDegrees ) - ( 100 * dblMinutes );

      dblValue = dblDegrees + (dblMinutes / 60) + (dblSeconds / 3600);

      //This checks to see if the data is in South or West Hemisphere and applies a negative value to the coordinate
      if (strHemisphereCode == "s" || strHemisphereCode == "S" || strHemisphereCode == "w" || strHemisphereCode == "W")
      {
         dblValue = (dblValue * -1.00);
      }

      return dblValue;
   }
};

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef COMMONPLUGINARGS_H__
#define COMMONPLUGINARGS_H__

#include <string>

/**
 *  Provides methods to obtain a standard name for common plug-in arguments.
 */
class SpectralCommon
{
public:
   /**
    *  A standard name for a plug-in argument to export metadata.
    *
    *  Plug-ins that specify an argument to export metadata can use this method
    *  to ensure a common and consistent name is used in each location instead
    *  of a string literal.
    *
    *  Typically, arguments with this name should be of the type bool.
    *
    *  @return  Returns "Export Metadata".
    */
   static std::string ExportMetadataArg();
};

#endif

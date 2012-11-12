/*
 * The information in this file is
 * Copyright(c) 2012 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef WANGBOVIKERR_H
#define WANGBOVIKERR_H

//////////////////////////////////////////////////////////////////////////////
// ERRORS
// WangBovik.cpp WBI001 - WBI099
//
//////////////////////////////////////////////////////////////////////////////
static const char WBIERR001[] = "Error WBIERR001: The sensor data input value is invalid.";
static const char WBIERR002[] = "Error WBIERR002: No valid signatures to process.";
static const char WBIERR003[] = "Error WBIERR003: The memory for storing the WBI results was deleted. "
   "WBI processing stopped.";
static const char WBIERR004[] = "Error WBIERR004: No signatures loaded.";
static const char WBIERR005[] = "Error WBIERR005: Error occured while allocating storage for the WBI results.";
static const char WBIERR006[] = "Error WBIERR006: There are no signatures to process.";
static const char WBIERR007[] = "Error WBIERR007: The sensor data is invalid.";
static const char WBIERR008[] = "Error WBIERR008: Complex data is not supported.";
static const char WBIERR009[] = "Error WBIERR009: Cannnot perform WBI on 1 Band data.";
static const char WBIERR010[] = "Error WBIERR010: An error occured processing the WBI results.  Please "
   "review the signature inputs and reprocess.";
static const char WBIERR011[] = "Error WBIERR011: Cannnot calculate mean.";

//////////////////////////////////////////////////////////////////////////////
// WARNING
// WangBovik.cpp WBI100 - WBI199
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// NORMAL
// WangBovik.cpp WBI200 - WBI 299
//
//////////////////////////////////////////////////////////////////////////////
static const char WBINORM200[] = "WBI Complete";

//////////////////////////////////////////////////////////////////////////////
// ABORT
// WangBovik.cpp WBI000
//
//////////////////////////////////////////////////////////////////////////////
static const char WBIABORT000[] = "Abort: WBI aborted by user";

#endif
/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef ACEErr_h__
#define ACEErr_h__

//////////////////////////////////////////////////////////////////////////////
// ERRORS
// Ace.cpp ACE001 - ACE099
//
//
//////////////////////////////////////////////////////////////////////////////
static const char ACEERR001[] = "Error ACEERR001: The sensor data input value is invalid.";
static const char ACEERR002[] = "Error ACEERR002: No valid signatures to process.";
static const char ACEERR003[] = "Error ACEERR003: The results matrix was deleted. ACE processing stopped.";
static const char ACEERR004[] = "Error ACEERR004: No signatures loaded.";
static const char ACEERR005[] = "Error ACEERR005: Error creating results matrix.";
static const char ACEERR006[] = "Error ACEERR006: There are no signatures to process.";
static const char ACEERR007[] = "Error ACEERR007: The sensor data is invalid.";
static const char ACEERR008[] = "Error ACEERR008: Complex data is not supported.";
static const char ACEERR009[] = "Error ACEERR009: Cannnot perform ACE on 1 Band data.";
static const char ACEERR010[] = "Error ACEERR010: An error occured processing the ACE results matrix.  Please "
   "review the signature inputs and reprocess.";
static const char ACEERR011[] = "Error ACEERR011: Cannnot calculate mean.";



//////////////////////////////////////////////////////////////////////////////
// WARNING
// ACE.cpp ACE100 - ACE199
//
//
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// NORMAL
// ACE.cpp ACE200 - ACE 299
//
//
//////////////////////////////////////////////////////////////////////////////
static const char ACENORM200[] = "ACE Complete";


//////////////////////////////////////////////////////////////////////////////
// ABORT
// ACE.cpp ACE000
//
//
//////////////////////////////////////////////////////////////////////////////
static const char ACEABORT000[] = "Abort: ACE Aborted by user";


#endif
/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SAMErr_h__
#define SAMErr_h__

//////////////////////////////////////////////////////////////////////////////
// ERRORS
// SAM.cpp SAM001 - SAM099
//
//
//////////////////////////////////////////////////////////////////////////////
static const char SAMERR001[] = "Error SAMERR001: The sensor data input value is invalid.";
static const char SAMERR002[] = "Error SAMERR002: The spectral cube is invalid.";
static const char SAMERR003[] = "Error SAMERR003: The sensor data is invalid.";
static const char SAMERR004[] = "Error SAMERR004: Empty cube provided.";
static const char SAMERR005[] = "Error SAMERR005: No valid signatures to process.";
static const char SAMERR006[] = "Error SAMERR006: The results matrix was deleted. SAM processing stopped.";
static const char SAMERR007[] = "Error SAMERR007: No signatures loaded.";
static const char SAMERR009[] = "Error SAMERR009: Error creating results matrix.";
static const char SAMERR010[] = "Error SAMERR010: Error allocating memory for results.";
static const char SAMERR011[] = "Error SAMERR011: There are no signatures to process.";
static const char SAMERR012[] = "Error SAMERR012: The sensor data is invalid.";
static const char SAMERR013[] = "Error SAMERR013: Complex data is not supported.";
static const char SAMERR014[] = "Error SAMERR014: Cannnot perform SAM on 1 Band data.";
static const char SAMERR015[] = "Error SAMERR015: Not all input values could be extracted.";
static const char SAMERR016[] = "Error SAMERR016: An error occured processing the SAM results matrix.  Please review the signature inputs and reprocess.";
//SAMERR016 - is harded coded in SamGui.cpp, method apply(). This error returns
//            runtime information and therefore could not be declared here.
static const char SAMERR017[] = "Error SAMERR017: Could not create results matrix.";
static const char SAMERR018[] = "Error SAMERR018: Invalid DataAccessor.";
static const char SAMERR019[] = "Error SAMERR019: Currently the SAM Plug-in does not support processing BIL data loaded on-disk. Please reopen the cube in memory.";

//////////////////////////////////////////////////////////////////////////////
// WARNING
// SAM.cpp SAM100 - SAM199
//
//
//////////////////////////////////////////////////////////////////////////////
static const char SAMWARN100[] = "Warning SAMWARN100: Reusing existing results matrix";


//////////////////////////////////////////////////////////////////////////////
// NORMAL
// SAM.cpp SAM200 - SAM 299
//
//
//////////////////////////////////////////////////////////////////////////////
static const char SAMNORM200[] = "SAM Complete";


//////////////////////////////////////////////////////////////////////////////
// ABORT
// SAM.cpp SAM000
//
//
//////////////////////////////////////////////////////////////////////////////
static const char SAMABORT000[] = "Abort: SAM Aborted by user";


#endif
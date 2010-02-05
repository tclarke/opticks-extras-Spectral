/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QString>
#include <QtGui/QMessageBox>

#include "AoiElement.h"
#include "AppVerify.h"
#include "BitMask.h"
#include "DataRequest.h"
#include "DesktopServices.h"
#include "FileResource.h"
#include "Iarr.h"
#include "IarrDlg.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "Progress.h"
#include "RasterDataDescriptor.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "SpectralVersion.h"
#include "StringUtilities.h"
#include "switchOnEncoding.h"
#include "TypeConverter.h"
#include "Undo.h"
#include "Units.h"

#include <fstream>
#include <ostream>
#include <string>
#include <vector>

using namespace std;

REGISTER_PLUGIN_BASIC(SpectralIarr, Iarr);

Iarr::Iarr() :
   mpProgress(NULL),
   mpInputRasterElement(NULL),
   mpInputRasterLayer(NULL),
   mpAoiElement(NULL),
   mRowStepFactor(1),
   mColumnStepFactor(1),
   mOutputDataType(FLT4BYTES),
   mInMemory(true),
   mDisplayResults(true),
   mExtension(".iarr")
{
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(SPECTRAL_COPYRIGHT);
   setVersion(SPECTRAL_VERSION_NUMBER);
   setProductionStatus(SPECTRAL_IS_PRODUCTION_RELEASE);
   setName("IARR");
   setDescription("This plug-in calculates Internal Average Relative Reflectance (IARR)");
   setShortDescription("This plug-in calculates Internal Average Relative Reflectance (IARR)");
   setDescriptorId("{7159D8FA-A632-4e69-ABEE-B5A0ADA883DA}");
   setMenuLocation("[Spectral]\\Preprocessing\\IARR");
   setAbortSupported(true);
}

Iarr::~Iarr()
{
}

bool Iarr::runOperationalTests(Progress* pProgress, ostream& failure)
{
   return runAllTests(pProgress, failure);
}

bool Iarr::runAllTests(Progress* pProgress, ostream& failure)
{
   // Test with a valid Progress*
   mpProgress = pProgress;

   // 2x2x1
   {
      const unsigned int numRows = 2;
      const unsigned int numColumns = 2;
      const unsigned int numBands = 1;
      const unsigned int pData[] = {
         // Band 1
         0x0004, 0x0008,
         0x0004, 0x0000 };

      if (runTest(numRows, numColumns, numBands, INT4UBYTES, pData, failure) == false)
      {
         return false;
      }
   }

   // 3x3x1
   {
      const unsigned int numRows = 3;
      const unsigned int numColumns = 3;
      const unsigned int numBands = 1;
      const char pData[] = {
         // Band 1
         0x6B, 0x7F, 0x1B,
         0x4E, 0x35, 0x2D,
         0x4B, 0x5A, 0x40 };

      if (runTest(numRows, numColumns, numBands, INT1SBYTE, pData, failure) == false)
      {
         return false;
      }
   }

   // 3x3x3
   {
      const unsigned int numRows = 3;
      const unsigned int numColumns = 3;
      const unsigned int numBands = 3;
      const unsigned short pData[] = {
         // Band 1
         0x1E5E, 0xD201, 0x25B9,
         0x8892, 0xC1EC, 0xC0AD,
         0xE243, 0x9089, 0x584B,

         // Band 2
         0x0BDA, 0x15A8, 0x997B,
         0x295A, 0x88DB, 0xC960,
         0xBA6E, 0xB563, 0x9C25,

         // Band 3
         0xAC78, 0x3561, 0x569E,
         0xBAE5, 0xEFC3, 0x51D8,
         0xED72, 0xC00C, 0x1DE1 };

      if (runTest(numRows, numColumns, numBands, INT2UBYTES, pData, failure) == false)
      {
         return false;
      }
   }

   // Test with an invalid Progress* (this will also make testing run faster)
   mpProgress = NULL;

   // 16x12x8
   {
      const unsigned int numRows = 16;
      const unsigned int numColumns = 12;
      const unsigned int numBands = 8;
      const int pData[] = {
         // Band 1
         0x0B88, 0x0067, 0x9C88, 0x3F43, 0x26D8, 0x04CA, 0x4EC7, 0xFCA0, 0x5151, 0xBF0E, 0xEFAA, 0x5A25,
         0x5BE0, 0x11FF, 0xAAED, 0x78A5, 0x78F2, 0x3B61, 0xCA7A, 0x2D96, 0x5B01, 0x8D5A, 0xDD1C, 0x7E67,
         0xE2F1, 0x2B32, 0x69FD, 0x2425, 0x5132, 0xD45F, 0x2BFC, 0x286E, 0x1FC0, 0xCBE4, 0xF1CC, 0x88BE,
         0x50E8, 0xBDFF, 0xF8E5, 0xECFB, 0x5FE5, 0xCA23, 0xE344, 0x7361, 0xBE47, 0x8C4A, 0x3B04, 0x85B6,
         0xDF2F, 0x3D8E, 0x89B5, 0xBFCF, 0xCC54, 0xE11B, 0xEF96, 0xE864, 0x59E3, 0x8C6A, 0x6791, 0xF839,
         0xDD22, 0xA9CE, 0x7E11, 0x3290, 0x7AEE, 0x6252, 0xAD9F, 0x49F2, 0xFC03, 0x0BE7, 0x3758, 0x9840,
         0x4715, 0xA1E4, 0xA329, 0x6C1E, 0x4BBA, 0x5DCC, 0xBAE6, 0x57A4, 0x9823, 0x4E75, 0x1D2A, 0x9D64,
         0x71A6, 0x0F6E, 0x6905, 0xA6C2, 0x801A, 0x82BF, 0xE983, 0x8899, 0x0CF9, 0xA0FA, 0xDAF8, 0xA12D,
         0xC464, 0x5A92, 0x3E26, 0x3976, 0x1EE2, 0x888E, 0x422E, 0x3FAF, 0x4302, 0xE882, 0x2F4C, 0x0D43,
         0x84C0, 0x2DE1, 0xEC70, 0x3FFC, 0x6DBE, 0x2298, 0x179C, 0x9F8C, 0x5D50, 0xD2FB, 0xA215, 0x1761,
         0xB056, 0xD104, 0x0D65, 0xBCCC, 0x7FE1, 0x85DF, 0x232D, 0xE575, 0xE7B1, 0x86C9, 0x53C6, 0x5220,
         0xE87E, 0x103D, 0x8EB3, 0x62C6, 0xCD0E, 0x8171, 0xC2E3, 0x5DF7, 0x2A23, 0xEA24, 0xD6BF, 0xCB8F,
         0x6931, 0xABBB, 0x4212, 0xDCBF, 0xE0EB, 0x26AF, 0x2FB3, 0xDA60, 0x8299, 0x7E4A, 0x300E, 0xBB9A,
         0x1841, 0x64B6, 0x806D, 0xAED6, 0x0EA7, 0xFA59, 0xD823, 0xC909, 0xCA0E, 0xBF80, 0xD275, 0xBF44,
         0x9ED8, 0x9264, 0xDD62, 0xA59F, 0x40EB, 0xBF6F, 0xBF30, 0xCD6A, 0xD5EE, 0x1EE0, 0xADCF, 0xACB3,
         0x8D50, 0x47B4, 0xE60C, 0xD11D, 0xD122, 0xCBE6, 0xF191, 0xED07, 0xFAC3, 0x89FC, 0x57B5, 0xECFF,

         // Band 2
         0xA156, 0x09E4, 0xFC22, 0x1885, 0x760D, 0xF121, 0x0240, 0x5628, 0xB142, 0x7D4D, 0x3B7F, 0x6BE4,
         0x0661, 0x17DD, 0x2D5F, 0x52DC, 0x3AA4, 0xEF4A, 0xA559, 0xA75E, 0x3697, 0xB174, 0xDD8D, 0x1A37,
         0xB674, 0x3D69, 0x2C44, 0xFA5D, 0x8650, 0x7F69, 0x4C42, 0xCDDE, 0x4B10, 0x484D, 0x2CE7, 0xF52D,
         0xDF32, 0x412C, 0x5521, 0x68B0, 0x3E6C, 0xE65E, 0xDB2D, 0x79AC, 0x000F, 0x9EC9, 0xE429, 0xAC75,
         0x6345, 0xD17B, 0xB973, 0xA5F8, 0xEB18, 0x1E9B, 0x69E1, 0x148D, 0x8B4E, 0x97A6, 0x00BB, 0xCA20,
         0x5D10, 0x13FA, 0x4690, 0xC3A3, 0xF564, 0x8BB7, 0x16D8, 0x59D7, 0x3677, 0x8BEA, 0x4065, 0x745C,
         0xC1B7, 0xB20C, 0xFAA2, 0xD41D, 0xEDBC, 0x41CE, 0xEBAA, 0x6A02, 0x5901, 0xB937, 0xB524, 0xBDF7,
         0x006E, 0x8C14, 0x20F7, 0x613D, 0xE9B2, 0xDDB1, 0xCBCB, 0x8418, 0x626C, 0x50E9, 0x6B5C, 0x7DBD,
         0xCA24, 0xE485, 0xA999, 0x7C99, 0xED13, 0xEBE7, 0x7998, 0x3AE9, 0xF2C3, 0x0408, 0x1753, 0xC79E,
         0xD271, 0x2AC6, 0x8740, 0x5C93, 0x6648, 0xD980, 0xA8AF, 0x4711, 0x0670, 0x3608, 0xD9FA, 0xE0A7,
         0xABE0, 0x53DB, 0x2989, 0x904C, 0xB60D, 0x81B1, 0x21A4, 0xEBD9, 0x3260, 0xA85A, 0x1010, 0xD4CA,
         0xB681, 0xBDEE, 0xF687, 0xB957, 0xCB73, 0x3F45, 0xEFF4, 0xD7D6, 0xEE77, 0xC6CB, 0x3F88, 0x9176,
         0x14CD, 0xA599, 0xE09A, 0xDB3F, 0xCE38, 0x97E3, 0xA858, 0xB069, 0xEA59, 0x7DB8, 0xFA4A, 0x9603,
         0xBBDB, 0x330B, 0x0CA3, 0x3BE7, 0xD869, 0x6A18, 0xBC60, 0x1A01, 0x8179, 0x9C08, 0xEC3E, 0x32E4,
         0x87E5, 0x07F6, 0x8079, 0xCDAC, 0xC457, 0xC13A, 0xD85D, 0x593B, 0x2D7F, 0xD305, 0xEAA9, 0x51B6,
         0x6A1D, 0x6B50, 0xE3BD, 0x3161, 0x01DE, 0x1D1A, 0x5AA2, 0x7EC2, 0x1200, 0x31F4, 0x14D6, 0xDF16,

         // Band 3
         0xA6D0, 0xFFE2, 0x58EF, 0x4616, 0x82F3, 0x6581, 0xD20C, 0x230F, 0x9E7C, 0x418E, 0x0719, 0xB047,
         0x15DA, 0x05AC, 0x57DE, 0x45AF, 0xBA8F, 0x5486, 0x94E0, 0xC1ED, 0x2CBA, 0xAE38, 0x2116, 0x03AF,
         0x876F, 0x3914, 0xA866, 0x6F52, 0x9EE3, 0x87A6, 0x5DFA, 0x86D3, 0xC466, 0x791A, 0xD360, 0x8F1E,
         0x2A28, 0x32E4, 0x5D7C, 0x4E98, 0xCBD7, 0x10BF, 0x0247, 0xC90D, 0xE409, 0xC0FD, 0x0366, 0x23E2,
         0x046F, 0x5D1F, 0xEC86, 0x819B, 0xA6E6, 0x9DC8, 0x3690, 0x05B2, 0x594D, 0x14F9, 0x80AB, 0xD1B7,
         0x802E, 0x819C, 0x4D0D, 0x1FCF, 0x9D3A, 0x2E65, 0x5B9A, 0x6D70, 0x2A91, 0x60FC, 0x825B, 0xB07C,
         0x07D8, 0xD77D, 0x30B8, 0x9EA5, 0x722B, 0x5C46, 0x6292, 0x0322, 0x97CD, 0x6113, 0x3F1D, 0x2BB7,
         0xA9BA, 0xB16A, 0x3A97, 0x5807, 0x8EF9, 0x315C, 0xC2C6, 0x4C3B, 0x1ACA, 0xAA90, 0x8B4A, 0xDBF7,
         0xDBA0, 0xAFA3, 0x5DC1, 0x98A3, 0x75E6, 0x8AD2, 0x79B5, 0x8CFA, 0x8CA0, 0x3BF9, 0x8D61, 0xF8F5,
         0x40CA, 0x85E4, 0x383F, 0x36FF, 0x3B9D, 0x15E4, 0x1B6C, 0x9578, 0x478F, 0xA9C8, 0x80D9, 0x5C8E,
         0x8133, 0x540F, 0x8A4B, 0xC762, 0x0EDF, 0xCE7E, 0xFB2F, 0x287B, 0x6825, 0xD2F9, 0x8739, 0x088E,
         0x3C21, 0x8DB2, 0xB5D9, 0x5F92, 0xD48E, 0x1FA8, 0x5877, 0xDF25, 0x11AF, 0x2870, 0x8C8A, 0x4A46,
         0xFA0D, 0x6C56, 0x4F77, 0xD853, 0xD3FE, 0x7ECD, 0xA041, 0xAF66, 0xCCFD, 0x8125, 0x370F, 0x6BF5,
         0x3DD6, 0xFA9F, 0xC477, 0xB8C9, 0x6C9D, 0xAAC8, 0xC7A8, 0xF348, 0xF07B, 0x8427, 0xEE51, 0xEA01,
         0x9844, 0xA63A, 0x0E6E, 0x9A9B, 0xE6E2, 0x6FC5, 0xA4D0, 0x1106, 0x1E90, 0x9A6C, 0xE782, 0x4B01,
         0xDDE0, 0x6DA4, 0x6FFD, 0x32ED, 0x4997, 0xFCF1, 0x6626, 0xA0F9, 0xC957, 0x786F, 0x4C21, 0x7991,

         // Band 4
         0x5217, 0x8BB4, 0x4AF1, 0xD327, 0x456B, 0xCAFB, 0x18EC, 0x2C47, 0xD3A2, 0x36A4, 0x6DFC, 0xB2FF,
         0x02AD, 0xC7FE, 0x04AC, 0x9990, 0x31D5, 0x1267, 0x3113, 0x8677, 0x384D, 0xF1B8, 0x037A, 0x04C2,
         0x1082, 0x4905, 0xF5E7, 0x0CB5, 0x144D, 0xD5A8, 0x3868, 0x9CBF, 0xC4E4, 0x089F, 0x7F3A, 0x5FC2,
         0x2AAB, 0xFC3A, 0x6AB6, 0x66A3, 0xBECA, 0x7018, 0x7314, 0xEC36, 0xE597, 0xDC76, 0x74FD, 0x3270,
         0xFCCE, 0x89CA, 0xBCEE, 0x60EC, 0xF49E, 0xC7B3, 0xB164, 0x85C4, 0x7D7F, 0x2B35, 0x01E4, 0x97B0,
         0xBFDE, 0xDC45, 0x6DCA, 0x8F85, 0xAC94, 0xF5AC, 0x13E8, 0x8EEF, 0xD333, 0xF42D, 0x5AFB, 0x1792,
         0xD31C, 0x3A08, 0x5EEC, 0x5367, 0x516A, 0x98C8, 0xF9DF, 0x6C76, 0x8AAB, 0xF15A, 0x5517, 0xF2D8,
         0x656C, 0xEA81, 0x0FA8, 0x5511, 0x288E, 0xA593, 0xE9F4, 0x6AB2, 0xAD74, 0x9982, 0x1203, 0x044F,
         0x32FA, 0x663E, 0xF49F, 0x99C7, 0xB03D, 0xD060, 0x9C49, 0xFED5, 0xCB39, 0xB728, 0xAAF9, 0x2CFA,
         0x492E, 0x28CE, 0xD8AE, 0x16B0, 0x23DD, 0x8017, 0x09D5, 0x93F1, 0x1C8F, 0x954C, 0xF372, 0x5409,
         0xECF1, 0xFD72, 0x4A2A, 0xECC0, 0x00B9, 0x57D8, 0x8D10, 0xE9CE, 0xC125, 0xAFF7, 0x5044, 0xF79F,
         0x7440, 0xEB9D, 0x276C, 0x1D69, 0xA001, 0x4A6B, 0x17EE, 0x0195, 0x0E2C, 0xF8A3, 0x9708, 0x426E,
         0x5315, 0xA845, 0x27AB, 0xDB2D, 0xEA21, 0x457E, 0x7330, 0xA249, 0xE227, 0xB564, 0x02DD, 0xC521,
         0x1893, 0xA302, 0x822D, 0x6EEF, 0x0365, 0x63BB, 0x94FC, 0x5F0D, 0x10F7, 0xDFEF, 0x3170, 0xB18E,
         0x8D95, 0x9802, 0xA2C2, 0x981F, 0x22ED, 0xB4A1, 0xFDC8, 0x3D3F, 0xE243, 0x1D67, 0x3D5D, 0xA2C4,
         0xE17A, 0xB6C3, 0xE48F, 0x9DB2, 0x63F2, 0x923D, 0x2F9F, 0xDD5D, 0x972D, 0x49FC, 0xD9D8, 0x03E2,

         // Band 5
         0xE24D, 0x57AB, 0x6E2B, 0xCAEA, 0xB758, 0x8AA1, 0x2FA3, 0x3BC2, 0x0A57, 0x8461, 0x88A8, 0xFABE,
         0x463A, 0x4665, 0x0E0D, 0x99F0, 0xDA98, 0xD43F, 0x17F2, 0xFF2C, 0x5332, 0xE205, 0xDC7A, 0xE661,
         0x0B52, 0x960E, 0x2E47, 0x5B39, 0x61EF, 0x5300, 0xB6CD, 0x5914, 0x86AC, 0x9D2C, 0xCC76, 0x6F4B,
         0xD99E, 0x073E, 0xD892, 0x7CC4, 0xD2E6, 0x323A, 0x4716, 0x6ED9, 0x7D1B, 0xDBC7, 0x1032, 0x2490,
         0x8585, 0xFFD0, 0xC4AE, 0x4B1C, 0xD224, 0x076F, 0x321F, 0x5DB5, 0xB288, 0x062C, 0x9DE8, 0xA7BD,
         0x9582, 0x0E88, 0x8009, 0x5E36, 0x5A94, 0x8BDF, 0xDBC3, 0xC788, 0x2941, 0xB290, 0x2009, 0x7192,
         0xDE24, 0x0580, 0x9EC0, 0x7D18, 0x07DA, 0xDFE4, 0x8AD4, 0xEE6F, 0x6DBB, 0x135E, 0x9115, 0x1C8B,
         0x2F66, 0x9F6C, 0xFAEB, 0x224D, 0x7216, 0x6628, 0x5BD8, 0x692F, 0x96CE, 0x0650, 0xDBC8, 0x4236,
         0x0854, 0xB2A7, 0x0A35, 0x8836, 0x98F9, 0x26A3, 0x3D14, 0x5C6C, 0x6931, 0xA267, 0x899B, 0xEC60,
         0x68FE, 0xFD15, 0x41D0, 0x481B, 0x5B2B, 0xC469, 0x0DEC, 0x4BAF, 0x7D53, 0x66A7, 0x8B89, 0x9308,
         0xA5BB, 0x79D5, 0x83AB, 0x8917, 0x06FC, 0x0652, 0xAF89, 0x7542, 0x797F, 0xE8A7, 0x0632, 0xAB2D,
         0x59C0, 0x3FC0, 0xA601, 0xBDD0, 0xEB6E, 0xEC60, 0x47DC, 0xC6D8, 0x5F51, 0x22F6, 0x3B43, 0xC163,
         0x5A05, 0x04B7, 0x0437, 0xF0FF, 0x0C82, 0x5809, 0x79E8, 0x5205, 0xE478, 0x4347, 0x7636, 0x2E3A,
         0xC676, 0x19C9, 0x2006, 0xA7C9, 0xD8E4, 0x4042, 0xBD5E, 0x6784, 0xDCCE, 0x1972, 0x105D, 0x517D,
         0x1F7C, 0x071F, 0x55F7, 0x50E8, 0xF1DB, 0x7862, 0xBD8A, 0x2855, 0xB2B9, 0x0848, 0x863D, 0x6132,
         0x6FCD, 0xB1BF, 0x9C34, 0x3DA2, 0x0A8F, 0x08CF, 0xCF8F, 0xC0A0, 0xF9E3, 0x8D2F, 0x953D, 0xC87B,

         // Band 6
         0x8D94, 0x0D1B, 0x5C9E, 0x3492, 0xD39C, 0x1086, 0x71F4, 0x1B71, 0xFD3B, 0x5796, 0x71A1, 0x1436,
         0x5DE5, 0x6C72, 0x4E41, 0x8E41, 0xEFFA, 0x4460, 0xDF81, 0x383F, 0x784C, 0xED32, 0x08D9, 0x757E,
         0x317F, 0x4B01, 0x6D08, 0xE790, 0xFF2D, 0xEC40, 0xB26B, 0x0745, 0x43E1, 0xE414, 0x5217, 0xCAEC,
         0x31E3, 0xBF04, 0xFED2, 0x58EC, 0xC2CD, 0x7FF7, 0x99D0, 0xD8A9, 0x26F8, 0xA982, 0xB245, 0x43B5,
         0xD8B6, 0x6B82, 0x9DC8, 0x4B5D, 0x3858, 0xC60E, 0x1484, 0x5877, 0xB10A, 0xD0B0, 0x6E38, 0x8B8F,
         0x7B7C, 0x02F6, 0x5F0C, 0xD456, 0xE35A, 0x994F, 0x4B2D, 0x206A, 0x289B, 0x0338, 0x2E45, 0x8A6D,
         0xE491, 0x63B7, 0x0BB7, 0xA567, 0x0DDD, 0x1B2D, 0xF0B2, 0xD580, 0x7921, 0x7171, 0x8E17, 0xB301,
         0x018A, 0x3A3D, 0x5522, 0x88AE, 0x2531, 0x9CED, 0x30F3, 0xD366, 0x4E38, 0xDB8C, 0xC0DD, 0xE01E,
         0x97D1, 0x3E31, 0x3786, 0x6F23, 0x28FD, 0xF6AD, 0xB6DA, 0x70B1, 0x202A, 0x2787, 0x43CB, 0xC2D7,
         0x159C, 0xEE4D, 0x4EE7, 0x15B0, 0x1CA9, 0x8A2E, 0xBFB2, 0xC7E4, 0x64BB, 0x84EA, 0xA1E0, 0xE37F,
         0x6633, 0xF10C, 0x4F4E, 0x271A, 0x9B0C, 0xC37D, 0x3CDC, 0x184A, 0xC957, 0x285B, 0x4206, 0x2E6D,
         0xE382, 0xF52D, 0x8C5C, 0x09B7, 0x6D77, 0x2E5A, 0x01C2, 0xB8A1, 0x7E7F, 0x8EFD, 0x517E, 0x1295,
         0x48FF, 0x2AFE, 0x801F, 0x1FFB, 0x3403, 0x1F80, 0x0E2A, 0x8A8B, 0x8F93, 0x54A0, 0xAB9E, 0x2FF2,
         0xC2DF, 0x4683, 0x7844, 0xAECB, 0x233B, 0xEAB1, 0xDBD1, 0x15DF, 0x4CE3, 0x9CC3, 0xE5D9, 0x95BF,
         0x0799, 0x1D63, 0x428F, 0x4CAA, 0xCC0D, 0xA698, 0xBF56, 0x1DBD, 0xCA15, 0x0462, 0x5C22, 0x927E,
         0x83BB, 0xC8A9, 0xF1AE, 0xDCAF, 0xF810, 0x887B, 0x6077, 0xD376, 0x66DB, 0x2F99, 0x5992, 0x11CE,

         // Band 7
         0x8E0F, 0x5854, 0xAF4D, 0x1B50, 0x9418, 0xC5BA, 0x379E, 0x9448, 0x67F1, 0xD715, 0x4369, 0x8919,
         0xBF10, 0x22B8, 0x9E8A, 0xC4F6, 0xA91C, 0x0C1D, 0x22C0, 0x39E2, 0xA17C, 0x7951, 0xE057, 0x790B,
         0x3CAB, 0x91B1, 0xCDAE, 0x3A6B, 0x6967, 0x89FB, 0x0684, 0xF1C2, 0x35A4, 0x87A8, 0xAB1E, 0x79D6,
         0x2D5B, 0x8E9C, 0x3938, 0xC60F, 0x4620, 0x8022, 0x82C2, 0xB456, 0x5990, 0x2F39, 0x3378, 0xD850,
         0x3183, 0x7633, 0xE13E, 0x6BE0, 0x211E, 0x70A1, 0xB254, 0x40FD, 0x35A6, 0xB393, 0x8E57, 0xCCD9,
         0xEB2D, 0xA222, 0xE515, 0x3E56, 0x800A, 0xC950, 0xFE29, 0xA5C9, 0xC823, 0x5135, 0xDF72, 0x2B14,
         0x9E07, 0x7C7E, 0xBE51, 0x5707, 0xDDD6, 0x3635, 0x04BC, 0x691A, 0xEAFC, 0xB5E5, 0xE420, 0xC06E,
         0xD4BB, 0x2705, 0x7B0F, 0x5224, 0xFB81, 0x71B4, 0x83C9, 0x3107, 0x5016, 0x04C9, 0x9B83, 0x2779,
         0x1A92, 0xB32E, 0x1693, 0x59BF, 0x582A, 0xAB90, 0x605B, 0x0295, 0xAAC6, 0x6E5A, 0xF30A, 0x3813,
         0xCB6B, 0xE507, 0xD834, 0xC6E2, 0x9F78, 0x7EB7, 0xBB2B, 0x10C2, 0xCDAB, 0x5A28, 0x9137, 0x0E5F,
         0xEAFC, 0x90E8, 0xC896, 0x517A, 0x384A, 0x7BEB, 0x0E4B, 0x1C59, 0xECCD, 0x1765, 0xA0C1, 0x8B8F,
         0x0E69, 0x76F6, 0x323E, 0xCB12, 0xE2BE, 0x3A2A, 0x6022, 0x60A0, 0xE518, 0x2749, 0xB3FB, 0x7F76,
         0x5927, 0xC46C, 0x3364, 0x7253, 0x5C86, 0x05F4, 0x8BB8, 0x14CD, 0x9A19, 0x1341, 0xBC95, 0x53FB,
         0x8532, 0x13C4, 0x6329, 0xCC65, 0x208C, 0x0B58, 0x8957, 0x734F, 0x5B18, 0xD0F2, 0x09A8, 0x4648,
         0x0190, 0x04A1, 0x820D, 0x1616, 0x2EE5, 0x29D6, 0xDC6E, 0x64E6, 0x627A, 0xBA06, 0x590F, 0x3FD8,
         0x1426, 0x6494, 0x3DBF, 0x43CC, 0xE618, 0x3D12, 0xFCDA, 0x9F8F, 0x5A76, 0x18CD, 0x0119, 0x294D,

         // Band 8
         0x20DF, 0xE2A9, 0x0039, 0x8A56, 0xF3AF, 0x1350, 0xDC66, 0x7237, 0x001C, 0x30B1, 0x1780, 0xE31A,
         0xE71B, 0x54CA, 0xD82B, 0x8481, 0x4321, 0xD6C7, 0x7DB3, 0x0D47, 0xC7A4, 0xF373, 0xBEB8, 0xBCFB,
         0xE77A, 0x93EE, 0x66B9, 0xDD7D, 0x1800, 0x15C2, 0x8C5A, 0x61FE, 0x9617, 0x2F3B, 0x738E, 0x863C,
         0xC5E9, 0xDC1A, 0xE185, 0x901D, 0x1983, 0x618D, 0x1D70, 0x8D94, 0x9244, 0x567E, 0x6D0E, 0x2FD4,
         0xC80E, 0xC933, 0x2A12, 0xB5D9, 0x8657, 0x6D3B, 0x6451, 0xDE38, 0xF7FF, 0xD8A7, 0x17C7, 0xF54D,
         0x60F9, 0xD79C, 0xEC66, 0xE4A8, 0x6BC5, 0xC632, 0x8EB9, 0x60D6, 0x05BC, 0x069B, 0x8D52, 0x1E7A,
         0xC627, 0x7BA7, 0xD212, 0x1CAB, 0xF026, 0x1A8E, 0xA134, 0xF6B0, 0xF871, 0x890D, 0x2F32, 0x4F02,
         0xA3D9, 0xCED7, 0xC475, 0x49A4, 0xB0A9, 0x114F, 0x6EDA, 0x0EC5, 0x15C8, 0x6898, 0x45FC, 0x61B9,
         0xC9BA, 0xBBEF, 0x405F, 0x4F3C, 0x2363, 0xAD5E, 0x965A, 0xDE0B, 0xC1A7, 0xA3B3, 0xB2DA, 0xD9C5,
         0x04CD, 0xCCD4, 0xB9F9, 0xA722, 0x1FBB, 0x4857, 0x9A57, 0x317A, 0xAF03, 0x5273, 0xB453, 0xDD9B,
         0xEBB8, 0x803B, 0x0805, 0x90EB, 0x5717, 0x162F, 0xFE18, 0xCADF, 0x1B06, 0x6216, 0xB765, 0xCBC5,
         0xD356, 0x2D2D, 0xF268, 0xCDD0, 0x03E5, 0x3AA3, 0x7D7E, 0x4A89, 0x0E7F, 0xD86B, 0x3CFB, 0x5279,
         0xC59D, 0x7B54, 0xB808, 0xF538, 0x7DED, 0x7478, 0x4955, 0xB9BE, 0xC2AD, 0xA9FC, 0xC39E, 0x2306,
         0x8ACF, 0x6A1D, 0xBCF7, 0x4C0B, 0x08FA, 0x4E89, 0x61F0, 0x8C06, 0x044E, 0x2113, 0xD78A, 0x2E08,
         0xC501, 0xE7AB, 0x30F1, 0x39DE, 0x91C6, 0xE9AE, 0xEE15, 0x4845, 0xB407, 0xD686, 0x1807, 0x7173,
         0x1FF1, 0xF191, 0xDA2A, 0x3DEA, 0x8F48, 0x5166, 0xBD39, 0xAE9E, 0x4E18, 0x325C, 0x6515, 0x596A };

      if (runTest(numRows, numColumns, numBands, INT4SBYTES, pData, failure) == false)
      {
         return false;
      }
   }

   return true;
}

bool Iarr::getInputSpecification(PlugInArgList*& pArgList)
{
   // Input arguments
   pArgList = Service<PlugInManagerServices>()->getPlugInArgList();
   VERIFY(pArgList != NULL);
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg()));
   VERIFY(pArgList->addArg<RasterElement>(Executable::DataElementArg()));

   // Batch arguments
   if (isBatch() == true)
   {
      VERIFY(pArgList->addArg<Filename>("Input Filename"));
      VERIFY(pArgList->addArg<AoiElement>("AOI Element"));
      VERIFY(pArgList->addArg<unsigned int>("Row Step Factor", mRowStepFactor));
      VERIFY(pArgList->addArg<unsigned int>("Column Step Factor", mColumnStepFactor));

      VERIFY(pArgList->addArg<EncodingType>("Output Data Type", mOutputDataType));
      VERIFY(pArgList->addArg<bool>("In Memory", mInMemory));

      VERIFY(pArgList->addArg<bool>("Display Results", mDisplayResults));
      VERIFY(pArgList->addArg<Filename>("Output Filename"));
   }

   return true;
}

bool Iarr::getOutputSpecification(PlugInArgList*& pArgList)
{
   // Output arguments
   pArgList = Service<PlugInManagerServices>()->getPlugInArgList();
   VERIFY(pArgList != NULL);
   VERIFY(pArgList->addArg<SpatialDataView>(Executable::ViewArg()));
   VERIFY(pArgList->addArg<RasterElement>(Executable::DataElementArg()));

   return true;
}

bool Iarr::execute(PlugInArgList* pInputArgList, PlugInArgList* pOutputArgList)
{
   StepResource pStep("Execute " + getName(), "spectral", "E5E0B5A3-7469-4cc5-A14F-97DA59196751");
   VERIFY(pStep.get() != NULL);

   // Extract all input arguments
   bool success = extractAndValidateInputArguments(pInputArgList);

   // Create the ErrorLog here so that mpProgress is initialized
   ErrorLog errorLog(pStep.get(), mpProgress);

   if (success == false)
   {
      errorLog.setError("Unable to extract and validate arguments.");
      return false;
   }

   // Create a new window for output
   // This work is done here so that if it fails the user is informed before actually running the algorithm
   SpatialDataView* pSpatialDataView = NULL;
   SpatialDataWindow* pSpatialDataWindow = NULL;
   if (Service<ApplicationServices>()->isBatch() == false && mDisplayResults == true)
   {
      pSpatialDataWindow = createWindow(mpInputRasterElement->getName() + " - " + getName());
      if (pSpatialDataWindow == NULL)
      {
         errorLog.setError("Unable to create a new window.");
         return false;
      }
   }

   // Run the algorithm
   RasterElement* pOutputRasterElement = runAlgorithm();
   if (pOutputRasterElement == NULL)
   {
      Service<DesktopServices>()->deleteWindow(pSpatialDataWindow);
      errorLog.setError("Unable to run the algorithm.");
      return false;
   }

   // Create a layer and display the results in pSpatialDataWindow
   if (pSpatialDataWindow != NULL)
   {
      pSpatialDataView = pSpatialDataWindow->getSpatialDataView();
      VERIFY(pSpatialDataView != NULL);
      UndoLock undoLock(pSpatialDataView);
      if (pSpatialDataView->setPrimaryRasterElement(pOutputRasterElement) == false ||
         pSpatialDataView->createLayer(RASTER, pOutputRasterElement) == NULL)
      {
         errorLog.addWarning("Unable to display the results.");
      }
   }

   if (pOutputArgList != NULL)
   {
      VERIFY(pOutputArgList->setPlugInArgValue(ViewArg(), pSpatialDataView) == true);
      VERIFY(pOutputArgList->setPlugInArgValue(DataElementArg(), pOutputRasterElement) == true);
   }

   return true;
}

bool Iarr::extractAndValidateInputArguments(PlugInArgList* pInputArgList)
{
   StepResource pStep("Extract and validate input arguments", "spectral", "8EE887EB-49E8-4746-8417-9E3E330E4CE8");
   VERIFY(pStep.get() != NULL);

   mpProgress = pInputArgList->getPlugInArgValue<Progress>(Executable::ProgressArg());
   ErrorLog errorLog(pStep.get(), mpProgress);

   mpInputRasterElement = pInputArgList->getPlugInArgValue<RasterElement>(Executable::DataElementArg());
   if (mpInputRasterElement == NULL)
   {
      errorLog.setError("No Raster Element was specified.");
      return false;
   }

   RasterDataDescriptor* pInputRasterDataDescriptor =
      dynamic_cast<RasterDataDescriptor*>(mpInputRasterElement->getDataDescriptor());
   if (pInputRasterDataDescriptor == NULL)
   {
      errorLog.setError("Unable to access the Raster Data Descriptor from the specified Raster Element.");
      return false;
   }

   const EncodingType inputDataType = pInputRasterDataDescriptor->getDataType();
   if (inputDataType == INT4SCOMPLEX || inputDataType == FLT8COMPLEX)
   {
      errorLog.setError(getName() + " cannot be used on data sets containing complex data.");
      return false;
   }

   // If mpInputRasterElement is backed by a file, then determine a default output filename for it
   const string filename = mpInputRasterElement->getFilename();
   string defaultFilename;
   if (filename.empty() == false)
   {
      QFileInfo fileInfo(QString::fromStdString(filename));
      QFileInfo defaultFileInfo(fileInfo.absoluteDir(),
         fileInfo.completeBaseName() + QString::fromStdString(mExtension));
      defaultFilename = defaultFileInfo.absoluteFilePath().toStdString();
   }

   // If operating in batch mode, extract the remaining arguments
   if (isBatch() == true)
   {
      Filename* pInputFilename = pInputArgList->getPlugInArgValue<Filename>("Input Filename");
      if (pInputFilename != NULL)
      {
         mInputFilename = pInputFilename->getFullPathAndName();
      }
      else if (QFile::exists(QString::fromStdString(defaultFilename)) == true)
      {
         mInputFilename = defaultFilename;
      }

      mpAoiElement = pInputArgList->getPlugInArgValue<AoiElement>("AOI Element");
      VERIFY(pInputArgList->getPlugInArgValue<unsigned int>("Row Step Factor", mRowStepFactor) == true);
      VERIFY(pInputArgList->getPlugInArgValue<unsigned int>("Column Step Factor", mColumnStepFactor) == true);

      VERIFY(pInputArgList->getPlugInArgValue<EncodingType>("Output Data Type", mOutputDataType) == true);
      VERIFY(pInputArgList->getPlugInArgValue<bool>("In Memory", mInMemory) == true);
      VERIFY(pInputArgList->getPlugInArgValue<bool>("Display Results", mDisplayResults) == true);

      Filename* pOutputFilename = pInputArgList->getPlugInArgValue<Filename>("Output Filename");
      if (pOutputFilename == NULL)
      {
         if (mInputFilename.empty() == true)
         {
            mOutputFilename = defaultFilename;
         }
      }
      else
      {
         mOutputFilename = pOutputFilename->getFullPathAndName();
      }
   }
   // If operating in interactive mode, display the dialog and extract information from it
   else
   {
      Service<ModelServices> pModelServices;
      const int numRows = static_cast<int>(pInputRasterDataDescriptor->getRowCount());
      const int numColumns = static_cast<int>(pInputRasterDataDescriptor->getColumnCount());
      const bool isDouble = (inputDataType == FLT8BYTES);
      const bool inMemory = (pInputRasterDataDescriptor->getProcessingLocation() == IN_MEMORY);
      const string aoiElementString = TypeConverter::toString<AoiElement>();
      const vector<string> aoiNames = pModelServices->getElementNames(mpInputRasterElement, aoiElementString);

      IarrDlg inputDialog(defaultFilename, aoiNames, numRows, numColumns,
         isDouble, inMemory, Service<DesktopServices>()->getMainWidget());
      if (inputDialog.exec() == QDialog::Rejected)
      {
         errorLog.aborted();
         return false;
      }

      mInputFilename = inputDialog.getInputFilename().toStdString();

      mpAoiElement = dynamic_cast<AoiElement*>(pModelServices->getElement(
         inputDialog.getAoiName().toStdString(), aoiElementString, mpInputRasterElement));

      mRowStepFactor = inputDialog.getRowStepFactor();
      mColumnStepFactor = inputDialog.getColumnStepFactor();

      mOutputDataType = inputDialog.getOutputDataType();
      mInMemory = (inputDialog.getProcessingLocation() == IN_MEMORY);
      mOutputFilename = inputDialog.getOutputFilename().toStdString();
      mDisplayResults = true;
   }

   // Additional argument validation
   const bool inputFilenameSpecified = (mInputFilename.empty() == false);
   const bool outputFilenameSpecified = (mOutputFilename.empty() == false);
   const bool aoiSpecified = (mpAoiElement != NULL);
   const bool stepFactorsSpecified = (mRowStepFactor != 1 || mColumnStepFactor != 1);
   if (inputFilenameSpecified == true)
   {
      if (QFile::exists(QString::fromStdString(mInputFilename)) == false)
      {
         errorLog.setError("An invalid Input Filename was specified.");
         return false;
      }

      if (outputFilenameSpecified == true)
      {
         errorLog.setError("An Output File cannot be created if an Input Filename is specified.");
         return false;
      }

      if (aoiSpecified == true)
      {
         errorLog.setError("An AOI Element cannot be specified if an Input Filename is specified.");
         return false;
      }

      if (stepFactorsSpecified == true)
      {
         errorLog.setError("Row and Column Step Factors cannot be specified if an Input Filename is specified.");
         return false;
      }
   }

   if (stepFactorsSpecified == true)
   {
      if (aoiSpecified == true)
      {
         errorLog.setError("Row and Column Step Factors cannot be specified if an AOI Element is specified.");
         return false;
      }

      if (mRowStepFactor < 1 || mColumnStepFactor < 1)
      {
         errorLog.setError("Row and Column Step Factors must be at least 1.");
         return false;
      }
   }

   if (mOutputDataType != FLT4BYTES && mOutputDataType != FLT8BYTES)
   {
      errorLog.setError("Output Data Type must be either " + StringUtilities::toDisplayString(FLT4BYTES) +
         " or " + StringUtilities::toDisplayString(FLT8BYTES) + ".");
      return false;
   }

   return true;
}

SpatialDataWindow* Iarr::createWindow(const string& windowName)
{
   StepResource pStep("Create " + getName() + " window", "spectral", "A8DFC9C1-7577-46ef-90F4-9F0FB10F91EB");
   VERIFYRV(pStep.get() != NULL, NULL);
   ErrorLog errorLog(pStep.get(), mpProgress);

   // If an IARR window already exists, make sure that the user wants to recreate it
   // In batch mode, do not prompt the user; simply destroy the window
   Service<DesktopServices> pDesktopServices;
   SpatialDataWindow* pSpatialDataWindow = dynamic_cast<SpatialDataWindow*>
      (pDesktopServices->getWindow(windowName, SPATIAL_DATA_WINDOW));
   if (pSpatialDataWindow != NULL)
   {
      const string message = "A window containing the " + getName() + " results already exists.\n"
         "Would you like to close the existing window and run the algorithm again?";

      if (isBatch() == false &&
         QMessageBox::question(NULL, QString::fromStdString(getName()), QString::fromStdString(message),
         QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
      {
         errorLog.setError("A window containing the " + getName() + " results already exists.");
         return NULL;
      }

      if (pDesktopServices->deleteWindow(pSpatialDataWindow) == false)
      {
         errorLog.setError("Desktop Services failed to close the existing " + getName() + " results window.");
         return NULL;
      }
   }

   // Create the new window and return it
   pSpatialDataWindow = dynamic_cast<SpatialDataWindow*>
      (pDesktopServices->createWindow(windowName, SPATIAL_DATA_WINDOW));
   if (pSpatialDataWindow == NULL)
   {
      errorLog.setError("Desktop Services failed to create a window for " + getName() + " results.");
      return NULL;
   }

   return pSpatialDataWindow;
}

RasterElement* Iarr::runAlgorithm()
{
   StepResource pStep("Run " + getName() + " algorithm", "spectral", "906AD62E-00FC-4dcf-94A3-0D719020AF65");
   VERIFYRV(pStep.get() != NULL, NULL);
   ErrorLog errorLog(pStep.get(), mpProgress);

   // Create a Raster Element for output
   // This work is done here so that if it fails the user is informed before actually running the algorithm
   ModelResource<RasterElement> pOutputRasterElement(createOutputRasterElement());
   if (pOutputRasterElement.get() == NULL)
   {
      errorLog.setError("Unable to create a Raster Element.");
      return NULL;
   }

   // Calculate (or load) gains
   vector<double> gains;
   if (determineGains(gains) == false)
   {
      errorLog.setError("Unable to determine gains.");
      return NULL;
   }

   // Apply the gains to the RasterElement
   if (applyGains(gains, pOutputRasterElement.get()) == false)
   {
      errorLog.setError("Unable to apply gains.");
      return NULL;
   }

   // Update mpProgress
   if (mpProgress != NULL)
   {
      mpProgress->updateProgress("Finished.", 100, NORMAL);
   }

   return pOutputRasterElement.release();
}

RasterElement* Iarr::createOutputRasterElement()
{
   StepResource pStep("Create output Raster Element", "spectral", "F10ED0BB-AE4D-4948-A9D9-7AF87543D2D6");
   VERIFYRV(pStep.get() != NULL, NULL);
   ErrorLog errorLog(pStep.get(), mpProgress);

   const string outputRasterElementName = mpInputRasterElement->getName() + " - " + getName();

   // If an IARR Raster Element already exists, make sure that the user wants to recreate it
   // In batch mode, do not prompt the user; simply destroy the Raster Element
   Service<ModelServices> pModelServices;
   RasterElement* pExistingRasterElement = dynamic_cast<RasterElement*>
      (pModelServices->getElement(outputRasterElementName, TypeConverter::toString<RasterElement>(), NULL));
   if (pExistingRasterElement != NULL)
   {
      const string message = "A Raster Element containing the " + getName() + " results already exists.\n"
         "Would you like to destroy the existing Raster Element and run the algorithm again?";

      if (isBatch() == false &&
         QMessageBox::question(NULL, QString::fromStdString(getName()), QString::fromStdString(message),
         QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
      {
         errorLog.setError("A Raster Element containing the " + getName() + " results already exists.");
         return NULL;
      }

      if (pModelServices->destroyElement(pExistingRasterElement) == false)
      {
         errorLog.setError("Model Services failed to destroy the existing " + getName() + " Raster Element.");
         return NULL;
      }
   }

   RasterDataDescriptor* pInputRasterDataDescriptor =
      dynamic_cast<RasterDataDescriptor*>(mpInputRasterElement->getDataDescriptor());
   VERIFYRV(pInputRasterDataDescriptor != NULL, NULL);
   const unsigned int numBands = pInputRasterDataDescriptor->getBandCount();
   const unsigned int numRows = pInputRasterDataDescriptor->getRowCount();
   const unsigned int numColumns = pInputRasterDataDescriptor->getColumnCount();

   // Create a RasterElement to store the results of the calculation
   RasterElement* pOutputRasterElement = RasterUtilities::createRasterElement(outputRasterElementName, numRows,
      numColumns, numBands, mOutputDataType, pInputRasterDataDescriptor->getInterleaveFormat(), mInMemory);
   if (pOutputRasterElement == NULL)
   {
      // If creating a RasterElement fails in memory, try to create it on disk
      if (mInMemory == true)
      {
         mInMemory = false;
         pOutputRasterElement = RasterUtilities::createRasterElement(outputRasterElementName, numRows,
            numColumns, numBands, mOutputDataType, pInputRasterDataDescriptor->getInterleaveFormat(), mInMemory);
      }

      if (pOutputRasterElement == NULL)
      {
         errorLog.setError("Raster Utilities failed to create a Raster Element for " + getName() + " results.");
         return NULL;
      }
   }

   // Copy the metadata from mpInputRasterElement to pOutputRasterDataDescriptor
   RasterDataDescriptor* pOutputRasterDataDescriptor =
      dynamic_cast<RasterDataDescriptor*>(pOutputRasterElement->getDataDescriptor());
   if (pOutputRasterDataDescriptor == NULL)
   {
      errorLog.setError("Unable to access output Raster Data Descriptor.");
      return NULL;
   }

   pOutputRasterDataDescriptor->setMetadata(mpInputRasterElement->getMetadata());

   // Update the Units for pOutputRasterElement
   Units* pOutputUnits = pOutputRasterDataDescriptor->getUnits();
   if (pOutputUnits == NULL)
   {
      errorLog.addWarning("Unable to access output Units.");
   }
   else
   {
      pOutputUnits->setUnitType(REFLECTANCE);
   }

   return pOutputRasterElement;
}

bool Iarr::determineGains(vector<double>& gains)
{
   StepResource pStep("Determine gains", "spectral", "B36F6CDF-406F-47f1-ADF0-0492B30BEAB8");
   VERIFY(pStep.get() != NULL);
   ErrorLog errorLog(pStep.get(), mpProgress);

   RasterDataDescriptor* pInputRasterDataDescriptor =
      dynamic_cast<RasterDataDescriptor*>(mpInputRasterElement->getDataDescriptor());
   VERIFY(pInputRasterDataDescriptor != NULL);
   const unsigned int numBands = pInputRasterDataDescriptor->getBandCount();
   const unsigned int numRows = pInputRasterDataDescriptor->getRowCount();
   const unsigned int numColumns = pInputRasterDataDescriptor->getColumnCount();
   const EncodingType inputDataType = pInputRasterDataDescriptor->getDataType();

   gains.clear();
   if (mInputFilename.empty() == false)
   {
      ifstream input(mInputFilename.c_str());
      if (input.good() == false)
      {
         errorLog.addWarning("Unable to open file \"" + mInputFilename + "\". Gains will be calculated.");
      }
      else
      {
         while (input.good() == true)
         {
            double gain;
            input >> gain;
            if (input.good() == true)
            {
               gains.push_back(gain);
            }
         }

         input.close();
         if (gains.size() != numBands)
         {
            gains.clear();
            errorLog.addWarning("Input file \"" + mInputFilename + "\" contains "
               "incompatible data. Gains will be calculated.");
         }
      }
   }

   if (gains.empty() == true)
   {
      const BitMask* pBitMask = NULL;
      if (mpAoiElement != NULL)
      {
         pBitMask = mpAoiElement->getSelectedPoints();
         if (pBitMask == NULL || pBitMask->getCount() <= 0)
         {
            errorLog.addWarning("The specified AOI contains no pixels.\nThe entire image will be processed.");
            pBitMask = NULL;
         }
      }

      // Set the maximum completion percentage to 99.9% so that mpProgress does not disappear between bands
      const double calculateAverageStep = 99.9 / (numRows / mRowStepFactor);
      for (unsigned int band = 0; band < numBands; ++band)
      {
         // Build a name to identify this band
         stringstream bandName;
         bandName << "Band " << (band + 1) << "/" << (numBands);

         // Obtain a DataAccessor for this band for pInputRasterElement
         DimensionDescriptor activeBand = pInputRasterDataDescriptor->getActiveBand(band);
         if (activeBand.isValid() == false)
         {
            errorLog.setError("Active Band is invalid for " + bandName.str() + ".");
            return false;
         }

         FactoryResource<DataRequest> pRequest;
         VERIFY(pRequest.get() != NULL);
         pRequest->setBands(activeBand, activeBand);
         DataAccessor daAccessor = mpInputRasterElement->getDataAccessor(pRequest.release());
         if (daAccessor.isValid() == false)
         {
            errorLog.setError("Unable to obtain a Data Accessor for " + bandName.str() + ".");
            return false;
         }

         double percent = 0.0;
         double totalValue = 0.0;
         unsigned int totalCounted = 0;
         for (unsigned int row = 0; row < numRows && daAccessor.isValid(); row += mRowStepFactor)
         {
            // Update mpProgress
            if (mpProgress != NULL)
            {
               percent += calculateAverageStep;
               mpProgress->updateProgress(bandName.str() + ": Calculating Gain (Step 1/2)",
                  static_cast<int>(percent), NORMAL);

               // Check if the operation has been cancelled
               if (isAborted() == true)
               {
                  errorLog.aborted();
                  return false;
               }
            }

            for (unsigned int column = 0; column < numColumns && daAccessor.isValid(); column += mColumnStepFactor)
            {
               if (pBitMask == NULL || pBitMask->getPixel(column, row) == true)
               {
                  daAccessor->toPixel(row, column);
                  if (daAccessor.isValid() == false)
                  {
                     errorLog.setError("Unable to read from the Data Accessor for " + bandName.str() + ".");
                     return false;
                  }

                  totalValue += Service<ModelServices>()->getDataValue(inputDataType, daAccessor->getColumn(), 0);
                  ++totalCounted;
               }
            }
         }

#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : Use \"fabs(totalValue / totalCounted)\"? (dadkins)")
         if (totalCounted == 0)
         {
            errorLog.setError("Unable to compute an average value.\nNo pixels within the image are selected.");
            return false;
         }

         const double averageValue = totalValue / totalCounted;
         double gain = 1.0;
         if (averageValue != 0.0)
         {
            gain /= averageValue;
         }

         gains.push_back(gain);
      }
   }

   if (mOutputFilename.empty() == false)
   {
      ofstream output(mOutputFilename.c_str());
      if (output.good() == false)
      {
         errorLog.addWarning("Unable to create results file \"" + mOutputFilename + "\". "
            "Gains will be applied but will not be written to disk.");
      }
      else
      {
         output.precision(17);
         for (vector<double>::const_iterator iter = gains.begin(); iter != gains.end(); ++iter)
         {
            output << *iter << endl;
         }

         output.close();
      }
   }

   return true;
}

bool Iarr::applyGains(const vector<double>& gains, RasterElement* pOutputRasterElement)
{
   StepResource pStep("Apply gains", "spectral", "667FA584-E2D9-47fc-9714-CDBE66C57151");
   VERIFYRV(pStep.get() != NULL, NULL);
   ErrorLog errorLog(pStep.get(), mpProgress);

   RasterDataDescriptor* pInputRasterDataDescriptor =
      dynamic_cast<RasterDataDescriptor*>(mpInputRasterElement->getDataDescriptor());
   VERIFY(pInputRasterDataDescriptor != NULL);
   const unsigned int numBands = pInputRasterDataDescriptor->getBandCount();
   const unsigned int numRows = pInputRasterDataDescriptor->getRowCount();
   const unsigned int numColumns = pInputRasterDataDescriptor->getColumnCount();
   const EncodingType inputDataType = pInputRasterDataDescriptor->getDataType();
   if (gains.size() != numBands)
   {
      stringstream errorMessage;
      errorMessage << "Expected " << numBands << " gain values, but received " << gains.size() << ".";
      errorLog.setError(errorMessage.str());
      return false;
   }

   VERIFY(pOutputRasterElement != NULL);
   RasterDataDescriptor* pOutputRasterDataDescriptor =
      dynamic_cast<RasterDataDescriptor*>(pOutputRasterElement->getDataDescriptor());
   VERIFY(pOutputRasterDataDescriptor != NULL);

   // Set the maximum completion percentage to 99.9% so that mpProgress does not disappear and reappear between bands
   const double applyGainsStep = 99.9 / numRows;
   Service<ModelServices> pModelServices;
   for (unsigned int band = 0; band < numBands; ++band)
   {
      // Build a name to identify this band
      stringstream bandName;
      bandName << "Band " << (band + 1) << "/" << (numBands);

      // Display the gain in the message log
      const double gain = gains[band];
      stringstream gainString;
      gainString.precision(17);
      gainString << "Gain: " << gain;
      pStep->addProperty(bandName.str(), gainString.str());

      // Obtain a DataAccessor for this band for pInputRasterElement
      const DimensionDescriptor inputBand = pInputRasterDataDescriptor->getActiveBand(band);
      if (inputBand.isValid() == false)
      {
         errorLog.setError("Unable to obtain the active input band for " + bandName.str() + ".");
         return false;
      }

      FactoryResource<DataRequest> pSourceRequest;
      VERIFYRV(pSourceRequest.get() != NULL, NULL);
      pSourceRequest->setBands(inputBand, inputBand);
      DataAccessor sourceAccessor = mpInputRasterElement->getDataAccessor(pSourceRequest.release());
      if (sourceAccessor.isValid() == false)
      {
         errorLog.setError("Unable to obtain a source Data Accessor for " + bandName.str() + ".");
         return false;
      }

      // Obtain a writable DataAccessor for this band for pOutputRasterElement
      const DimensionDescriptor outputBand = pOutputRasterDataDescriptor->getActiveBand(band);
      if (outputBand.isValid() == false)
      {
         errorLog.setError("Unable to obtain the active output band for " + bandName.str() + ".");
         return false;
      }

      FactoryResource<DataRequest> pDestinationRequest;
      VERIFYRV(pDestinationRequest.get() != NULL, NULL);
      pDestinationRequest->setWritable(true);
      pDestinationRequest->setBands(outputBand, outputBand);
      DataAccessor destinationAccessor = pOutputRasterElement->getDataAccessor(pDestinationRequest.release());
      if (destinationAccessor.isValid() == false)
      {
         errorLog.setError("Unable to obtain a writable destination Data Accessor for " + bandName.str() + ".");
         return false;
      }

      // Apply the gain for this band to each pixel
      double percent = 0.0;
      for (unsigned int row = 0;
         row < numRows && sourceAccessor.isValid() == true && destinationAccessor.isValid() == true;
         ++row, sourceAccessor->nextRow(), destinationAccessor->nextRow())
      {
         // Update mpProgress
         if (mpProgress != NULL)
         {
            percent += applyGainsStep;
            mpProgress->updateProgress(bandName.str() + ": Applying Gain (Step 2/2)",
               static_cast<int>(percent), NORMAL);

            // Check if the operation has been cancelled
            if (isAborted() == true)
            {
               errorLog.aborted();
               return false;
            }
         }

         for (unsigned int column = 0;
            column < numColumns && sourceAccessor.isValid() == true && destinationAccessor.isValid() == true;
            ++column, sourceAccessor->nextColumn(), destinationAccessor->nextColumn())
         {
            // Copy the value from pInputRasterElement to mpOutputRasterElement, scaling appropriately
            const double sourceData = pModelServices->getDataValue(inputDataType, sourceAccessor->getColumn(), 0);
            switchOnEncoding(mOutputDataType, writeData, destinationAccessor->getColumn(), sourceData * gain);
         }
      }
   }

   pOutputRasterElement->updateData();
   return true;
}

Iarr::ErrorLog::ErrorLog(Step* pStep, Progress* pProgress) :
   mpStep(pStep),
   mpProgress(pProgress),
   mFinalized(false)
{

}

Iarr::ErrorLog::~ErrorLog()
{
   if (mFinalized == false && mpStep != NULL)
   {
      mpStep->finalize();
   }
}

void Iarr::ErrorLog::aborted()
{
   const string message = "The user cancelled the operation.";
   if (mpProgress != NULL)
   {
      mpProgress->updateProgress(message, 0, ABORT);
   }

   if (mpStep != NULL)
   {
      VERIFYNR(mFinalized == false);
      mpStep->finalize(Message::Abort, message);
      mFinalized = true;
   }
}

void Iarr::ErrorLog::setError(const std::string& message)
{
   Message::Result result = Message::Failure;
   if (mpProgress != NULL)
   {
      string text;
      int percent;
      ReportingLevel granularity;
      mpProgress->getProgress(text, percent, granularity);
      if (granularity == NORMAL || granularity == WARNING)
      {
         mpProgress->updateProgress(message, 0, ERRORS);
      }
      else if (granularity == ABORT)
      {
         result = Message::Abort;
      }
   }

   if (mpStep != NULL)
   {
      VERIFYNR(mFinalized == false);
      mpStep->finalize(result, message);
      mFinalized = true;
   }
}

void Iarr::ErrorLog::addWarning(const std::string& message)
{
   if (mpProgress != NULL)
   {
      string text;
      int percent;
      ReportingLevel granularity;
      mpProgress->getProgress(text, percent, granularity);
      if (granularity == NORMAL || granularity == WARNING)
      {
         mpProgress->updateProgress(message, percent, WARNING);
      }
   }
}

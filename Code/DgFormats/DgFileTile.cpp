/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "DgFileTile.h"
#include "StringUtilities.h"
#include "xmlreader.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

XERCES_CPP_NAMESPACE_USE 

std::vector<DgFileTile> DgFileTile::getTiles(XERCES_CPP_NAMESPACE_QUALIFIER DOMDocument* pDocument,
   const std::string& filename,
   unsigned int& height,
   unsigned int& width)
{
   std::vector<DgFileTile> tiles;
   DOMElement* pRoot = pDocument->getDocumentElement();
   if (pRoot == NULL || !XMLString::equals(pRoot->getNodeName(), X("isd")))
   {
      return tiles;
   }

   DOMNodeList* pTilList = pRoot->getElementsByTagName(X("TIL"));
   if (pTilList == NULL || pTilList->getLength() != 1)
   {
      return tiles;
   }
   DOMNode* pTil = pTilList->item(0);
   if (pTil == NULL || pTil->getNodeType() != DOMNode::ELEMENT_NODE)
   {
      return tiles;
   }
   DOMElement* pTilElement = static_cast<DOMElement*>(pTil);
   DOMNodeList* pTilesList = pTilElement->getElementsByTagName(X("TILE"));
   if (pTilesList == NULL)
   {
      return tiles;
   }
   height = 0;
   width = 0;
   bool error = false;
   QFileInfo fileInfo(QString::fromStdString(filename));
   QDir fileDir = fileInfo.dir();
   for (unsigned int i = 0; i < pTilesList->getLength(); ++i)
   {
      DgFileTile curTile;
      DOMNode* pNode = pTilesList->item(i);
      if (pNode == NULL || pNode->getNodeType() != DOMNode::ELEMENT_NODE)
      {
         continue;
      }
      DOMElement* pElement = static_cast<DOMElement*>(pNode);
      DOMElement* pChildElement = pElement->getFirstElementChild();
      while (pChildElement != NULL)
      {
         std::string textContent = A(pChildElement->getTextContent());
         if (XMLString::equals(pChildElement->getNodeName(), X("FILENAME")))
         {
            curTile.mTilFilename = fileDir.filePath(QString::fromStdString(textContent)).toStdString();
         }
         if (XMLString::equals(pChildElement->getNodeName(), X("ULCOLOFFSET")))
         {
            curTile.mStartCol = StringUtilities::fromXmlString<unsigned int>(textContent, &error);
         }
         if (XMLString::equals(pChildElement->getNodeName(), X("ULROWOFFSET")))
         {
            curTile.mStartRow = StringUtilities::fromXmlString<unsigned int>(textContent, &error);
         }
         if (XMLString::equals(pChildElement->getNodeName(), X("LRCOLOFFSET")))
         {
            curTile.mEndCol = StringUtilities::fromXmlString<unsigned int>(textContent, &error);
         }
         if (XMLString::equals(pChildElement->getNodeName(), X("LRROWOFFSET")))
         {
            curTile.mEndRow = StringUtilities::fromXmlString<unsigned int>(textContent, &error);
         }
         pChildElement = pChildElement->getNextElementSibling();
      }
      tiles.push_back(curTile);
      if (curTile.mEndCol > width)
      {
         width = curTile.mEndCol;
      }
      if (curTile.mEndRow > height)
      {
         height = curTile.mEndRow;
      }
   }
   return tiles;
}
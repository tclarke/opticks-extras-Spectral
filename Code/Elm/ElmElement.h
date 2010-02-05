/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef ELMELEMENT_H
#define ELMELEMENT_H

#include "AoiLayer.h"
#include "AttachmentPtr.h"
#include "SpatialDataView.h"

class AoiElement;
class Signature;

class ElmElement
{
public:
   ElmElement(SpatialDataView* mpView);
   ~ElmElement();

   Signature* getSignature() const;
   void setSignature(Signature* pSignature);

   AoiElement* getAoiElement();

   void showLayer();
   void hideLayer();

private:
   Signature* mpSignature;
   AttachmentPtr<AoiLayer> mpAoiLayer;
   AttachmentPtr<SpatialDataView> mpView;

   bool createAoiLayer(bool activate = true);
   bool deleteAoiLayer();
};

#endif

/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AoiElement.h"
#include "AppVerify.h"
#include "ElmElement.h"
#include "Signature.h"
#include "SpatialDataView.h"

ElmElement::ElmElement(SpatialDataView* pView) :
   mpView(pView),
   mpSignature(NULL)
{
   VERIFYNRV(createAoiLayer());
}

ElmElement::~ElmElement()
{
   VERIFYNRV(deleteAoiLayer());
}

Signature* ElmElement::getSignature() const
{
   return mpSignature;
}

void ElmElement::setSignature(Signature* pSignature)
{
   mpSignature = pSignature;
}

AoiElement* ElmElement::getAoiElement()
{
   if (createAoiLayer(false) == true)
   {
      return dynamic_cast<AoiElement*>(mpAoiLayer->getDataElement());
   }

   return NULL;
}

void ElmElement::showLayer()
{
   if (createAoiLayer() == true)
   {
      mpView->showLayer(dynamic_cast<Layer*>(mpAoiLayer.get()));
   }
}

void ElmElement::hideLayer()
{
   if (createAoiLayer(false) == true)
   {
      mpView->hideLayer(dynamic_cast<Layer*>(mpAoiLayer.get()));
   }
}

bool ElmElement::createAoiLayer(bool activate)
{
   if (mpView.get() == NULL)
   {
      return false;
   }

   if (mpAoiLayer.get() == NULL)
   {
      // Create an AoiLayer and attach to it.
      mpAoiLayer.reset(dynamic_cast<AoiLayer*>(mpView->createLayer(AOI_LAYER, NULL)));
      VERIFY(mpAoiLayer.get() != NULL);
   }

   if (activate == true)
   {
      mpView->setActiveLayer(mpAoiLayer.get());
      mpView->setMouseMode("LayerMode");
   }

   return true;
}

bool ElmElement::deleteAoiLayer()
{
   if (mpView.get() != NULL)
   {
      if (mpAoiLayer.get() != NULL)
      {
         VERIFY(mpView->deleteLayer(dynamic_cast<Layer*>(mpAoiLayer.get())));
      }

      mpView->setMouseMode(NULL);
   }

   return (mpAoiLayer.get() == NULL);
}

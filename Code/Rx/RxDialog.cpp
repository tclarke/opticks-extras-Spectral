/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "RxDialog.h"
#include <QtCore/QList>
#include <QtCore/QPair>

RxDialog::RxDialog(QWidget* pParent) : QDialog(pParent)
{
   setupUi(this);
}

RxDialog::~RxDialog()
{
}

void RxDialog::setAoiList(const QList<QPair<QString, QString> >& aois)
{
   for (QList<QPair<QString, QString> >::const_iterator aoi = aois.begin(); aoi != aois.end(); ++aoi)
   {
      mpAoi->addItem(aoi->first, aoi->second);
   }
}

void RxDialog::setThreshold(double threshold)
{
   mpThreshold->setValue(threshold);
}

void RxDialog::setAoi(const QString& sessionId)
{
   int idx = mpAoi->findData(sessionId);
   mpAoi->setCurrentIndex(idx < 0 ? 0 : idx);
}

void RxDialog::setLocal(bool enabled)
{
   mpLocalGroup->setChecked(enabled);
}

void RxDialog::setLocalSize(unsigned int width, unsigned int height)
{
   mpGroupWidth->setValue(width);
   mpGroupHeight->setValue(height);
}

void RxDialog::setSubspace(bool enabled)
{
   mpSubspaceGroup->setChecked(enabled);
}

void RxDialog::setSubspaceComponents(unsigned int components)
{
   mpComponents->setValue(components);
}

double RxDialog::getThreshold() const
{
   return mpThreshold->value();
}

QString RxDialog::getAoi() const
{
   QVariant id = mpAoi->itemData(mpAoi->currentIndex());
   if (id.isValid())
   {
      return id.toString();
   }
   return QString();
}

bool RxDialog::isLocal() const
{
   return mpLocalGroup->isChecked();
}

void RxDialog::getLocalSize(unsigned int& width, unsigned int& height) const
{
   width = mpGroupWidth->value();
   height = mpGroupHeight->value();
}

bool RxDialog::isSubspace() const
{
   return mpSubspaceGroup->isChecked();
}

unsigned int RxDialog::getSubspaceComponents() const
{
   return mpComponents->value();
}

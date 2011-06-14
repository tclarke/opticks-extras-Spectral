/*
 * The information in this file is
 * Copyright(c) 2011 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "TadDialog.h"
#include <QtCore/QList>
#include <QtCore/QPair>

TadDialog::TadDialog(QWidget* pParent) : QDialog(pParent)
{
   setupUi(this);
}

TadDialog::~TadDialog()
{
}

void TadDialog::setAoiList(const QList<QPair<QString, QString> >& aois)
{
   for (QList<QPair<QString, QString> >::const_iterator aoi = aois.begin(); aoi != aois.end(); ++aoi)
   {
      mpAoi->addItem(aoi->first, aoi->second);
   }
}

void TadDialog::setPercentBackground(double threshold)
{
   mpEdgesInBackground->setValue(threshold);
}

void TadDialog::setComponentSize(double threshold)
{
   mpComponentSize->setValue(threshold);
}

void TadDialog::setSampleSize(unsigned int size)
{
   mpSampleSize->setValue(size);
}

void TadDialog::setAoi(const QString& sessionId)
{
   int idx = mpAoi->findData(sessionId);
   mpAoi->setCurrentIndex(idx < 0 ? 0 : idx);
}

double TadDialog::getPercentBackground() const
{
   return mpEdgesInBackground->value();
}

double TadDialog::getComponentSize() const
{
   return mpComponentSize->value();
}

QString TadDialog::getAoi() const
{
   QVariant id = mpAoi->itemData(mpAoi->currentIndex());
   if (id.isValid())
   {
      return id.toString();
   }
   return QString();
}

unsigned int TadDialog::getSampleSize() const
{
   return mpSampleSize->value();
}

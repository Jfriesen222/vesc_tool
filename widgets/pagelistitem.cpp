/*
    Copyright 2016 - 2017 Benjamin Vedder	benjamin@vedder.se

    This file is part of VESC Tool.

    VESC Tool is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    VESC Tool is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

#include "pagelistitem.h"
#include "utility.h"
#include <QHBoxLayout>
#include <QPainter>

PageListItem::PageListItem(QString name,
                           QString icon,
                           QWidget *parent) : QWidget(parent)
{
    mIconLabel = new QLabel;
    mNameLabel = new QLabel;
    mChangedDot = new QLabel;
    mSpaceStart = new QSpacerItem(8, 0);
    mSelected = false;
    mIndented = false;
    mBold = false;

    // Marks pages with parameters that are not written to the VESC
    mChangedDot->setPixmap(Utility::getChangedDot());
    mChangedDot->setToolTip(tr("Changes not written to the VESC"));
    mChangedDot->hide();

    mIconLabel->setScaledContents(true);

    setName(name);
    setIcon(icon);

    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 8, 0);
    layout->setSpacing(9);

    layout->addSpacerItem(mSpaceStart);
    layout->addWidget(mIconLabel);
    layout->addWidget(mNameLabel);
    layout->addStretch();
    layout->addWidget(mChangedDot);

    this->setLayout(layout);
}

void PageListItem::setName(const QString &name)
{
    mNameLabel->setText(name);
}

void PageListItem::setIcon(const QString &path)
{
    mIcon = QPixmap(path);
    mIconLabel->setFixedSize(fontMetrics().height(), fontMetrics().height());
    updateColors();
}

QString PageListItem::name()
{
    return mNameLabel->text();
}

void PageListItem::setBold(bool bold)
{
    QFont f = mNameLabel->font();
    f.setWeight(bold ? QFont::Medium : QFont::Normal);
    mNameLabel->setFont(f);
    mBold = bold;
    updateColors();
}

void PageListItem::setIndented(bool indented)
{
    mSpaceStart->changeSize(indented ? 30 : 8, 0);
    mIndented = indented;
    update();
}

void PageListItem::setSelected(bool selected)
{
    mSelected = selected;
    updateColors();
    update();
}

// The page that is open is tinted and has a mark on the left edge. The pages of a group are under a line.
void PageListItem::paintEvent(QPaintEvent *event)
{
    (void)event;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    if (mSelected) {
        painter.setBrush(Utility::getAppQColor("accentDim"));
        painter.drawRoundedRect(QRectF(rect()).adjusted(8, 0, 0, 0), 7, 7);
        painter.setBrush(Utility::getAppQColor("accent"));
        painter.drawRoundedRect(QRectF(0, 5, 3, height() - 10), 1.5, 1.5);
    }

    if (mIndented) {
        painter.fillRect(QRectF(24, 0, 1, height()), Utility::getAppQColor("line"));
    }
}

// The icon is drawn in the color of the text, or in the accent color for the page that is open
void PageListItem::updateColors()
{
    bool top = mBold;
    QPalette p = mNameLabel->palette();
    p.setColor(QPalette::WindowText, Utility::getAppQColor(mSelected || top ? "titleText" : "lightText"));
    mNameLabel->setPalette(p);

    mIconLabel->setPixmap(Utility::tinted(mIcon, Utility::getAppQColor(mSelected ? "accentText" : top ? "lightText" : "mutedText")));
}

void PageListItem::setChanged(bool changed)
{
    mChangedDot->setVisible(changed);
}

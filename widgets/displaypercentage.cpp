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

#include "displaypercentage.h"
#include "utility.h"
#include <QPainter>
#include <QPaintEvent>
#include <cmath>

DisplayPercentage::DisplayPercentage(QWidget *parent) : QWidget(parent)
{
    mText = "";
    mIsDual = false;
    mValue = 0.0;

    mTextLast = mText;
    mIsDualLast = mIsDual;
    mValueLast = mValue;

    mTimer = new QTimer(this);
    mTimer->start(34);
    connect(mTimer, SIGNAL(timeout()), this, SLOT(timerSlot()));
}

QString DisplayPercentage::text() const
{
    return mText;
}

void DisplayPercentage::setText(const QString &text)
{
    mText = text;
}

bool DisplayPercentage::isDual() const
{
    return mIsDual;
}

void DisplayPercentage::setDual(bool hasNegative)
{
    mIsDual = hasNegative;
}

double DisplayPercentage::value() const
{
    return mValue;
}

void DisplayPercentage::setValue(double value)
{
    mValue = value;
}

void DisplayPercentage::timerSlot()
{
    if (mText != mTextLast || mIsDual != mIsDualLast || mValue != mValueLast) {
        update();

        mTextLast = mText;
        mIsDualLast = mIsDual;
        mValueLast = mValue;
    }
}

// The text over a slim level line, or the line alone when a few pixels high. With the tape property it is a
// tape of segments with the text in a readout at the right, for live inputs.
void DisplayPercentage::paintEvent(QPaintEvent *event)
{
    (void)event;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!isEnabled()) {
        painter.setOpacity(0.45);
    }

    const int w = width(), h = height();
    const double v = qBound(-1.0, mValue / 100.0, 1.0);

    if (!property("tape").toBool()) {
        const double lineH = h <= 6 ? h : 4.0;
        const double x0 = mIsDual ? w / 2.0 : (v < 0.0 ? w : 0.0);
        const double len = v * (mIsDual ? w / 2.0 : w);
        painter.setPen(Utility::getAppQColor("mutedText"));
        painter.drawText(QRectF(0, 0, w, h - lineH - 2), Qt::AlignLeft | Qt::AlignVCenter, mText);
        painter.fillRect(QRectF(0, h - lineH, w, lineH), Utility::getAppQColor("line"));
        painter.fillRect(QRectF(qMin(x0, x0 + len), h - lineH, fabs(len), lineH), Utility::getAppQColor("lightAccent"));
        if (mIsDual) {
            painter.fillRect(QRectF(x0 - 0.5, h - lineH, 1, lineH), Utility::getAppQColor("mutedText"));
        }
        return;
    }

    // It sits on a black housing in both themes, so the colors are not from the theme
    QFont font = this->font();
    font.setWeight(QFont::Medium);
    painter.setFont(font);
    const int readW = qMax(92, painter.fontMetrics().horizontalAdvance(mText) + 20);

    painter.setPen(QColor(255, 255, 255, 18));
    painter.setBrush(QColor(6, 6, 8));
    painter.drawRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 8, 8);

    QRectF read(w - readW - 3.5, 3.5, readW, h - 7);
    painter.setPen(QColor(255, 255, 255, 23));
    painter.setBrush(Qt::black);
    painter.drawRoundedRect(read, 5, 5);
    painter.setPen(QColor(244, 244, 245));
    painter.drawText(read.adjusted(0, 0, -9, 0), Qt::AlignRight | Qt::AlignVCenter, mText);

    // Segments with the pitch and colors of the bottom bar, from the zero mark in the middle when dual
    const double pitch = 4.5, segW = 3.0, segH = qMin(15.0, h - 14.0), top = (h - segH) / 2.0 - 2.0;
    const double left = 12.0, right = read.left() - 12.0;
    const double cx = mIsDual ? std::round((left + right) / 2.0) : left - 2.5;
    const int n = ((mIsDual ? (right - left) / 2.0 : right - left) - 2.5) / pitch;
    painter.setPen(Qt::NoPen);
    for (int side = mIsDual ? -1 : 1;side <= 1;side += 2) {
        for (int i = 0;i < n;i++) {
            double f = (i + 0.5) / n;
            bool lit = side * v > 0.0 && f <= fabs(v) + 0.5 / n;
            painter.setBrush(lit ? QColor(47, 183, 242) : QColor(255, 255, 255, 19));
            painter.drawRoundedRect(QRectF(side > 0 ? cx + 2.5 + i * pitch : cx - 2.5 - i * pitch - segW, top, segW, segH), 1, 1);
        }

        for (double f: {0.25, 0.5, 0.75, 1.0}) {
            painter.fillRect(QRectF(std::round(cx + side * (2.5 + f * n * pitch)), top + segH + 2, 1, 2), QColor(255, 255, 255, 71));
        }
    }

    if (mIsDual) {
        painter.fillRect(QRectF(cx - 0.5, top - 1, 1, segH + 4), QColor(255, 255, 255, 216));
    }
}

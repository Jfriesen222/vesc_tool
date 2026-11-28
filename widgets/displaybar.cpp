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

#include "displaybar.h"
#include <QPainter>
#include <cmath>

DisplayBar::DisplayBar(QWidget *parent) : QWidget(parent)
{
    mName = "Current";
    mRange = 60.0;
    mRangeNeg = 0.0;
    mVal = 0.0;
    mPeak = 0.0;
    mBraking = false;
    mUnit = " A";
    mDecimals = 2;
}

// The name, a tape of segments that fills from the zero in the middle and the value in a readout
void DisplayBar::paintEvent(QPaintEvent *event)
{
    (void)event;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // It is on the black housing of the bottom bar in both themes, so the colors are not from the theme
    const QColor cText(244, 244, 245), cMuted(140, 139, 147), cLit(47, 183, 242), cBrake(63, 212, 135), cHot(242, 181, 74);
    const int w = width(), h = height();
    // A narrow bar leaves out the name and then the segments, the unit of the value still tells the bars apart
    const int nameW = w < 230 ? 0 : 64, readW = 92;

    QFont font = this->font();
    font.setPointSizeF(font.pointSizeF() * 0.78);
    font.setWeight(QFont::Medium);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
    painter.setFont(font);
    painter.setPen(cMuted);
    painter.drawText(QRect(0, 0, nameW, h), Qt::AlignLeft | Qt::AlignVCenter, mName.toUpper());

    QRectF read(w - readW + 0.5, 1.5, readW - 1, h - 3);
    painter.setPen(QColor(255, 255, 255, 23));
    painter.setBrush(Qt::black);
    painter.drawRoundedRect(read, 5, 5);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0);
    painter.setFont(font);
    painter.setPen(cMuted);
    painter.drawText(read.adjusted(0, 1, -7, 0), Qt::AlignRight | Qt::AlignVCenter, mUnit.trimmed());
    font.setPointSizeF(this->font().pointSizeF() * 1.18);
    painter.setFont(font);
    painter.setPen(cText);
    painter.drawText(read.adjusted(0, 0, -20, 0), Qt::AlignRight | Qt::AlignVCenter, QString::number(mVal, 'f', mDecimals));

    // Segments: lit up to the value, amber in the last tenth before the limit of their side, dark past it
    const double pitch = 4.5, segW = 3.0, segH = h - 9.0, top = 2.0;
    const double cx = std::round((nameW + w - readW) / 2.0);
    const int n = ((w - readW - nameW) / 2.0 - 12.0) / pitch;
    if (n < 1) {
        return;
    }
    const double v = qBound(-1.0, mVal / mRange, 1.0);
    const double peak = qBound(-1.0, mPeak / mRange, 1.0);
    painter.setPen(Qt::NoPen);
    for (int side = -1;side <= 1;side += 2) {
        double limit = (side < 0 && mRangeNeg > 0.0) ? mRangeNeg / mRange : 1.0;
        for (int i = 0;i < n;i++) {
            double f = (i + 0.5) / n;
            bool lit = side * v > 0.0 && f <= fabs(v) + 0.5 / n;
            bool hot = f > limit * 0.9;
            QColor c = f > limit + 0.5 / n ? QColor(255, 255, 255, 7) : hot ? QColor(224, 160, 48, 66) : QColor(255, 255, 255, 19);
            if (lit) {
                c = hot ? cHot : mBraking ? cBrake : cLit;
            }
            painter.setBrush(c);
            painter.drawRoundedRect(QRectF(side > 0 ? cx + 2.5 + i * pitch : cx - 2.5 - i * pitch - segW, top, segW, segH), 1, 1);
        }
    }

    painter.fillRect(QRectF(cx - 0.5, top - 1, 1, segH + 4), QColor(255, 255, 255, 216));
    for (double f: {0.25, 0.5, 0.75, 1.0}) {
        for (int side = -1;side <= 1;side += 2) {
            painter.fillRect(QRectF(std::round(cx + side * (2.5 + f * n * pitch)), top + segH + 2, 1, 2), QColor(255, 255, 255, 71));
        }
    }

    if (fabs(peak) > fabs(v) + 0.02) {
        painter.fillRect(QRectF(cx + (peak > 0.0 ? 1 : -1) * (2.5 + fabs(peak) * n * pitch) - 1, top, 2, segH), Qt::white);
    }
}

int DisplayBar::decimals() const
{
    return mDecimals;
}

void DisplayBar::setDecimals(int decimals)
{
    mDecimals = decimals;
}

QString DisplayBar::unit() const
{
    return mUnit;
}

void DisplayBar::setUnit(const QString &unit)
{
    mUnit = unit;
}

double DisplayBar::val() const
{
    return mVal;
}

void DisplayBar::setVal(double val)
{
    mVal = val;
    // The mark of the peak falls back in about a second
    mPeak = fabs(val) > fabs(mPeak) ? val : mPeak * 0.97;
    update();
}

// The limit of the negative side, when it is lower than the range
void DisplayBar::setRangeNeg(double range)
{
    mRangeNeg = range;
    update();
}

// Braking is shown in green
void DisplayBar::setBraking(bool braking)
{
    mBraking = braking;
}

double DisplayBar::range() const
{
    return mRange;
}

void DisplayBar::setRange(double range)
{
    mRange = range;
    update();
}

QString DisplayBar::name() const
{
    return mName;
}

void DisplayBar::setName(const QString &name)
{
    mName = name;
    update();
}

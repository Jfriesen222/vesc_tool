/*
    Copyright 2026 Benjamin Vedder	benjamin@vedder.se

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

#include "sparkline.h"
#include "utility.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

Sparkline::Sparkline(QWidget *parent) : QWidget(parent)
{
    setFixedSize(74, 26);
}

void Sparkline::setColor(QColor color)
{
    mColor = color;
}

void Sparkline::setData(const QVector<double> &values)
{
    // Every value is drawn. Leaving some out makes short peaks come and go as the values move along.
    mValues = values;
    update();
}

void Sparkline::paintEvent(QPaintEvent *event)
{
    (void)event;
    if (mValues.size() < 2) {
        return;
    }

    auto range = std::minmax_element(mValues.begin(), mValues.end());
    double min = *range.first;
    double span = *range.second - min;
    QRectF r = QRectF(rect()).adjusted(3, 3, -3, -3);
    QPainterPath path;
    for (int i = 0;i < mValues.size();i++) {
        QPointF p(r.left() + r.width() * i / (mValues.size() - 1),
                  span > 1e-9 ? r.bottom() - r.height() * (mValues.at(i) - min) / span : r.center().y());
        if (i == 0) {
            path.moveTo(p);
        } else {
            path.lineTo(p);
        }
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor line = mColor;
    line.setAlphaF(0.9);
    painter.setPen(QPen(line, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(path);
    painter.setPen(Qt::NoPen);
    painter.setBrush(mColor);
    painter.drawEllipse(path.currentPosition(), 2.2, 2.2);
}

LevelBar::LevelBar(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(6);
}

void LevelBar::setLevel(double value, double limitStart, double limitEnd)
{
    mValue = value;
    mLimitStart = limitStart;
    mLimitEnd = limitEnd;
    update();
}

void LevelBar::paintEvent(QPaintEvent *event)
{
    (void)event;
    QRectF r(0, height() - 3, width(), 3);
    double scale = mLimitEnd > 0.0 ? r.width() / mLimitEnd : 0.0;
    QColor zone = Utility::getAppQColor("warning");
    zone.setAlphaF(0.45);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Utility::getAppQColor("line"));
    painter.drawRoundedRect(r, 1.5, 1.5);
    if (scale > 0.0) {
        painter.setBrush(zone);
        painter.drawRoundedRect(r.adjusted(qBound(0.0, mLimitStart * scale, r.width()), 0, 0, 0), 1.5, 1.5);
        painter.setBrush(Utility::getAppQColor("infoText"));
        painter.drawRoundedRect(QRectF(r.topLeft(), QSizeF(qBound(0.0, mValue * scale, r.width()), r.height())), 1.5, 1.5);
    }
}

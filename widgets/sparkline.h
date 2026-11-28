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

#ifndef SPARKLINE_H
#define SPARKLINE_H

#include <QWidget>
#include <QVector>

// A small trend line of the latest values, ending in a dot
class Sparkline : public QWidget
{
public:
    explicit Sparkline(QWidget *parent = nullptr);
    void setColor(QColor color);
    void setData(const QVector<double> &values);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> mValues;
    QColor mColor;
};

// A level bar on a scale up to a limit, with the zone where limiting starts
class LevelBar : public QWidget
{
public:
    explicit LevelBar(QWidget *parent = nullptr);
    void setLevel(double value, double limitStart, double limitEnd);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double mValue = 0.0;
    double mLimitStart = 0.0;
    double mLimitEnd = 0.0;
};

#endif // SPARKLINE_H

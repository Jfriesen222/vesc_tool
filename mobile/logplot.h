/*
    Copyright 2026 Jeffrey Friesen

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

#ifndef LOGPLOT_H
#define LOGPLOT_H

#include <QQuickPaintedItem>
#include <QPolygonF>
#include <QVariantList>

class LogPlot : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList series READ series WRITE setSeries NOTIFY seriesChanged)
    Q_PROPERTY(double xMin READ xMin WRITE setXMin NOTIFY rangeChanged)
    Q_PROPERTY(double xMax READ xMax WRITE setXMax NOTIFY rangeChanged)
    Q_PROPERTY(double cursor READ cursor WRITE setCursor NOTIFY cursorChanged)
    Q_PROPERTY(QRectF plotRect READ plotRect NOTIFY plotRectChanged)
    Q_PROPERTY(QString timeLabel READ timeLabel WRITE setTimeLabel NOTIFY timeLabelChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY colorsChanged)

public:
    explicit LogPlot(QQuickItem *parent = nullptr);

    QVariantList series() const;
    void setSeries(const QVariantList &series);
    double xMin() const;
    void setXMin(double value);
    double xMax() const;
    void setXMax(double value);
    double cursor() const;
    void setCursor(double value);
    QRectF plotRect() const;
    QString timeLabel() const;
    void setTimeLabel(const QString &label);
    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &color);
    QColor textColor() const;
    void setTextColor(const QColor &color);
    QColor gridColor() const;
    void setGridColor(const QColor &color);

    Q_INVOKABLE double timeAt(double x) const;
    void paint(QPainter *painter) override;

signals:
    void seriesChanged();
    void rangeChanged();
    void cursorChanged();
    void plotRectChanged();
    void timeLabelChanged();
    void colorsChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    struct PlotSeries {
        QColor color;
        int axis = 0;
        QPolygonF points;
        QVector<QPolygonF> lines;
        double minimum;
        double maximum;
    };

    QVariantList mSeriesData;
    QVector<PlotSeries> mSeries;
    double mMinimum[2] = {0.0, 0.0};
    double mMaximum[2] = {1.0, 1.0};
    bool mHasRightAxis = false;
    double mXMin = 0.0;
    double mXMax = 1.0;
    double mCursor = -1.0;
    QString mTimeLabel = "Time (s)";
    QColor mBackgroundColor = QColor("#202020");
    QColor mTextColor = QColor("#cccccc");
    QColor mGridColor = QColor("#444444");
    QQuickItem *mCursorItem;

    void updatePlot();
    void updateCursor();
};

#endif // LOGPLOT_H

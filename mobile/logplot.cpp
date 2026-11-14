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

#include "logplot.h"
#include <QPainter>
#include <QSGSimpleRectNode>
#include <cmath>
#include <limits>
#include <utility>

static void padRange(double &minimum, double &maximum)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
        minimum = 0.0;
        maximum = 1.0;
    } else {
        double padding = (maximum - minimum) * 0.05;
        if (padding == 0.0) {
            padding = qMax(1.0, std::abs(minimum) * 0.05);
        }
        minimum -= padding;
        maximum += padding;
    }
}

static QString rangeLabel(double value, double span)
{
    if (!std::isfinite(span) || span <= 0.0 || std::abs(value) >= 100000.0) {
        return QString::number(value, 'g', 4);
    }
    const int decimals = qBound(0, int(std::ceil(-std::log10(span))) + 2, 8);
    return QString::number(value, 'f', decimals);
}

static QString elapsedLabel(double value, double span)
{
    if (!std::isfinite(value) || std::abs(value) > 1.0e12) {
        return QString::number(value, 'g', 5);
    }
    const bool fractional = span < 5.0;
    const qint64 factor = fractional ? 100 : 1;
    const qint64 ticks = qRound64(std::abs(value) * factor);
    const qint64 seconds = ticks / factor;
    const qint64 hours = seconds / 3600;
    QString label;
    if (hours > 0) {
        label = QString("%1:%2:%3").arg(hours).arg((seconds / 60) % 60, 2, 10, QChar('0'))
                .arg(seconds % 60, 2, 10, QChar('0'));
    } else {
        label = QString("%1:%2").arg(seconds / 60, 2, 10, QChar('0'))
                .arg(seconds % 60, 2, 10, QChar('0'));
    }
    if (fractional) label += QString(".%1").arg(ticks % 100, 2, 10, QChar('0'));
    return value < 0.0 ? "-" + label : label;
}

class LogPlotCursor : public QQuickItem
{
public:
    explicit LogPlotCursor(QQuickItem *parent) : QQuickItem(parent)
    {
        setFlag(ItemHasContents, true);
    }

    void setColor(const QColor &color)
    {
        mColor = color;
        update();
    }

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override
    {
        auto node = static_cast<QSGSimpleRectNode *>(oldNode);
        if (!node) {
            node = new QSGSimpleRectNode;
        }
        node->setRect(boundingRect());
        node->setColor(mColor);
        return node;
    }

private:
    QColor mColor;
};

LogPlot::LogPlot(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setOpaquePainting(true);
    auto cursor = new LogPlotCursor(this);
    cursor->setColor(mTextColor);
    cursor->setVisible(false);
    cursor->setZ(1.0);
    mCursorItem = cursor;
}

QVariantList LogPlot::series() const
{
    return mSeriesData;
}

void LogPlot::setSeries(const QVariantList &series)
{
    mSeriesData = series;
    mSeries.clear();
    const QColor colors[] = {QColor("#42a5f5"), QColor("#ffb74d"),
                             QColor("#66bb6a"), QColor("#ef5350")};
    for (const QVariant &entry : series) {
        const QVariantMap data = entry.toMap();
        PlotSeries item;
        double scale = data.value("scale", 1.0).toDouble();
        if (!std::isfinite(scale) || scale <= 0.0) {
            scale = 1.0;
        }
        item.axis = data.value("axis").toInt() == 1 ? 1 : 0;
        item.color = data.value("color").value<QColor>();
        if (!item.color.isValid()) {
            item.color = colors[mSeries.size() % 4];
        }
        item.minimum = std::numeric_limits<double>::infinity();
        item.maximum = -std::numeric_limits<double>::infinity();
        const QVariantList points = data.value("points").toList();
        item.points.reserve(points.size());
        for (const QVariant &value : points) {
            QPointF point(qQNaN(), qQNaN());
            if (value.canConvert<QPointF>()) {
                point = value.toPointF();
            } else {
                const QVariantMap coordinates = value.toMap();
                bool xOk = false;
                bool yOk = false;
                double x = coordinates.value("x").toDouble(&xOk);
                double y = coordinates.value("y").toDouble(&yOk);
                if (xOk && yOk) {
                    point = QPointF(x, y);
                }
            }
            point.setY(point.y() * scale);
            item.points.append(point);
            if (std::isfinite(point.x()) && std::isfinite(point.y())) {
                item.minimum = qMin(item.minimum, point.y());
                item.maximum = qMax(item.maximum, point.y());
            }
        }
        bool minOk = false;
        bool maxOk = false;
        const double minimum = data.value("minimum").toDouble(&minOk) * scale;
        const double maximum = data.value("maximum").toDouble(&maxOk) * scale;
        if (minOk && maxOk && std::isfinite(minimum) && std::isfinite(maximum)
                && minimum <= maximum) {
            item.minimum = minimum;
            item.maximum = maximum;
        }
        mSeries.append(item);
    }
    updatePlot();
    emit seriesChanged();
}

double LogPlot::xMin() const
{
    return mXMin;
}

void LogPlot::setXMin(double value)
{
    if (!std::isfinite(value) || mXMin == value) {
        return;
    }
    mXMin = value;
    updatePlot();
    emit rangeChanged();
}

double LogPlot::xMax() const
{
    return mXMax;
}

void LogPlot::setXMax(double value)
{
    if (!std::isfinite(value) || mXMax == value) {
        return;
    }
    mXMax = value;
    updatePlot();
    emit rangeChanged();
}

double LogPlot::cursor() const
{
    return mCursor;
}

void LogPlot::setCursor(double value)
{
    if (mCursor == value || (std::isnan(mCursor) && std::isnan(value))) {
        return;
    }
    mCursor = value;
    updateCursor();
    emit cursorChanged();
}

QRectF LogPlot::plotRect() const
{
    const double right = mHasRightAxis ? 48.0 : 12.0;
    return QRectF(48.0, 12.0, qMax(0.0, width() - 48.0 - right), qMax(0.0, height() - 56.0));
}

QString LogPlot::timeLabel() const
{
    return mTimeLabel;
}

void LogPlot::setTimeLabel(const QString &label)
{
    if (mTimeLabel == label) {
        return;
    }
    mTimeLabel = label;
    update();
    emit timeLabelChanged();
}

QColor LogPlot::backgroundColor() const
{
    return mBackgroundColor;
}

void LogPlot::setBackgroundColor(const QColor &color)
{
    if (mBackgroundColor == color) {
        return;
    }
    mBackgroundColor = color;
    setOpaquePainting(color.alpha() == 255);
    update();
    emit colorsChanged();
}

QColor LogPlot::textColor() const
{
    return mTextColor;
}

void LogPlot::setTextColor(const QColor &color)
{
    if (mTextColor == color) {
        return;
    }
    mTextColor = color;
    static_cast<LogPlotCursor *>(mCursorItem)->setColor(color);
    update();
    emit colorsChanged();
}

QColor LogPlot::gridColor() const
{
    return mGridColor;
}

void LogPlot::setGridColor(const QColor &color)
{
    if (mGridColor == color) {
        return;
    }
    mGridColor = color;
    update();
    emit colorsChanged();
}

double LogPlot::timeAt(double x) const
{
    const QRectF rect = plotRect();
    if (rect.width() <= 0.0 || mXMax <= mXMin) {
        return mXMin;
    }
    return mXMin + qBound(0.0, (x - rect.left()) / rect.width(), 1.0) * (mXMax - mXMin);
}

void LogPlot::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        updatePlot();
        emit plotRectChanged();
    }
}

void LogPlot::updatePlot()
{
    const QRectF oldRect = plotRect();
    mHasRightAxis = false;
    for (int axis = 0; axis < 2; axis++) {
        mMinimum[axis] = std::numeric_limits<double>::infinity();
        mMaximum[axis] = -std::numeric_limits<double>::infinity();
        for (const PlotSeries &item : std::as_const(mSeries)) {
            if (item.axis != axis) continue;
            if (axis == 1) mHasRightAxis = true;
            mMinimum[axis] = qMin(mMinimum[axis], item.minimum);
            mMaximum[axis] = qMax(mMaximum[axis], item.maximum);
        }
        padRange(mMinimum[axis], mMaximum[axis]);
    }
    const QRectF rect = plotRect();
    for (PlotSeries &item : mSeries) {
        item.lines.clear();
        QPolygonF line;
        line.reserve(item.points.size());
        if (mXMax <= mXMin || rect.isEmpty()) {
            continue;
        }
        for (const QPointF &point : std::as_const(item.points)) {
            const double x = rect.left() + (point.x() - mXMin) / (mXMax - mXMin) * rect.width();
            const double y = rect.bottom() - (point.y() - mMinimum[item.axis])
                    / (mMaximum[item.axis] - mMinimum[item.axis]) * rect.height();
            if (!std::isfinite(x) || !std::isfinite(y)) {
                if (!line.isEmpty()) {
                    item.lines.append(line);
                    line.clear();
                }
                continue;
            }
            line.append(QPointF(x, y));
        }
        if (!line.isEmpty()) {
            item.lines.append(line);
        }
    }
    update();
    updateCursor();
    if (oldRect != rect) {
        emit plotRectChanged();
    }
}

void LogPlot::updateCursor()
{
    const QRectF rect = plotRect();
    if (!std::isfinite(mCursor) || mCursor < mXMin || mCursor > mXMax || mXMax <= mXMin || rect.isEmpty()) {
        mCursorItem->setVisible(false);
        return;
    }
    const double x = rect.left() + (mCursor - mXMin) / (mXMax - mXMin) * rect.width();
    mCursorItem->setPosition(QPointF(x - 0.5, rect.top()));
    mCursorItem->setSize(QSizeF(1.0, rect.height()));
    mCursorItem->setVisible(true);
}

void LogPlot::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), mBackgroundColor);
    const QRectF rect = plotRect();
    if (rect.isEmpty()) {
        return;
    }
    QFont font = painter->font();
    font.setPixelSize(11);
    painter->setFont(font);
    painter->setRenderHint(QPainter::Antialiasing, false);
    const int xTicks = qBound(2, int(rect.width() / 75.0), 4);
    for (int tick = 0; tick <= xTicks; tick++) {
        const double fraction = double(tick) / xTicks;
        const double x = rect.left() + fraction * rect.width();
        painter->setPen(mGridColor);
        painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        painter->setPen(mTextColor);
        const double time = mXMin + fraction * (mXMax - mXMin);
        painter->drawText(QRectF(qBound(0.0, x - 36.0, qMax(0.0, width() - 72.0)),
                                rect.bottom() + 5.0, 72.0, 16.0), Qt::AlignCenter,
                          mTimeLabel == "Sample" ? QString::number(time, 'g', 6) : elapsedLabel(time, mXMax - mXMin));
    }
    for (int tick = 0; tick <= 4; tick++) {
        const double fraction = tick / 4.0;
        const double y = rect.bottom() - fraction * rect.height();
        painter->setPen(mGridColor);
        painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        painter->setPen(mTextColor);
        for (int axis = 0; axis < 2; axis++) {
            if (axis == 1 && !mHasRightAxis) continue;
            const QRectF label = axis == 0 ? QRectF(0.0, y - 9.0, rect.left() - 6.0, 18.0)
                                          : QRectF(rect.right() + 6.0, y - 9.0, width() - rect.right() - 6.0, 18.0);
            const double span = mMaximum[axis] - mMinimum[axis];
            painter->drawText(label, (axis == 0 ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter,
                              rangeLabel(mMinimum[axis] + fraction * span, span));
        }
    }
    painter->drawText(QRectF(rect.left(), height() - 20.0, rect.width(), 18.0), Qt::AlignCenter,
                      mTimeLabel == "Sample" ? tr("Sample") : tr("Elapsed time"));
    painter->save();
    painter->setClipRect(rect, Qt::IntersectClip);
    painter->setRenderHint(QPainter::Antialiasing, true);
    for (const PlotSeries &item : std::as_const(mSeries)) {
        painter->setPen(QPen(item.color, 1.0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        for (const QPolygonF &line : item.lines) {
            painter->drawPolyline(line);
            if (line.size() == 1) {
                painter->drawEllipse(line.first(), 1.0, 1.0);
            }
        }
    }
    painter->restore();
}

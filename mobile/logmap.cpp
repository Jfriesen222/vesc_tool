/*
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
    along with VESC Tool.  If not, see <http://www.gnu.org/licenses/>.
    */

#include "logmap.h"
#include <QPainter>
#include <QPainterPath>
#include <QQuickWindow>
#include <QStandardPaths>
#include <cmath>
#include <limits>

LogMap::LogMap(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setOpaquePainting(true);
    mOsm = new OsmClient(this);
    mOsm->setTileServerUrl("https://c.osm.rrze.fau.de/osmhd");
    mOsm->setCacheDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/osm_tiles/hd");
    mOsm->setMaxMemoryTiles(64);
    mUpdateTimer.setSingleShot(true);
    mUpdateTimer.setInterval(16);
    connect(&mUpdateTimer, &QTimer::timeout, this, &LogMap::updateMap);
    connect(mOsm, &OsmClient::tileReady, this, [this]() { scheduleUpdate(); });
    connect(this, &QQuickItem::widthChanged, this, &LogMap::scheduleUpdate);
    connect(this, &QQuickItem::heightChanged, this, &LogMap::scheduleUpdate);
    connect(this, &QQuickItem::visibleChanged, this, &LogMap::scheduleUpdate);
    connect(this, &QQuickItem::windowChanged, this, &LogMap::scheduleUpdate);
}

LogMap::~LogMap()
{
    disconnect(this, nullptr, this, nullptr);
}

bool LogMap::hasRoute() const { return mHasRoute; }
bool LogMap::mapsEnabled() const { return mMapsEnabled; }
QColor LogMap::routeColor() const { return mRouteColor; }
QColor LogMap::backgroundColor() const { return mBackgroundColor; }
QColor LogMap::gridColor() const { return mGridColor; }

void LogMap::setMapsEnabled(bool enabled)
{
    if (mMapsEnabled == enabled) return;
    mMapsEnabled = enabled;
    emit mapsEnabledChanged();
    scheduleUpdate();
}

void LogMap::setRouteColor(const QColor &color)
{
    if (mRouteColor == color) return;
    mRouteColor = color;
    emit colorsChanged();
    scheduleUpdate();
}

void LogMap::setBackgroundColor(const QColor &color)
{
    if (mBackgroundColor == color) return;
    mBackgroundColor = color;
    emit colorsChanged();
    scheduleUpdate();
}

void LogMap::setGridColor(const QColor &color)
{
    if (mGridColor == color) return;
    mGridColor = color;
    emit colorsChanged();
    scheduleUpdate();
}

QPointF LogMap::project(double latitude, double longitude)
{
    if (!std::isfinite(latitude) || !std::isfinite(longitude)
            || latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0) {
        const double invalid = std::numeric_limits<double>::quiet_NaN();
        return QPointF(invalid, invalid);
    }
    const double lat = qBound(-85.05112878, latitude, 85.05112878) * M_PI / 180.0;
    return QPointF((longitude + 180.0) / 360.0,
                   (1.0 - std::asinh(std::tan(lat)) / M_PI) / 2.0);
}

void LogMap::setRoute(const QVariantList &points)
{
    mRoute.clear();
    mHasRoute = false;
    mCursorValid = false;
    const int count = qMin(5000, int(points.size()));
    mRoute.reserve(count);
    QPointF minimum, maximum, previous;
    for (int i = 0;i < count;i++) {
        const int index = count > 1 ? qint64(i) * (points.size() - 1) / (count - 1) : 0;
        const auto point = points[index].toMap();
        bool latOk = false, lonOk = false;
        const double latitude = point.value("latitude").toDouble(&latOk);
        const double longitude = point.value("longitude").toDouble(&lonOk);
        QPointF pos = project(latOk ? latitude : 100.0, lonOk ? longitude : 200.0);
        if (std::isfinite(pos.x())) {
            if (mHasRoute) {
                pos.rx() += std::round(previous.x() - pos.x());
                minimum.setX(qMin(minimum.x(), pos.x()));
                minimum.setY(qMin(minimum.y(), pos.y()));
                maximum.setX(qMax(maximum.x(), pos.x()));
                maximum.setY(qMax(maximum.y(), pos.y()));
            } else {
                minimum = maximum = pos;
                mHasRoute = true;
            }
            previous = pos;
        }
        mRoute.append(pos);
    }
    mBounds = QRectF(minimum, maximum);
    emit hasRouteChanged();
    fitRoute();
}

void LogMap::setCursor(double latitude, double longitude)
{
    mCursor = project(latitude, longitude);
    mCursorValid = std::isfinite(mCursor.x());
    update();
}

void LogMap::clearCursor()
{
    if (!mCursorValid) return;
    mCursorValid = false;
    update();
}

void LogMap::fitRoute()
{
    mAutoFit = true;
    scheduleUpdate();
}

void LogMap::pan(double dx, double dy)
{
    if (!mHasRoute || !std::isfinite(dx) || !std::isfinite(dy)) return;
    mAutoFit = false;
    mCenter -= QPointF(dx, dy) / (256.0 * std::pow(2.0, mZoom));
    mCenter.setX(mCenter.x() - std::floor(mCenter.x()));
    mCenter.setY(qBound(0.0, mCenter.y(), 1.0));
    scheduleUpdate();
}

void LogMap::zoom(double factor, double x, double y)
{
    if (!mHasRoute || !std::isfinite(factor) || factor <= 0.0
            || !std::isfinite(x) || !std::isfinite(y)) return;
    mAutoFit = false;
    const QPointF offset(x - width() / 2.0, y - height() / 2.0);
    const double oldScale = 256.0 * std::pow(2.0, mZoom);
    mZoom = qBound(1.0, mZoom + std::log2(factor), 19.0);
    mCenter += offset / oldScale - offset / (256.0 * std::pow(2.0, mZoom));
    mCenter.setX(mCenter.x() - std::floor(mCenter.x()));
    mCenter.setY(qBound(0.0, mCenter.y(), 1.0));
    scheduleUpdate();
}

QPointF LogMap::screenPoint(const QPointF &point) const
{
    return (point - mImageCenter) * mImageScale + QPointF(width() / 2.0, height() / 2.0);
}

void LogMap::scheduleUpdate()
{
    if (isVisible() && !mUpdateTimer.isActive()) mUpdateTimer.start();
}

void LogMap::updateMap()
{
    if (!isVisible() || width() < 1.0 || height() < 1.0) return;
    if (mAutoFit && mHasRoute) {
        mCenter = mBounds.center();
        const double scale = qMin(qMax(1.0, width() - 64.0) / qMax(0.00001, mBounds.width()),
                                 qMax(1.0, height() - 64.0) / qMax(0.00001, mBounds.height()));
        mZoom = qBound(1.0, std::log2(scale / 256.0), 19.0);
    }
    mImageCenter = mCenter;
    mImageScale = 256.0 * std::pow(2.0, mZoom);
    const double ratio = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    mImage = QImage(QSize(qCeil(width() * ratio), qCeil(height() * ratio)), QImage::Format_ARGB32_Premultiplied);
    mImage.setDevicePixelRatio(ratio);
    mImage.fill(mBackgroundColor);
    QPainter painter(&mImage);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (mHasRoute) {
        const int level = qBound(1, int(std::floor(mZoom)), 19);
        const int tileCount = 1 << level;
        const double tileSize = mImageScale / tileCount;
        const QPointF topLeft = screenPoint(QPointF(0.0, 0.0));
        const int xStart = int(std::floor(-topLeft.x() / tileSize));
        const int xEnd = int(std::ceil((width() - topLeft.x()) / tileSize));
        const int yStart = qMax(0, int(std::floor(-topLeft.y() / tileSize)));
        const int yEnd = qMin(tileCount, int(std::ceil((height() - topLeft.y()) / tileSize)));
        for (int y = yStart;y < yEnd;y++) {
            for (int x = xStart;x < xEnd;x++) {
                const QRectF rect(topLeft + QPointF(x * tileSize, y * tileSize), QSizeF(tileSize, tileSize));
                const int tileX = (x % tileCount + tileCount) % tileCount;
                int result = 0;
                if (mMapsEnabled) {
                    const OsmTile tile = mOsm->getTile(level, tileX, y, result);
                    if (result > 0) {
                        painter.drawPixmap(rect, tile.pixmap(), QRectF(tile.pixmap().rect()));
                    } else if (result == 0 && !mOsm->downloadQueueFull()) {
                        mOsm->downloadTile(level, tileX, y);
                    }
                }
                if (result <= 0) {
                    painter.setPen(mGridColor);
                    painter.drawRect(rect);
                }
            }
        }
        QPainterPath path;
        bool start = true;
        const double offset = std::round(mCenter.x() - mBounds.center().x());
        QPointF last;
        for (int i = 0;i < mRoute.size();i++) {
            if (!std::isfinite(mRoute[i].x())) {
                start = true;
                continue;
            }
            const QPointF pos = screenPoint(mRoute[i] + QPointF(offset, 0.0));
            if (start) {
                path.moveTo(pos);
                start = false;
            } else if (i == mRoute.size() - 1 || (pos - last).manhattanLength() > 1.0) {
                path.lineTo(pos);
            } else {
                continue;
            }
            last = pos;
        }
        painter.setPen(QPen(QColor(0, 0, 0, 120), 6.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
        painter.setPen(QPen(mRouteColor, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
        if (path.elementCount() > 0) {
            const auto first = path.elementAt(0);
            const auto last = path.elementAt(path.elementCount() - 1);
            painter.setPen(QPen(Qt::white, 1.5));
            painter.setBrush(QColor(76, 175, 80));
            painter.drawEllipse(QPointF(first.x, first.y), 4.0, 4.0);
            painter.setBrush(mRouteColor);
            painter.drawEllipse(QPointF(last.x, last.y), 4.0, 4.0);
        }
    }
    painter.end();
    update();
}

void LogMap::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), mBackgroundColor);
    if (!mImage.isNull()) painter->drawImage(boundingRect(), mImage);
    if (mHasRoute && mCursorValid) {
        QPointF pos = mCursor;
        pos.rx() += std::round(mImageCenter.x() - pos.x());
        painter->setPen(QPen(Qt::white, 2.0));
        painter->setBrush(mRouteColor);
        painter->drawEllipse(screenPoint(pos), 6.0, 6.0);
    }
}

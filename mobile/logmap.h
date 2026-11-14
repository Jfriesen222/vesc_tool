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

#ifndef LOGMAP_H
#define LOGMAP_H

#include <QQuickPaintedItem>
#include <QTimer>
#include <QVariantList>
#include "map/osmclient.h"

class LogMap : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(bool hasRoute READ hasRoute NOTIFY hasRouteChanged)
    Q_PROPERTY(bool mapsEnabled READ mapsEnabled WRITE setMapsEnabled NOTIFY mapsEnabledChanged)
    Q_PROPERTY(QColor routeColor READ routeColor WRITE setRouteColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY colorsChanged)

public:
    explicit LogMap(QQuickItem *parent = nullptr);
    ~LogMap();

    bool hasRoute() const;
    bool mapsEnabled() const;
    void setMapsEnabled(bool enabled);
    QColor routeColor() const;
    void setRouteColor(const QColor &color);
    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &color);
    QColor gridColor() const;
    void setGridColor(const QColor &color);

    Q_INVOKABLE void setRoute(const QVariantList &points);
    Q_INVOKABLE void setCursor(double latitude, double longitude);
    Q_INVOKABLE void clearCursor();
    Q_INVOKABLE void fitRoute();
    Q_INVOKABLE void pan(double dx, double dy);
    Q_INVOKABLE void zoom(double factor, double x, double y);

    void paint(QPainter *painter) override;

signals:
    void hasRouteChanged();
    void mapsEnabledChanged();
    void colorsChanged();

private:
    OsmClient *mOsm;
    QTimer mUpdateTimer;
    QVector<QPointF> mRoute;
    QRectF mBounds;
    QPointF mCenter;
    QPointF mCursor;
    QPointF mImageCenter;
    QImage mImage;
    double mZoom = 15.0;
    double mImageScale = 1.0;
    bool mHasRoute = false;
    bool mCursorValid = false;
    bool mMapsEnabled = true;
    bool mAutoFit = true;
    QColor mRouteColor = QColor(41, 182, 246);
    QColor mBackgroundColor = QColor(32, 32, 32);
    QColor mGridColor = QColor(55, 55, 55);

    static QPointF project(double latitude, double longitude);
    QPointF screenPoint(const QPointF &point) const;
    void scheduleUpdate();
    void updateMap();
};

#endif // LOGMAP_H

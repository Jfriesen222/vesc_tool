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

#ifndef PARAMTABLE_H
#define PARAMTABLE_H

#include <QWidget>
#include <QTableWidget>
#include <QPointer>
#include "configparams.h"

class ParamTable : public QTableWidget
{
    Q_OBJECT
public:
    ParamTable(QWidget *parent = nullptr);
    bool addParamRow(ConfigParams *params, QString paramName);
    void addRowSeparator(QString text);
    void addParamSubgroup(ConfigParams *params, QString groupName, QString subgroupName);
    void clearParams();
    bool hasChanges();
    QVector<QPair<QPointer<ConfigParams>, QString>> params() const;
    QWidget *showParam(const QString &paramName);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool viewportEvent(QEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    // Parameters and separators in the order they were added. A separator has no parameters and ::sep:: before
    // its title as name.
    QVector<QPair<QPointer<ConfigParams>, QString>> mRows;
    // Where each of them is in the table, as column and row of the name
    QVector<QPoint> mCells;
    // First row and number of rows of each card
    QVector<QPair<int, int>> mBlocks;
    bool mLayoutPending;
    int mMarkedRow;
    int mHoverRow;

    void scheduleLayout();
    void layoutRows();
    int wantedColumns() const;
    bool rowChanged(int row);
    QString referenceString(int row);
    void updateChanged();
    void showContextMenu(const QPoint &pos);
    void revertRow(int row);
    void updateHover();
    void setRowActive(int row, bool active);

};

#endif // PARAMTABLE_H

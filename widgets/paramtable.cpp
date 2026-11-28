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

#include "paramtable.h"
#include "utility.h"
#include <QBoxLayout>
#include <QDebug>
#include <QHeaderView>
#include <QPainter>
#include <QTimer>
#include <QMenu>
#include <QTabWidget>
#include <QPushButton>

ParamTable::ParamTable(QWidget *parent) : QTableWidget(parent)
{
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);
    setShowGrid(false);
    setWordWrap(false);
    setTextElideMode(Qt::ElideRight);
    viewport()->setMouseTracking(true);
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    horizontalHeader()->setVisible(false);
    horizontalHeader()->setMinimumSectionSize(0);
    verticalHeader()->setVisible(false);
    verticalHeader()->setMinimumSectionSize(0);
    mLayoutPending = false;
    mMarkedRow = -1;
    mHoverRow = -1;

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &ParamTable::showContextMenu);
}

bool ParamTable::addParamRow(ConfigParams *params, QString paramName)
{
    if (!params->hasParam(paramName) || params->getParam(paramName)->type == CFG_T_UNDEFINED) {
        return false;
    }

    mRows.append({params, paramName});
    connect(params, &ConfigParams::changedParamsChanged, this, &ParamTable::updateChanged, Qt::UniqueConnection);
    scheduleLayout();
    return true;
}

void ParamTable::addRowSeparator(QString text)
{
    mRows.append({nullptr, "::sep::" + text});
    scheduleLayout();
}

void ParamTable::addParamSubgroup(ConfigParams *params, QString groupName, QString subgroupName)
{
    // Parameters before the first separator of the XML are under the name of their subgroup
    QStringList names = params->getParamsFromSubgroup(groupName, subgroupName);
    if (!names.isEmpty() && !names.first().startsWith("::sep::")) {
        addRowSeparator(subgroupName);
    }

    foreach (auto p, names) {
        if (p.startsWith("::sep::")) {
            addRowSeparator(p.mid(7));
        } else {
            addParamRow(params, p);
        }
    }
}

void ParamTable::clearParams()
{
    setRowCount(0);
    mRows.clear();
    mCells.clear();
    mBlocks.clear();
    mMarkedRow = -1;
    mHoverRow = -1;
    scheduleLayout();
}

bool ParamTable::hasChanges()
{
    for (int row = 0;row < mRows.size();row++) {
        if (rowChanged(row)) {
            return true;
        }
    }
    return false;
}

// The rows are laid out once after they are added, not for each one
void ParamTable::scheduleLayout()
{
    if (!mLayoutPending) {
        mLayoutPending = true;
        QTimer::singleShot(0, this, [this]() {
            if (mLayoutPending) {
                layoutRows();
            }
        });
    }
}

// The parameters between two separators are a block, which is on a card.
void ParamTable::layoutRows()
{
    // The rows are lower in a compact window
    bool compact = window()->property("compact").toBool();
    const int paramHeight = compact ? 30 : 38, titleHeight = compact ? 30 : 38, editorWidth = 236;

    // Parameters of a configuration that no longer exists are left out
    mRows.removeIf([](const QPair<QPointer<ConfigParams>, QString> &r) {
        return !r.first && !r.second.startsWith("::sep::");
    });

    mLayoutPending = false;
    setUpdatesEnabled(false);
    setRowCount(0);
    setColumnCount(2);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);

    mCells.fill(QPoint(-1, -1), mRows.size());
    mBlocks.clear();
    mHoverRow = -1;
    int widestEditor = editorWidth;
    for (int i = 0;i < mRows.size();) {
        int row = rowCount();
        if (!mRows.at(i).first) {
            // A gap above the title, but not above the first one
            setRowCount(row + 1);
            setRowHeight(row, row == 0 ? 22 : titleHeight);
            setSpan(row, 0, 1, columnCount());
            mCells[i++] = QPoint(0, row);
            continue;
        }

        int end = i;
        while (end < mRows.size() && mRows.at(end).first) {
            end++;
        }
        int rows = end - i;
        setRowCount(row + rows);
        mBlocks.append(qMakePair(row, rows));
        for (int r = row;r < row + rows;r++) {
            setRowHeight(r, paramHeight);
        }
        for (int n = 0;i < end;i++, n++) {
            QPoint cell(0, row + n);
            QTableWidgetItem *item = new QTableWidgetItem(mRows.at(i).first->getLongName(mRows.at(i).second));
            item->setFlags(Qt::ItemIsEnabled);
            setItem(cell.y(), cell.x(), item);
            QWidget *editor = mRows.at(i).first->getEditor(mRows.at(i).second);
            editor->installEventFilter(this);
            setCellWidget(cell.y(), cell.x() + 1, editor);
            // Editors with more than one line, such as bit fields, make their row higher and get the width they need
            setRowHeight(cell.y(), qMax(rowHeight(cell.y()), editor->sizeHint().height() + (compact ? 8 : 12)));
            if (editor->sizeHint().height() > paramHeight) {
                widestEditor = qMax(widestEditor, editor->sizeHint().width() + 28);
            }
            mCells[i] = cell;
            setRowActive(i, false);
        }
    }

    horizontalHeader()->resizeSection(1, widestEditor);

    // Alone in a layout it stays at the top instead of in the middle of the room that is left over
    QBoxLayout *box = parentWidget() ? qobject_cast<QBoxLayout*>(parentWidget()->layout()) : nullptr;
    if (box && box->count() == 1) {
        box->setAlignment(this, Qt::AlignTop);
    }

    updateChanged();
    updateGeometry();
    setUpdatesEnabled(true);
}

void ParamTable::changeEvent(QEvent *event)
{
    QTableWidget::changeEvent(event);
    if (event->type() == QEvent::StyleChange) {
        scheduleLayout();
    }
}

// Draws the cards, the lines between the rows, the titles and a dot after the names of changed parameters.
// The cells are drawn over it.
void ParamTable::paintEvent(QPaintEvent *event)
{
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    QColor line = Utility::getAppQColor("line");

    for (auto block: mBlocks) {
        int top = rowViewportPosition(block.first);
        int bottom = rowViewportPosition(block.first + block.second - 1) + rowHeight(block.first + block.second - 1);
        QRectF card(0.5, top + 0.5, viewport()->width() - 1, bottom - top - 1);
        painter.setPen(line);
        painter.setBrush(Utility::getAppQColor("cardBackground"));
        painter.drawRoundedRect(card, 10, 10);
        for (int r = 1;r < block.second;r++) {
            double y = rowViewportPosition(block.first + r) + 0.5;
            painter.drawLine(QPointF(card.left(), y), QPointF(card.right(), y));
        }
    }

    for (int i = 0;i < mCells.size();i++) {
        QRect rect = visualRect(model()->index(mCells.at(i).y(), mCells.at(i).x()));
        if (mRows.at(i).second.startsWith("::sep::")) {
            Utility::drawSectionHeader(&painter, rect.adjusted(2, rect.height() - 22, -2, -6), mRows.at(i).second.mid(7));
            continue;
        }

        // The row that was searched for is marked, the row under the mouse is a little lighter
        if (i == mMarkedRow || i == mHoverRow) {
            QColor tint = Utility::getAppQColor("titleText");
            tint.setAlpha(8);
            if (i == mMarkedRow) {
                tint = Utility::getAppQColor("accentDim");
            }
            painter.setPen(Qt::NoPen);
            painter.setBrush(tint);
            painter.drawRoundedRect(rect.united(visualRect(model()->index(mCells.at(i).y(), mCells.at(i).x() + 1))).adjusted(3, 3, -3, -3), 7, 7);
        }
        if (i != mMarkedRow && rowChanged(i)) {
            int textWidth = qMin(fontMetrics().horizontalAdvance(item(mCells.at(i).y(), mCells.at(i).x())->text()), rect.width() - 34);
            painter.setPen(Qt::NoPen);
            painter.setBrush(Utility::getAppQColor("accent"));
            painter.drawEllipse(QPointF(rect.left() + 14 + textWidth + 11, rect.center().y() + 1), 3, 3);
        }
    }

    painter.end();
    QTableWidget::paintEvent(event);
}

// The parameters in the order they were added, with the separators left out
QVector<QPair<QPointer<ConfigParams>, QString>> ParamTable::params() const
{
    QVector<QPair<QPointer<ConfigParams>, QString>> res;
    for (const auto &r: mRows) {
        if (r.first) {
            res.append(r);
        }
    }
    return res;
}

// Scrolls to the parameter and marks it for a moment. Returns its editor, or null if it is not here.
QWidget *ParamTable::showParam(const QString &paramName)
{
    int row = -1;
    for (int i = 0;i < mRows.size() && row < 0;i++) {
        if (mRows.at(i).first && mRows.at(i).second == paramName) {
            row = i;
        }
    }
    if (row < 0) {
        return nullptr;
    }

    if (mLayoutPending) {
        layoutRows();
    }

    mMarkedRow = row;
    scrollTo(model()->index(mCells.at(row).y(), mCells.at(row).x()), QAbstractItemView::PositionAtCenter);
    viewport()->update();
    QTimer::singleShot(1500, this, [this]() {
        mMarkedRow = -1;
        viewport()->update();
    });
    return cellWidget(mCells.at(row).y(), mCells.at(row).x() + 1);
}

bool ParamTable::rowChanged(int row)
{
    const auto &r = mRows.at(row);
    return r.first && r.first->getChangedParams().contains(r.second);
}

// Parameters that differ from the device have a dot after the name, an accent edge on the value and the
// device value as tooltip
void ParamTable::updateChanged()
{
    for (int i = 0;i < mCells.size();i++) {
        QTableWidgetItem *it = mRows.at(i).first ? item(mCells.at(i).y(), mCells.at(i).x()) : nullptr;
        if (!it) {
            continue;
        }

        bool changed = rowChanged(i);
        it->setToolTip(changed ? tr("Value on the VESC: %1").arg(referenceString(i)) : it->text());
        Utility::setStyleProperty(cellWidget(mCells.at(i).y(), mCells.at(i).x() + 1), "changed", changed);
    }
    viewport()->update();

    // Dot on the tab that holds this table
    for (QWidget *w = this;w->parentWidget();w = w->parentWidget()) {
        QTabWidget *tabs = qobject_cast<QTabWidget*>(w->parentWidget()->parentWidget());
        int ind = tabs ? tabs->indexOf(w) : -1;
        if (ind >= 0) {
            bool changed = hasChanges();
            for (ParamTable *t: w->findChildren<ParamTable*>()) {
                changed = changed || t->hasChanges();
            }
            tabs->setTabIcon(ind, changed ? QIcon(Utility::getChangedDot()) : QIcon());
            break;
        }
    }
}

QString ParamTable::referenceString(int row)
{
    return mRows.at(row).first->getReference()->getParamValueString(mRows.at(row).second);
}

void ParamTable::showContextMenu(const QPoint &pos)
{
    QModelIndex index = indexAt(pos);
    int row = mCells.indexOf(QPoint(0, index.row()));
    int changedRows = 0;
    for (int i = 0;i < mRows.size();i++) {
        changedRows += rowChanged(i);
    }

    QMenu menu(this);
    if (row >= 0 && row < mRows.size() && rowChanged(row)) {
        menu.addAction(tr("Revert to %1").arg(referenceString(row)), this, [this, row]() { revertRow(row); });
    }

    if (changedRows > 1) {
        menu.addAction(tr("Revert all %1 changes here").arg(changedRows), this, [this]() {
            for (int i = 0;i < mRows.size();i++) {
                revertRow(i);
            }
        });
    }

    if (!menu.isEmpty()) {
        menu.exec(viewport()->mapToGlobal(pos));
    }
}

void ParamTable::revertRow(int row)
{
    if (rowChanged(row)) {
        ConfigParams *params = mRows.at(row).first;
        QString name = mRows.at(row).second;
        params->updateParamFromOther(name, params->getReference()->getParamCopy(name), nullptr);
    }
}

// The row under the mouse is lighter and its action buttons are bright; the buttons of the other rows are dim
void ParamTable::updateHover()
{
    QPoint pos = viewport()->mapFromGlobal(QCursor::pos());
    QModelIndex index = viewport()->rect().contains(pos) ? indexAt(pos) : QModelIndex();
    int row = index.isValid() ? mCells.indexOf(QPoint(0, index.row())) : -1;
    if (row != mHoverRow) {
        setRowActive(mHoverRow, false);
        setRowActive(row, true);
        mHoverRow = row;
        viewport()->update();
    }
}

void ParamTable::setRowActive(int row, bool active)
{
    QWidget *editor = (row >= 0 && row < mCells.size() && mRows.at(row).first) ?
                cellWidget(mCells.at(row).y(), mCells.at(row).x() + 1) : nullptr;
    if (!editor) {
        return;
    }

    for (QPushButton *button: editor->findChildren<QPushButton*>()) {
        button->setIcon(Utility::tinted(button->icon().pixmap(button->iconSize(), devicePixelRatioF()),
                                        Utility::getAppQColor(active ? "lightText" : "disabledText")));
    }
}

bool ParamTable::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::MouseMove || event->type() == QEvent::Leave) {
        updateHover();
    }
    return QTableWidget::viewportEvent(event);
}

bool ParamTable::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
        updateHover();
    }
    return QTableWidget::eventFilter(object, event);
}

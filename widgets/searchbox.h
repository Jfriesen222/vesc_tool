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

#ifndef SEARCHBOX_H
#define SEARCHBOX_H

#include <QFrame>
#include <QPointer>
#include <QIcon>

class QLineEdit;
class QLabel;
class QListView;
class QListWidget;
class QMainWindow;
class QStackedWidget;
class QStandardItemModel;
class QTabWidget;
class VescInterface;
class SearchDelegate;

// The search of the sidebar. It finds parameters, pages, tabs, menu actions, buttons, sections, help texts, CAN
// devices and examples, and lists them under the field. The list is a part of the window that takes no focus,
// so typing stays in the field.
class SearchBox : public QFrame
{
    Q_OBJECT
public:
    SearchBox(QLineEdit *edit, QListWidget *pages, QStackedWidget *stack, QListWidget *canList,
              VescInterface *vesc, QMainWindow *window);
    bool eventFilter(QObject *object, QEvent *event) override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // Also the order of the groups in the list
    enum Kind {Changed, Param, Page, Tab, Action, Button, Section, Help, Can, Example, Kinds};

    struct Entry {
        Kind kind;
        QString title;
        QString place;
        QString value;
        QIcon icon;
        // Lower case text that is matched first, and text that is matched after it (help, tool tip, options)
        QString name;
        QString more;
        int page;
        QVector<QPair<QPointer<QTabWidget>, int>> tabs;
        QPointer<QObject> target;
        QString key;
        // The row of an example, the id of a CAN device
        int row;
        bool changed;
    };

    QLineEdit *mEdit;
    QListWidget *mPages;
    QStackedWidget *mStack;
    QListWidget *mCanList;
    VescInterface *mVesc;
    QMainWindow *mWindow;
    QListView *mView;
    QStandardItemModel *mModel;
    SearchDelegate *mDelegate;
    QLabel *mKey;
    QVector<Entry> mEntries;

    Entry &add(Kind kind, const QString &title, const QString &place, int page);
    bool reachable(QWidget *w, QWidget *page, QVector<QPair<QPointer<QTabWidget>, int>> &tabs);
    void buildIndex();
    void refresh();
    void moveCurrent(int step);
    void activate(int entry);
    void open(const Entry &e);
    void mark(QWidget *w, QRect rect);
    void dismiss();
    void updateKey();

};

#endif // SEARCHBOX_H

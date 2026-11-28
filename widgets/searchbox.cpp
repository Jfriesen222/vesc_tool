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

#include "searchbox.h"
#include "paramtable.h"
#include "pagelistitem.h"
#include "canlistitem.h"
#include "helpdialog.h"
#include "utility.h"
#include "vescinterface.h"
#include <QLineEdit>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QGroupBox>
#include <QToolButton>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QScrollArea>
#include <QShortcut>
#include <QPainter>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QTimer>
#include <QTextLayout>
#include <QtMath>
#include <QTabBar>
#include <QMenu>
#include <QVBoxLayout>
#include <QAbstractButton>
#include <algorithm>
#include <functional>

namespace {
enum Roles {EntryRole = Qt::UserRole, KindRole, PlaceRole, ValueRole};
const int shadowSide = 18, shadowTop = 4, shadowBottom = 26, footerHeight = 34;
const QString sep = " › ";

QString plain(QString html)
{
    static const QRegularExpression tags("<style.*?</style>|<[^>]*>", QRegularExpression::DotMatchesEverythingOption);
    return html.remove(tags).replace("&nbsp;", " ").replace("&amp;", "&")
            .replace("&lt;", "<").replace("&gt;", ">").simplified();
}

// The first sentence of a tool tip, as the title of a result that has no text of its own
QString firstSentence(const QString &text)
{
    int end = text.indexOf(". ");
    return (end > 0 ? text.left(end) : text).left(80);
}

// The first picture of a list item, such as the icon of a page or a CAN device
QIcon iconOf(QWidget *item)
{
    for (QLabel *l: item->findChildren<QLabel*>()) {
        if (!l->pixmap().isNull()) {
            return QIcon(l->pixmap());
        }
    }
    return QIcon();
}

// Where the word is found at the start of a word of the text, or -1
int wordAt(const QString &text, const QString &word, Qt::CaseSensitivity cs = Qt::CaseSensitive)
{
    for (int at = text.indexOf(word, 0, cs);at >= 0;at = text.indexOf(word, at + 1, cs)) {
        if (at == 0 || !text.at(at - 1).isLetterOrNumber()) {
            return at;
        }
    }
    return -1;
}

}

// Paints group headers, results as a title over their place with the value at the right, and the line for
// no results
class SearchDelegate : public QStyledItemDelegate
{
public:
    QStringList mWords;

    QFont smallFont(const QFont &font) const
    {
        QFont f = font;
        f.setPointSizeF(font.pointSizeF() * 0.85);
        return f;
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        int entry = index.data(EntryRole).toInt();
        int small = QFontMetrics(smallFont(option.font), option.widget).height();
        int normal = QFontMetrics(option.font, option.widget).height();
        return QSize(100, entry == -1 ? small + 14 : (entry == -2 ? 40 : normal + small + 14));
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        QRect r = option.rect;
        QFont small = smallFont(option.font);
        painter->setFont(option.font);
        QFontMetrics fm(option.font, painter->device()), sfm(small, painter->device());
        QString title = index.data().toString();
        QString value = index.data(ValueRole).toString();
        int entry = index.data(EntryRole).toInt();

        if (entry == -1) {
            small.setWeight(QFont::Medium);
            small.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
            small.setCapitalization(QFont::AllUppercase);
            painter->setFont(small);
            painter->setPen(Utility::getAppQColor("mutedText"));
            QRect text = r.adjusted(10, 6, -10, 0);
            painter->drawText(text, Qt::AlignLeft | Qt::AlignVCenter, title);
            painter->drawText(text, Qt::AlignRight | Qt::AlignVCenter, value);
        } else if (entry == -2) {
            painter->setPen(Utility::getAppQColor("mutedText"));
            painter->drawText(r.adjusted(10, 0, -10, 0), Qt::AlignLeft | Qt::AlignVCenter, title);
        } else {
            if (option.state & QStyle::State_Selected) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(Utility::getAppQColor("accentDim"));
                painter->drawRoundedRect(r, 8, 8);
            }
            index.data(Qt::DecorationRole).value<QIcon>().paint(painter, QRect(r.left() + 10, r.center().y() - 8, 16, 16));

            // Whole pixels, so that a value that fits is not elided
            int valueWidth = value.isEmpty() ? 0 : qMin(qCeil(QFontMetricsF(option.font, painter->device()).horizontalAdvance(value)), r.width() / 3);
            painter->setPen(Utility::getAppQColor("lightText"));
            painter->drawText(QRect(r.right() - 12 - valueWidth, r.top(), valueWidth, r.height()),
                              Qt::AlignRight | Qt::AlignVCenter, fm.elidedText(value, Qt::ElideRight, valueWidth));

            // The title with the parts that match the words in the accent text colour
            int x = r.left() + 36;
            int right = r.right() - 12 - (valueWidth > 0 ? valueWidth + 12 : 0);
            int top = r.top() + (r.height() - fm.height() - sfm.height()) / 2;
            QTextCharFormat hit;
            hit.setFontWeight(QFont::Medium);
            hit.setForeground(Utility::getAppQColor("accentText"));
            QList<QTextLayout::FormatRange> hits;
            for (const QString &w: mWords) {
                int at = wordAt(title, w, Qt::CaseInsensitive);
                at = at >= 0 ? at : title.indexOf(w, 0, Qt::CaseInsensitive);
                if (at >= 0) {
                    hits.append({at, int(w.size()), hit});
                }
            }
            QTextLayout layout(title, option.font, painter->device());
            layout.setFormats(hits);
            layout.beginLayout();
            layout.createLine();
            layout.endLayout();
            painter->save();
            painter->setClipRect(QRect(x, r.top(), right - x, r.height()));
            painter->setPen(Utility::getAppQColor("titleText"));
            layout.draw(painter, QPointF(x, top));
            painter->restore();

            QRect place(r.left() + 36, top + fm.height(), right - r.left() - 36, sfm.height());
            painter->setFont(small);
            painter->setPen(Utility::getAppQColor("mutedText"));
            painter->drawText(place, Qt::AlignLeft | Qt::AlignVCenter,
                              sfm.elidedText(index.data(PlaceRole).toString(), Qt::ElideRight, place.width()));
        }
        painter->restore();
    }
};

SearchBox::SearchBox(QLineEdit *edit, QListWidget *pages, QStackedWidget *stack, QListWidget *canList,
                     VescInterface *vesc, QMainWindow *window) :
    QFrame(window, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
{
    // A window of its own, as a page with a native view would otherwise cover it. It never takes the focus.
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    mEdit = edit;
    mPages = pages;
    mStack = stack;
    mCanList = canList;
    mVesc = vesc;
    mWindow = window;

    mModel = new QStandardItemModel(this);
    mDelegate = new SearchDelegate;
    mDelegate->setParent(this);
    mView = new QListView;
    mView->setModel(mModel);
    mView->setItemDelegate(mDelegate);
    mView->setFocusPolicy(Qt::NoFocus);
    mView->setFrameShape(QFrame::NoFrame);
    mView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mView->setMouseTracking(true);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(shadowSide + 6, shadowTop + 6, shadowSide + 6, shadowBottom + footerHeight);
    layout->addWidget(mView);
    setFocusPolicy(Qt::NoFocus);
    hide();

    connect(mView, &QListView::clicked, this, [this](const QModelIndex &index) {
        activate(index.data(EntryRole).toInt());
    });
    connect(mView, &QListView::entered, this, [this](const QModelIndex &index) {
        if (index.data(EntryRole).toInt() >= 0) {
            mView->setCurrentIndex(index);
        }
    });
    connect(mView->selectionModel(), &QItemSelectionModel::currentChanged, this, [this]() { update(); });

    // The field has a magnifier at the left and shows its shortcut, or esc while it is in use, at the right
    mEdit->addAction(QIcon(Utility::getIcon("icons/search.png")), QLineEdit::LeadingPosition);
    mKey = new QLabel(mEdit);
    mKey->setObjectName("searchKey");
    mKey->setCursor(Qt::ArrowCursor);
    QShortcut *shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_K), window);
    connect(shortcut, &QShortcut::activated, this, [this]() { mEdit->setFocus(Qt::ShortcutFocusReason); });
    connect(mEdit, &QLineEdit::textChanged, this, [this]() {
        updateKey();
        if (mEdit->hasFocus()) {
            refresh();
        }
    });
    // On the application, to see the shortcut before the text fields of the window take it as their own key
    qApp->installEventFilter(this);
    updateKey();
}

bool SearchBox::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride && object->isWidgetType() &&
            static_cast<QWidget*>(object)->window() == mWindow &&
            static_cast<QKeyEvent*>(event)->keyCombination() == QKeyCombination(Qt::ControlModifier, Qt::Key_K)) {
        // Not passed on, so that the shortcut gets it and not a text field that deletes to the line end with it
        event->ignore();
        return true;
    } else if (event->type() == QEvent::MouseButtonPress && mEdit->hasFocus() && object->isWidgetType() &&
               object != mEdit && !mEdit->isAncestorOf(static_cast<QWidget*>(object)) && static_cast<QWidget*>(object)->window() != this) {
        // A click anywhere else closes the search, also where the click does not take the focus
        dismiss();
    } else if (object == mWindow) {
        if (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::Move || event->type() == QEvent::Resize) {
            hide();
        }
    } else if (object == mKey) {
        if (event->type() == QEvent::MouseButtonPress && mEdit->hasFocus()) {
            dismiss();
            return true;
        }
    } else if (object != mEdit) {
        return false;
    } else if (event->type() == QEvent::FocusIn) {
        // Coming back to the window keeps the index
        if (static_cast<QFocusEvent*>(event)->reason() != Qt::ActiveWindowFocusReason || mEntries.isEmpty()) {
            buildIndex();
        }
        refresh();
        updateKey();
    } else if (event->type() == QEvent::FocusOut) {
        hide();
        updateKey();
    } else if (event->type() == QEvent::Resize) {
        updateKey();
    } else if (event->type() == QEvent::KeyPress) {
        switch (static_cast<QKeyEvent*>(event)->key()) {
        case Qt::Key_Down: moveCurrent(1); return true;
        case Qt::Key_Up: moveCurrent(-1); return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (mView->currentIndex().isValid() && isVisible()) {
                activate(mView->currentIndex().data(EntryRole).toInt());
            }
            return true;
        case Qt::Key_Escape: dismiss(); return true;
        default: break;
        }
    }
    return false;
}

void SearchBox::paintEvent(QPaintEvent *event)
{
    (void)event;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRect box = rect().adjusted(shadowSide, shadowTop, -shadowSide, -shadowBottom);

    // A soft shadow under the box
    painter.setPen(Qt::NoPen);
    for (int i = 1;i <= 8;i++) {
        painter.setBrush(QColor(0, 0, 0, Utility::isDarkMode() ? 14 : 5));
        painter.drawRoundedRect(QRectF(box).adjusted(-i * 1.5, -i * 0.5 + 6, i * 1.5, i * 2.0 + 6), 12 + i, 12 + i);
    }
    painter.setPen(Utility::getAppQColor("lineStrong"));
    painter.setBrush(Utility::getAppQColor(Utility::isDarkMode() ? "lightBackground" : "normalBackground"));
    painter.drawRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);

    // The footer with the keys
    int y = box.bottom() - footerHeight + 4;
    painter.setPen(Utility::getAppQColor("line"));
    painter.drawLine(box.left() + 10, y, box.right() - 10, y);
    QFont font = this->font();
    font.setPointSizeF(font.pointSizeF() * 0.85);
    painter.setFont(font);
    static const char *hints[Kinds] = {
        QT_TR_NOOP("Go to parameter"), QT_TR_NOOP("Go to parameter"), QT_TR_NOOP("Open page"), QT_TR_NOOP("Open tab"),
        QT_TR_NOOP("Run"), QT_TR_NOOP("Show button"), QT_TR_NOOP("Show section"), QT_TR_NOOP("Open help"),
        QT_TR_NOOP("Select device"), QT_TR_NOOP("Show example")};
    QList<QPair<QString, QString>> keys = {{"↑↓", tr("Move")}, {"esc", tr("Close")}};
    if (mView->currentIndex().isValid()) {
        keys.prepend({"↵", tr(hints[mView->currentIndex().data(KindRole).toInt()])});
    }
    int x = box.left() + 14;
    QRect row(x, y + 1, 0, box.bottom() - y - 1);
    for (const auto &k: keys) {
        int w = painter.fontMetrics().horizontalAdvance(k.first) + 10;
        QRect key(x, row.center().y() - 8, w, 17);
        painter.setPen(Utility::getAppQColor("lineStrong"));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(key).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
        painter.setPen(Utility::getAppQColor("mutedText"));
        painter.drawText(key, Qt::AlignCenter, k.first);
        x += w + 7;
        painter.drawText(QRect(x, row.top(), 200, row.height()), Qt::AlignLeft | Qt::AlignVCenter, k.second);
        x += painter.fontMetrics().horizontalAdvance(k.second) + 18;
    }
}

SearchBox::Entry &SearchBox::add(Kind kind, const QString &title, const QString &place, int page)
{
    Entry e;
    e.kind = kind;
    e.title = title;
    e.place = place;
    e.name = title.toLower();
    e.page = page;
    e.row = -1;
    e.changed = false;
    mEntries.append(e);
    return mEntries.last();
}

// False if the widget is hidden on its page by design or disabled. Fills in the tabs that it is in, the
// outermost first.
bool SearchBox::reachable(QWidget *w, QWidget *page, QVector<QPair<QPointer<QTabWidget>, int>> &tabs)
{
    if (!w->isEnabled()) {
        return false;
    }
    for (QWidget *x = w;x != page;x = x->parentWidget()) {
        QWidget *p = x->parentWidget();
        QStackedWidget *stack = qobject_cast<QStackedWidget*>(p);
        QTabWidget *tabWidget = stack ? qobject_cast<QTabWidget*>(stack->parentWidget()) : nullptr;
        if (!p) {
            return false;
        } else if (tabWidget) {
            int index = tabWidget->indexOf(x);
            if (!tabWidget->isTabVisible(index) || !tabWidget->isTabEnabled(index)) {
                return false;
            }
            tabs.prepend({tabWidget, index});
        } else if (stack ? stack->currentWidget() != x :
                   x->isHidden() && x->testAttribute(Qt::WA_WState_ExplicitShowHide)) {
            return false;
        }
    }
    return true;
}

// Lists everything that can be found, as it is now: pages hidden for the motor type or app and their content
// are left out
void SearchBox::buildIndex()
{
    mEntries.clear();
    QIcon icons[Kinds];
    const char *iconNames[Kinds] = {"Horizontal Settings Mixer-96", "Horizontal Settings Mixer-96", "", "arrow_r",
                                    "Circled Play-96", "Circled Play-96", "arrow_r", "Help-96", "can_off", "Open Folder-96"};
    for (int i = 0;i < Kinds;i++) {
        icons[i] = QIcon(Utility::getIcon(QString("icons/%1.png").arg(iconNames[i])));
    }

    QString group;
    for (int i = 0;i < mPages->count();i++) {
        PageListItem *item = dynamic_cast<PageListItem*>(mPages->itemWidget(mPages->item(i)));
        QWidget *page = mStack->widget(i);
        bool indented = mPages->item(i)->data(Qt::UserRole).toBool();
        if (!item || !page) {
            continue;
        }
        if (!indented) {
            group = item->name();
        }
        if (mPages->item(i)->isHidden()) {
            continue;
        }

        add(Page, item->name(), indented ? group : QString(), i).icon = QIcon(item->icon());

        QString pagePlace = indented ? group + sep + item->name() : item->name();
        QVector<QPair<QPointer<QTabWidget>, int>> tabs;
        auto placeOf = [&]() {
            QString place = pagePlace;
            for (const auto &t: tabs) {
                place += sep + t.first->tabText(t.second).remove('&');
            }
            return place;
        };

        for (ParamTable *t: page->findChildren<ParamTable*>()) {
            tabs.clear();
            if (!reachable(t, page, tabs)) {
                continue;
            }
            for (const auto &param: t->params()) {
                ConfigParam *c = param.first ? param.first->getParam(param.second) : nullptr;
                if (!c) {
                    continue;
                }
                Entry &e = add(Param, c->longName, placeOf(), i);
                e.name += " " + param.second + " " + QString(param.second).replace('_', ' ');
                e.more = (plain(c->description) + " " + c->enumNames.join(" ")).toLower();
                e.value = param.first->getParamValueString(param.second);
                e.target = t;
                e.key = param.second;
                e.tabs = tabs;
                e.changed = param.first->getChangedParams().contains(param.second);
            }
        }

        for (QTabWidget *t: page->findChildren<QTabWidget*>()) {
            tabs.clear();
            if (!reachable(t, page, tabs)) {
                continue;
            }
            QString place = placeOf();
            for (int j = 0;j < t->count();j++) {
                if (t->isTabVisible(j) && t->isTabEnabled(j)) {
                    Entry &e = add(Tab, t->tabText(j).remove('&'), place, i);
                    e.tabs = tabs;
                    e.tabs.append({t, j});
                }
            }
        }

        // Buttons by their text or tool tip, except the editors of parameters and the parts of other controls
        for (QAbstractButton *b: page->findChildren<QAbstractButton*>()) {
            QString text = b->text().remove('&').simplified();
            QString tip = plain(b->toolTip());
            QToolButton *tool = qobject_cast<QToolButton*>(b);
            QWidget *p = b->parentWidget();
            while (p && !qobject_cast<ParamTable*>(p)) {
                p = p->parentWidget();
            }
            tabs.clear();
            if ((text.isEmpty() && tip.isEmpty()) || p || (tool && tool->defaultAction()) ||
                    qobject_cast<QTabBar*>(b->parentWidget()) || qobject_cast<QLineEdit*>(b->parentWidget()) ||
                    !reachable(b, page, tabs)) {
                continue;
            }
            // Short labels such as RL or a symbol, and labels that another button beside it has too, are named by
            // their tool tip
            QString title = text.size() > 3 || tip.isEmpty() ? text : firstSentence(tip);
            QString place = placeOf();
            if (!tip.isEmpty() && std::any_of(mEntries.begin(), mEntries.end(), [&](const Entry &o) {
                    return o.kind == Button && o.place == place && o.title == title; })) {
                title = firstSentence(tip);
            }
            Entry &e = add(Button, title, place, i);
            e.name += " " + text.toLower();
            e.more = tip.toLower();
            e.value = b->isCheckable() ? (b->isChecked() ? tr("On") : tr("Off")) : QString();
            e.icon = b->icon().isNull() ? icons[Button] : b->icon();
            e.target = b;
            e.tabs = tabs;
        }

        for (QGroupBox *g: page->findChildren<QGroupBox*>()) {
            tabs.clear();
            if (!g->title().isEmpty() && reachable(g, page, tabs)) {
                Entry &e = add(Section, g->title().remove('&'), placeOf(), i);
                e.target = g;
                e.tabs = tabs;
            }
        }

        // The examples of the scripting pages
        for (QListWidget *l: page->findChildren<QListWidget*>("exampleList")) {
            tabs.clear();
            if (!reachable(l, page, tabs)) {
                continue;
            }
            for (int j = 0;j < l->count();j++) {
                if (l->item(j)->isHidden()) {
                    continue;
                }
                Entry &e = add(Example, l->item(j)->text(), placeOf(), i);
                e.target = l;
                e.row = j;
                e.tabs = tabs;
            }
        }
    }

    // Menu and toolbar actions. The toolbar names its actions in their tool tip.
    std::function<void(QList<QAction*>, QString)> addActions = [&](QList<QAction*> actions, QString place) {
        for (QAction *a: actions) {
            QString tip = plain(a->toolTip());
            QString text = a->text().remove('&');
            if (a->menu()) {
                addActions(a->menu()->actions(), place.isEmpty() ? text : place + sep + text);
            } else if (!a->isSeparator() && a->isVisible() && a->isEnabled() && !text.isEmpty()) {
                Entry &e = add(Action, text.at(0).isLower() && !tip.isEmpty() ? firstSentence(tip) : text, place, -1);
                e.name += " " + tip.toLower() + " " + place.toLower();
                e.value = a->isCheckable() ? (a->isChecked() ? tr("On") : tr("Off")) :
                                             a->shortcut().toString(QKeySequence::NativeText);
                e.icon = a->icon().isNull() ? icons[Action] : a->icon();
                e.target = a;
            }
        }
    };
    addActions(mWindow->menuBar()->actions(), QString());
    QList<QAction*> toolbar;
    for (QToolButton *b: mWindow->findChildren<QToolButton*>()) {
        QAction *a = b->defaultAction();
        QList<QObject*> users = a ? a->associatedObjects() : QList<QObject*>();
        if (a && !mStack->isAncestorOf(b) && !toolbar.contains(a) &&
                std::none_of(users.begin(), users.end(), [](QObject *o) { return qobject_cast<QMenu*>(o); })) {
            toolbar.append(a);
        }
    }
    addActions(toolbar, tr("Toolbar"));

    // The help texts of the tool
    ConfigParams *info = mVesc->infoConfig();
    for (const QString &name: info->getParamOrder()) {
        ConfigParam *c = info->getParam(name);
        if (c && !c->description.isEmpty() && !name.startsWith("fw_") && !name.startsWith("ios_")) {
            Entry &e = add(Help, c->longName.trimmed(), tr("Help"), -1);
            // The text of the license and the warranty has nearly every word, so they are found by their title only
            e.more = c->description.size() < 4000 ? plain(c->description).toLower() : QString();
            e.key = name;
        }
    }

    for (int i = 0;i < mCanList->count();i++) {
        CANListItem *item = dynamic_cast<CANListItem*>(mCanList->itemWidget(mCanList->item(i)));
        if (item && mCanList->isEnabled()) {
            Entry &e = add(Can, plain(item->name()), tr("CAN devices"), -1);
            e.icon = iconOf(item);
            e.row = item->getID();
            e.value = e.row < 0 ? tr("local") : QString::number(e.row);
        }
    }

    for (Entry &e: mEntries) {
        if (e.icon.isNull()) {
            e.icon = icons[e.kind];
        }
    }
}

// Every word must be found, in any order. Results are grouped by kind; in a group, matches in the name come
// before matches in the help text, and short names and early matches first.
void SearchBox::refresh()
{
    QString query = mEdit->text().simplified().toLower();
    QStringList words = query.split(' ', Qt::SkipEmptyParts);
    QVector<QVector<QPair<int, int>>> found(Kinds);
    QStringList changed;
    QStringList params;

    for (int i = 0;i < mEntries.size();i++) {
        const Entry &e = mEntries.at(i);
        if ((words.isEmpty() || query == "changed") && e.changed && !changed.contains(e.key)) {
            changed.append(e.key);
            found[Changed].append({changed.size(), i});
        }

        int score = e.title.size();
        for (int j = 0;j < words.size() && score >= 0;j++) {
            // Inside a word, or past the title in the hidden part of the name, counts less
            int at = wordAt(e.name, words.at(j));
            bool inside = at < 0;
            at = inside ? e.name.indexOf(words.at(j)) : at;
            if (at >= 0) {
                at += (inside ? 50 : 0) + (at >= e.title.size() ? 100 : 0);
            }
            score = at >= 0 ? score + at : (wordAt(e.more, words.at(j)) >= 0 ? score + 1000 : -1);
        }
        // A parameter that is in more than one table is listed once
        if (e.kind == Param && score >= 0) {
            score = params.contains(e.key) ? -1 : score;
            params.append(e.key);
        }
        if (!words.isEmpty() && score >= 0) {
            found[e.kind].append({score, i});
        }
    }

    static const char *headers[Kinds] = {
        QT_TR_NOOP("Changed"), QT_TR_NOOP("Parameters"), QT_TR_NOOP("Pages"), QT_TR_NOOP("Tabs"),
        QT_TR_NOOP("Actions"), QT_TR_NOOP("Buttons"), QT_TR_NOOP("Sections"), QT_TR_NOOP("Help"),
        QT_TR_NOOP("CAN devices"), QT_TR_NOOP("Examples")};
    // The changed parameters first, then the groups by their best result
    QVector<int> order;
    for (int k = 0;k < Kinds;k++) {
        std::sort(found[k].begin(), found[k].end());
        if (!found.at(k).isEmpty()) {
            order.append(k);
        }
    }
    std::stable_sort(order.begin(), order.end(), [&found](int a, int b) {
        return (a == Changed ? -1 : found.at(a).first().first) < (b == Changed ? -1 : found.at(b).first().first);
    });
    mModel->clear();
    for (int k: order) {
        QStandardItem *header = new QStandardItem(tr(headers[k]));
        header->setData(-1, EntryRole);
        header->setData(found.at(k).size(), ValueRole);
        header->setFlags(Qt::NoItemFlags);
        mModel->appendRow(header);
        // A group alone is listed in full
        for (int n = 0;n < found.at(k).size() && (k == Changed || order.size() == 1 || n < (k == Param ? 6 : 4));n++) {
            const Entry &e = mEntries.at(found.at(k).at(n).second);
            QStandardItem *item = new QStandardItem(e.icon, e.title);
            item->setData(found.at(k).at(n).second, EntryRole);
            item->setData(k, KindRole);
            item->setData(e.place, PlaceRole);
            item->setData(e.value, ValueRole);
            mModel->appendRow(item);
        }
    }

    if (mModel->rowCount() == 0) {
        if (words.isEmpty()) {
            hide();
            return;
        }
        QStandardItem *none = new QStandardItem(tr("No results for “%1”").arg(mEdit->text().simplified()));
        none->setData(-2, EntryRole);
        none->setFlags(Qt::NoItemFlags);
        mModel->appendRow(none);
    }
    mDelegate->mWords = words;
    mView->setCurrentIndex(QModelIndex());
    moveCurrent(1);

    int height = mView->frameWidth() * 2;
    for (int i = 0;i < mModel->rowCount();i++) {
        height += mView->sizeHintForRow(i);
    }
    QPoint at = mEdit->mapToGlobal(QPoint(-shadowSide, mEdit->height() + 2));
    QPoint end = mWindow->mapToGlobal(QPoint(mWindow->width(), mWindow->height()));
    height = qMin(height + shadowTop + shadowBottom + footerHeight + 6, end.y() - at.y() - 4);
    setGeometry(at.x(), at.y(), qMin(520 + 2 * shadowSide, end.x() - at.x()), height);
    show();
    raise();
    update();
}

// Moves the current row to the next result up or down
void SearchBox::moveCurrent(int step)
{
    for (int row = mView->currentIndex().row() + step;row >= 0 && row < mModel->rowCount();row += step) {
        if (mModel->index(row, 0).data(EntryRole).toInt() >= 0) {
            mView->setCurrentIndex(mModel->index(row, 0));
            return;
        }
    }
}

// Goes to the result. Only actions and CAN devices do something: buttons are shown, not pressed.
void SearchBox::activate(int entry)
{
    if (entry < 0 || entry >= mEntries.size()) {
        return;
    }
    Entry e = mEntries.at(entry);
    dismiss();

    switch (e.kind) {
    case Changed:
    case Param: {
        ParamTable *table = qobject_cast<ParamTable*>(e.target);
        if (!table) {
            break;
        }
        open(e);
        if (!table->showParam(e.key)) {
            break;
        }
        // The keyboard goes to the number or text of the editor, selected so that typing replaces it. Not to
        // choices and switches, which the arrow keys would change, not while the arrow keys drive the motor, and
        // not when another row has the same name.
        QAction *keys = mWindow->findChild<QAction*>("actionKeyboardControl");
        QList<QWidget*> boxes;
        for (int r = 0;r < table->rowCount();r++) {
            for (int c = 0;c < table->columnCount();c++) {
                QWidget *editor = table->item(r, c) && table->item(r, c)->text() == e.title ? table->cellWidget(r, c + 1) : nullptr;
                if (editor) {
                    QList<QWidget*> all = editor->findChildren<QWidget*>();
                    auto box = std::find_if(all.begin(), all.end(), [](QWidget *w) {
                        return qobject_cast<QAbstractSpinBox*>(w) || qobject_cast<QLineEdit*>(w); });
                    boxes.append(box == all.end() ? nullptr : *box);
                }
            }
        }
        if (boxes.size() == 1 && boxes.first() && !(keys && keys->isChecked())) {
            boxes.first()->setFocus(Qt::OtherFocusReason);
            QMetaObject::invokeMethod(boxes.first(), "selectAll");
        }
    } break;
    case Page:
    case Tab:
        open(e);
        break;
    case Action:
        if (QAction *a = qobject_cast<QAction*>(e.target)) {
            QTimer::singleShot(0, a, &QAction::trigger);
        }
        break;
    case Button:
    case Section:
        if (QWidget *w = qobject_cast<QWidget*>(e.target)) {
            open(e);
            mark(w, w->rect());
        }
        break;
    case Help:
        QTimer::singleShot(0, this, [this, e]() { HelpDialog::showHelp(mWindow, mVesc->infoConfig(), e.key); });
        break;
    case Can:
        // By its id, as the list may have been scanned again since
        for (int i = 0;i < mCanList->count() && mCanList->isEnabled();i++) {
            CANListItem *item = dynamic_cast<CANListItem*>(mCanList->itemWidget(mCanList->item(i)));
            if (item && item->getID() == e.row) {
                mCanList->setCurrentRow(i);
                break;
            }
        }
        break;
    case Example:
        if (QListWidget *l = qobject_cast<QListWidget*>(e.target); l && e.row < l->count()) {
            open(e);
            l->setCurrentRow(e.row);
            l->scrollToItem(l->item(e.row));
            mark(l->viewport(), l->visualItemRect(l->item(e.row)));
        }
        break;
    default:
        break;
    }
}

// Opens the page and the tabs of the result
void SearchBox::open(const Entry &e)
{
    if (e.page >= 0 && e.page < mPages->count() && !mPages->item(e.page)->isHidden()) {
        mPages->setCurrentRow(e.page);
    }
    for (const auto &t: e.tabs) {
        if (t.first) {
            t.first->setCurrentIndex(t.second);
        }
    }
}

// Draws a frame in the accent colour around a part of a widget for a moment, without changing the widget
void SearchBox::mark(QWidget *w, QRect rect)
{
    for (QWidget *p = w->parentWidget();p;p = p->parentWidget()) {
        if (QScrollArea *area = qobject_cast<QScrollArea*>(p)) {
            area->ensureWidgetVisible(w);
        }
    }
    QPointer<QWidget> target = w;
    QTimer::singleShot(50, this, [this, target, rect]() {
        if (!target || !target->isVisible()) {
            return;
        }
        QFrame *frame = new QFrame(mWindow);
        frame->setObjectName("searchMark");
        frame->setAttribute(Qt::WA_TransparentForMouseEvents);
        frame->setGeometry(QRect(target->mapTo(mWindow, rect.topLeft()), rect.size()).adjusted(-4, -4, 4, 4));
        frame->show();
        frame->raise();
        QTimer::singleShot(1600, frame, &QObject::deleteLater);
    });
}

// Empties the field and gives the focus back to the window, which closes the list
void SearchBox::dismiss()
{
    hide();
    mEdit->clearFocus();
    mEdit->clear();
}

// The key at the right of the field: the shortcut while the field is empty and not in use, esc while it is
void SearchBox::updateKey()
{
    bool focus = mEdit->hasFocus();
    mKey->setText(focus ? "esc" : QKeySequence(Qt::CTRL | Qt::Key_K).toString(QKeySequence::NativeText));
    mKey->setVisible(focus || mEdit->text().isEmpty());
    mKey->adjustSize();
    mKey->move(mEdit->width() - mKey->width() - 7, (mEdit->height() - mKey->height()) / 2);
    mEdit->setTextMargins(0, 0, mKey->isVisible() ? mKey->width() + 6 : 0, 0);
}

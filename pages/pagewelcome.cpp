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

#include "pagewelcome.h"
#include "ui_pagewelcome.h"
#include "setupwizardmotor.h"
#include "setupwizardapp.h"
#include "widgets/detectallfocdialog.h"
#include "widgets/dirsetup.h"
#include "widgets/nrfpair.h"

#include <QMessageBox>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QQmlEngine>
#include <QQmlContext>
#include <QSettings>
#include <QmlHighlighter>
#include <QLibraryInfo>
#include <QVersionNumber>
#include <QQuickWindow>
#include <QNativeGestureEvent>

namespace {
// Shows a tool of the start page in a dialog with a Close button
void showTool(QWidget *tool, const QString &title, QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(tool);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    // None of the buttons is the one that Return presses
    for (QPushButton *button: dialog.findChildren<QPushButton*>()) {
        button->setAutoDefault(false);
    }
    dialog.exec();
}
}

PageWelcome::PageWelcome(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageWelcome)
{
    ui->setupUi(this);

    // Qt 6.12 maps QQuickWidget mouse positions wrong unless it is a native window, and on Windows the touchpad
    // gestures from wintouchpad.cpp need one. The other attributes keep the widgets around it alien.
    bool nativeWindow = QLibraryInfo::version() >= QVersionNumber(6, 12);
#ifdef Q_OS_WIN
    nativeWindow = true;
#endif
    if (nativeWindow) {
        QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
        ui->qmlWidget->setAttribute(Qt::WA_DontCreateNativeAncestors);
        ui->qmlWidget->setAttribute(Qt::WA_NativeWindow);
    }

    // QQuickWidget does not forward touchpad pinches
    ui->qmlWidget->installEventFilter(this);

    mUtil = new Utility(this);

    // Clear of the round corners of the page panel
    layout()->setContentsMargins(4, 4, 4, 4);
    mVesc = nullptr;
}

bool PageWelcome::eventFilter(QObject *object, QEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
    if (object == ui->qmlWidget && event->type() == QEvent::NativeGesture) {
        auto e = static_cast<QNativeGestureEvent*>(event);
        QNativeGestureEvent mapped(e->gestureType(), e->pointingDevice(), e->fingerCount(), e->position(),
                                   e->position(), e->globalPosition(), e->value(), e->delta());
        QCoreApplication::sendEvent(ui->qmlWidget->quickWindow(), &mapped);
        return true;
    }
#endif
    return QWidget::eventFilter(object, event);
}

PageWelcome::~PageWelcome()
{
    delete ui;
}

void PageWelcome::startSetupWizardFocSimple()
{
    if (mVesc) {
        DetectAllFocDialog::showDialog(mVesc, this);
    }
}

void PageWelcome::startSetupWizardFocQml()
{
    if (mVesc) {
        mQmlUi.startCustomGui(mVesc, "qrc:/res/qml/SetupMotorWindow.qml");
    }
}

void PageWelcome::startSetupWizardMotor()
{
    if (mVesc) {
        SetupWizardMotor w(mVesc, this);
        w.exec();
    }
}

void PageWelcome::startSetupWizardApp()
{
    if (mVesc) {
        SetupWizardApp w(mVesc, this);
        w.exec();
    }
}

VescInterface *PageWelcome::vesc() const
{
    return mVesc;
}

void PageWelcome::setVesc(VescInterface *vesc)
{
    mVesc = vesc;

    ui->qmlWidget->engine()->rootContext()->setContextProperty("VescIf", mVesc);
    ui->qmlWidget->engine()->rootContext()->setContextProperty("QmlUi", this);
    ui->qmlWidget->engine()->rootContext()->setContextProperty("Utility", mUtil);

    ui->qmlWidget->setSource(QUrl(QLatin1String("qrc:/res/qml/WelcomeQmlPanel.qml")));
}

void PageWelcome::autoConnect()
{
    Utility::autoconnectBlockingWithProgress(mVesc, this);
}

// Runs an action of the main window, such as the configuration backup
void PageWelcome::triggerWindowAction(const QString &name)
{
    QAction *action = window()->findChild<QAction*>(name);
    if (action) {
        action->trigger();
    }
}

void PageWelcome::showDirSetup()
{
    DirSetup *setup = new DirSetup;
    setup->setVesc(mVesc);
    setup->setMinimumSize(560, 420);
    setup->scanVescs();
    showTool(setup, tr("Direction Setup"), this);
}

void PageWelcome::showNrfPair()
{
    NrfPair *pair = new NrfPair;
    pair->setVesc(mVesc);
    showTool(pair, tr("NRF Pairing"), this);
}

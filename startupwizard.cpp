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

#include "startupwizard.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QScrollBar>
#include <QDesktopServices>
#include <QPainter>
#include <QPushButton>
#include "utility.h"
#include "widgets/helpdialog.h"

StartupWizard::StartupWizard(VescInterface *vesc, QWidget *parent)
    : QWizard(parent)
{
    QSettings().setValue("intro_done", false);

    setPage(Page_Intro, new StartupIntroPage(vesc));
    setPage(Page_Usage, new StartupUsagePage(vesc));
    setPage(Page_Warranty, new StartupWarrantyPage(vesc));
    setPage(Page_Conclusion, new StartupConclusionPage(vesc));

    setStartId(Page_Intro);
    setUpWizardFrame(this);
    setWindowTitle(tr("VESC Tool Introduction"));
}

void setUpWizardFrame(QWizard *wizard)
{
    // The Mac style puts the title and subtitle above the page, on every platform. A transparent
    // background replaces its default picture.
    QPixmap background(1, 1);
    background.fill(Qt::transparent);
    wizard->setWizardStyle(QWizard::MacStyle);
    wizard->setPixmap(QWizard::BackgroundPixmap, background);
    wizard->setButtonLayout({QWizard::CancelButton, QWizard::Stretch, QWizard::BackButton,
                             QWizard::NextButton, QWizard::CommitButton, QWizard::FinishButton});
    wizard->setButtonText(QWizard::BackButton, QWizard::tr("Back"));
    wizard->setButtonText(QWizard::NextButton, QWizard::tr("Next"));
    wizard->resize(880, 560);

    // Back and Next point the way they go, Cancel is a plain outline
    wizard->button(QWizard::BackButton)->setIcon(Utility::getIcon("icons/chevron_l.png"));
    wizard->button(QWizard::NextButton)->setIcon(Utility::getIcon("icons/chevron_r.png"));
    wizard->button(QWizard::NextButton)->setLayoutDirection(Qt::RightToLeft);
    qobject_cast<QPushButton*>(wizard->button(QWizard::CancelButton))->setFlat(true);

    // The pages visited, the current one and those that follow from the choices made so far
    QLabel *steps = new QLabel;
    steps->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    steps->setFixedWidth(236);
    steps->setProperty("steps", true);
    wizard->setSideWidget(steps);

    QObject::connect(wizard, &QWizard::currentIdChanged, steps, [wizard, steps](int id) {
        if (!wizard->page(id)) {
            return;
        }

        // The page sits on the dialog without a frame, its subtitle as a caption under the title
        if (auto *frame = qobject_cast<QFrame*>(wizard->page(id)->parentWidget())) {
            frame->setFrameShape(QFrame::NoFrame);
            frame->setAutoFillBackground(false);
            frame->layout()->setContentsMargins(0, 0, 0, 0);
            for (QLabel *subTitle: frame->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) {
                Utility::setStyleProperty(subTitle, "hint", true);
            }
        }

        QList<int> ids = wizard->visitedIds();
        if (!ids.contains(id)) {
            ids.append(id);
        }
        for (int n = wizard->page(id)->nextId();wizard->page(n) && !ids.contains(n);n = wizard->page(n)->nextId()) {
            ids.append(n);
        }

        // One row per page: a numbered circle, green with a tick when done, ringed when current, joined by lines
        const int rowH = 38;
        QPixmap pm(QSize(steps->width(), 16 + ids.size() * rowH) * steps->devicePixelRatioF());
        pm.setDevicePixelRatio(steps->devicePixelRatioF());
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        QFont numberFont = steps->font();
        numberFont.setPixelSize(10);
        numberFont.setWeight(QFont::Medium);
        for (int i = 0;i < ids.size();i++) {
            bool now = ids.at(i) == id;
            bool done = !now && wizard->hasVisitedPage(ids.at(i));
            QRectF circle(16.5, 16.5 + i * rowH + 9, 19, 19);
            QRectF text(48, 16 + i * rowH, steps->width() - 56, rowH);

            p.setBrush(Qt::NoBrush);
            if (i > 0) {
                p.setPen(Utility::getAppQColor("lineStrong"));
                p.drawLine(QPointF(circle.center().x(), circle.top() - 15), QPointF(circle.center().x(), circle.top() - 3));
            }
            if (now) {
                p.setPen(QPen(Utility::getAppQColor("accentDim"), 3));
                p.drawEllipse(circle.adjusted(-2, -2, 2, 2));
            }
            p.setPen(done ? Qt::NoPen : QPen(Utility::getAppQColor(now ? "accent" : "lineStrong")));
            p.setBrush(done ? QBrush(Utility::getAppQColor("positive")) : QBrush());
            p.drawEllipse(circle);
            if (done) {
                p.setPen(QPen(Qt::white, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                QPointF c = circle.center();
                p.drawPolyline(QPolygonF({c + QPointF(-4, 0), c + QPointF(-1.2, 2.8), c + QPointF(4, -2.6)}));
            } else {
                p.setFont(numberFont);
                p.setPen(Utility::getAppQColor(now ? "accent" : "mutedText"));
                p.drawText(circle, Qt::AlignCenter, QString::number(i + 1));
            }

            QFont titleFont = steps->font();
            titleFont.setWeight(now ? QFont::Medium : QFont::Normal);
            p.setFont(titleFont);
            p.setPen(Utility::getAppQColor(now ? "titleText" : (done ? "lightText" : "mutedText")));
            p.drawText(text, Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(titleFont).elidedText(wizard->page(ids.at(i))->title(), Qt::ElideRight, text.width()));
        }
        steps->setPixmap(pm);
    });
}

StartupIntroPage::StartupIntroPage(VescInterface *vesc, QWidget *parent)
    : QWizardPage(parent)
{
    mBrowser = new VTextBrowser;
    mBrowser->setFrameStyle(QFrame::NoFrame);
    mBrowser->viewport()->setAutoFillBackground(false);

    ConfigParam *p = vesc->infoConfig()->getParam("wizard_startup_intro");
    if (p != 0) {
        setTitle(p->longName);
        mBrowser->setHtml(p->description);
    } else {
        setTitle("Text not found.");
        mBrowser->setText("Text not found.");
    }

    HelpDialog::setReadingSpacing(mBrowser);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(mBrowser);
    setLayout(layout);
}

int StartupIntroPage::nextId() const
{
    return StartupWizard::Page_Usage;
}

StartupUsagePage::StartupUsagePage(VescInterface *vesc, QWidget *parent)
    : QWizardPage(parent)
{
    mBrowser = new VTextBrowser;
    mAcceptBox = new QCheckBox("Yes, I understand and accept");

    ConfigParam *p = vesc->infoConfig()->getParam("wizard_startup_usage");
    if (p != 0) {
        setTitle(p->longName);
        setSubTitle(p->valString);
        mBrowser->setHtml(p->description);
    } else {
        setTitle("Text not found.");
        setSubTitle("Text not found");
        mBrowser->setText("Text not found.");
    }

    registerField("usageAccept*", mAcceptBox);

    connect(mBrowser->verticalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(scrollValueChanged(int)));
    connect(mBrowser->verticalScrollBar(), SIGNAL(rangeChanged(int,int)),
            this, SLOT(scrollRangeChanged()));

    HelpDialog::setReadingSpacing(mBrowser);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(mBrowser);
    layout->addWidget(mAcceptBox);
    setLayout(layout);
}

int StartupUsagePage::nextId() const
{
    return StartupWizard::Page_Warranty;
}

void StartupUsagePage::scrollValueChanged(int value)
{
    if (value == mBrowser->verticalScrollBar()->maximum()) {
        mAcceptBox->setEnabled(true);
    } else {
        mAcceptBox->setEnabled(false);
        mAcceptBox->setChecked(false);
    }
}

void StartupUsagePage::scrollRangeChanged()
{
    scrollValueChanged(mBrowser->verticalScrollBar()->value());
}

StartupWarrantyPage::StartupWarrantyPage(VescInterface *vesc, QWidget *parent)
    : QWizardPage(parent)
{
    mBrowser = new VTextBrowser;
    mAcceptBox = new QCheckBox("Yes, I understand and accept");

    ConfigParam *p = vesc->infoConfig()->getParam("wizard_startup_warranty");
    if (p != 0) {
        setTitle(p->longName);
        setSubTitle(p->valString);
        mBrowser->setHtml(p->description);
    } else {
        setTitle("Text not found.");
        setSubTitle("Text not found");
        mBrowser->setText("Text not found.");
    }

    registerField("warrantyAccept*", mAcceptBox);

    connect(mBrowser->verticalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(scrollValueChanged(int)));
    connect(mBrowser->verticalScrollBar(), SIGNAL(rangeChanged(int,int)),
            this, SLOT(scrollRangeChanged()));

    HelpDialog::setReadingSpacing(mBrowser);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(mBrowser);
    layout->addWidget(mAcceptBox);
    setLayout(layout);
}

int StartupWarrantyPage::nextId() const
{
    return StartupWizard::Page_Conclusion;
}

void StartupWarrantyPage::scrollValueChanged(int value)
{
    if (value == mBrowser->verticalScrollBar()->maximum()) {
        mAcceptBox->setEnabled(true);
    } else {
        mAcceptBox->setEnabled(false);
        mAcceptBox->setChecked(false);
    }
}

void StartupWarrantyPage::scrollRangeChanged()
{
    scrollValueChanged(mBrowser->verticalScrollBar()->value());
}

StartupConclusionPage::StartupConclusionPage(VescInterface *vesc, QWidget *parent)
    : QWizardPage(parent)
{
    mBrowser = new VTextBrowser;
    mBrowser->setFrameStyle(QFrame::NoFrame);
    mBrowser->viewport()->setAutoFillBackground(false);

    ConfigParam *p = vesc->infoConfig()->getParam("wizard_startup_conclusion");
    if (p != 0) {
        setTitle(p->longName);
        mBrowser->setHtml(p->description);
    } else {
        setTitle("Text not found.");
        mBrowser->setText("Text not found.");
    }

    HelpDialog::setReadingSpacing(mBrowser);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(mBrowser);
    setLayout(layout);
}

int StartupConclusionPage::nextId() const
{
    return -1;
}

bool StartupConclusionPage::validatePage()
{
    QSettings().setValue("intro_done", true);
    return true;
}

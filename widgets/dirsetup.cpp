/*
    Copyright 2019 Benjamin Vedder	benjamin@vedder.se

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

#include "dirsetup.h"
#include "ui_dirsetup.h"

#include "utility.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QFrame>

DirSetup::DirSetup(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DirSetup)
{
    ui->setupUi(this);
    ui->refreshButton->setIcon(Utility::getIcon("icons/Refresh-96.png"));
    mVesc = 0;
}

DirSetup::~DirSetup()
{
    delete ui;
}

VescInterface *DirSetup::vesc() const
{
    return mVesc;
}

void DirSetup::setVesc(VescInterface *vesc)
{
    mVesc = vesc;
}

void DirSetup::scanVescs(bool scanCan)
{
    if (mVesc) {
        if (!mVesc->isPortConnected()) {
            mVesc->emitMessageDialog("Direction Setup",
                                     "You are not connected to the VESC. Connect in order to run direction setup.",
                                     false, false);
            return;
        }

        auto *wOld = ui->vescArea->widget();
        if (wOld) {
            wOld->deleteLater();
        }

        ui->refreshButton->setEnabled(false);
        ui->progressBar->setRange(0, 0);

        auto addViewer = [this](QString name, int id, int dev) {
            auto *border = new QFrame;
            border->setProperty("card", true);
            auto *l = new QHBoxLayout;
            l->setContentsMargins(12, 10, 12, 10);
            l->setSpacing(10);

            auto *imageLabel = new QLabel;
            imageLabel->setPixmap(Utility::getIcon(dev >= 0 ? "icons/motor_side.png" : "icons/Connected-96.png"));
            imageLabel->setFixedSize(22, 22);
            imageLabel->setScaledContents(true);
            l->addWidget(imageLabel);
            auto *nameLabel = new QLabel;
            nameLabel->setText(QString("%1<br><span style=\"color:%2\">ID %3</span>").
                               arg(name, Utility::getAppHexColor("mutedText")).arg(id));
            l->addWidget(nameLabel);
            auto *invBox = new QCheckBox;
            invBox->setChecked(Utility::getInvertDirection(mVesc, dev));
            invBox->setText(tr("Inverted"));
            l->addStretch();
            l->addWidget(invBox);

            auto *fwdBt = new QPushButton;
            fwdBt->setText(tr("Forward"));
            l->addWidget(fwdBt);
            auto *revBt = new QPushButton;
            revBt->setText(tr("Reverse"));
            l->addWidget(revBt);

            connect(fwdBt, &QPushButton::clicked, [this,dev]() {
                setEnabled(false);
                ui->progressBar->setRange(0, 0);
                Utility::testDirection(mVesc, dev, 0.1, 2000);
                ui->progressBar->setRange(0, 100);
                setEnabled(true);
            });

            connect(revBt, &QPushButton::clicked, [this,dev]() {
                setEnabled(false);
                ui->progressBar->setRange(0, 0);
                Utility::testDirection(mVesc, dev, -0.1, 2000);
                ui->progressBar->setRange(0, 100);
                setEnabled(true);
            });

            connect(invBox, &QCheckBox::toggled, [this,dev](bool checked) {
                setEnabled(false);
                ui->progressBar->setRange(0, 0);
                Utility::setInvertDirection(mVesc, dev, checked);
                ui->progressBar->setRange(0, 100);
                setEnabled(true);
            });

            border->setLayout(l);
            return border;
        };

        QVBoxLayout *l = new QVBoxLayout;
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(8);
        l->addWidget(addViewer(tr("This VESC"), mVesc->appConfig()->getParamInt("controller_id"), -1));

        if (scanCan) {
            auto canDevs = Utility::scanCanVescOnly(mVesc);
            for (auto d: canDevs) {
                l->addWidget(addViewer(tr("VESC on CAN-bus"), d, d));
            }
        }

        l->addStretch();
        auto *w = new QWidget;
        w->setLayout(l);
        ui->vescArea->setWidget(w);
        ui->vescArea->updateGeometry();

        ui->progressBar->setRange(0, 100);
        ui->refreshButton->setEnabled(true);
    }
}

void DirSetup::on_refreshButton_clicked()
{
    scanVescs();
}

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

#include "pagedebugprint.h"
#include "ui_pagedebugprint.h"
#include "utility.h"
#include <QDateTime>

PageDebugPrint::PageDebugPrint(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageDebugPrint)
{
    ui->setupUi(this);
    ui->clearButton->setIcon(Utility::getIcon("icons/Delete-96.png"));
    layout()->setContentsMargins(0, 0, 0, 0);
}

PageDebugPrint::~PageDebugPrint()
{
    delete ui;
}

void PageDebugPrint::printConsole(QString str)
{
    // The time is dimmed so that the message stands out
    ui->consoleBrowser->queueConsoleText(QString("<font color=\"%1\">%2</font>").
                                        arg(Utility::getAppHexColor("mutedText"),
                                            QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss: ")) + str, true);
}

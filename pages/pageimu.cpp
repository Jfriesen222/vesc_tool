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

#include "pageimu.h"
#include "ui_pageimu.h"
#include "utility.h"

#include <cmath>

PageImu::PageImu(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageImu)
{
    ui->setupUi(this);
    layout()->setContentsMargins(0, 0, 0, 0);
    mVesc = 0;

    mTimer = new QTimer(this);
    mTimer->start(20);
    mUpdatePlots = false;
    mSecondCounter = 0.0;
    mLastUpdateTime = 0;

    connect(mTimer, SIGNAL(timeout()),
            this, SLOT(timerSlot()));

    QCustomPlot *plots[4] = {ui->rpyPlot, ui->accelPlot, ui->gyroPlot, ui->magPlot };
    for(int i = 0; i<4; i++)
    {
        Utility::setPlotColors(plots[i]);
        plots[i]->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

        Utility::setPlotLegendRow(plots[i]);
    }

    int graphIndex = 0;

    ui->rpyPlot->addGraph();
    ui->rpyPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph1")));
    ui->rpyPlot->graph(graphIndex)->setName("Roll");
    graphIndex++;

    ui->rpyPlot->addGraph();
    ui->rpyPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph2")));
    ui->rpyPlot->graph(graphIndex)->setName("Pitch");
    graphIndex++;

    ui->rpyPlot->addGraph();
    ui->rpyPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph3")));
    ui->rpyPlot->graph(graphIndex)->setName("Yaw");
    graphIndex++;

    graphIndex = 0;

    ui->accelPlot->addGraph();
    ui->accelPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph1")));
    ui->accelPlot->graph(graphIndex)->setName("Acc X");
    graphIndex++;

    ui->accelPlot->addGraph();
    ui->accelPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph2")));
    ui->accelPlot->graph(graphIndex)->setName("Acc Y");
    graphIndex++;

    ui->accelPlot->addGraph();
    ui->accelPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph3")));
    ui->accelPlot->graph(graphIndex)->setName("Acc Z");
    graphIndex++;

    graphIndex = 0;

    ui->gyroPlot->addGraph();
    ui->gyroPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph1")));
    ui->gyroPlot->graph(graphIndex)->setName("Gyro X");
    graphIndex++;

    ui->gyroPlot->addGraph();
    ui->gyroPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph2")));
    ui->gyroPlot->graph(graphIndex)->setName("Gyro Y");
    graphIndex++;

    ui->gyroPlot->addGraph();
    ui->gyroPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph3")));
    ui->gyroPlot->graph(graphIndex)->setName("Gyro Z");
    graphIndex++;

    graphIndex = 0;

    ui->magPlot->addGraph();
    ui->magPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph1")));
    ui->magPlot->graph(graphIndex)->setName("Mag X");
    graphIndex++;

    ui->magPlot->addGraph();
    ui->magPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph2")));
    ui->magPlot->graph(graphIndex)->setName("Mag Y");
    graphIndex++;

    ui->magPlot->addGraph();
    ui->magPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph3")));
    ui->magPlot->graph(graphIndex)->setName("Mag Z");
    graphIndex++;

    ui->rpyPlot->xAxis->setLabel("Seconds (s)");
    ui->rpyPlot->yAxis->setLabel("Angle (Deg)");

    ui->accelPlot->xAxis->setLabel("Seconds (s)");
    ui->accelPlot->yAxis->setLabel("Acceleration (G)");

    ui->gyroPlot->xAxis->setLabel("Seconds (s)");
    ui->gyroPlot->yAxis->setLabel("Angular Velocity (Deg/s)");

    ui->magPlot->xAxis->setLabel("Seconds (s)");
    ui->magPlot->yAxis->setLabel("Magnetic Field (µT)");

    m3dView = new Vesc3DView(this);
    m3dView->setMinimumWidth(200);
    m3dView->setRollPitchYaw(20, 20, 0);
    m3dView->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    ui->viewWell->layout()->addWidget(m3dView);
}

PageImu::~PageImu()
{
    delete ui;
}

VescInterface *PageImu::vesc() const
{
    return mVesc;
}

void PageImu::setVesc(VescInterface *vesc)
{
    mVesc = vesc;

    if (mVesc) {
        connect(mVesc->commands(), SIGNAL(valuesImuReceived(IMU_VALUES,uint)),
                this, SLOT(valuesReceived(IMU_VALUES,uint)));
    }
}

void PageImu::timerSlot()
{
    if (mUpdatePlots) {
        mUpdatePlots = false;

        int dataSize = mRollVec.size();

        QVector<double> xAxis(dataSize);
        for (int i = 0;i < mSeconds.size();i++) {
            xAxis[i] = mSeconds[i];
        }

        int graphIndex = 0;
        ui->rpyPlot->graph(graphIndex++)->setData(xAxis, mRollVec);
        ui->rpyPlot->graph(graphIndex++)->setData(xAxis, mPitchVec);
        ui->rpyPlot->graph(graphIndex++)->setData(xAxis, mYawVec);

        graphIndex = 0;
        ui->accelPlot->graph(graphIndex++)->setData(xAxis, mAccXVec);
        ui->accelPlot->graph(graphIndex++)->setData(xAxis, mAccYVec);
        ui->accelPlot->graph(graphIndex++)->setData(xAxis, mAccZVec);

        graphIndex = 0;
        ui->gyroPlot->graph(graphIndex++)->setData(xAxis, mGyroXVec);
        ui->gyroPlot->graph(graphIndex++)->setData(xAxis, mGyroYVec);
        ui->gyroPlot->graph(graphIndex++)->setData(xAxis, mGyroZVec);

        graphIndex = 0;
        ui->magPlot->graph(graphIndex++)->setData(xAxis, mMagXVec);
        ui->magPlot->graph(graphIndex++)->setData(xAxis, mMagYVec);
        ui->magPlot->graph(graphIndex++)->setData(xAxis, mMagZVec);

        // The legend shows the latest value of each trace
        QCustomPlot *plots[4] = {ui->rpyPlot, ui->accelPlot, ui->gyroPlot, ui->magPlot};
        QStringList names = {"Roll", "Pitch", "Yaw", "Acc X", "Acc Y", "Acc Z",
                             "Gyro X", "Gyro Y", "Gyro Z", "Mag X", "Mag Y", "Mag Z"};
        QVector<double> *values[12] = {&mRollVec, &mPitchVec, &mYawVec, &mAccXVec, &mAccYVec, &mAccZVec,
                                       &mGyroXVec, &mGyroYVec, &mGyroZVec, &mMagXVec, &mMagYVec, &mMagZVec};
        QStringList units = {"\u00B0", " g", " \u00B0/s", " \u00B5T"};
        for (int i = 0;i < 12;i++) {
            plots[i / 3]->graph(i % 3)->setName(Utility::plotLegendName(names.at(i), values[i]->last(),
                                                                         i / 3 == 1 ? 2 : 1, units.at(i / 3)));
        }

        ui->rpyPlot->rescaleAxes();
        ui->accelPlot->rescaleAxes();
        ui->gyroPlot->rescaleAxes();
        ui->magPlot->rescaleAxes();

        ui->rpyPlot->replotWhenVisible();
        ui->accelPlot->replotWhenVisible();
        ui->gyroPlot->replotWhenVisible();
        ui->magPlot->replotWhenVisible();
    }
}

void PageImu::valuesReceived(IMU_VALUES values, unsigned int mask)
{
    (void)mask;

    const int maxS = 500;

    m3dView->setRollPitchYaw(values.roll * 180.0 / M_PI, values.pitch * 180.0 / M_PI,
                             ui->useYawBox->isChecked() ? values.yaw * 180.0 / M_PI : 0.0);

    // The angles of the 3D view, with yaw dimmed while the view does not use it
    QString angle = "%1 <font color=\"%2\">%3\u00B0</font>";
    ui->anglesLabel->setText(
                angle.arg(tr("Roll"), Utility::getAppHexColor("titleText")).arg(values.roll * 180.0 / M_PI, 0, 'f', 1) +
                "&nbsp;&nbsp;&nbsp;" +
                angle.arg(tr("Pitch"), Utility::getAppHexColor("titleText")).arg(values.pitch * 180.0 / M_PI, 0, 'f', 1) +
                "&nbsp;&nbsp;&nbsp;" +
                angle.arg(tr("Yaw"), Utility::getAppHexColor(ui->useYawBox->isChecked() ? "titleText" : "disabledText")).
                arg(values.yaw * 180.0 / M_PI, 0, 'f', 1));

    appendDoubleAndTrunc(&mRollVec, values.roll * 180.0 / M_PI, maxS);
    appendDoubleAndTrunc(&mPitchVec, values.pitch * 180.0 / M_PI, maxS);
    appendDoubleAndTrunc(&mYawVec, values.yaw * 180.0 / M_PI, maxS);

    appendDoubleAndTrunc(&mAccXVec, values.accX, maxS);
    appendDoubleAndTrunc(&mAccYVec, values.accY, maxS);
    appendDoubleAndTrunc(&mAccZVec, values.accZ, maxS);

    appendDoubleAndTrunc(&mGyroXVec, values.gyroX, maxS);
    appendDoubleAndTrunc(&mGyroYVec, values.gyroY, maxS);
    appendDoubleAndTrunc(&mGyroZVec, values.gyroZ, maxS);

    appendDoubleAndTrunc(&mMagXVec, values.magX, maxS);
    appendDoubleAndTrunc(&mMagYVec, values.magY, maxS);
    appendDoubleAndTrunc(&mMagZVec, values.magZ, maxS);

    qint64 tNow = QDateTime::currentMSecsSinceEpoch();

    double elapsed = (double)(tNow - mLastUpdateTime) / 1000.0;
    if (elapsed > 1.0) {
        elapsed = 1.0;
    }

    mSecondCounter += elapsed;

    appendDoubleAndTrunc(&mSeconds, mSecondCounter, maxS);

    mLastUpdateTime = tNow;

    mUpdatePlots = true;
}

void PageImu::appendDoubleAndTrunc(QVector<double> *vec, double num, int maxSize)
{
    vec->append(num);

    if(vec->size() > maxSize) {
        vec->remove(0, vec->size() - maxSize);
    }
}

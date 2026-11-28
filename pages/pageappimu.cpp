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

#include "pageappimu.h"
#include "ui_pageappimu.h"
#include "utility.h"
#include <QDateTime>
#include <QQuickItem>
#include <cmath>

PageAppImu::PageAppImu(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageAppImu)
{
    ui->setupUi(this);
    mVesc = nullptr;

    mTimer = new QTimer(this);
    mTimer->start(20);
    mUpdatePlots = false;
    mSecondCounter = 0.0;
    mLastUpdateTime = 0;

    connect(mTimer, SIGNAL(timeout()),
            this, SLOT(timerSlot()));

    QCustomPlot *plots[3] = {ui->rpyPlot, ui->accelPlot, ui->gyroPlot};
    for(int i = 0; i<3; i++)
    {
        Utility::setPlotColors(plots[i]);
        plots[i]->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
        Utility::setPlotLegendRow(plots[i]);
        // Lower than the legend and the axes need, as the plots in the old tool
        plots[i]->setMinimumSize(40, 35);
    }
    ui->splitter->setSizes(QList<int>({1000, 1030}));

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

    ui->accelPlot->xAxis->setLabel("Seconds (s)");
    ui->accelPlot->yAxis->setLabel("Acceleration (G)");
    ui->rpyPlot->xAxis->setLabel("Seconds (s)");
    ui->rpyPlot->yAxis->setLabel("Angle (Deg)");
    ui->gyroPlot->xAxis->setLabel("Seconds (s)");
    ui->gyroPlot->yAxis->setLabel("Angular Velocity (Deg/s)");

    m3dView = new Vesc3DView(this);
    m3dView->setMinimumWidth(120);
    m3dView->setRollPitchYaw(20, 20, 0);
    m3dView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    mUseYawBox = new QCheckBox("Use Yaw (will drift)");
    ui->cornerWidget->layout()->addWidget(mUseYawBox);
    ui->cornerWidget->layout()->addWidget(m3dView);
}

PageAppImu::~PageAppImu()
{
    delete ui;
}

VescInterface *PageAppImu::vesc() const
{
    return mVesc;
}

void PageAppImu::setVesc(VescInterface *vesc)
{
    mVesc = vesc;

    if (mVesc) {
        reloadParams();

        connect(mVesc->commands(), SIGNAL(valuesImuReceived(IMU_VALUES,uint)),
                this, SLOT(valuesReceived(IMU_VALUES,uint)));
    }
}

void PageAppImu::reloadParams()
{
    if (mVesc) {
        ui->tableWidget->clearParams();
        ui->tableWidget->addParamSubgroup(mVesc->appConfig(), "imu", "general");
    }
}

// A compact window gives most of the height to the plots
void PageAppImu::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);

    if (event->type() != QEvent::StyleChange || window()->property("compact").toBool() == mCompact) {
        return;
    }

    mCompact = !mCompact;
    ui->splitter->setSizes(mCompact ? QList<int>({1000, 2000}) : QList<int>({1000, 1030}));
}

void PageAppImu::timerSlot()
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

        // The legend shows the latest value of each trace
        QCustomPlot *plots[3] = {ui->rpyPlot, ui->accelPlot, ui->gyroPlot};
        QStringList names = {"Roll", "Pitch", "Yaw", "Acc X", "Acc Y", "Acc Z", "Gyro X", "Gyro Y", "Gyro Z"};
        QVector<double> *values[9] = {&mRollVec, &mPitchVec, &mYawVec, &mAccXVec, &mAccYVec, &mAccZVec,
                                      &mGyroXVec, &mGyroYVec, &mGyroZVec};
        QStringList units = {"°", " G", " °/s"};
        for (int i = 0;i < 9;i++) {
            plots[i / 3]->graph(i % 3)->setName(Utility::plotLegendName(names.at(i), values[i]->last(), 3, units.at(i / 3)));
        }

        // Two rows when the values do not fit next to each other, and no axis title where it does not fit
        QStringList yLabels = {"Angle (Deg)", "Acceleration (G)", "Angular Velocity (Deg/s)"};
        for (int i = 0;i < 3;i++) {
            QCPLegend *legend = plots[i]->legend;
            int width = 20 + 2 * legend->columnSpacing();
            for (int j = 0;j < legend->elementCount();j++) {
                if (legend->elementAt(j)) {
                    width += legend->elementAt(j)->minimumOuterSizeHint().width();
                }
            }
            int wrap = width > plots[i]->width() ? 2 : 0;
            if (legend->wrap() != wrap) {
                legend->setWrap(wrap);
                legend->setFillOrder(QCPLayoutGrid::foColumnsFirst, true);
            }
            int height = plots[i]->axisRect()->height();
            plots[i]->yAxis->setLabel(QFontMetrics(plots[i]->yAxis->labelFont()).horizontalAdvance(yLabels.at(i)) < height ?
                                          yLabels.at(i) : "");
        }

        ui->rpyPlot->rescaleAxes();
        ui->accelPlot->rescaleAxes();
        ui->gyroPlot->rescaleAxes();

        ui->rpyPlot->replotWhenVisible();
        ui->accelPlot->replotWhenVisible();
        ui->gyroPlot->replotWhenVisible();
    }
}

void PageAppImu::valuesReceived(IMU_VALUES values, unsigned int mask)
{
    (void)mask;

    const int maxS = 500;

    m3dView->setRollPitchYaw(values.roll * 180.0 / M_PI, values.pitch * 180.0 / M_PI,
                             mUseYawBox->isChecked() ? values.yaw * 180.0 / M_PI : 0.0);

    appendDoubleAndTrunc(&mRollVec, values.roll * 180.0 / M_PI, maxS);
    appendDoubleAndTrunc(&mPitchVec, values.pitch * 180.0 / M_PI, maxS);
    appendDoubleAndTrunc(&mYawVec, values.yaw * 180.0 / M_PI, maxS);

    appendDoubleAndTrunc(&mAccXVec, values.accX, maxS);
    appendDoubleAndTrunc(&mAccYVec, values.accY, maxS);
    appendDoubleAndTrunc(&mAccZVec, values.accZ, maxS);

    appendDoubleAndTrunc(&mGyroXVec, values.gyroX, maxS);
    appendDoubleAndTrunc(&mGyroYVec, values.gyroY, maxS);
    appendDoubleAndTrunc(&mGyroZVec, values.gyroZ, maxS);

    qint64 tNow = QDateTime::currentMSecsSinceEpoch();

    double elapsed = double((tNow - mLastUpdateTime)) / 1000.0;
    if (elapsed > 1.0) {
        elapsed = 1.0;
    }

    mSecondCounter += elapsed;
    appendDoubleAndTrunc(&mSeconds, mSecondCounter, maxS);

    mLastUpdateTime = tNow;
    mUpdatePlots = true;
}

void PageAppImu::appendDoubleAndTrunc(QVector<double> *vec, double num, int maxSize)
{
    vec->append(num);

    if(vec->size() > maxSize) {
        vec->remove(0, vec->size() - maxSize);
    }
}

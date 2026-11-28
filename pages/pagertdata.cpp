/*
    Copyright 2016 - 2022 Benjamin Vedder	benjamin@vedder.se

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

#include "pagertdata.h"
#include "ui_pagertdata.h"
#include <QFileDialog>
#include <QMessageBox>
#include "utility.h"

#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QRegularExpression>

namespace {
// A reading with its unit smaller and dimmed
QString readingText(double value, int decimals, QString unit = "")
{
    return QString("%1 <small><font color=\"%2\">%3</font></small>").arg(value, 0, 'f', decimals).
            arg(Utility::getAppHexColor("mutedText"), unit);
}

// A whole number with thin spaces between groups of three digits
QString groupedText(qint64 value)
{
    return QString::number(value).replace(QRegularExpression("(\\d)(?=(\\d{3})+$)"), "\\1\u2009");
}
}

PageRtData::PageRtData(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageRtData)
{
    ui->setupUi(this);
    layout()->setContentsMargins(0, 0, 0, 0);
    mVesc = nullptr;

    // The streaming rate shares the row of the tabs. The values sit under the plot of the
    // first tab and the plot tools over the top right of the well of the open tab.
    mLiveChip = new QLabel;
    mLiveChip->setProperty("chip", "live");
    mLiveChip->hide();
    ui->tabWidget->setCornerWidget(mLiveChip, Qt::TopRightCorner);
    Utility::placeOnTabs(ui->tabWidget, ui->buttonFrame);

    // The checked state shows as a raised button, so the icons stay the same
    ui->rescaleButton->setIcon(Utility::getIcon("icons/size_off.png"));
    ui->zoomHButton->setIcon(Utility::getIcon("icons/expand_off.png"));
    ui->zoomVButton->setIcon(Utility::getIcon("icons/expand_v_off.png"));
    QIcon logIcon(Utility::getIcon("icons/led_off.png"));
    logIcon.addPixmap(Utility::getIcon("icons/led_bad.png"), QIcon::Normal, QIcon::On);
    ui->logRtButton->setIcon(logIcon);
    ui->logRtButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // Each tile shows a trend line in the colour of the chip before its caption
    QList<QPair<QLabel*, Sparkline*>> tiles = {
        {ui->powerCaption, ui->powerSpark}, {ui->dutyCaption, ui->dutySpark}, {ui->erpmCaption, ui->erpmSpark},
        {ui->currentInCaption, ui->currentInSpark}, {ui->currentMotorCaption, ui->currentMotorSpark}};
    QStringList tileColors = {"plot_graph7", "plot_graph3", "plot_graph4", "plot_graph1", "plot_graph2"};
    for (int i = 0;i < tiles.size();i++) {
        QColor color = Utility::getAppQColor(tileColors.at(i));
        tiles.at(i).second->setColor(color);
        tiles.at(i).first->setText(QString("<span style=\"color:%1\">&#9632;</span>&nbsp; %2").
                                   arg(color.name(), tiles.at(i).first->text()));
    }

    mTimer = new QTimer(this);
    mTimer->start(20);

    mSecondCounter = 0.0;
    mLastUpdateTime = 0;
    mRateCount = 0;
    mRateStart = 0;

    mUpdateValPlot = false;
    mUpdatePosPlot = false;

    QCustomPlot* allPlots[] =
                {ui->currentPlot, ui->tempPlot, ui->focPlot,
                ui->posPlot, ui->rpmPlot};
    for(int j = 0;j < 5; j++) {
        Utility::setPlotColors(allPlots[j]);
        allPlots[j]->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

        // The position plot has a single trace, so the axis label is enough
        if (allPlots[j] != ui->posPlot) {
            Utility::setPlotLegendRow(allPlots[j]);
            // As many columns as fit beside the plot tools
            QCustomPlot *plot = allPlots[j];
            connect(plot, &QCustomPlot::beforeReplot, this, [this, plot]() {
                Utility::fitPlotLegend(plot, ui->buttonFrame->width() + 16);
            });
            // A press on a trace in the legend hides it, and shows it again. What is hidden is kept by name, for
            // when the traces of the temperatures are made anew.
            auto entryAt = [plot](QPoint pos) {
                for (int i = 0;i < plot->legend->itemCount();i++) {
                    QCPPlottableLegendItem *entry = qobject_cast<QCPPlottableLegendItem*>(plot->legend->item(i));
                    if (entry && entry->outerRect().contains(pos)) {
                        return entry;
                    }
                }
                return (QCPPlottableLegendItem*)nullptr;
            };
            connect(plot, &QCustomPlot::mouseMove, this, [plot, entryAt](QMouseEvent *event) {
                plot->setCursor(entryAt(event->pos()) ? Qt::PointingHandCursor : Qt::ArrowCursor);
            });
            connect(plot, &QCustomPlot::mousePress, this, [this, plot, entryAt](QMouseEvent *event) {
                if (QCPPlottableLegendItem *entry = entryAt(event->pos())) {
                    QString name = entry->plottable()->name().split("   ").first();
                    if (!mHiddenTraces.remove(name)) {
                        mHiddenTraces.insert(name);
                    }
                    entry->plottable()->setVisible(!mHiddenTraces.contains(name));
                    plot->rescaleAxes(true);
                    plot->replot();
                }
            });
        }
    }

    // Current and duty
    int graphIndex = 0;
    ui->currentPlot->addGraph();

    ui->currentPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph1")));
    ui->currentPlot->graph(graphIndex)->setName("Current in");
    graphIndex++;

    ui->currentPlot->addGraph();
    ui->currentPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph2")));
    ui->currentPlot->graph(graphIndex)->setName("Current motor");
    graphIndex++;

    ui->currentPlot->addGraph(ui->currentPlot->xAxis, ui->currentPlot->yAxis2);
    ui->currentPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph3")));
    ui->currentPlot->graph(graphIndex)->setName("Duty cycle");
    graphIndex++;

    // RPM
    graphIndex = 0;
    ui->rpmPlot->addGraph();
    ui->rpmPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph1")));
    ui->rpmPlot->graph(graphIndex)->setName("ERPM");
    graphIndex++;

    // FOC
    graphIndex = 0;
    ui->focPlot->addGraph();
    ui->focPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph1")));
    ui->focPlot->graph(graphIndex)->setName("D Current");
    graphIndex++;

    ui->focPlot->addGraph();
    ui->focPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph2")));
    ui->focPlot->graph(graphIndex)->setName("Q Current");
    graphIndex++;

    ui->focPlot->addGraph(ui->focPlot->xAxis, ui->focPlot->yAxis2);
    ui->focPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph3")));
    ui->focPlot->graph(graphIndex)->setName("D Voltage");
    graphIndex++;

    ui->focPlot->addGraph(ui->focPlot->xAxis, ui->focPlot->yAxis2);
    ui->focPlot->graph(graphIndex)->setPen(QPen(Utility::getAppQColor("plot_graph4")));
    ui->focPlot->graph(graphIndex)->setName("Q Voltage");
    graphIndex++;

    ui->currentPlot->xAxis->setLabel("Seconds (s)");
    ui->currentPlot->yAxis->setLabel("Ampere (A)");
    ui->currentPlot->yAxis2->setLabel("Duty Cycle");

    ui->tempPlot->xAxis->setLabel("Seconds (s)");
    ui->tempPlot->yAxis->setLabel("Temperature MOSFET (\u00B0C)");
    ui->tempPlot->yAxis2->setLabel("Temperature Motor (\u00B0C)");

    ui->rpmPlot->xAxis->setLabel("Seconds (s)");
    ui->rpmPlot->yAxis->setLabel("ERPM");

    ui->focPlot->xAxis->setLabel("Seconds (s)");
    ui->focPlot->yAxis->setLabel("Current");
    ui->focPlot->yAxis2->setLabel("Voltage");

    ui->currentPlot->yAxis->setRange(-20, 130);
    ui->currentPlot->yAxis2->setRange(-0.2, 1.3);
    ui->currentPlot->yAxis2->setVisible(true);
    ui->tempPlot->yAxis->setRange(0, 120);
    ui->tempPlot->yAxis2->setRange(0, 120);
    ui->tempPlot->yAxis2->setVisible(true);
    ui->rpmPlot->yAxis->setRange(0, 120);
    ui->focPlot->yAxis->setRange(0, 120);
    ui->focPlot->yAxis2->setRange(0, 120);
    ui->focPlot->yAxis2->setVisible(true);

    ui->posPlot->addGraph();
    ui->posPlot->graph(0)->setPen(QPen(Utility::getAppQColor("plot_graph1")));
    ui->posPlot->graph(0)->setName("Position");

    ui->posPlot->xAxis->setLabel("Sample");
    ui->posPlot->yAxis->setLabel("Degrees");

    connect(mTimer, SIGNAL(timeout()),
            this, SLOT(timerSlot()));

    updateZoom();
}

PageRtData::~PageRtData()
{
    delete ui;
}

VescInterface *PageRtData::vesc() const
{
    return mVesc;
}

void PageRtData::setVesc(VescInterface *vesc)
{
    mVesc = vesc;

    if (mVesc) {
        connect(mVesc->commands(), SIGNAL(valuesReceived(MC_VALUES,uint)),
                this, SLOT(valuesReceived(MC_VALUES,uint)));
        connect(mVesc->commands(), SIGNAL(rotorPosReceived(double)),
                this, SLOT(rotorPosReceived(double)));
    }
}

// A compact window has the tiles in two rows beside the strip of values, and the second half of the strip and of the
// buttons of the rotor position under the first
void PageRtData::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);

    if (event->type() != QEvent::StyleChange || window()->property("compact").toBool() == mCompact) {
        return;
    }

    mCompact = !mCompact;

    // The tiles stand at the left of the strip, the two currents in a second row
    ui->valueLayout->setDirection(mCompact ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    for (QFrame *tile: {ui->powerTile, ui->dutyTile, ui->erpmTile, ui->currentInTile, ui->currentMotorTile}) {
        tile->layout()->setContentsMargins(mCompact ? 8 : 12, mCompact ? 4 : 8, mCompact ? 8 : 12, mCompact ? 4 : 8);
        if (tile == ui->currentInTile || tile == ui->currentMotorTile) {
            (mCompact ? ui->tileRow2 : ui->tileLayout)->addWidget(tile);
        }
    }

    QList<QPair<QGridLayout*, int>> grids = {{ui->stripLayout, 6}, {ui->horizontalLayout_7, 4}};
    for (auto grid: grids) {
        for (int i = grid.first->count() - 1;i >= 0;i--) {
            int row, column, rowSpan, columnSpan;
            grid.first->getItemPosition(i, &row, &column, &rowSpan, &columnSpan);
            if (mCompact ? column >= grid.second : row >= 3) {
                QLayoutItem *item = grid.first->takeAt(i);
                grid.first->addItem(item, mCompact ? row + 3 : row - 3, mCompact ? column - grid.second : column + grid.second,
                                    rowSpan, columnSpan);
            }
        }
    }
    for (int c = 6;c < 12;c++) {
        ui->stripLayout->setColumnStretch(c, (mCompact || c == 9) ? 0 : 4);
    }
    ui->stripLayout->setHorizontalSpacing(mCompact ? 10 : 16);
    ui->stripLayout->setVerticalSpacing(mCompact ? 5 : 2);
    ui->stripLayout->setContentsMargins(mCompact ? 8 : 12, 8, mCompact ? 8 : 12, 8);
}

void PageRtData::timerSlot()
{
    if (mVesc) {
        if (mVesc->isRtLogOpen() != ui->logRtButton->isChecked()) {
            ui->logRtButton->setChecked(mVesc->isRtLogOpen());
        }
    }

    // The header shows the rate at which values arrive, while they do
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - mRateStart >= 1000) {
        if (mRateCount > 0) {
            // In a table the lamp and the text are centered on each other
            mLiveChip->setText(QString("<table cellspacing=\"0\" cellpadding=\"0\"><tr><td valign=\"middle\"><img src=\"%1desktop/icons/led_ok.png\" "
                                       "width=\"16\" height=\"16\"></td><td valign=\"middle\">&nbsp;%2</td></tr></table>").
                               arg(Utility::getThemePath(), tr("Streaming · %1 Hz").arg(qRound(mRateCount * 1000.0 / (now - mRateStart)))));
        }
        mRateCount = 0;
        mRateStart = now;
    }
    mLiveChip->setVisible(now - mLastUpdateTime < 2000 && !mLiveChip->text().isEmpty() && !mCompact);

    if (mUpdateValPlot) {
        int dataSize = mTempMosVec.size();

        QVector<double> xAxis(dataSize);
        for (int i = 0;i < mSeconds.size();i++) {
            xAxis[i] = mSeconds[i];
        }

        // Current and duty-plot
        int graphIndex = 0;
        ui->currentPlot->graph(graphIndex++)->setData(xAxis, mCurrInVec);
        ui->currentPlot->graph(graphIndex++)->setData(xAxis, mCurrMotorVec);
        ui->currentPlot->graph(graphIndex++)->setData(xAxis, mDutyVec);
        ui->currentPlot->graph(0)->setName(Utility::plotLegendName("Current in", mCurrInVec.last(), 2, " A"));
        ui->currentPlot->graph(1)->setName(Utility::plotLegendName("Current motor", mCurrMotorVec.last(), 2, " A"));
        ui->currentPlot->graph(2)->setName(Utility::plotLegendName("Duty cycle", mDutyVec.last() * 100.0, 1, " %"));

        ui->powerSpark->setData(mPowerVec);
        ui->dutySpark->setData(mDutyVec);
        ui->erpmSpark->setData(mRpmVec);
        ui->currentInSpark->setData(mCurrInVec);
        ui->currentMotorSpark->setData(mCurrMotorVec);

        // Temperature plot. The hardware decides which traces there are, so they are made anew only when that changes.
        QVector<QPair<QString, QVector<double>*>> temps = {{"MOSFET", &mTempMosVec}};
        if (!mTempMos1Vec.isEmpty() && mTempMos1Vec.last() != 0.0) {
            temps.append({"MOSFET 1", &mTempMos1Vec});
            temps.append({"MOSFET 2", &mTempMos2Vec});
            temps.append({"MOSFET 3", &mTempMos3Vec});
        }
        temps.append({"Motor", &mTempMotorVec});
        if (ui->tempPlot->graphCount() != temps.size()) {
            ui->tempPlot->clearGraphs();
            for (int i = 0;i < temps.size();i++) {
                bool motor = i == temps.size() - 1;
                ui->tempPlot->addGraph(ui->tempPlot->xAxis, motor ? ui->tempPlot->yAxis2 : ui->tempPlot->yAxis);
                ui->tempPlot->graph(i)->setPen(QPen(Utility::getAppQColor(QString("plot_graph%1").arg(motor ? 5 : i + 1))));
                ui->tempPlot->graph(i)->setVisible(!mHiddenTraces.contains(temps.at(i).first));
            }
        }
        for (int i = 0;i < temps.size();i++) {
            ui->tempPlot->graph(i)->setName(Utility::plotLegendName(temps.at(i).first, temps.at(i).second->last(), 1, " \u00B0C"));
            ui->tempPlot->graph(i)->setData(xAxis, *temps.at(i).second);
        }

        // RPM plot
        graphIndex = 0;
        ui->rpmPlot->graph(graphIndex++)->setData(xAxis, mRpmVec);
        ui->rpmPlot->graph(0)->setName(Utility::plotLegendName("ERPM", mRpmVec.last(), 0));

        // FOC plot
        graphIndex = 0;
        ui->focPlot->graph(graphIndex++)->setData(xAxis, mIdVec);
        ui->focPlot->graph(graphIndex++)->setData(xAxis, mIqVec);
        ui->focPlot->graph(graphIndex++)->setData(xAxis, mVdVec);
        ui->focPlot->graph(graphIndex++)->setData(xAxis, mVqVec);
        ui->focPlot->graph(0)->setName(Utility::plotLegendName("D Current", mIdVec.last(), 2, " A"));
        ui->focPlot->graph(1)->setName(Utility::plotLegendName("Q Current", mIqVec.last(), 2, " A"));
        ui->focPlot->graph(2)->setName(Utility::plotLegendName("D Voltage", mVdVec.last(), 2, " V"));
        ui->focPlot->graph(3)->setName(Utility::plotLegendName("Q Voltage", mVqVec.last(), 2, " V"));

        if (ui->autoscaleButton->isChecked()) {
            ui->currentPlot->rescaleAxes(true);
            ui->tempPlot->rescaleAxes(true);
            ui->rpmPlot->rescaleAxes(true);
            ui->focPlot->rescaleAxes(true);
        }

        ui->currentPlot->replotWhenVisible();
        ui->tempPlot->replotWhenVisible();
        ui->rpmPlot->replotWhenVisible();
        ui->focPlot->replotWhenVisible();

        mUpdateValPlot = false;
    }

    if (mUpdatePosPlot) {
        QVector<double> xAxis(mPositionVec.size());
        for (int i = 0;i < mPositionVec.size();i++) {
            xAxis[i] = double(i);
        }

        ui->posBar->setValue(int(fabs(mPositionVec.last())));
        ui->posPlot->graph(0)->setData(xAxis, mPositionVec);

        if (ui->autoscaleButton->isChecked()) {
            ui->posPlot->rescaleAxes();
        }

        ui->posPlot->replotWhenVisible();

        mUpdatePosPlot = false;
    }
}

void PageRtData::valuesReceived(MC_VALUES values, unsigned int mask)
{
    (void)mask;

    ui->powerValue->setText(readingText(values.v_in * values.current_in, 1, "W"));
    ui->dutyValue->setText(readingText(values.duty_now * 100.0, 1, "%"));
    ui->erpmValue->setText(groupedText(qRound64(values.rpm)));
    ui->currentInValue->setText(readingText(values.current_in, 2, "A"));
    ui->currentMotorValue->setText(readingText(values.current_motor, 2, "A"));
    ui->tempMosValue->setText(readingText(values.temp_mos, 1, "\u00B0C"));
    ui->tempMotorValue->setText(readingText(values.temp_motor, 1, "\u00B0C"));
    ConfigParams *mcConf = mVesc->mcConfig();
    ui->tempMosBar->setLevel(values.temp_mos, mcConf->getParamDouble("l_temp_fet_start"),
                             mcConf->getParamDouble("l_temp_fet_end"));
    ui->tempMotorBar->setLevel(values.temp_motor, mcConf->getParamDouble("l_temp_motor_start"),
                               mcConf->getParamDouble("l_temp_motor_end"));
    bool noFault = values.fault_code == FAULT_CODE_NONE;
    ui->faultValue->setText(noFault ? tr("None") : values.fault_str.mid(11));
    Utility::setStyleProperty(ui->faultValue, "ok", noFault);
    ui->vinValue->setText(readingText(values.v_in, 1, "V"));
    ui->ahDrawnValue->setText(readingText(values.amp_hours * 1000.0, 1, "mAh"));
    ui->ahChargedValue->setText(readingText(values.amp_hours_charged * 1000.0, 1, "mAh"));
    ui->whDrawnValue->setText(readingText(values.watt_hours, 2, "Wh"));
    ui->whChargedValue->setText(readingText(values.watt_hours_charged, 2, "Wh"));
    ui->tachValue->setText(groupedText(values.tachometer));
    ui->tachAbsValue->setText(groupedText(values.tachometer_abs));

    const int maxS = 500;

    appendDoubleAndTrunc(&mTempMosVec, values.temp_mos, maxS);
    appendDoubleAndTrunc(&mTempMos1Vec, values.temp_mos_1, maxS);
    appendDoubleAndTrunc(&mTempMos2Vec, values.temp_mos_2, maxS);
    appendDoubleAndTrunc(&mTempMos3Vec, values.temp_mos_3, maxS);
    appendDoubleAndTrunc(&mTempMotorVec, values.temp_motor, maxS);
    appendDoubleAndTrunc(&mCurrInVec, values.current_in, maxS);
    appendDoubleAndTrunc(&mCurrMotorVec, values.current_motor, maxS);
    appendDoubleAndTrunc(&mIdVec, values.id, maxS);
    appendDoubleAndTrunc(&mIqVec, values.iq, maxS);
    appendDoubleAndTrunc(&mDutyVec, values.duty_now, maxS);
    appendDoubleAndTrunc(&mRpmVec, values.rpm, maxS);
    appendDoubleAndTrunc(&mVdVec, values.vd, maxS);
    appendDoubleAndTrunc(&mVqVec, values.vq, maxS);
    appendDoubleAndTrunc(&mPowerVec, values.v_in * values.current_in, maxS);
    mRateCount++;

    qint64 tNow = QDateTime::currentMSecsSinceEpoch();

    double elapsed = double((tNow - mLastUpdateTime)) / 1000.0;
    if (elapsed > 1.0) {
        elapsed = 1.0;
    }

    mSecondCounter += elapsed;

    appendDoubleAndTrunc(&mSeconds, mSecondCounter, maxS);

    mLastUpdateTime = tNow;

    mUpdateValPlot = true;
}

void PageRtData::rotorPosReceived(double pos)
{
    appendDoubleAndTrunc(&mPositionVec, pos, 1500);
    mUpdatePosPlot = true;
}

void PageRtData::appendDoubleAndTrunc(QVector<double> *vec, double num, int maxSize)
{
    vec->append(num);

    if(vec->size() > maxSize) {
        vec->remove(0, vec->size() - maxSize);
    }
}

void PageRtData::updateZoom()
{
    Qt::Orientations plotOrientations = Qt::Orientations(
            ((ui->zoomHButton->isChecked() ? Qt::Horizontal : 0) |
             (ui->zoomVButton->isChecked() ? Qt::Vertical : 0)));

    ui->currentPlot->axisRect()->setRangeZoom(plotOrientations);
    ui->tempPlot->axisRect()->setRangeZoom(plotOrientations);
    ui->rpmPlot->axisRect()->setRangeZoom(plotOrientations);
    ui->focPlot->axisRect()->setRangeZoom(plotOrientations);
    ui->posPlot->axisRect()->setRangeZoom(plotOrientations);
}

void PageRtData::on_zoomHButton_toggled(bool checked)
{
    (void)checked;
    updateZoom();
}

void PageRtData::on_zoomVButton_toggled(bool checked)
{
    (void)checked;
    updateZoom();
}

void PageRtData::on_rescaleButton_clicked()
{
    ui->currentPlot->rescaleAxes(true);
    ui->tempPlot->rescaleAxes(true);
    ui->rpmPlot->rescaleAxes(true);
    ui->focPlot->rescaleAxes(true);
    ui->posPlot->rescaleAxes();

    ui->currentPlot->replotWhenVisible();
    ui->tempPlot->replotWhenVisible();
    ui->rpmPlot->replotWhenVisible();
    ui->focPlot->replotWhenVisible();
    ui->posPlot->replotWhenVisible();
}

void PageRtData::on_posInductanceButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_INDUCTANCE);
    }
}

void PageRtData::on_posObserverButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_OBSERVER);
    }
}

void PageRtData::on_posEncoderButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_ENCODER);
    }
}

void PageRtData::on_posPidButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_PID_POS);
    }
}

void PageRtData::on_posPidErrorButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_PID_POS_ERROR);
    }
}

void PageRtData::on_posEncoderObserverErrorButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_ENCODER_OBSERVER_ERROR);
    }
}

void PageRtData::on_posStopButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_NONE);
    }
}

void PageRtData::on_logRtButton_toggled(bool checked)
{
    QSettings set;
    set.sync();
    if (checked) {
        if (mVesc) {
            mVesc->openRtLogFile(set.value("path_rt_log", "./log").toString());
        }
    } else {
        mVesc->closeRtLogFile();
    }
}

void PageRtData::on_posHallObserverErrorButton_clicked()
{
    if (mVesc) {
        mVesc->commands()->setDetect(DISP_POS_MODE_HALL_OBSERVER_ERROR);
    }
}


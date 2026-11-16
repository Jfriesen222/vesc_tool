/*
    Copyright 2020 Benjamin Vedder	benjamin@vedder.se

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

import QtQuick 2.7
import QtQuick.Controls 2.10
import QtQuick.Layouts 1.3

import Vedder.vesc.vescinterface 1.0
import Vedder.vesc.commands 1.0
import Vedder.vesc.utility 1.0

Item {
    property Commands mCommands: VescIf.commands()
    property var mVal
    property bool mValSet: false
    property bool isHorizontal: width > height

    function openActions(item) {
        menu.popup(item, 0, item.height)
    }

    GridLayout {
        anchors.fill: parent
        columns: isHorizontal ? 2 : 1
        rows: isHorizontal ? 1 : 2
        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.column: isHorizontal ? 1 : 0
            Layout.row: 0
            Layout.preferredWidth: isHorizontal ? parent.width/2 : parent.width
            spacing: 0

            TabBar {
                id: tabBar
                Layout.fillWidth: true

                background: Rectangle {
                    color: Utility.getAppHexColor("lightBackground")
                }

                TabButton {
                    text: "Cells"
                }
                TabButton {
                    text: "Temps"
                }
            }

            StackLayout {
                currentIndex: tabBar.currentIndex

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 10

                Page {
                    Canvas {
                        id: cellCanvas
                        anchors.fill: parent

                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.reset()

                            var xOfsLeft = valMetrics.width * 4.5
                            var xOfsRight = valMetrics.width * 8
                            var heightBars = (height - valMetrics.height * 1.2)
                            var cellWidth = width - xOfsLeft - xOfsRight

                            ctx.font = '%1pt \"%2\"'.arg(valMetrics.font.pointSize).arg(valMetrics.font.family)

                            var lineDivs = 6
                            var lineStep = cellWidth / lineDivs

                            ctx.beginPath()

                            for (var i = 0; i <= lineDivs;i++) {
                                ctx.moveTo(xOfsLeft + lineStep * i, 0)
                                ctx.lineTo(xOfsLeft + lineStep * i, heightBars)
                            }

                            ctx.strokeStyle = Utility.getAppHexColor("lightAccent")
                            ctx.lineWidth = 1.2
                            ctx.stroke()

                            ctx.fillStyle = Utility.getAppHexColor("lightAccent")
                            ctx.textBaseline = "top"

                            ctx.textAlign = "start";
                            ctx.fillText("V", valMetrics.width / 2, heightBars)
                            ctx.textAlign = "center"

                            for (i = 0; i <= lineDivs;i++) {
                                ctx.fillText(parseFloat(3 + i * (1.2 / lineDivs)).toFixed(1), xOfsLeft + lineStep * i, heightBars)
                            }

                            if (!mValSet) {
                                return
                            }

                            var vCells = mVal.v_cells
                            var isBal = mVal.is_balancing

                            var cellNum = vCells.length
                            var cellHeight = (heightBars / cellNum) * 0.6
                            var cellDist = (heightBars / cellNum) * 0.4

                            ctx.textAlign = "start"
                            ctx.textBaseline = "middle"

                            for (i = 0;i < cellNum;i++) {
                                ctx.fillStyle = Utility.getAppHexColor("lightAccent")

                                var cellW = ((vCells[i] - 3.0) / 1.2) * cellWidth
                                if (cellW > cellWidth) {
                                    cellW = cellWidth
                                }
                                if (cellW < 1.2) {
                                    cellW = 1.2
                                }

                                var txtY = i * (cellHeight + cellDist) + cellDist / 2 + cellHeight / 2

                                ctx.fillText("C" + parseFloat(i + 1).toFixed(0),
                                             valMetrics.width / 2, txtY)
                                ctx.fillText(parseFloat(vCells[i]).toFixed(3) + " V",
                                             xOfsLeft + cellWidth + valMetrics.width / 2, txtY)

                                if (isBal[i]) {
                                    ctx.fillStyle = Utility.getAppHexColor("orange")
                                } else {
                                    ctx.fillStyle = Qt.rgba(0, 0.9, 0, 1)
                                }

                                ctx.fillRect(xOfsLeft, i * (cellHeight + cellDist) + cellDist / 2,
                                             cellW, cellHeight)
                            }
                        }
                    }
                }

                Page {
                    Canvas {
                        id: tempCanvas
                        anchors.fill: parent

                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.reset()

                            if (!mValSet) {
                                return
                            }

                            var temps =  mVal.temps
                            var tempNum = temps.length

                            var xOfsLeft = valMetrics.width * 8.5
                            var xOfsRight = valMetrics.width * 8

                            var cellHeight = (height / tempNum) * 0.7
                            var cellDist = (height / tempNum) * 0.3
                            var cellWidth = width - xOfsLeft - xOfsRight

                            var tInd = 1
                            var tLabels = []

                            if (mVal.data_version === 1) {
                                tLabels = ["IC", "Cell Min", "Cell Max", "Mosfet", "Ambient"]
                            }

                            for (var i = 0;i < tempNum;i++) {
                                ctx.fillStyle = Utility.getAppHexColor("lightAccent")
                                ctx.textAlign = "start";
                                ctx.textBaseline = "middle";
                                ctx.font = '%1pt \"%2\"'.arg(valMetrics.font.pointSize).arg(valMetrics.font.family)

                                var cellW = ((temps[i] + 10) / 70) * cellWidth
                                if (cellW > cellWidth) {
                                    cellW = cellWidth
                                }
                                if (cellW < 0) {
                                    cellW = 0
                                }

                                var txtY = i * (cellHeight + cellDist) + cellDist / 2 + cellHeight / 2

                                if (i < tLabels.length) {
                                    ctx.fillText(tLabels[i], valMetrics.width / 2, txtY)
                                } else {
                                    ctx.fillText("T" + parseFloat(tInd++).toFixed(0),
                                                 valMetrics.width / 2, txtY)
                                }

                                var txtTemp = parseFloat(temps[i]).toFixed(2) + " °C"
                                if (temps[i] <= -280.0) {
                                    txtTemp = " N/A"
                                }

                                ctx.fillText(txtTemp, xOfsLeft + cellW + valMetrics.width / 2, txtY)

                                ctx.fillStyle = Qt.rgba(0, 0, 1, 1)
                                ctx.fillRect(xOfsLeft, i * (cellHeight + cellDist) + cellDist / 2,
                                             cellW, cellHeight)
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: isHorizontal ? parent.width/2 : parent.width
            Layout.column:0
            Layout.row: isHorizontal ? 0 : 1
            Layout.alignment: Qt.AlignTop
            Layout.margins: 10

            Label {
                text: "No Data"
                color: Utility.getAppHexColor("lightText")
                visible: !mValSet
            }

            ReadoutGrid {
                id: packGrid
                title: "Pack"
                Layout.fillWidth: true
                visible: mValSet
                labels: ["Voltage", "Current", "Power", "SoC", "Cell min / max", "Diff", "Ah count", "Wh count",
                        "Ah chg total", "Ah dis total", "Charge voltage", "Pressure", "Humidity", "Status"]
            }
        }
    }

    Menu {
        id: menu

        MenuItem {
            text: "Balance On"
            onTriggered: {
                mCommands.bmsForceBalance(true)
            }
        }
        MenuItem {
            text: "Balance Off"
            onTriggered: {
                mCommands.bmsForceBalance(false)
            }
        }
        MenuItem {
            text: "Reset Ah Counter"
            onTriggered: {
                mCommands.bmsResetCounters(true, false)
            }
        }
        MenuItem {
            text: "Reset Wh Counter"
            onTriggered: {
                mCommands.bmsResetCounters(false, true)
            }
        }
        MenuItem {
            text: "Allow Charging"
            onTriggered: {
                mCommands.bmsSetChargeAllowed(true)
            }
        }
        MenuItem {
            text: "Disable Charging"
            onTriggered: {
                mCommands.bmsSetChargeAllowed(false)
            }
        }
        MenuItem {
            text: "Zero Current Offset"
            onTriggered: {
                mCommands.bmsZeroCurrentOffset()
            }
        }
    }

    TextMetrics {
        id: valMetrics
        font.family: "DejaVu Sans Mono"
        text: "A"
    }

    Connections {
        target: mCommands

        function onBmsValuesRx(val) {
            mVal = val
            mValSet = true

            var cvMin = Math.min(...mVal.v_cells)
            var cvMax = Math.max(...mVal.v_cells)

            packGrid.values = [
                    parseFloat(val.v_tot).toFixed(2) + " V",
                    parseFloat(val.i_in_ic).toFixed(2) + " A",
                    parseFloat(val.v_tot * val.i_in_ic).toFixed(2) + " W",
                    parseFloat(val.soc * 100).toFixed(1) + " %",
                    parseFloat(cvMin).toFixed(3) + " / " + parseFloat(cvMax).toFixed(3),
                    parseFloat(cvMax - cvMin).toFixed(3) + " V",
                    parseFloat(val.ah_cnt).toFixed(3) + " Ah",
                    parseFloat(val.wh_cnt).toFixed(2) + " Wh",
                    parseFloat(val.ah_cnt_chg_total).toFixed(3) + " Ah",
                    parseFloat(val.ah_cnt_dis_total).toFixed(3) + " Ah",
                    parseFloat(val.v_charge).toFixed(2) + " V",
                    parseFloat(val.pressure).toFixed(0) + " Pa",
                    parseFloat(val.humidity).toFixed(1) + " % (" + parseFloat(val.temp_hum_sensor).toFixed(1) + " \u00B0C)",
                    val.status]

            cellCanvas.requestPaint()
            tempCanvas.requestPaint()
        }
    }
}

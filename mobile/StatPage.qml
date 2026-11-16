/*
    Copyright 2021 Benjamin Vedder	benjamin@vedder.se

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
    property bool isHorizontal: width > height

    Component.onCompleted: {
        mCommands.emitEmptyStats()
        mCommands.emitEmptySetupValues()
    }

    GridLayout {
        anchors.fill: parent
        columns: isHorizontal ? 2 : 1

        RowLayout {
            id: heroRow
            property string efficiencyUnit
            property var values: []
            Layout.fillWidth: true
            Layout.columnSpan: isHorizontal ? 2 : 1

            Repeater {
                model: ["Distance", "Time", heroRow.efficiencyUnit]

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: heroColumn.implicitHeight + 16
                    radius: 10
                    color: Utility.getAppHexColor("normalBackground")
                    border.color: Utility.getAppHexColor("lightBackground")

                    ColumnLayout {
                        id: heroColumn
                        anchors.centerIn: parent
                        spacing: 0

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: heroRow.values[index] || ""
                            color: Utility.getAppHexColor("white")
                            font.pointSize: 16
                            font.bold: true
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData
                            color: Utility.getAppHexColor("normalText")
                            font.pointSize: 10
                        }
                    }
                }
            }
        }

        ReadoutGrid {
            id: tripGrid
            title: "This trip"
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            labels: ["Avg speed", "Max speed", "Avg power", "Max power", "Energy", "Charge",
                    "Motor temp", "FET temp", "Avg current", "Max current"]
        }

        ReadoutGrid {
            id: statGrid
            title: "Since power-up"
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            labels: ["Distance", "Abs. distance", "Wh drawn", "Wh charged", "Ah drawn", "Ah charged"]
        }

        Item {
            Layout.fillHeight: true
            Layout.columnSpan: isHorizontal ? 2 : 1
        }
    }

    Connections {
        target: mCommands

        function onValuesSetupReceived(values, mask) {
            statGrid.values = [
                    parseFloat(values.tachometer).toFixed(2) + " m",
                    parseFloat(values.tachometer_abs).toFixed(2) + " m",
                    parseFloat(values.watt_hours).toFixed(3) + " Wh",
                    parseFloat(values.watt_hours_charged).toFixed(3) + " Wh",
                    parseFloat(values.amp_hours * 1000).toFixed(1) + " mAh",
                    parseFloat(values.amp_hours_charged * 1000).toFixed(1) + " mAh"]
        }

        function onStatsRx(val, mask) {
            var useImperial = VescIf.useImperialUnits()
            var impFact = useImperial ? 0.621371192 : 1.0
            var speedUnit = useImperial ? "mph" : "km/h"

            heroRow.efficiencyUnit = useImperial ? "Wh/mi" : "Wh/km"
            heroRow.values = [
                    parseFloat(val.distance() / 1000 * impFact).toFixed(1) + (useImperial ? " mi" : " km"),
                    new Date(val.count_time * 1000).toISOString().substr(11, 8),
                    parseFloat(val.efficiency() / impFact || 0).toFixed(1)]

            tripGrid.values = [
                    parseFloat(val.speed_avg * 3.6 * impFact).toFixed(1) + " " + speedUnit,
                    parseFloat(val.speed_max * 3.6 * impFact).toFixed(1) + " " + speedUnit,
                    parseFloat(val.power_avg).toFixed(1) + " W",
                    parseFloat(val.power_max).toFixed(1) + " W",
                    parseFloat(val.energy()).toFixed(3) + " Wh",
                    parseFloat(val.ah()).toFixed(3) + " Ah",
                    parseFloat(val.temp_motor_avg).toFixed(1) + " / " + parseFloat(val.temp_motor_max).toFixed(1) + " \u00B0C",
                    parseFloat(val.temp_mos_avg).toFixed(1) + " / " + parseFloat(val.temp_mos_max).toFixed(1) + " \u00B0C",
                    parseFloat(val.current_avg).toFixed(1) + " A",
                    parseFloat(val.current_max).toFixed(1) + " A"]
        }
    }
}

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

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Vedder.vesc.utility 1.0

ColumnLayout {
    property alias title: titleLabel.text
    property var labels: []
    property var values: []
    property alias columns: grid.columns

    spacing: 2

    Label {
        id: titleLabel
        color: Utility.getAppHexColor("normalText")
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1
        font.pointSize: 10
    }

    GridLayout {
        id: grid
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 12
        rowSpacing: 0

        Repeater {
            model: labels

            Item {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                implicitHeight: row.implicitHeight + 10

                RowLayout {
                    id: row
                    anchors.fill: parent

                    Label {
                        text: modelData
                        color: Utility.getAppHexColor("normalText")
                        font.pointSize: 11
                    }

                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: values[index] || ""
                        color: Utility.getAppHexColor("white")
                        font.bold: true
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Utility.getAppHexColor("lightBackground")
                }
            }
        }
    }
}

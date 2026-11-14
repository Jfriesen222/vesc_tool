/*
    This file is part of VESC Tool.

    VESC Tool is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    VESC Tool is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import QtQuick.Dialogs
import QtCore
import Vedder.vesc.vescinterface 1.0
import Vedder.vesc.utility 1.0
import Vedder.vesc.ridelog 1.0

Item {
    id: root
    property var selectedFields: []
    property var rightFields: []
    property var cursorSample: ({values: [], hasPosition: false})
    property real viewStart: 0
    property real viewEnd: 1
    property real cursorTime: 0
    property bool inspectMode: true
    property bool remoteFiles: false
    property bool useImperial: VescIf.useImperialUnits()
    property bool showMapCredit: true
    property bool expandedPlot: false
    property string localDirectory: StandardPaths.writableLocation(StandardPaths.DocumentsLocation) + "/logs"
    property string remoteDirectory: "log_can"
    property string distanceText: "—"
    readonly property bool loaded: ride.sampleCount > 0
    readonly property color panelColor: Utility.getAppHexColor("lightBackground")
    readonly property color textColor: Utility.getAppHexColor("lightText")
    readonly property color mutedColor: Utility.getAppHexColor("normalText")
    readonly property color accentColor: Utility.getAppHexColor("lightAccent")
    readonly property var colors: [accentColor, "#ffb74d", "#a5d6a7", "#ce93d8"]

    function formatTime(seconds) {
        if (ride.timeLabel === "Sample") return Math.round(seconds).toString()
        var value = Math.max(0, Math.floor(seconds))
        var hours = Math.floor(value / 3600)
        var minutes = Math.floor(value / 60) % 60
        var secs = value % 60
        return (hours > 0 ? hours + ":" : "") +
                (hours > 0 && minutes < 10 ? "0" : "") + minutes + ":" +
                (secs < 10 ? "0" : "") + secs
    }

    function fieldValue(field) {
        var value = cursorSample.values ? cursorSample.values[field.index] : NaN
        if (!isFinite(value)) return "—"
        value *= fieldScale(field)
        var unit = fieldUnit(field)
        return value.toFixed(Math.abs(value) >= 1000 ? 0 : 1) + (unit ? " " + unit : "")
    }

    function fieldScale(field) {
        if (field.unit === "m/s") return useImperial ? 2.236936292 : 3.6
        if (field.unit === "km/h" && useImperial) return 0.621371192
        if (field.unit === "mph" && !useImperial) return 1.609344
        return 1
    }

    function fieldUnit(field) {
        if (field.unit === "m/s" || field.unit === "km/h" || field.unit === "mph") {
            return useImperial ? "mph" : "km/h"
        }
        if (field.unit === "degC") return "°C"
        if (field.unit === "deg") return "°"
        return field.unit
    }

    function toggleAxis(field) {
        var next = rightFields.slice()
        var index = next.indexOf(field.index)
        if (index >= 0) next.splice(index, 1)
        else next.push(field.index)
        rightFields = next
        plotTimer.restart()
    }

    function setCursor(time) {
        cursorTime = Math.max(viewStart, Math.min(viewEnd, time))
        cursorSample = ride.sample(cursorTime)
        if (cursorSample.hasPosition) {
            routeMap.setCursor(cursorSample.latitude, cursorSample.longitude)
        } else {
            routeMap.clearCursor()
        }
    }

    function setRange(start, end) {
        var total = Math.max(ride.duration, 0.001)
        var span = Math.max(Math.min(total, end - start), Math.min(total, total / 10000))
        viewStart = Math.max(0, Math.min(total - span, start))
        viewEnd = viewStart + span
        plotTimer.restart()
    }

    function zoomRange(factor, anchor) {
        var span = viewEnd - viewStart
        var ratio = Math.max(0, Math.min(1, (anchor - viewStart) / span))
        var nextSpan = Math.max(ride.duration / 10000, Math.min(ride.duration, span / factor))
        setRange(anchor - nextSpan * ratio, anchor + nextSpan * (1 - ratio))
    }

    function canSelect(field) {
        return selectedFields.indexOf(field.index) >= 0 || selectedFields.length < 4
    }

    function toggleField(field) {
        var next = selectedFields.slice()
        var index = next.indexOf(field.index)
        if (index >= 0) next.splice(index, 1)
        else if (canSelect(field)) {
            next.push(field.index)
            if (/^(W|kW|°C|degC)$/.test(field.unit) && rightFields.indexOf(field.index) < 0) {
                rightFields = rightFields.concat([field.index])
            }
        }
        selectedFields = next
        plotTimer.restart()
    }

    function updatePlot() {
        if (ride.sampleCount <= 0) return
        var data = ride.series(selectedFields, viewStart, viewEnd, Math.max(100, Math.round(plot.plotRect.width)))
        for (var i = 0; i < data.length; i++) {
            data[i].color = colors[i]
            var field = ride.fields[data[i].field]
            data[i].scale = fieldScale(field)
            data[i].unit = fieldUnit(field)
            data[i].axis = rightFields.indexOf(field.index) >= 0 ? 1 : 0
        }
        plot.series = data
        setCursor(cursorTime)
    }

    function browse() {
        if (remoteFiles) ride.listRemote(remoteDirectory)
        else ride.listLocal(localDirectory)
    }

    function updateDistance() {
        distanceText = "—"
        if (ride.sampleCount <= 0) return
        var first = ride.sample(0)
        var last = ride.sample(ride.duration)
        for (var i = 0; i < ride.fields.length; i++) {
            if (/^(trip_vesc_abs|trip_gnss|tacho_abs_meters|tacho_meters|distance)$/.test(ride.fields[i].key)) {
                var distance = last.values[i] - first.values[i]
                if (isFinite(distance) && distance >= 0) {
                    distanceText = (distance / (useImperial ? 1609.344 : 1000)).toFixed(1) + (useImperial ? " mi" : " km")
                    break
                }
            }
        }
    }

    Connections {
        target: VescIf
        function onUseImperialUnitsChanged(useImperialUnits) {
            root.useImperial = useImperialUnits
            root.updateDistance()
            plotTimer.restart()
        }
    }

    RideLog {
        id: ride
        vesc: VescIf
        onLogChanged: {
            if (sampleCount <= 0) return
            var defaults = [[/^(speed|speed_meters_per_sec|kmh_vesc)$/i, /^(gnss_h_vel|kmh_gnss|gnss_gVel)$/i],
                [/^(current_motor|setup_curr_motor|current_motor_setup|motor_current|i_motor|v1_curr_motor)$/i, /^M\d+ Current$/i],
                [/^(temp_mos_max|temp_mos|t_mosfet)$/i, /^(temp_motor|t_motor)$/i, /^M\d+ Temp Motor$/i, /Temp Fet/i]]
            var selected = []
            for (var i = 0; i < defaults.length; i++) {
                var found = false
                for (var p = 0; p < defaults[i].length && !found; p++) {
                    for (var j = 0; j < fields.length; j++) {
                        if (defaults[i][p].test(fields[j].key)) {
                            selected.push(fields[j].index)
                            found = true
                            break
                        }
                    }
                }
            }
            if (!selected.length) {
                for (var k = 0; k < fields.length && selected.length < 2; k++) {
                    if (!/time|t_day|ms_today|gnss|latitude|longitude/i.test(fields[k].key)) selected.push(fields[k].index)
                }
            }
            root.selectedFields = selected
            root.rightFields = selected.filter(function(index) { return /^(W|kW|°C|degC)$/.test(fields[index].unit) })
            root.viewStart = 0
            root.viewEnd = Math.max(duration, 0.001)
            root.cursorTime = 0
            routeMap.setRoute(route)
            root.updateDistance()
            plotTimer.restart()
            fileDialog.close()
        }
        onFilesChanged: {
            if (root.remoteFiles) root.remoteDirectory = directory
            else root.localDirectory = directory
        }
    }

    Timer {
        id: plotTimer
        interval: 30
        onTriggered: root.updatePlot()
    }

    Rectangle {
        anchors.fill: parent
        color: Utility.getAppHexColor("normalBackground")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Label {
                    text: root.loaded ? ride.name : "Ride logs"
                    textFormat: Text.PlainText
                    font.pixelSize: 21
                    font.bold: true
                    color: root.textColor
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
                Label {
                    text: root.loaded ? "Route and telemetry" : "Explore a ride from device or vehicle storage"
                    color: root.mutedColor
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
            Button {
                text: "Open"
                enabled: !ride.busy
                onClicked: fileDialog.open()
            }
        }

        Rectangle {
            visible: ride.error.length > 0
            Layout.fillWidth: true
            implicitHeight: errorLabel.implicitHeight + 16
            radius: 4
            color: root.panelColor
            Label {
                id: errorLabel
                anchors.fill: parent
                anchors.margins: 8
                text: ride.error
                color: Utility.getAppHexColor("red")
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            visible: ride.busy
            Layout.fillWidth: true
            ProgressBar {
                Layout.fillWidth: true
                value: ride.progress
                indeterminate: visible && ride.progress < 0
            }
            Button { text: "Cancel"; onClicked: ride.cancel() }
        }

        Item {
            visible: !root.loaded
            Layout.fillWidth: true
            Layout.fillHeight: true
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 24, 360)
                spacing: 14
                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: "qrc" + Utility.getThemePath() + "icons/Line Chart-96.png"
                    sourceSize: Qt.size(64, 64)
                    opacity: 0.7
                }
                Label {
                    Layout.fillWidth: true
                    text: "Open a ride log"
                    font.pixelSize: 24
                    color: root.textColor
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    Layout.fillWidth: true
                    text: "Follow the route, compare signals and inspect any moment of your ride."
                    color: root.mutedColor
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Browse logs"
                    enabled: !ride.busy
                    onClicked: fileDialog.open()
                }
            }
        }

        ScrollView {
            id: rideScroll
            visible: root.loaded
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            contentHeight: rideColumn.implicitHeight
            clip: true

            ColumnLayout {
                id: rideColumn
                width: rideScroll.availableWidth
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    visible: !root.expandedPlot
                    spacing: 8
                    Repeater {
                        model: [{name: ride.timeLabel === "Sample" ? "RANGE" : "DURATION", value: root.formatTime(ride.duration)},
                            {name: "DISTANCE", value: root.distanceText},
                            {name: "SAMPLES", value: ride.sampleCount.toLocaleString(Qt.locale(), 'f', 0)}]
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            implicitHeight: 58
                            radius: 5
                            color: root.panelColor
                            Column {
                                anchors.fill: parent
                                anchors.margins: 9
                                spacing: 3
                                Label { text: modelData.name; color: root.mutedColor; font.pixelSize: 10; font.letterSpacing: 1 }
                                Label { text: modelData.value; color: root.textColor; font.pixelSize: 18; font.bold: true }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: !root.expandedPlot
                    implicitHeight: routeMap.hasRoute ? Math.max(150, Math.min(190, root.height * 0.23)) : 48
                    color: Utility.getAppHexColor("darkBackground")
                    radius: 5
                    clip: true

                    LogMap {
                        id: routeMap
                        anchors.fill: parent
                        routeColor: root.accentColor
                        backgroundColor: Utility.getAppHexColor("darkBackground")
                    }
                    PinchArea {
                        anchors.fill: parent
                        enabled: routeMap.hasRoute
                        property real previousScale: 1
                        onPinchStarted: previousScale = 1
                        onPinchUpdated: {
                            routeMap.zoom(pinch.scale / previousScale, pinch.center.x, pinch.center.y)
                            previousScale = pinch.scale
                        }
                        MouseArea {
                            anchors.fill: parent
                            preventStealing: true
                            property real lastX: 0
                            property real lastY: 0
                            onPressed: mouse => { lastX = mouse.x; lastY = mouse.y }
                            onPositionChanged: mouse => {
                                if (pressed) routeMap.pan(mouse.x - lastX, mouse.y - lastY)
                                lastX = mouse.x
                                lastY = mouse.y
                            }
                            onWheel: wheel => routeMap.zoom(Math.pow(1.0015, wheel.angleDelta.y), wheel.x, wheel.y)
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: !routeMap.hasRoute
                        text: "No GPS track in this log"
                        color: root.mutedColor
                    }
                    Row {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 6
                        spacing: 3
                        visible: routeMap.hasRoute
                        Button {
                            text: "Fit"
                            implicitWidth: 52
                            implicitHeight: 38
                            enabled: routeMap.hasRoute
                            background: Rectangle { radius: 4; color: root.panelColor }
                            onClicked: routeMap.fitRoute()
                        }
                        Button {
                            text: routeMap.mapsEnabled ? "Map on" : "Route only"
                            implicitHeight: 38
                            checkable: true
                            checked: routeMap.mapsEnabled
                            background: Rectangle { radius: 4; color: root.panelColor }
                            onClicked: routeMap.mapsEnabled = checked
                        }
                    }
                    ToolButton {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.margins: 6
                        visible: routeMap.hasRoute && routeMap.mapsEnabled
                        text: root.showMapCredit ? "© OpenStreetMap" : ""
                        icon.source: root.showMapCredit ? "" : "qrc" + Utility.getThemePath() + "icons/About-96.png"
                        icon.color: root.textColor
                        width: root.showMapCredit ? 130 : 38
                        height: 38
                        background: Rectangle { radius: 4; color: root.panelColor }
                        font.pixelSize: 12
                        Accessible.name: "Map information"
                        onClicked: mapInfo.open()
                    }
                    Timer {
                        interval: 5000
                        running: routeMap.visible && routeMap.hasRoute && routeMap.mapsEnabled && root.showMapCredit
                        onTriggered: root.showMapCredit = false
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: root.expandedPlot ? rideScroll.availableHeight : Math.max(440, root.height * 0.6)
                    radius: 5
                    color: root.panelColor
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 0
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: "Telemetry"
                                font.bold: true
                                color: root.textColor
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "Signals · " + root.selectedFields.length
                                flat: true
                                onClicked: signalDialog.open()
                            }
                            ToolButton {
                                text: root.expandedPlot ? "Overview" : "Expand"
                                onClicked: {
                                    root.expandedPlot = !root.expandedPlot
                                    rideScroll.contentItem.contentY = 0
                                }
                            }
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 4
                            columnSpacing: 12
                            Repeater {
                                model: root.selectedFields
                                Column {
                                    required property int index
                                    required property var modelData
                                    property var field: ride.fields[modelData]
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 1
                                    Label {
                                        width: parent.width
                                        text: field ? field.name : ""
                                        textFormat: Text.PlainText
                                        font.pixelSize: 11
                                        color: root.colors[index]
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        text: field ? root.fieldValue(field) + (root.rightFields.indexOf(modelData) >= 0 ? "  R" : "  L") : ""
                                        font.pixelSize: 14
                                        color: root.textColor
                                    }
                                }
                            }
                        }
                        LogPlot {
                            id: plot
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 80
                            xMin: root.viewStart
                            xMax: root.viewEnd
                            cursor: root.cursorTime
                            timeLabel: ride.timeLabel
                            backgroundColor: root.panelColor
                            textColor: root.mutedColor
                            gridColor: Utility.getAppHexColor("lightestBackground")
                            onWidthChanged: plotTimer.restart()
                            Label {
                                anchors.centerIn: parent
                                visible: root.selectedFields.length === 0
                                text: "Choose signals to plot"
                                color: root.mutedColor
                            }
                            PinchArea {
                                anchors.fill: parent
                                property real previousScale: 1
                                onPinchStarted: previousScale = 1
                                onPinchUpdated: {
                                    root.zoomRange(pinch.scale / previousScale, plot.timeAt(pinch.center.x))
                                    previousScale = pinch.scale
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    preventStealing: true
                                    property real lastX: 0
                                    onPressed: mouse => {
                                        lastX = mouse.x
                                        if (root.inspectMode) root.setCursor(plot.timeAt(mouse.x))
                                    }
                                    onPositionChanged: mouse => {
                                        if (!pressed) return
                                        if (root.inspectMode) root.setCursor(plot.timeAt(mouse.x))
                                        else {
                                            var shift = (lastX - mouse.x) / Math.max(1, plot.plotRect.width) * (root.viewEnd - root.viewStart)
                                            root.setRange(root.viewStart + shift, root.viewEnd + shift)
                                        }
                                        lastX = mouse.x
                                    }
                                    onDoubleClicked: root.setRange(0, ride.duration)
                                    onWheel: wheel => root.zoomRange(Math.pow(1.0015, wheel.angleDelta.y), plot.timeAt(wheel.x))
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            ToolButton {
                                text: root.inspectMode ? "Inspect" : "Pan"
                                onClicked: root.inspectMode = !root.inspectMode
                            }
                            Label {
                                text: root.formatTime(root.cursorTime)
                                font.family: "DejaVu Sans Mono"
                                color: root.textColor
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                            }
                            ToolButton { text: "−"; onClicked: root.zoomRange(0.5, (root.viewStart + root.viewEnd) / 2) }
                            ToolButton { text: "+"; onClicked: root.zoomRange(2, (root.viewStart + root.viewEnd) / 2) }
                            ToolButton { text: "All"; onClicked: root.setRange(0, ride.duration) }
                        }
                    }
                }

            }
        }
    }

    Dialog {
        id: mapInfo
        title: "Map information"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(380, root.width - 20)
        height: 220
        standardButtons: Dialog.Close
        contentItem: Label {
            text: "© <a href='https://www.openstreetmap.org/copyright'>OpenStreetMap contributors</a><br>Tiles: RRZE, Friedrich-Alexander-Universität Erlangen-Nürnberg"
            textFormat: Text.RichText
            wrapMode: Text.WordWrap
            color: root.textColor
            linkColor: root.accentColor
            onLinkActivated: link => Qt.openUrlExternally(link)
        }
    }

    Dialog {
        id: signalDialog
        title: "Signals"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(520, root.width - 20)
        height: Math.min(620, root.height - 20)
        standardButtons: Dialog.Close
        ColumnLayout {
            anchors.fill: parent
            GridLayout {
                Layout.fillWidth: true
                columns: root.width < 360 ? 1 : 2
                columnSpacing: 6
                rowSpacing: 0
                Repeater {
                    model: root.selectedFields
                    RowLayout {
                        required property int index
                        required property var modelData
                        property var field: ride.fields[modelData]
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        spacing: 0
                        Button {
                            Layout.fillWidth: true
                            implicitHeight: 38
                            onClicked: root.toggleField(field)
                            contentItem: RowLayout {
                                Label {
                                    Layout.fillWidth: true
                                    text: field ? field.name : ""
                                    textFormat: Text.PlainText
                                    color: root.colors[index]
                                    elide: Text.ElideRight
                                }
                                Label { text: "×"; color: root.colors[index] }
                            }
                        }
                        ToolButton {
                            text: root.rightFields.indexOf(modelData) >= 0 ? "R" : "L"
                            implicitWidth: 36
                            implicitHeight: 38
                            Accessible.name: text === "R" ? "Right axis" : "Left axis"
                            onClicked: root.toggleAxis(field)
                        }
                    }
                }
            }
            TextField {
                id: fieldSearch
                Layout.fillWidth: true
                placeholderText: "Find a signal"
                selectByMouse: true
            }
            Label {
                Layout.fillWidth: true
                text: root.selectedFields.length + " of 4 selected. Tap to add or remove. L / R chooses the axis."
                color: root.mutedColor
                wrapMode: Text.WordWrap
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: ride.fields.filter(function(field) {
                    return (field.name + " " + field.key + " " + field.unit).toLowerCase().indexOf(fieldSearch.text.toLowerCase()) >= 0
                })
                delegate: ItemDelegate {
                    required property var modelData
                    width: ListView.view.width
                    enabled: root.canSelect(modelData)
                    onClicked: root.toggleField(modelData)
                    contentItem: RowLayout {
                        CheckBox {
                            checked: root.selectedFields.indexOf(modelData.index) >= 0
                            enabled: false
                            opacity: 1
                        }
                        Label { text: modelData.name; textFormat: Text.PlainText; color: root.textColor; Layout.fillWidth: true; elide: Text.ElideRight }
                        Label { text: root.fieldUnit(modelData); textFormat: Text.PlainText; color: root.mutedColor }
                    }
                }
                ScrollBar.vertical: ScrollBar {}
            }
        }
    }

    Dialog {
        id: fileDialog
        title: "Open ride log"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(600, root.width - 20)
        height: Math.min(650, root.height - 20)
        standardButtons: Dialog.Close
        onOpened: root.browse()
        ColumnLayout {
            anchors.fill: parent
            TabBar {
                Layout.fillWidth: true
                currentIndex: root.remoteFiles ? 1 : 0
                TabButton { text: "On device"; onClicked: { root.remoteFiles = false; root.browse() } }
                TabButton { text: "On vehicle"; onClicked: { root.remoteFiles = true; root.browse() } }
                enabled: !ride.busy
            }
            RowLayout {
                Layout.fillWidth: true
                ToolButton {
                    text: "Up"
                    enabled: !ride.busy
                    onClicked: {
                        if (root.remoteFiles) {
                            var path = root.remoteDirectory.replace(/\/$/, "")
                            root.remoteDirectory = path.substring(0, path.lastIndexOf("/")) || "/"
                        } else root.localDirectory += "/.."
                        root.browse()
                    }
                }
                Label {
                    text: root.remoteFiles ? root.remoteDirectory : root.localDirectory
                    Layout.fillWidth: true
                    color: root.mutedColor
                    elide: Text.ElideMiddle
                }
                ToolButton { text: "Refresh"; enabled: !ride.busy; onClicked: root.browse() }
            }
            Label {
                visible: ride.error.length > 0
                Layout.fillWidth: true
                text: ride.error
                color: Utility.getAppHexColor("red")
                wrapMode: Text.WordWrap
            }
            ProgressBar {
                visible: ride.busy
                Layout.fillWidth: true
                value: ride.progress
                indeterminate: visible && ride.progress < 0
            }
            ListView {
                id: fileList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: ride.files
                enabled: !ride.busy
                delegate: ItemDelegate {
                    required property var modelData
                    width: ListView.view.width
                    text: (modelData.isDir ? "▸  " : "") + modelData.name
                    onClicked: {
                        if (modelData.isDir) {
                            if (root.remoteFiles) root.remoteDirectory = modelData.path
                            else root.localDirectory = modelData.path
                            root.browse()
                        } else if (root.remoteFiles) ride.openRemote(modelData.path)
                        else ride.openLocal(modelData.path)
                    }
                }
                Label {
                    anchors.centerIn: parent
                    width: parent.width - 20
                    visible: !ride.busy && fileList.count === 0 && ride.error.length === 0
                    text: root.remoteFiles ? "No logs in this folder. Select the vehicle device that holds your logs." : "No logs in this folder. Use Browse files to open another location."
                    color: root.mutedColor
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
                ScrollBar.vertical: ScrollBar {}
            }
            RowLayout {
                Layout.fillWidth: true
                Button { text: "Browse files"; visible: !root.remoteFiles; enabled: !ride.busy; onClicked: localPicker.open() }
                Item { Layout.fillWidth: true }
                Button { text: "Cancel transfer"; visible: ride.busy; onClicked: ride.cancel() }
            }
        }
    }

    FileDialog {
        id: localPicker
        title: "Open ride log"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Ride logs (*.csv *.log)", "All files (*)"]
        onAccepted: ride.openLocal(selectedFile.toString())
    }
}

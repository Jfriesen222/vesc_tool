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
import QtCore as Core

import Vedder.vesc.utility 1.0

import "qrc:/mobile"

// Start tab of the welcome page: the device finder or the connected device, the setup wizards and the tools
ScrollView {
    id: start
    // A narrow page stacks what is side by side: the finder under the title, the cards in fewer columns
    readonly property bool narrow: availableWidth < 760

    // WelcomeQmlPanel, which runs the wizards and tools
    property var panel
    property bool connected: VescIf.isPortConnected()
    property string connName: ""
    property string connLink: ""
    property string linkName: ""
    property string hwName: ""
    property string fwVersion: ""
    property string canId: ""
    property string uuid: ""
    property real vin: 0
    property real battery: -1
    property real tempMos: 0
    property real tempMotor: 0
    property string fault: ""
    property var focFacts: []
    property var inputFacts: []
    property var imuFacts: []

    contentWidth: availableWidth

    // Prefers the user's BLE name, then the hardware name, over the port address
    function updateConnectionInfo() {
        var m = VescIf.getConnectedPortName().match(/^Connected (?:\((.*)\) )?to (.*?)(, limited mode)?$/)
        var bleName = m && m[1] === "BLE" ? VescIf.getBleName(VescIf.getLastBleAddr()) : ""
        connLink = m && m[1] ? m[1] : ""
        connName = bleName !== "" ? bleName : m && m[1] && hwName !== "" ? hwName.replace(/_/g, " ") : m ? m[2] : VescIf.getConnectedPortName()
    }

    // Plain facts from the configuration, not whether setup was done
    function readFacts() {
        var mc = VescIf.mcConfig()
        var app = VescIf.appConfig()
        var appName = app.getParamValueString("app_to_use")
        // The NRF app reads its remote through the nunchuk configuration
        var conf = appName.indexOf("PPM") >= 0 ? "app_ppm_conf." :
                   appName.indexOf("ADC") >= 0 ? "app_adc_conf." :
                   appName.indexOf("Nunchuk") >= 0 || appName === "NRF" ? "app_chuk_conf." : ""
        // IMU_TYPE_EXTERNAL_BMI160 reads "External · BMI160", AHRS_MODE_MADGWICK_FUSION "Madgwick Fusion"
        var word = function(w) { return w.charAt(0) + w.slice(1).toLowerCase() }
        var imuType = app.getParamValueString("imu_conf.type").replace(/^IMU_TYPE_/, "").split("_")
        focFacts = [[qsTr("Motor type"), mc.getParamValueString("motor_type")],
                    [qsTr("Sensor mode"), mc.getParamValueString(mc.getParamEnum("motor_type") === 2 ? "foc_sensor_mode" : "sensor_mode")],
                    [qsTr("Current limits"), qsTr("Motor %1 A · Battery %2 A").arg(Math.round(mc.getParamDouble("l_current_max")))
                                                                             .arg(Math.round(mc.getParamDouble("l_in_current_max")))]]
        inputFacts = [[qsTr("App to use"), appName]].concat(
                    conf !== "" ? [[qsTr("Control mode"), app.getParamValueString(conf + "ctrl_type")],
                                   [qsTr("Throttle curve"), app.getParamValueString(conf + "throttle_exp_mode") + " · " +
                                                            app.getParamValueString(conf + "throttle_exp")]] :
                    appName.indexOf("UART") >= 0 ? [[qsTr("Baud rate"), app.getParamValueString("app_uart_baudrate")]] : [])
        imuFacts = [[qsTr("IMU type"), [word(imuType[0])].concat(imuType.slice(1)).join(" · ")],
                    [qsTr("Sample rate"), app.getParamValueString("imu_conf.sample_rate_hz")],
                    [qsTr("Filter"), app.getParamValueString("imu_conf.mode").replace(/^AHRS_MODE_/, "").split("_").map(word).join(" ")]]
        canId = app.getParamValueString("controller_id")
    }

    Core.Settings {
        id: lastDevice
        category: "lastDevice"
        property string name: ""
        property real time: 0
        property string icon: "icons/Connected-96.png"
        property string addr: ""
    }

    Connections {
        target: VescIf

        function onPortConnectedChanged() {
            connected = VescIf.isPortConnected()
            if (!connected) {
                vin = 0
                battery = -1
                fault = ""
            }
        }

        function onFwRxChanged(rx, limited) {
            if (!rx) {
                return
            }

            var p = VescIf.getLastFwRxParams()
            hwName = p.hw
            fwVersion = p.major + "." + ("0" + p.minor).slice(-2)
            uuid = Utility.uuid2Str(p.uuid, true)
            updateConnectionInfo()
            lastDevice.name = connName
            lastDevice.time = Date.now()
            // connectTcp() clears the hub ID, so it tells a hub link from plain TCP
            var tcp = connLink === "TCP" && VescIf.getLastTcpHubVescID() === ""
            linkName = connLink === "BLE" ? qsTr("Bluetooth") : connLink === "serial" ? qsTr("USB") :
                       connLink === "TCP" && !tcp ? qsTr("TCP hub") : connLink
            lastDevice.icon = connLink === "BLE" ? "icons/bluetooth.png" :
                              connLink === "serial" ? "icons/USB-96.png" :
                              tcp ? "icons/LAN-96.png" :
                              connLink === "TCP" ? "icons/Globe-96.png" : "icons/Connected-96.png"
            lastDevice.addr = connLink === "BLE" ? VescIf.getLastBleAddr() : tcp ? VescIf.getLastTcpServer() : ""
        }
    }

    Connections {
        target: VescIf.mcConfig()
        function onUpdated() { readFacts() }
    }

    Connections {
        target: VescIf.appConfig()
        function onUpdated() { readFacts() }
    }

    Connections {
        target: VescIf.commands()

        function onValuesReceived(values, mask) {
            if (mask & (1 << 0)) tempMos = values.temp_mos
            if (mask & (1 << 1)) tempMotor = values.temp_motor
            if (mask & (1 << 8)) vin = values.v_in
            if (mask & (1 << 15)) fault = values.fault_code !== 0 ? values.fault_str : ""
        }

        function onValuesSetupReceived(values, mask) {
            if (mask & (1 << 0)) tempMos = values.temp_mos
            if (mask & (1 << 1)) tempMotor = values.temp_motor
            if (mask & (1 << 7)) vin = values.v_in
            if (mask & (1 << 8)) battery = values.battery_level
            if (mask & (1 << 16)) fault = values.fault_code !== 0 ? values.fault_str : ""
        }
    }

    // Light polling for the tiles, only while they are on screen
    Timer {
        interval: 1000
        repeat: true
        triggeredOnStart: true
        running: connected && start.SwipeView.isCurrentItem && start.Window.window !== null && start.Window.window.visible

        onTriggered: {
            if (VescIf.getLastFwRxParams().hwTypeStr() === "VESC" && VescIf.getFwUploadProgress() < 0) {
                VescIf.commands().getValuesSetupSelective(0x10183)
            }
        }
    }

    component Caption: Text {
        font.pixelSize: 10
        font.weight: Font.Medium
        font.letterSpacing: 1.1
        font.capitalization: Font.AllUppercase
        color: Utility.getAppHexColor("mutedText")
    }

    // Connection state over the hero title: a lamp, which glows when connected, and a caption
    component Eyebrow: RowLayout {
        property alias text: eyebrowText.text
        property bool live: false
        spacing: 8

        Rectangle {
            width: 7
            height: 7
            radius: 3.5
            color: Utility.getAppHexColor(live ? "positive" : "disabledText")

            Rectangle {
                anchors.centerIn: parent
                z: -1
                width: 15
                height: 15
                radius: 7.5
                visible: live
                color: parent.color
                opacity: 0.25
            }
        }

        Caption {
            id: eyebrowText
            font.pixelSize: 11
            font.letterSpacing: 1.5
            color: Utility.getAppHexColor(live ? "positiveText" : "mutedText")
        }
    }

    component HeroTitle: Text {
        Layout.fillWidth: true
        Layout.topMargin: 11
        Layout.bottomMargin: 8
        font.pixelSize: 34
        font.weight: Font.Bold
        font.letterSpacing: -0.5
        color: Utility.getAppHexColor("titleText")
    }

    component Section: RowLayout {
        property alias text: sectionLabel.text
        Layout.fillWidth: true
        Layout.topMargin: 16
        Layout.bottomMargin: 9
        spacing: 12

        Text {
            id: sectionLabel
            font.family: "Exan"
            font.pixelSize: 11
            font.letterSpacing: 2.1
            font.capitalization: Font.AllUppercase
            color: Utility.getAppHexColor("lightAccent")
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: Qt.alpha(Utility.getAppHexColor("lightAccent"), 0.4) }
                GradientStop { position: 1; color: Qt.alpha(Utility.getAppHexColor("lightAccent"), 0.08) }
            }
        }
    }

    component Btn: AbstractButton {
        id: btn
        property string iconPath: ""
        property bool primary: false
        property bool big: false

        implicitHeight: big ? 36 : 28
        implicitWidth: btnRow.implicitWidth + (big ? 34 : 24)
        hoverEnabled: true
        font.pixelSize: big ? 14 : 13
        font.weight: Font.Medium
        ToolTip.visible: hovered && ToolTip.text !== ""
        ToolTip.delay: 700

        contentItem: Item {
            Row {
                id: btnRow
                anchors.centerIn: parent
                spacing: btn.big ? 9 : 7

                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    width: btn.big ? 16 : 14
                    height: width
                    visible: btn.iconPath !== ""
                    source: btn.iconPath !== "" ? "qrc" + Utility.getThemePath() + btn.iconPath : ""
                    mipmap: true
                    opacity: btn.enabled ? 1 : 0.35
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: btn.text
                    font: btn.font
                    color: btn.primary ? "white" : Utility.getAppHexColor(btn.enabled ? "titleText" : "disabledText")
                }
            }
        }

        background: Rectangle {
            radius: btn.big ? 8 : 7
            color: btn.primary ? Utility.getAppHexColor(btn.hovered ? "accent" : "accentFill") :
                   !btn.enabled ? "transparent" : Utility.getAppHexColor(btn.big ? "wellBackground" : "raisedBackground")
            border.width: btn.primary ? 0 : 1
            border.color: Utility.getAppHexColor(btn.big && btn.enabled ? "lineStrong" : "line")

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: Utility.getAppHexColor("titleText")
                opacity: btn.primary || !btn.enabled ? 0 : btn.pressed ? 0.12 : btn.hovered ? 0.06 : 0
            }
        }
    }

    component ToolBtn: AbstractButton {
        id: tool
        property string iconPath: ""

        Layout.fillWidth: true
        implicitHeight: 40
        hoverEnabled: true
        font.pixelSize: 13
        font.weight: Font.Medium

        contentItem: Item {
            Row {
                x: 14
                width: parent.width - 28
                anchors.verticalCenter: parent.verticalCenter
                spacing: 11

                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 15
                    height: 15
                    source: "qrc" + Utility.getThemePath() + tool.iconPath
                    mipmap: true
                    opacity: tool.enabled ? 1 : 0.35
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 26
                    elide: Text.ElideRight
                    text: tool.text
                    font: tool.font
                    color: Utility.getAppHexColor(tool.enabled ? "titleText" : "disabledText")
                }
            }
        }

        background: Rectangle {
            radius: 9
            color: tool.enabled && tool.hovered ? Utility.getAppHexColor("raisedBackground") : "transparent"
            border.color: Utility.getAppHexColor(tool.enabled ? "lineStrong" : "line")
        }
    }

    component Tile: Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: 10
        color: Utility.getAppHexColor("cardBackground")
        border.color: Utility.getAppHexColor("line")
    }

    component IconBox: Rectangle {
        id: iconBox
        property string iconPath: ""
        width: 34
        height: 34
        radius: 9
        color: Utility.getAppHexColor("raisedBackground")

        Image {
            anchors.centerIn: parent
            width: 19
            height: 19
            source: "qrc" + Utility.getThemePath() + iconBox.iconPath
            mipmap: true
        }
    }

    component Value: Row {
        property alias value: valueText.text
        property alias unit: unitText.text
        property alias valueColor: valueText.color
        spacing: 4

        Text {
            id: valueText
            font.pixelSize: 22
            font.weight: Font.Medium
            color: Utility.getAppHexColor("titleText")
        }

        Text {
            id: unitText
            anchors.baseline: valueText.baseline
            font.pixelSize: 12
            color: Utility.getAppHexColor("mutedText")
        }
    }

    component AutoConnectBtn: Btn {
        text: qsTr("AutoConnect")
        iconPath: "desktop/icons/Connected-96.png"
        ToolTip.text: qsTr("Automatically connect to the VESC using the USB connection.")
        onClicked: QmlUi.autoConnect()
    }

    component WizardCard: Tile {
        id: card
        property string iconPath: ""
        property string title: ""
        property string subtitle: ""
        property string tip: ""
        property bool optional: false
        property bool ready: false
        property var facts: []
        // Only the name and the key, for a narrow page
        property bool compact: false
        signal run()

        implicitHeight: cardColumn.implicitHeight + 27

        ColumnLayout {
            id: cardColumn
            anchors.fill: parent
            anchors.topMargin: 13
            anchors.bottomMargin: 14
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: 11

                IconBox {
                    visible: !card.compact
                    iconPath: card.iconPath
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        Layout.fillWidth: true
                        text: card.title
                        elide: Text.ElideRight
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: Utility.getAppHexColor("titleText")
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: !card.compact
                        text: card.subtitle
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        color: Utility.getAppHexColor("mutedText")
                    }
                }

                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    visible: card.optional && !card.compact
                    implicitWidth: optionalText.implicitWidth + 16
                    implicitHeight: 20
                    radius: 10
                    color: Qt.alpha(Utility.getAppHexColor("titleText"), 0.06)

                    Text {
                        id: optionalText
                        anchors.centerIn: parent
                        text: qsTr("Optional")
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        color: Utility.getAppHexColor("lightText")
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 12
                height: 1
                visible: card.facts.length > 0 && !card.compact
                color: Utility.getAppHexColor("line")
            }

            Repeater {
                model: card.compact ? [] : card.facts

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 27
                        spacing: 0

                        Text {
                            Layout.preferredWidth: 104
                            text: modelData[0]
                            font.pixelSize: 12
                            color: Utility.getAppHexColor("mutedText")
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData[1]
                            elide: Text.ElideRight
                            font.pixelSize: 12
                            color: Utility.getAppHexColor("titleText")
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Utility.getAppHexColor("line")
                    }
                }
            }

            Item {
                Layout.fillHeight: true
                Layout.preferredHeight: 12
            }

            RowLayout {
                spacing: 12

                Btn {
                    Layout.fillWidth: card.compact
                    text: qsTr("Run wizard")
                    iconPath: "desktop/icons/Wizard-96.png"
                    enabled: card.ready
                    ToolTip.text: card.tip
                    onClicked: card.run()
                }

                Image {
                    visible: !card.ready && !card.compact
                    Layout.preferredWidth: 11
                    Layout.preferredHeight: 11
                    source: "qrc" + Utility.getThemePath() + "desktop/icons/Lock-96.png"
                    mipmap: true
                    opacity: 0.6
                }

                Text {
                    visible: !card.ready && !card.compact
                    text: qsTr("Connect first")
                    font.pixelSize: 12
                    color: Utility.getAppHexColor("mutedText")
                }
            }
        }
    }

    ColumnLayout {
        x: 18
        width: start.availableWidth - 36
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 16
            // Not connected, the finder gets the height that the setup and the tools leave free, for a long device list
            Layout.preferredHeight: connected ? (start.narrow ? heroInfo.implicitHeight + 150 : 214) : Math.max(start.narrow ? heroLeft.implicitHeight + 340 : 286, heroLeft.implicitHeight + 48, start.availableHeight -
                                    setupTitle.implicitHeight - setupRow.implicitHeight - toolsTitle.implicitHeight -
                                    toolsRow.implicitHeight - 84)
            radius: 12
            color: Utility.getAppHexColor("wellBackground")
            border.color: Utility.getAppHexColor("line")

            // Shade under the top edge, as the well is sunk into the page
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 1
                y: 1
                height: 10
                visible: Utility.isDarkMode()
                topLeftRadius: 11
                topRightRadius: 11
                gradient: Gradient {
                    GradientStop { position: 0; color: "#66000000" }
                    GradientStop { position: 1; color: "transparent" }
                }
            }

            // Three link waves behind the connected device, fading out before the tiles
            Canvas {
                anchors.fill: parent
                visible: connected && !start.narrow
                opacity: 0.42
                onWidthChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    ;["accent", "vescBlue", "titleText"].forEach(function(name, k) {
                        ctx.beginPath()
                        ctx.moveTo(0, height / 2 + Math.sin(k * 2.094) * 78)
                        for (var x = 4;x <= width + 3;x += 4) {
                            ctx.lineTo(x, height / 2 + Math.sin(x * 960 / width / 118 + k * 2.094) * 78)
                        }
                        ctx.strokeStyle = Utility.getAppHexColor(name)
                        ctx.globalAlpha = 0.1
                        ctx.lineWidth = 7
                        ctx.stroke()
                        ctx.globalAlpha = k === 2 ? 0.5 : 0.8
                        ctx.lineWidth = 1.6
                        ctx.stroke()
                    })
                    var mask = ctx.createLinearGradient(0, 0, width, 0)
                    mask.addColorStop(0, Qt.rgba(0, 0, 0, 0.18))
                    mask.addColorStop(0.3, Qt.rgba(0, 0, 0, 0.3))
                    mask.addColorStop(0.46, "black")
                    mask.addColorStop(0.58, "black")
                    mask.addColorStop(0.64, "transparent")
                    ctx.globalAlpha = 1
                    ctx.globalCompositeOperation = "destination-in"
                    ctx.fillStyle = mask
                    ctx.fillRect(0, 0, width, height)
                }
            }

            // Not connected: what to do on the left, the device finder on the right
            GridLayout {
                anchors.fill: parent
                visible: !connected
                columns: start.narrow ? 1 : 2
                rowSpacing: 0
                columnSpacing: 0

                ColumnLayout {
                    id: heroLeft
                    Layout.fillWidth: start.narrow
                    Layout.preferredWidth: 272
                    Layout.fillHeight: !start.narrow
                    Layout.leftMargin: 26
                    Layout.rightMargin: 6
                    Layout.topMargin: 26
                    Layout.bottomMargin: start.narrow ? 4 : 22
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.rightMargin: start.narrow ? 20 : 14
                        spacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Eyebrow {
                                text: qsTr("Not connected")
                            }

                            HeroTitle {
                                text: qsTr("Connect your VESC")
                                wrapMode: Text.WordWrap
                                lineHeight: 1.05
                            }
                        }

                        // A narrow page has the key here, so that the list of devices under it is longer
                        AutoConnectBtn {
                            visible: start.narrow
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: !start.narrow
                        text: qsTr("Devices in range appear as they are found, over Bluetooth, USB, Wi-Fi or a TCP hub.")
                        wrapMode: Text.WordWrap
                        font.pixelSize: 13
                        color: Utility.getAppHexColor("mutedText")
                    }

                    Caption {
                        Layout.topMargin: 20
                        Layout.bottomMargin: 7
                        visible: lastDevice.name !== "" && !start.narrow
                        text: qsTr("Last connected")
                    }

                    // On a narrow page it is the first row over the list of devices
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.topMargin: start.narrow ? 12 : 0
                        Layout.rightMargin: start.narrow ? 20 : 0
                        implicitHeight: start.narrow ? 44 : 54
                        visible: lastDevice.name !== ""
                        radius: 11
                        color: Utility.getAppHexColor("cardBackground")
                        border.color: Utility.getAppHexColor("line")

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 9
                            anchors.rightMargin: 10
                            spacing: 11

                            IconBox {
                                iconPath: "desktop/" + lastDevice.icon
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Text {
                                    Layout.fillWidth: true
                                    text: lastDevice.name
                                    elide: Text.ElideRight
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    color: Utility.getAppHexColor("titleText")
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: start.narrow ? qsTr("Last connected %1").arg(panel.timeAgo(lastDevice.time)) :
                                                         panel.timeAgo(lastDevice.time)
                                    elide: Text.ElideRight
                                    font.pixelSize: 11
                                    color: Utility.getAppHexColor("mutedText")
                                }
                            }

                            Btn {
                                text: qsTr("Reconnect")
                                primary: true
                                onClicked: VescIf.reconnectLastPort()
                            }
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                        Layout.minimumHeight: 16
                        visible: !start.narrow
                    }

                    AutoConnectBtn {
                        visible: !start.narrow
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 12
                    Layout.leftMargin: 14

                    ConnectScreen {
                        anchors.fill: parent
                        embedded: true
                        dialogParent: panel

                        Component.onCompleted: VescIf.bleDevice().emitScanDone()
                    }
                }
            }

            // Connected: the device and its live values
            GridLayout {
                anchors.fill: parent
                visible: connected
                columns: start.narrow ? 1 : 2
                rowSpacing: 0
                columnSpacing: 0

                ColumnLayout {
                    id: heroInfo
                    Layout.fillWidth: true
                    Layout.fillHeight: !start.narrow
                    Layout.leftMargin: 28
                    Layout.rightMargin: 16
                    Layout.topMargin: 26
                    spacing: 0

                    Eyebrow {
                        live: true
                        text: [qsTr("Connected"), linkName].filter(Boolean).join(" · ")
                    }

                    HeroTitle {
                        text: connName
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: [qsTr("Hardware %1").arg(hwName), qsTr("Firmware %1").arg(fwVersion),
                               canId !== "" ? qsTr("CAN ID %1").arg(canId) : "",
                               uuid !== "" ? qsTr("UUID %1").arg(uuid) : ""].filter(Boolean).join("  ·  ")
                        elide: Text.ElideRight
                        font.pixelSize: 13
                        color: Utility.getAppHexColor("lightText")
                    }

                    RowLayout {
                        Layout.topMargin: 22
                        spacing: 10

                        Btn {
                            big: true
                            text: qsTr("Disconnect")
                            iconPath: "desktop/icons/Disconnected-96.png"
                            onClicked: VescIf.disconnectPort()
                        }

                        Btn {
                            big: true
                            text: qsTr("Backup Configs")
                            iconPath: "desktop/icons/Save-96.png"
                            ToolTip.text: qsTr("Back up the configuration of this VESC and of the VESCs on CAN")
                            onClicked: QmlUi.triggerWindowAction("actionBackupConfigurationsCAN")
                        }

                        Btn {
                            big: true
                            text: qsTr("Restore Configs")
                            iconPath: "desktop/icons/Open Folder-96.png"
                            ToolTip.text: qsTr("Write the backups back to this VESC and to the VESCs on CAN")
                            onClicked: QmlUi.triggerWindowAction("actionRestoreConfigurationsCAN")
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }

                GridLayout {
                    Layout.fillWidth: start.narrow
                    Layout.preferredWidth: 404
                    Layout.fillHeight: true
                    Layout.margins: 12
                    columns: start.narrow ? 4 : 2
                    rowSpacing: 8
                    columnSpacing: 8
                    uniformCellHeights: true
                    uniformCellWidths: true

                    Tile {
                        Layout.columnSpan: 2

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 13
                            anchors.rightMargin: 13
                            spacing: 14

                            Caption {
                                Layout.preferredWidth: 64
                                text: qsTr("Battery")
                            }

                            Value {
                                value: vin > 0 ? vin.toFixed(1) : "–"
                                unit: "V"
                            }

                            Text {
                                visible: battery >= 0
                                text: Math.round(battery * 100) + " %"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                color: Utility.getAppHexColor("positiveText")
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 6
                                radius: 3
                                color: Utility.getAppHexColor("line")

                                Rectangle {
                                    width: parent.width * Math.min(Math.max(battery, 0), 1)
                                    height: parent.height
                                    radius: 3
                                    color: Utility.getAppHexColor("positive")
                                }
                            }
                        }
                    }

                    Repeater {
                        model: [[qsTr("MOSFET"), tempMos], [qsTr("Motor"), tempMotor]]

                        Tile {
                            Column {
                                anchors.left: parent.left
                                anchors.leftMargin: 13
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 7

                                Caption {
                                    text: modelData[0]
                                }

                                Value {
                                    value: vin > 0 ? modelData[1].toFixed(1) : "–"
                                    unit: "°C"
                                }
                            }
                        }
                    }

                    Tile {
                        Layout.columnSpan: start.narrow ? 4 : 2

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 13
                            anchors.rightMargin: 13
                            spacing: 14

                            Caption {
                                Layout.preferredWidth: 64
                                text: qsTr("Faults")
                            }

                            Value {
                                Layout.fillWidth: true
                                value: vin <= 0 ? "–" : fault !== "" ? fault : qsTr("None")
                                valueColor: Utility.getAppHexColor(vin <= 0 ? "titleText" : fault !== "" ? "dangerText" : "positiveText")
                            }

                            AbstractButton {
                                id: printFaults
                                hoverEnabled: true
                                onClicked: QmlUi.triggerWindowAction("actionTerminalPrintFaults")

                                contentItem: Row {
                                    spacing: 5

                                    Text {
                                        text: qsTr("Print faults")
                                        font.pixelSize: 12
                                        color: Utility.getAppHexColor(printFaults.hovered ? "titleText" : "mutedText")
                                    }

                                    Image {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 11
                                        height: 11
                                        source: "qrc" + Utility.getThemePath() + "desktop/icons/arrow_r.png"
                                        mipmap: true
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Section {
            id: setupTitle
            text: qsTr("Setup")
        }

        RowLayout {
            id: setupRow
            Layout.fillWidth: true
            spacing: 10
            uniformCellSizes: true

            WizardCard {
                iconPath: "desktop/icons/motor.png"
                title: qsTr("Setup Motors FOC")
                compact: start.narrow
                subtitle: qsTr("Motor detection and limits")
                tip: qsTr("Simple FOC setup for all VESCs on CAN-bus")
                ready: connected
                facts: connected ? focFacts : []
                onRun: panel.setupMotors()
            }

            WizardCard {
                iconPath: "desktop/icons/Controller-96.png"
                title: qsTr("Setup Input")
                compact: start.narrow
                subtitle: qsTr("PPM, ADC, UART or remote")
                ready: connected
                facts: connected ? inputFacts : []
                onRun: QmlUi.startSetupWizardApp()
            }

            WizardCard {
                iconPath: "desktop/icons/Gyroscope-96.png"
                title: qsTr("Setup IMU")
                compact: start.narrow
                subtitle: qsTr("Orientation and calibration")
                optional: true
                ready: connected
                facts: connected ? imuFacts : []
                onRun: panel.openWizardIMU()
            }
        }

        Section {
            id: toolsTitle
            text: qsTr("Tools")
        }

        GridLayout {
            id: toolsRow
            Layout.fillWidth: true
            Layout.bottomMargin: 16
            columns: start.narrow ? 2 : 4
            rowSpacing: 10
            columnSpacing: 10
            uniformCellWidths: true

            ToolBtn {
                text: qsTr("Multi Settings")
                iconPath: "desktop/icons/Settings-96.png"
                enabled: connected
                onClicked: panel.openMultiSettings()
            }

            ToolBtn {
                text: qsTr("Invert Motor Directions")
                iconPath: "desktop/icons/Process-96.png"
                enabled: connected
                onClicked: panel.dirSetup()
            }

            ToolBtn {
                text: qsTr("NRF Quick Pair")
                iconPath: "desktop/icons/icons8-fantasy-96.png"
                enabled: connected
                onClicked: panel.nrfQuickPair()
            }

            ToolBtn {
                text: qsTr("Setup Bluetooth Module")
                iconPath: "desktop/icons/bluetooth.png"
                enabled: connected
                onClicked: panel.openBleSetup()
            }
        }
    }
}

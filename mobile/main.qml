/*
    Copyright 2017 - 2019 Benjamin Vedder	benjamin@vedder.se

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
import QtQuick.Controls.Material 2.2
import QtQuick.Layouts 1.3
import QtQuick.Window

import Vedder.vesc.vescinterface 1.0
import Vedder.vesc.commands 1.0
import Vedder.vesc.configparams 1.0
import Vedder.vesc.utility 1.0
import Vedder.vesc.vesc3ditem 1.0

ApplicationWindow {
    id: appWindow
    property Commands mCommands: VescIf.commands()
    property ConfigParams mMcConf: VescIf.mcConfig()
    property ConfigParams mAppConf: VescIf.appConfig()
    property ConfigParams mInfoConf: VescIf.infoConfig()
    property bool connected: false
    property bool fwReadCorrectly: false
    property bool fwConfig: false
    property string customConfName: ""
    property bool imuFitted: false
    property bool bmsFound: false
    property var fwParams: VescIf.getLastFwRxParams()
    property string targetBus: "LOCAL"
    property var canDevs: []
    property var liveItem: tabBar.currentIndex == 2 ? livePage.currentItem : null

    visible: true
    width: 500
    height: 850
    title: qsTr("VESC Tool")

    // Full screen iPhone X workaround:
    property int notchLeft: 0
    property int notchRight: 0
    property int notchBot: 0
    property int notchTop: 0
    property bool mainIsHorizontal: appWindow.width > appWindow.height

    // https://github.com/ekke/c2gQtWS_x/blob/master/qml/main.qml
    flags: Qt.platform.os === "ios" ? (Qt.Window | Qt.MaximizeUsingFullscreenGeometryHint) : Qt.Window

    function updateNotch() {
        notchTop = SafeArea.margins.top
        notchLeft = SafeArea.margins.left
        notchRight = SafeArea.margins.right
        notchBot = SafeArea.margins.bottom
    }

    onClosing: function(close) {
        if (tabBar.visible && !connScreen.opened && mainSwipeView.currentItem.depth > 1) {
            mainSwipeView.currentItem.pop()
            close.accepted = false
        }
    }

    Timer {
        id: oriTimer
        interval: 100
        running: true
        repeat: true

        onTriggered: {
            updateNotch()
        }
    }

    Component.onCompleted: {
        tabBar.setCurrentIndex(2)
        if (!VescIf.isIntroDone()) {
            introWizard.openDialog()
        }
        updateNotch()
        Utility.keepScreenOn(VescIf.keepScreenOn())
        Utility.allowScreenRotation(VescIf.getAllowScreenRotation())
        Utility.stopGnssForegroundService()
    }

    SetupWizardIntro {
        id: introWizard
        dialogParent: mainSwipeView
    }

    Controls {
        id: controls
        parentWidth: appWindow.width
        parentHeight: mainSwipeView.height
        dialogParent: mainSwipeView
    }

    MultiSettings {
        id: multiSettings
        dialogParent: mainSwipeView
    }

    Loader {
        id: settingsLoader
        anchors.fill: parent
        asynchronous: true
        visible: status == Loader.Ready
        sourceComponent: Settings {
            id: settings
            dialogParent: mainSwipeView
        }
    }

    Loader {
        id: canDrawerLoader
        anchors.fill: parent
        asynchronous: true
        visible: status == Loader.Ready
        sourceComponent: Drawer {
            id: canDrawer
            edge: Qt.RightEdge
            width: Math.min(0.6 * appWindow.width, 0.8 * appWindow.height)
            height: appWindow.height > appWindow.width ?  appWindow.height - footer.height - header.height : appWindow.height
            y: appWindow.height > appWindow.width ?  header.height : 0
            dragMargin: 20
            interactive: false

            Overlay.modal: Rectangle {
                color: "#AA000000"
            }

            CanScreen {
                id: canScreen
                anchors.fill: parent
                dialogParent: mainSwipeView
            }

            onVisibleChanged: {
                if (visible) {
                    canScreen.scanIfEmpty()
                }
            }
        }
    }

    Drawer {
        id: drawer
        width: Math.min(0.5 *appWindow.width, 0.75 *appWindow.height)
        height: appWindow.height > appWindow.width ?  appWindow.height - footer.height - header.height : appWindow.height
        y: appWindow.height > appWindow.width ?  header.height : 0
        dragMargin: 20
        interactive: false

        Overlay.modal: Rectangle {
            color: "#AA000000"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 0

            Image {
                id: image
                Layout.preferredWidth: Math.min(parent.width, parent.height)*0.8
                Layout.preferredHeight: (sourceSize.height * Layout.preferredWidth) / sourceSize.width
                Layout.margins: Math.min(parent.width, parent.height)*0.1
                Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
                source: "qrc" + Utility.getThemePath() + "/logo.png"
                antialiasing: true

            }

            Button {
                id: reconnectButton
                Layout.fillWidth: true
                text: connected ? "Disconnect" : "Connect"
                flat: true
                onClicked: {
                    if (connected) {
                        VescIf.disconnectPort()
                    } else {
                        connScreen.opened = true
                    }

                    drawer.close()
                }
            }

            Item {
                // Spacer
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Button {
                Layout.fillWidth: true
                text: "Settings"
                flat: true

                onClicked: {
                    drawer.close()
                    settingsLoader.item.openDialog()
                }
            }

            Button {
                Layout.fillWidth: true
                text: "About"
                flat: true
                onClicked: {
                    VescIf.emitMessageDialog(
                                "About",
                                Utility.aboutText(),
                                true, true)
                }
            }

            Button {
                Layout.fillWidth: true
                text: "Changelog"
                flat: true

                onClicked: {
                    VescIf.emitMessageDialog(
                                "VESC Tool Changelog",
                                Utility.vescToolChangeLog(),
                                true, false)
                }
            }

            Button {
                Layout.fillWidth: true
                text: "License"
                flat: true

                onClicked: {
                    if (Qt.platform.os == "ios"){
                        VescIf.emitMessageDialog(
                                    mInfoConf.getLongName("ios_license_text"),
                                    mInfoConf.getDescription("ios_license_text"),
                                    true, true)
                    } else {
                        VescIf.emitMessageDialog(
                                    mInfoConf.getLongName("gpl_text"),
                                    mInfoConf.getDescription("gpl_text"),
                                    true, true)
                    }
                }
            }

            Button {
                Layout.fillWidth: true
                text: "Privacy Policy"
                flat: true

                onClicked: {
                    Qt.openUrlExternally("https://vesc-project.com/privacy_policies")
                }
            }
        }
    }

    component DetailHeader: ToolBar {
        property Item page: parent
        default property alias actions: actionRow.data

        background: Rectangle {
            implicitHeight: 48
            color: Utility.getAppHexColor("normalBackground")
        }

        ToolButton {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            visible: page.StackView.index > 0
            text: visible ? page.StackView.view.get(page.StackView.index - 1).title : ""
            icon.source: "qrc" + Utility.getThemePath() + "icons/arrow_l.png"
            display: AbstractButton.TextBesideIcon
            onClicked: page.StackView.view.pop()

            background: Rectangle {
                radius: 6
                color: Utility.getAppHexColor("lightBackground")
                border.color: Utility.getAppHexColor("lightestBackground")
            }
        }

        Label {
            anchors.centerIn: parent
            text: page.title
            font.bold: true
        }

        RowLayout {
            id: actionRow
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            Material.foreground: Material.accent
        }
    }

    component HubRow: ItemDelegate {
        id: hubRow
        property string iconName
        property string meta

        Layout.fillWidth: true

        contentItem: RowLayout {
            spacing: 10

            Image {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                fillMode: Image.PreserveAspectFit
                source: "qrc" + Utility.getThemePath() + "icons/" + hubRow.iconName + ".png"
            }

            Label {
                Layout.fillWidth: true
                text: hubRow.text
                elide: Text.ElideRight
            }

            Label {
                text: hubRow.meta
                color: Utility.getAppHexColor("disabledText")
            }

            Label {
                text: "›"
                color: Utility.getAppHexColor("disabledText")
            }
        }
    }

    component LiveButton: ToolButton {
        property string iconName

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        icon.source: "qrc" + Utility.getThemePath() + "icons/" + iconName + ".png"
        display: AbstractButton.TextUnderIcon
        font.pointSize: 11

        background: Rectangle {
            radius: 7
            color: Utility.getAppHexColor("lightBackground")
            border.color: Utility.getAppHexColor("lightestBackground")
        }
    }

    component DeviceCard: GroupBox {
        title: qsTr("This device")
        Layout.fillWidth: true

        RowLayout {
            anchors.fill: parent

            Image {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                source: "qrc" + Utility.getThemePath() + "icons/Electronics-96.png"
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    text: fwParams.hw
                    font.bold: true
                }

                Label {
                    text: "FW " + fwParams.major + "." + String(fwParams.minor).padStart(2, "0") + " · " + targetBus
                    color: Utility.getAppHexColor("normalText")
                }
            }

            ToolButton {
                text: "⋮"
                onClicked: deviceMenu.open()

                Menu {
                    id: deviceMenu

                    MenuItem {
                        text: "Disconnect"
                        onTriggered: VescIf.disconnectPort()
                    }

                    MenuItem {
                        text: "Reboot"
                        onTriggered: mCommands.reboot()
                    }

                    MenuItem {
                        text: "Shut Down"
                        onTriggered: mCommands.shutdown()
                    }
                }
            }
        }
    }

    SwipeView {
        id: mainSwipeView
        currentIndex: tabBar.currentIndex
        anchors.fill: parent
        anchors.leftMargin: notchLeft*0.75
        anchors.rightMargin: notchRight*0.75
        clip: true
        interactive: false
        contentItem: ListView {
            model: mainSwipeView.contentModel
            interactive: mainSwipeView.interactive
            currentIndex: mainSwipeView.currentIndex

            spacing: mainSwipeView.spacing
            orientation: mainSwipeView.orientation
            snapMode: ListView.SnapOneItem
            boundsBehavior: Flickable.StopAtBounds

            highlightRangeMode: ListView.StrictlyEnforceRange
            preferredHighlightBegin: 0
            preferredHighlightEnd: 0
            highlightMoveDuration: 250

            maximumFlickVelocity: 8 * (mainSwipeView.orientation ===
                                       Qt.Horizontal ? width : height)
        }

        Page {
            Loader {
                id: startLoader
                anchors.fill: parent
                asynchronous: true
                visible: status == Loader.Ready
                sourceComponent: StartPage {
                    id: connBle
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    isHorizontal: mainIsHorizontal

                    onRequestOpenControls: {
                        controls.openDialog()
                    }

                    onRequestSkipToLive: {
                        tabBar.setCurrentIndex(2)
                    }
                }
            }
        }

        StackView {
            id: configPage
            initialItem: ScrollView {
                id: configScroll
                property string title: "Config"
                padding: 10
                contentWidth: availableWidth

                ColumnLayout {
                    width: configScroll.availableWidth
                    spacing: 10

                    HubRow {
                        visible: connected
                        iconName: "motor_side"
                        text: "Writes to " + fwParams.hw
                        meta: targetBus
                        onClicked: canDrawerLoader.item.open()

                        background: Rectangle {
                            radius: 8
                            color: Utility.getAppHexColor("darkBackground")
                            border.color: Utility.getAppHexColor("lightestBackground")
                        }
                    }

                    GroupBox {
                        title: qsTr("Settings")
                        Layout.fillWidth: true
                        visible: fwConfig || customConfName !== ""

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            HubRow {
                                visible: fwConfig
                                iconName: "mcconf"
                                text: "Motor config"
                                meta: "FOC · PID · limits"
                                onClicked: configPage.push(confPageMotor)
                            }

                            HubRow {
                                visible: fwConfig
                                iconName: "appconf"
                                text: "App config"
                                meta: "inputs · CAN · IMU"
                                onClicked: configPage.push(confPageApp)
                            }

                            HubRow {
                                visible: customConfName !== ""
                                iconName: "Electronics-96"
                                text: customConfName + " settings"
                                meta: "custom config"
                                onClicked: configPage.push(confCustomPage)
                            }
                        }
                    }

                    GroupBox {
                        title: qsTr("Profiles")
                        Layout.fillWidth: true

                        HubRow {
                            anchors.fill: parent
                            iconName: "Speed-96"
                            text: profileBox.count > 0 ? profileBox.model.join(" · ") : "Add a profile"
                            meta: profileBox.currentIndex < 0 ? "" : "active: " + profileBox.currentText
                            onClicked: configPage.push(profilesPage)
                        }
                    }

                    GroupBox {
                        title: qsTr("All VESCs on CAN")
                        Layout.fillWidth: true

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            HubRow {
                                iconName: "Settings-96"
                                text: "Multi Settings"
                                meta: "write to all"
                                onClicked: multiSettings.openDialog()
                            }

                            HubRow {
                                id: backupConfButton
                                iconName: "Save-96"
                                text: "Backup configs"
                                onClicked: startLoader.item.backupConfigDialog.open()
                            }

                            HubRow {
                                id: restoreConfButton
                                iconName: "Open Folder-96"
                                text: "Restore configs"
                                onClicked: startLoader.item.restoreConfigDialog.open()
                            }

                            Timer {
                                id: toolsTimer
                                interval: 100
                                repeat: true
                                running: true
                                onTriggered: {
                                    restoreConfButton.enabled = VescIf.customConfigRxDone()
                                    backupConfButton.enabled = VescIf.customConfigRxDone()
                                    deviceBackupButton.enabled = VescIf.customConfigRxDone()
                                }
                            }
                        }
                    }

                    RowLayout {
                        visible: fwConfig

                        ToolButton {
                            text: "DETECT && CALIBRATE ▾"
                            onClicked: {
                                configPage.push(confPageMotor)
                                confMotorLoader.item.detectMenu.open()
                            }
                        }

                        ToolButton {
                            text: "FILE ▾"
                            onClicked: {
                                configPage.push(confPageMotor)
                                confMotorLoader.item.fileMenu.open()
                            }
                        }

                        ToolButton {
                            text: "INPUT TOOLS ▾"
                            onClicked: {
                                configPage.push(confPageApp)
                                confAppLoader.item.inputMenu.open()
                            }
                        }
                    }
                }
            }
        }

        StackView {
            id: livePage
            initialItem: Page {
                id: dashPage
                title: "Dash"

                ScrollView {
                    id: offlineScroll
                    anchors.fill: parent
                    z: 1
                    visible: !connected
                    padding: 10
                    contentWidth: availableWidth

                    background: Rectangle {
                        color: Utility.getAppHexColor("normalBackground")
                    }

                    ColumnLayout {
                        width: offlineScroll.availableWidth
                        spacing: 10

                        Image {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: parent.width * 0.6
                            Layout.preferredHeight: sourceSize.height * Layout.preferredWidth / sourceSize.width
                            source: "qrc" + Utility.getThemePath() + "/logo.png"
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: "Not connected"
                            font.bold: true
                        }

                        Button {
                            Layout.fillWidth: true
                            text: "Connect"
                            highlighted: true
                            onClicked: connScreen.opened = true
                        }

                        GroupBox {
                            title: qsTr("Works offline")
                            Layout.fillWidth: true

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 0

                                HubRow {
                                    iconName: "Line Chart-96"
                                    text: "Open a log"
                                    onClicked: tabBar.setCurrentIndex(3)
                                }

                                HubRow {
                                    iconName: "Speed-96"
                                    text: "Edit profiles"
                                    onClicked: {
                                        tabBar.setCurrentIndex(1)
                                        configPage.pop(null)
                                        configPage.push(profilesPage)
                                    }
                                }

                                HubRow {
                                    iconName: "lambda"
                                    text: "LispBM editor"
                                    onClicked: {
                                        tabBar.setCurrentIndex(4)
                                        devicePage.pop(null)
                                        devicePage.push(lispPage)
                                    }
                                }

                                HubRow {
                                    iconName: "Package-96"
                                    text: "Browse packages"
                                    onClicked: startLoader.item.packageDialog.open()
                                }

                                HubRow {
                                    iconName: "Download-96"
                                    text: "Download firmware files"
                                    onClicked: startLoader.item.firmwareDialog.open()
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    enabled: connected
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.rightMargin: 10

                        ComboBox {
                            id: profileBox
                            flat: true
                            visible: count > 0
                            displayText: currentIndex < 0 ? "Profile" : currentText

                            function updateProfiles() {
                                var prof = VescIf.getProfiles()
                                model = prof.map(function(p) { return p.name })
                                currentIndex = prof.findIndex(function(p, i) { return VescIf.isProfileInUse(i) })
                            }

                            Component.onCompleted: updateProfiles()

                            onActivated: function(index) {
                                mCommands.setMcconfTemp(VescIf.getProfile(index), true, false, true, false, true)
                                VescIf.updateMcconfFromProfile(VescIf.getProfile(index))
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Label {
                            id: recLabel
                            visible: rtTimer.logStart > 0
                            color: Utility.getAppHexColor("red")
                            font.bold: true
                        }
                    }

                    Loader {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        asynchronous: true
                        sourceComponent: RtDataSetup {
                            anchors.fill: parent
                            dialogParent: mainSwipeView
                            updateData: liveItem == dashPage
                            isHorizontal: mainIsHorizontal
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: 6
                        spacing: 5

                        LiveButton {
                            text: "Motor"
                            iconName: "motor_side"
                            onClicked: livePage.push(rtDataPage)
                        }

                        LiveButton {
                            visible: bmsFound
                            text: "Battery"
                            iconName: "icons8-battery-100"
                            onClicked: livePage.push(bmsPage)
                        }

                        LiveButton {
                            text: "Stats"
                            iconName: "Polyline-96"
                            onClicked: livePage.push(statPage)
                        }

                        LiveButton {
                            visible: imuFitted
                            text: "IMU"
                            iconName: "imu_off"
                            onClicked: livePage.push(imuPage)
                        }

                        LiveButton {
                            visible: appUiObj != 0
                            text: uiAppPage.title
                            iconName: "Package-96"
                            onClicked: livePage.push(uiAppPage)
                        }

                        LiveButton {
                            visible: hwUiObj != 0
                            text: uiHwPage.title
                            iconName: "Package-96"
                            onClicked: livePage.push(uiHwPage)
                        }
                    }
                }
            }
        }

        Page {
            Loader {
                anchors.fill: parent
                asynchronous: true
                visible: status == Loader.Ready
                sourceComponent: LogAnalysis {
                    anchors.fill: parent
                }
            }
        }

        StackView {
            id: devicePage
            initialItem: ScrollView {
                id: deviceScroll
                property string title: "Device"
                padding: 10
                contentWidth: availableWidth

                ColumnLayout {
                    width: deviceScroll.availableWidth
                    spacing: 10

                    DeviceCard {
                        visible: connected
                    }

                    GroupBox {
                        title: qsTr("Firmware & Packages")
                        Layout.fillWidth: true

                        GridLayout {
                            anchors.fill: parent
                            columns: 2
                            columnSpacing: 5
                            rowSpacing: 5

                            ImageButton {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 500
                                Layout.preferredHeight: 60

                                buttonText: "Update\nFirmware"
                                imageSrc: "qrc" + Utility.getThemePath() + "icons/Electronics-96.png"

                                onClicked: {
                                    startLoader.item.firmwareDialog.open()
                                }
                            }

                            ImageButton {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 500
                                Layout.preferredHeight: 60

                                buttonText: "Package\nStore"
                                imageSrc: "qrc" + Utility.getThemePath() + "icons/Package-96.png"

                                onClicked: {
                                    startLoader.item.packageDialog.open()
                                }
                            }

                            HubRow {
                                id: deviceBackupButton
                                Layout.columnSpan: 2
                                iconName: "Save-96"
                                text: "Back up configs first"
                                meta: "opens here"
                                onClicked: startLoader.item.backupConfigDialog.open()
                            }
                        }
                    }

                    GroupBox {
                        title: qsTr("Developer")
                        Layout.fillWidth: true

                        GridLayout {
                            anchors.fill: parent
                            columns: 2
                            columnSpacing: 5

                            ImageButton {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 500
                                Layout.preferredHeight: 60

                                buttonText: "LispBM\nScripting"
                                imageSrc: "qrc" + Utility.getThemePath() + "icons/lambda.png"

                                onClicked: {
                                    devicePage.push(lispPage)
                                }
                            }

                            ImageButton {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 500
                                Layout.preferredHeight: 60

                                buttonText: "Terminal"
                                imageSrc: "qrc" + Utility.getThemePath() + "icons/Console-96.png"

                                onClicked: {
                                    devicePage.push(terminalPage)
                                }
                            }
                        }
                    }

                    GroupBox {
                        title: qsTr("Bridges")
                        Layout.fillWidth: true

                        GridLayout {
                            anchors.fill: parent
                            columns: 2
                            columnSpacing: 5

                            ImageButton {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 500
                                Layout.preferredHeight: 60

                                buttonText: "TCP Bridge\nto computer"
                                imageSrc: "qrc" + Utility.getThemePath() + "icons/Online-96.png"

                                onClicked: {
                                    devicePage.push(tcpPage)
                                }
                            }

                            ImageButton {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 500
                                Layout.preferredHeight: 60

                                buttonText: "TCP Hub\nserver"
                                imageSrc: "qrc" + Utility.getThemePath() + "icons/Globe-96.png"

                                onClicked: {
                                    devicePage.push(tcpHubPage)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    footer: TabBar {
        id: tabBar
        currentIndex: mainSwipeView.currentIndex
        bottomPadding: notchBot

        background: Rectangle {
            color: Utility.getAppHexColor("lightBackground")
        }

        Repeater {
            model: [["Setup", "Wizard-96"], ["Config", "Horizontal Settings Mixer-96"], ["Live", "Speed-96"],
                ["Logs", "Line Chart-96"], ["Device", "Electronics-96"]]

            TabButton {
                property bool wasChecked
                text: modelData[0]
                icon.source: "qrc" + Utility.getThemePath() + "icons/" + modelData[1] + ".png"
                display: AbstractButton.TextUnderIcon
                onPressed: wasChecked = checked
                onClicked: {
                    if (wasChecked && mainSwipeView.currentItem.depth > 1) {
                        mainSwipeView.currentItem.pop(null)
                    }
                }
            }
        }
    }

    Page {
        id: uiHwPage
        visible: false
        header: DetailHeader {
            visible: tabBar.visible
        }

        Item {
            id: uiHw
            anchors.fill: parent
            property var tabBarItem: tabBar
            property var swipeViewItem: mainSwipeView
        }
    }

    Page {
        id: uiAppPage
        visible: false
        header: DetailHeader {
            visible: tabBar.visible
        }

        Item {
            id: uiApp
            anchors.fill: parent
            property var tabBarItem: tabBar
            property var swipeViewItem: mainSwipeView
        }
    }

    Page {
        id: confPageMotor
        title: "Motor Config"
        visible: false
        header: DetailHeader {}

        Loader {
            id: confMotorLoader
            anchors.fill: parent
            asynchronous: true
            sourceComponent: ConfigPageMotor {
                anchors.fill: parent
                dialogParent: mainSwipeView
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                isHorizontal: mainIsHorizontal
            }
        }
    }

    Page {
        id: confPageApp
        title: "App Config"
        visible: false
        header: DetailHeader {}

        Loader {
            id: confAppLoader
            anchors.fill: parent
            asynchronous: true
            sourceComponent: ConfigPageApp {
                dialogParent: mainSwipeView
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                isHorizontal: mainIsHorizontal
            }
        }
    }

    Page {
        id: confCustomPage
        title: customConfName
        visible: false
        header: DetailHeader {}

        Loader {
            id: confCustomLoader
            anchors.fill: parent
            asynchronous: true
            sourceComponent: ConfigPageCustom {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                isHorizontal: mainIsHorizontal
            }
        }
    }

    Page {
        id: rtDataPage
        title: "Motor"
        visible: false
        // Sized up front so the gauges get their final size before the first paint
        width: livePage.width
        height: livePage.height
        header: DetailHeader {}

        Loader {
            anchors.fill: parent
            asynchronous: true
            visible: status == Loader.Ready
            sourceComponent: RtData {
                anchors.fill: parent
                updateData: liveItem == rtDataPage
                isHorizontal: mainIsHorizontal

                onRequestFaultLog: {
                    tabBar.setCurrentIndex(4)
                    devicePage.pop(null)
                    devicePage.push(terminalPage)
                    mCommands.sendTerminalCmd("faults")
                }
            }
        }
    }

    Page {
        id: imuPage
        title: "IMU"
        visible: false
        header: DetailHeader {}

        RtDataIMU {
            id: rtIMU
            anchors.top: parent.top
            width: parent.width
            Layout.fillWidth: true
            z:2
        }
        CheckBox {
            anchors.top: rtIMU.bottom
            Layout.fillWidth: true
            id: useYawBox
            text: "Use Yaw (will drift)"
            checked: false
            z:2
        }
        Loader {
            id: vesc3dLoader
            asynchronous: true
            visible: status == Loader.Ready
            anchors.centerIn: parent
            anchors.verticalCenterOffset: rtIMU.height/2
            width:  Math.min(parent.width*Screen.devicePixelRatio, (parent.height - rtIMU.height)*Screen.devicePixelRatio)
            antialiasing: true
            height: width
            onLoaded: {
                item.setRotation(0.1, 0.75, 0.4)
            }
            z:1
            sourceComponent:
                Vesc3dItem {
                id: vesc3d
                anchors.fill: parent
                scale: 1.0 / Screen.devicePixelRatio
                z:1
            }
        }
    }

    Page {
        id: statPage
        title: "Stats"
        visible: false
        header: DetailHeader {
            ToolButton {
                text: "RESET"
                font.pointSize: 10
                onClicked: mCommands.resetStats(false)
            }
        }

        Loader {
            anchors.fill: parent
            anchors.margins: 10
            asynchronous: true

            sourceComponent: StatPage {
                anchors.fill: parent
                isHorizontal: mainIsHorizontal
            }
        }
    }

    Page {
        id: bmsPage
        title: "Battery"
        visible: false
        header: DetailHeader {
            ToolButton {
                visible: appUiObj != 0
                text: uiAppPage.title
                font.pointSize: 10
                onClicked: livePage.push(uiAppPage)
            }

            ToolButton {
                visible: hwUiObj != 0
                text: uiHwPage.title
                font.pointSize: 10
                onClicked: livePage.push(uiHwPage)
            }

            ToolButton {
                text: "ACTIONS ▾"
                font.pointSize: 10
                onClicked: bmsLoader.item.openActions(this)
            }
        }

        Loader {
            id: bmsLoader
            anchors.fill: parent
            asynchronous: true
            visible: status == Loader.Ready
            sourceComponent: BMS {
                anchors.fill: parent
                isHorizontal: mainIsHorizontal
            }
        }
    }

    Page {
        id: profilesPage
        title: "Profiles"
        visible: false
        header: DetailHeader {}

        Loader {
            anchors.fill: parent
            asynchronous: true
            visible: status == Loader.Ready
            sourceComponent: Profiles {
                anchors.fill: parent
                anchors.topMargin: 5
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                dialogParent: mainSwipeView
            }
        }
    }

    Page {
        id: terminalPage
        title: "Terminal"
        visible: false
        header: DetailHeader {}

        Loader {
            anchors.fill: parent
            asynchronous: true
            visible: status == Loader.Ready
            sourceComponent: Terminal {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 10
            }
        }
    }

    Page {
        id: lispPage
        title: "LispBM"
        visible: false
        header: DetailHeader {}

        Loader {
            anchors.fill: parent
            asynchronous: true
            visible: status == Loader.Ready
            sourceComponent: Lisp {
                anchors.fill: parent
            }
        }
    }

    Page {
        id: modulePage
        visible: false

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10

            DeviceCard {}

            GroupBox {
                title: qsTr("On the CAN bus")
                Layout.fillWidth: true
                visible: canDevs.length > 0

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Repeater {
                        model: canDevs

                        HubRow {
                            iconName: "can_off"
                            text: "CAN " + modelData
                            meta: "View"
                            onClicked: mCommands.setSendCan(true, modelData)
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Utility.getAppHexColor("disabledText")
                text: "Tap View to switch target, same as picking it in the CAN drawer."
            }

            Item {
                Layout.fillHeight: true
            }

            RowLayout {
                Layout.fillWidth: true

                LiveButton {
                    visible: appUiObj != 0
                    text: uiAppPage.title
                    iconName: "Package-96"
                    onClicked: livePage.push(uiAppPage)
                }

                LiveButton {
                    visible: hwUiObj != 0
                    text: uiHwPage.title
                    iconName: "Package-96"
                    onClicked: livePage.push(uiHwPage)
                }
            }
        }
    }

    Page {
        id: tcpPage
        title: "TCP Bridge"
        visible: false
        header: DetailHeader {}

        TcpBox {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
        }
    }

    Page {
        id: tcpHubPage
        title: "TCP Hub"
        visible: false
        header: DetailHeader {}

        TcpHubBox {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
        }
    }

    header: Rectangle {
        id: connectedRect
        clip: true
        color: Utility.getAppHexColor("lightBackground")
        width: parent.width
        height: 35 + notchTop
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top:parent.top
            height: parent.height/2.0
            gradient: Gradient {
                    GradientStop { position: 0.0; color: "#15ffffff"}
                    GradientStop { position: 0.3; color: "#04ffffff"}
                    GradientStop { position: 1.0; color: "transparent" }
            }
        }
        Behavior on color {
            ColorAnimation {
                duration: 200;
                easing.type: Easing.OutBounce
                easing.overshoot: 3
            }
        }

        RowLayout{
            enabled:true
            anchors.fill: parent
            anchors.topMargin: notchTop
            spacing: 0
            ToolButton {
                id:settingsButton
                Layout.fillHeight: true
                Layout.preferredWidth: 70
                Image {
                    anchors.centerIn: parent
                    antialiasing: true
                    height: parent.width*0.35
                    width: height
                    source: "qrc" + Utility.getThemePath() + "icons/Settings-96.png"
                }
                onClicked: {
                    if (drawer.visible) {
                        drawer.close()
                    } else {
                        drawer.open()
                    }
                }

            }
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "#33000000"
            }
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color:  "#33ffffff"
            }
            ColumnLayout{
                spacing: 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                Text {
                    id: connectedText
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Utility.getAppHexColor("lightText")
                    text: VescIf.getConnectedPortName()
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
                Text {
                    Layout.fillWidth: true
                    visible: targetBus !== "LOCAL"
                    color: Utility.getAppHexColor("lightAccent")
                    text: targetBus + " · " + fwParams.hw
                    horizontalAlignment: Text.AlignHCenter
                    font.pointSize: 9
                }
            }
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "#33000000"
            }
            Rectangle{
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "#33ffffff"
            }
            ToolButton {
                Layout.fillHeight: true
                Layout.preferredWidth: 70
                Image {
                    anchors.centerIn: parent
                    antialiasing: true
                    height: parent.width*0.35
                    width: height
                    source: "qrc" + Utility.getThemePath() + (targetBus === "LOCAL" ? "icons/can_off.png" : "icons/can_on.png")
                }
                onClicked: {
                    if (canDrawerLoader.item.visible) {
                        canDrawerLoader.item.close()
                    } else {
                        canDrawerLoader.item.open()
                    }
                }
            }
        }
    }

    Rectangle {
        parent: Overlay.overlay
        anchors.fill: parent
        color: "black"

        ConnectScreen {
            id: connScreen
            x: 0
            y: 0
            height: parent.height
            width: parent.width
            opened: true
            fullLogo: true

            dialogParent: mainSwipeView

            Component.onCompleted: {
                startBleScan()
            }

            onYChanged: {
                parent.color.a = Math.min(1, Math.max(1 - y / height, 0))

                if (opened) {
                    drawer.interactive = false
                    canDrawerLoader.item.interactive = false
                    drawer.close()
                    canDrawerLoader.item.close()
                } else {
                    drawer.interactive = true
                    canDrawerLoader.item.interactive = true
                }
            }
        }
    }

    Timer {
        id: statusTimer
        interval: 1600
        running: false
        repeat: false
        onTriggered: {
            connectedText.text = VescIf.getConnectedPortName()
            connectedRect.color = Utility.getAppHexColor("lightBackground")
        }
    }

    Timer {
        id: uiTimer
        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            if (!statusTimer.running && connectedText.text !== VescIf.getConnectedPortName()) {
                connectedText.text = VescIf.getConnectedPortName()
            }
        }
    }

    Timer {
        id: confTimer
        interval: 1000
        running: true
        repeat: true

        property bool mcConfRx: false
        property bool appConfRx: false

        onTriggered: {
            if (VescIf.isPortConnected() && VescIf.getLastFwRxParams().hwTypeStr() === "VESC") {
                if (!mcConfRx) {
                    mCommands.getMcconf()
                }

                if (!appConfRx) {
                    mCommands.getAppConf()
                }
            }
        }
    }

    Timer {
        id: rtTimer
        property double logStart: 0
        interval: 50
        running: true
        repeat: true

        onTriggered: {
            if (VescIf.isPortConnected()) {
                // Sample RT data when the corresponding page is selected, or when
                // RT logging is active.

                if (VescIf.isRtLogOpen()) {
                    if (logStart == 0) {
                        logStart = Date.now()
                    }
                    recLabel.text = "REC " + new Date(Date.now() - logStart).toISOString().substr(14, 5)
                    interval = 50
                    mCommands.getValues()
                    mCommands.getValuesSetup()
                    mCommands.getImuData(0xFFFF)

                    if (liveItem == bmsPage) {
                        mCommands.bmsGetValues()
                    }
                } else {
                    logStart = 0

                    if (liveItem == rtDataPage) {
                        interval = 50
                        mCommands.getValues()
                    }

                    if (liveItem == dashPage) {
                        interval = 50
                        mCommands.getValuesSetup()
                        mCommands.getImuData(0x2)
                    }

                    if (liveItem == imuPage) {
                        interval = 20
                        mCommands.getImuData(0x1FF)
                    }

                    if (liveItem == statPage) {
                        interval = 100
                        mCommands.getValuesSetupSelective(0x7E00)
                        mCommands.getStats(0xFFFFFFFF)
                    }

                    if (liveItem == bmsPage) {
                        interval = 100
                        mCommands.bmsGetValues()
                    }
                }
            }
        }
    }

    Timer {
        id: bleDisconnectTimer
        interval: 1000
        running: false
        repeat: true
        property int trysLeft: 0

        onTriggered: {
            if(trysLeft < 1 || fwReadCorrectly) {
                bleDisconnectTimer.stop()
                connScreen.opened = VescIf.isPortConnected() ? false : true
                return
            }
            if(VescIf.getLastBleAddr().length > 0) {
                VescIf.connectBle(VescIf.getLastBleAddr())
            }
            trysLeft = trysLeft - 1
        }
    }

    Dialog {
        id: vescDialog
        standardButtons: Dialog.Ok
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        Overlay.modal: Rectangle {
            color: "#AA000000"
        }

        width: parent.width - 20 - notchLeft - notchRight
        height: Math.min(implicitHeight, parent.height - 40 - notchBot - notchTop)
        x: (parent.width - width) / 2
        y: (parent.height - height + notchTop) / 2
        parent: mainSwipeView

        ScrollView {
            id: vescDialogScroll
            anchors.fill: parent
            clip: true
            contentWidth: availableWidth

            Text {
                id: vescDialogLabel
                color: {color = Utility.getAppHexColor("lightText")}
                linkColor: {linkColor = Utility.getAppHexColor("lightAccent")}
                verticalAlignment: Text.AlignVCenter
                anchors.fill: parent
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                textFormat: Text.RichText
                onLinkActivated: {
                    Qt.openUrlExternally(link)
                }
            }
        }
    }

    property var hwUiObj: 0
    property var appUiObj: 0

    function updateHwAppUi () {
        if (livePage.currentItem == uiHwPage || livePage.currentItem == uiAppPage) {
            livePage.pop()
        }

        if (hwUiObj != 0) {
            hwUiObj.destroy()
            hwUiObj = 0
        }

        if (appUiObj != 0) {
            appUiObj.destroy()
            appUiObj = 0
        }

        tabBar.visible = true

        if (VescIf.isPortConnected() && VescIf.qmlHwLoaded()) {
            hwUiObj = Qt.createQmlObject(VescIf.qmlHw(), uiHw, "HwUi")
            uiHwPage.title = hwUiObj.tabTitle ? hwUiObj.tabTitle : "HwUi"

            if (VescIf.getLastFwRxParams().qmlHwFullscreen) {
                tabBar.visible = false
                tabBar.setCurrentIndex(2)
                livePage.push(uiHwPage)
            }
        }

        if (VescIf.isPortConnected() && VescIf.qmlAppLoaded()) {
            appUiObj = Qt.createQmlObject(VescIf.qmlApp(), uiApp, "AppUi")
            uiAppPage.title = appUiObj.tabTitle ? appUiObj.tabTitle : "AppUi"

            if (VescIf.getLastFwRxParams().qmlAppFullscreen) {
                tabBar.visible = false
                tabBar.setCurrentIndex(2)
                livePage.push(uiAppPage)
            }
        }
    }

    Timer {
        id: confCustomTimer
        running: false
        triggeredOnStart: true
        interval: 500
        repeat: true
        onTriggered: {
            if (confCustomLoader.status == Loader.Ready) {
                stop()

                if (VescIf.isPortConnected() && VescIf.customConfig(0) !== null) {
                    confCustomLoader.item.reloadConfig()
                    customConfName = VescIf.customConfig(0).getLongName("hw_name")
                } else {
                    customConfName = ""
                }
            }
        }
    }

    function updateConfCustom () {
        confCustomTimer.start()
    }

    Connections {
        target: VescIf
        function onPortConnectedChanged() {
            connectedText.text = VescIf.getConnectedPortName()
            if (!VescIf.isPortConnected()) {
                confTimer.mcConfRx = false
                confTimer.appConfRx = false
                connected = false
                fwReadCorrectly = false
                imuFitted = false
                canDevs = []
            } else {
                connected = true
            }

            if (VescIf.useWakeLock()) {
                VescIf.setWakeLock(VescIf.isPortConnected())
            }
            if(!bleDisconnectTimer.running) {
                connScreen.opened = VescIf.isPortConnected() ? false : true
                if (VescIf.isPortConnected()) {
                    tabBar.setCurrentIndex(0)
                }
            }
        }

        function onUnintentionalBleDisconnect() {
            bleDisconnectTimer.trysLeft = 5
            bleDisconnectTimer.start()
        }

        function onStatusMessage(msg, isGood) {
            connectedText.text = msg
            connectedRect.color = isGood ? Utility.getAppHexColor("lightAccent") : Utility.getAppHexColor("red")
            statusTimer.restart()
        }

        function onMessageDialog(title, msg, isGood, richText) {
            if (!richText && msg.trim().startsWith("#")) {
                vescDialogLabel.textFormat = Text.MarkdownText
            } else {
                vescDialogLabel.textFormat = richText ? Text.RichText : Text.AutoText
            }

            vescDialog.title = title
            vescDialogLabel.text = (richText ? "<style>a:link { color: lightblue; }</style>" : "") + msg
            vescDialogScroll.ScrollBar.vertical.position = 0
            vescDialog.open()
        }

        function onFwRxChanged(rx, limited) {
            configPage.pop(null)
            livePage.pop(null)
            fwParams = VescIf.getLastFwRxParams()
            targetBus = rx && mCommands.getSendCan() ? "CAN " + mCommands.getCanSendId() : "LOCAL"
            bmsFound = false

            var root = rx && {"VESC BMS": bmsPage, "Custom Module": modulePage}[fwParams.hwTypeStr()] || dashPage
            if (livePage.get(0) !== root) {
                livePage.replace(null, root)
            }

            if (rx) {
                fwConfig = VescIf.getFwSupportsConfiguration()

                if (fwConfig) {
                    confTimer.restart()
                    confTimer.mcConfRx = false
                    confTimer.appConfRx = false

                    mCommands.getMcconf()
                    mCommands.getAppConf()
                }

                fwReadCorrectly = true
                bleDisconnectTimer.stop()
                mCommands.bmsGetValues()
            } else {
                updateConfCustom()
            }

            updateHwAppUi()
        }

        function onProfilesUpdated() {
            profileBox.updateProfiles()
        }

        function onQmlLoadDone() {
            if (VescIf.askQmlLoad()) {
                qmlLoadDialog.open()
            } else {
                updateHwAppUi()
            }
        }

        function onCustomConfigLoadDone() {
            updateConfCustom()
        }
    }

    Connections {
        target: mMcConf

        function onUpdated() {
            confTimer.mcConfRx = true
            profileBox.updateProfiles()
        }
    }

    Connections {
        target: mAppConf

        function onUpdated() {
            confTimer.appConfRx = true
            imuFitted = mAppConf.getParamEnum("imu_conf.type") !== 0
        }
    }

    Connections {
        target: mCommands
        function onBmsValuesRx(val) {
            bmsFound = val.v_cells.length > 0
        }

        function onPingCanRx(devs, isTimeout) {
            if (!isTimeout) {
                canDevs = devs
            }
        }

        function onValuesImuReceived(values, mask) {
            if (liveItem == imuPage) {
                vesc3dLoader.item.setRotation(values.roll, values.pitch,
                                              useYawBox.checked ? values.yaw : 0)
                rtIMU.updateText(values)
            }
        }

        function onDeserializeConfigFailed(isMc, isApp) {
            if (isMc) {
                confTimer.mcConfRx = true
            }

            if (isApp) {
                confTimer.appConfRx = true
            }
        }
    }

    Dialog {
        id: qmlLoadDialog
        standardButtons: Dialog.Yes | Dialog.Cancel
        modal: true
        focus: true
        rightMargin: 10
        leftMargin: 10
        closePolicy: Popup.CloseOnEscape
        title: "Load Custom User Interface"

        Overlay.modal: Rectangle {
            color: "#AA000000"
        }

        parent: mainSwipeView
        y: parent.y + parent.height / 2 - height / 2
        width: parent.width - 20

        ColumnLayout {
            anchors.fill: parent

            Text {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Utility.getAppHexColor("lightText")
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
                text:
                    "The hardware you are connecting to contains code that will alter the " +
                    "user interface of VESC Tool. This code has not been verified by the " +
                    "authors of VESC Tool and could contain bugs and security problems. \n\n" +
                    "Do you want to load this custom user interface?"
            }

            CheckBox {
                Layout.fillWidth: true
                id: qmlDoNotAskAgainBox
                text: "Load without asking"
            }
        }

        onAccepted: {
            VescIf.setAskQmlLoad(!qmlDoNotAskAgainBox.checked)
            VescIf.storeSettings()
            updateHwAppUi()
        }

        onRejected: {
            VescIf.disconnectPort()
        }
    }
}

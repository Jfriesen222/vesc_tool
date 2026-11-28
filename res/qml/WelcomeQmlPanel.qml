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

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2

import Vedder.vesc.utility 1.0

import "qrc:/mobile"

Item {
    id: container
    width: 100

    // Full screen iPhone X workaround:
    property int notchLeft: 0
    property int notchRight: 0
    property int notchBot: 0
    property int notchTop: 0

    // Globals that the mobile components expect from mobile/main.qml
    property bool mainIsHorizontal: container.width > container.height
    property bool mainIsTablet: container.width >= 700 && Math.min(Screen.width, Screen.height) >= 600
    readonly property bool phoneLandscape: mainIsHorizontal && !mainIsTablet
    readonly property var rideSession: null
    property real rtVin: welcomeStart.vin
    property string lastDeviceAddr: ""
    property string lastDeviceName: ""
    property string lastDeviceIcon: "icons/Connected-96.png"
    property real lastDeviceTime: 0

    function timeAgo(time) {
        var minutes = Math.round((Date.now() - time) / 60000)
        return minutes < 2 ? qsTr("just now") :
               minutes < 60 ? qsTr("%1 min ago").arg(minutes) :
               minutes < 2880 ? qsTr("%1 h ago").arg(Math.round(minutes / 60)) :
                                qsTr("%1 days ago").arg(Math.round(minutes / 1440))
    }

    function displayPath(path) {
        return path
    }

    function showToast(text, detail) {
        VescIf.emitStatusMessage(text + ": " + detail, true)
    }

    function setupMotors() {
        if (!VescIf.isPortConnected()) {
            VescIf.emitMessageDialog("Setup motors FOC",
                                     "Not connected. Please connect in order to run the FOC wizard.",
                                     false, false)
        } else {
            focWizard.openDialog()
        }
    }

    function nrfQuickPair() {
        if (!VescIf.isPortConnected()) {
            VescIf.emitMessageDialog("NRF Quick Pair",
                                     "You are not connected to the VESC. Please connect in order " +
                                     "to quick pair an NRF-based remote.", false, false)
        } else {
            QmlUi.showNrfPair()
        }
    }

    function dirSetup() {
        if (!VescIf.isPortConnected()) {
            VescIf.emitMessageDialog("Directions",
                                     "You are not connected to the VESC. Please connect in order " +
                                     "to map directions.", false, false)
        } else {
            QmlUi.showDirSetup()
        }
    }

    function openMultiSettings() {
        multiSettings.openDialog()
    }

    function openWizardIMU() {
        imuWizard.openDialog()
    }

    function openBleSetup() {
        if (!VescIf.isPortConnected()) {
            VescIf.emitMessageDialog("BLE Setup",
                                     "You are not connected to the VESC.", false, false)
        } else {
            if (VescIf.getLastFwRxParams().nrfNameSupported &&
                    VescIf.getLastFwRxParams().nrfPinSupported) {
                bleSetupDialog.openDialog()
            } else {
                VescIf.emitMessageDialog("BLE Setup",
                                         "The BLE module does not support setup. You can try " +
                                         "updating the firmware on it from the SWD programmer page.",
                                         false, false)
            }
        }
    }

    // The wizards and tools open in this column instead of across the whole page
    Item {
        id: dialogArea
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, 650)
        height: parent.height
    }

    SetupWizardFoc {
        id: focWizard
        dialogParent: dialogArea
    }

    SetupWizardIMU {
        id: imuWizard
        dialogParent: dialogArea
    }

    MultiSettings {
        id: multiSettings
        dialogParent: dialogArea
    }

    BleSetupDialog {
        id: bleSetupDialog
        dialogParent: dialogArea
    }

    Rectangle {
        anchors.fill: parent
        color: Utility.getAppHexColor("darkBackground")
    }

    component HeaderTab: TabButton {
        id: headerTab
        width: implicitWidth
        // Marks a tab whose page comes from the connected hardware or app
        property string badge: ""
        implicitWidth: tabRow.implicitWidth + 4
        implicitHeight: 44
        padding: 0
        hoverEnabled: true

        contentItem: Item {
            Row {
                id: tabRow
                anchors.centerIn: parent
                spacing: 6

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: headerTab.text
                    font.pixelSize: 13
                    font.weight: headerTab.checked ? Font.Medium : Font.Normal
                    color: Utility.getAppHexColor(headerTab.checked ? "titleText" : headerTab.hovered ? "lightText" : "mutedText")
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: headerTab.badge !== ""
                    width: badgeText.implicitWidth + 9
                    height: 16
                    radius: 4
                    color: "transparent"
                    border.color: Qt.alpha(Utility.getAppHexColor("infoText"), 0.4)

                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: headerTab.badge
                        font.pixelSize: 9
                        font.weight: Font.Medium
                        font.letterSpacing: 0.7
                        color: Utility.getAppHexColor("infoText")
                    }
                }
            }
        }

        background: Item {
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 2
                radius: 1
                visible: headerTab.checked
                color: Utility.getAppHexColor("accent")
            }
        }
    }

    ColumnLayout {
        id: gaugeColumn
        anchors.fill: parent
        spacing: 0

        Item {
            id: header
            Layout.fillWidth: true
            Layout.preferredHeight: 44

            Text {
                id: pageTitle
                x: 16
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Welcome & Wizards")
                font.pointSize: 14
                font.weight: Font.Medium
                color: Utility.getAppHexColor("titleText")
            }

            Rectangle {
                id: titleDivider
                anchors.left: pageTitle.right
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: 18
                color: Utility.getAppHexColor("lineStrong")
            }

            TabBar {
                id: tabBar
                anchors.left: titleDivider.right
                anchors.leftMargin: 16
                anchors.right: parent.right
                height: parent.height
                spacing: 14
                clip: true

                onCurrentIndexChanged: {
                    if (swipeView.currentIndex !== currentIndex) {
                        swipeView.setCurrentIndex(currentIndex)
                    }
                }

                background: Item {}

                Repeater {
                    id: rep
                    model: ["Start", "RT Data", "Profiles", "LogView"]

                    HeaderTab {
                        text: modelData
                    }
                }
            }
        }

        Rectangle {
            id: headerRule
            Layout.fillWidth: true
            height: 1
            color: Utility.getAppHexColor("line")
        }

        HeaderTab {
            id: uiHwButton
            visible: uiHwPage.visible
            text: "HwUi"
            badge: "HW"
        }

        Page {
            id: uiHwPage
            visible: false

            Item {
                id: uiHw
                anchors.fill: parent
                property var tabBarItem: tabBar
                property var swipeViewItem: swipeView
            }
        }

        HeaderTab {
            id: uiAppButton
            visible: uiAppPage.visible
            text: "AppUi"
            badge: "APP"
        }

        Page {
            id: uiAppPage
            visible: false

            Item {
                id: uiApp
                anchors.fill: parent
                property var tabBarItem: tabBar
                property var swipeViewItem: swipeView
            }
        }

        SwipeView {
            id: swipeView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Component.onCompleted: {
                contentItem.highlightMoveDuration = 200
                swipeThroughScrollViews(swipeView)
                wheelPager.attach(swipeView)
            }

            onCurrentIndexChanged: {
                if (tabBar.currentIndex !== currentIndex) {
                    tabBar.setCurrentIndex(currentIndex)
                }
            }

            WelcomeStart {
                id: welcomeStart
                panel: container
            }

            Page {
                PageIndicator {
                    count: rtSwipeView.count
                    currentIndex: rtSwipeView.currentIndex
                    interactive: true
                    onCurrentIndexChanged: {
                        if (rtSwipeView.currentIndex !== currentIndex) {
                            rtSwipeView.setCurrentIndex(currentIndex)
                        }
                    }
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    rotation: 90
                    z: 2
                }

                SwipeView {
                    id: rtSwipeView
                    enabled: true
                    clip: true
                    currentIndex: 0
                    anchors.fill: parent
                    orientation: Qt.Vertical

                    Component.onCompleted: contentItem.highlightMoveDuration = 200

                    Page {
                        RtDataSetup {
                            anchors.fill: parent
                            dialogParent: container
                        }
                    }

                    Page {
                        StatPage {
                            anchors.fill: parent
                            anchors.margins: 20
                        }
                    }
                }
            }

            Page {
                Profiles {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    dialogParent: container
                }
            }

            Page {
                id: logViewPage
                // Loaded on the first visit, as the map takes a while. By url, so that the
                // rest of the panel does not wait for the map modules or depend on them.
                Loader {
                    anchors.fill: parent
                    active: logViewPage.SwipeView.isCurrentItem || status != Loader.Null
                    source: "qrc:/mobile/LogAnalysis.qml"
                }
            }
        }

    }

    Connections {
        target: VescIf
        function onPortConnectedChanged() {
            if (!VescIf.isPortConnected()) {
                swipeView.setCurrentIndex(0)
            }
        }
    }

    WheelPager {
        id: wheelPager
    }

    // A ScrollView takes a mouse drag that starts on one of its controls and then does not move, as it only
    // scrolls with the wheel and scroll bars on the desktop, so the pages would not swipe. Touch is not affected.
    function swipeThroughScrollViews(item) {
        if (item instanceof ScrollView) {
            item.contentItem.acceptedButtons = Qt.NoButton
        }

        for (var i = 0; i < item.children.length; i++) {
            swipeThroughScrollViews(item.children[i])
        }
    }

    property var hwUiObj: 0
    property var appUiObj: 0

    function updateHwAppUi () {
        if (hwUiObj != 0) {
            hwUiObj.destroy()
            hwUiObj = 0
        }

        if (appUiObj != 0) {
            appUiObj.destroy()
            appUiObj = 0
        }

        swipeView.interactive = true
        tabBar.visible = true
        tabBar.enabled = true
        header.visible = true
        headerRule.visible = true

        if (VescIf.isPortConnected() && VescIf.qmlHwLoaded()) {
            if (VescIf.getLastFwRxParams().qmlHwFullscreen) {
                swipeView.interactive = false
                tabBar.visible = false
                tabBar.enabled = false
                header.visible = false
                headerRule.visible = false
            }

            hwUiObj = Qt.createQmlObject(VescIf.qmlHw(), uiHw, "HwUi")
            swipeThroughScrollViews(hwUiObj)
            wheelPager.attach(hwUiObj)

            uiHwButton.text = "HwUi"
            if (hwUiObj.tabTitle) {
                uiHwButton.text = hwUiObj.tabTitle
            }

            uiHwButton.visible = true
            swipeView.insertItem(rep.count, uiHwPage)
            tabBar.insertItem(rep.count, uiHwButton)
            uiHwPage.visible = true
            swipeView.setCurrentIndex(rep.count)
        } else {
            uiHwPage.visible = false
            uiHwPage.parent = null
            uiHwButton.parent = null
        }

        if (VescIf.isPortConnected() && VescIf.qmlAppLoaded()) {
            if (VescIf.getLastFwRxParams().qmlAppFullscreen) {
                swipeView.interactive = false
                tabBar.visible = false
                tabBar.enabled = false
                header.visible = false
                headerRule.visible = false
            }

            appUiObj = Qt.createQmlObject(VescIf.qmlApp(), uiApp, "AppUi")
            swipeThroughScrollViews(appUiObj)
            wheelPager.attach(appUiObj)

            uiAppButton.text = "AppUi"
            if (appUiObj.tabTitle) {
                uiAppButton.text = appUiObj.tabTitle
            }

            uiAppButton.visible = true
            swipeView.insertItem(rep.count, uiAppPage)
            tabBar.insertItem(rep.count, uiAppButton)
            uiAppPage.visible = true
            swipeView.setCurrentIndex(rep.count)
        } else {
            uiAppPage.visible = false
            uiAppPage.parent = null
            uiAppButton.parent = null
        }
    }

    Connections {
        target: VescIf

        function onFwRxChanged(rx, limited) {
            updateHwAppUi()
        }

        function onQmlLoadDone() {
            // The QML on the Virtual ESC is from VESC Tool or uploaded by the user
            if (VescIf.askQmlLoad() && !VescIf.isVirtualConnected()) {
                qmlLoadDialog.open()
            } else {
                updateHwAppUi()
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

        parent: dialogArea
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

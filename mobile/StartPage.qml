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

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.3
import QtCore

import Vedder.vesc.vescinterface 1.0
import Vedder.vesc.bleuart 1.0
import Vedder.vesc.commands 1.0
import Vedder.vesc.utility 1.0

Item {
    id: topItem
    property Commands mCommands: VescIf.commands()
    property bool isHorizontal: width > height
    property string stepKey: ""
    property string wizardKey: ""
    property bool motorDone: false
    property bool inputDone: false
    property bool fwUpdateAvailable: false
    property string fwUpdateText: ""
    property int stepCount: fwUpdateAvailable ? 3 : 2
    property int doneCount: (motorDone ? 1 : 0) + (inputDone ? 1 : 0)
    property alias backupConfigDialog: backupConfigDialog
    property alias restoreConfigDialog: restoreConfigDialog
    property alias firmwareDialog: firmwareDialog
    property alias packageDialog: packageDialog
    signal requestOpenControls()
    signal requestSkipToLive()

    function updateSteps() {
        stepKey = "setupDone/" + Utility.uuid2Str(VescIf.getLastFwRxParams().uuid, false) + "/"
        motorDone = setupSettings.value(stepKey + "motor", false)
        inputDone = setupSettings.value(stepKey + "input", false)
    }

    function setStepDone(step) {
        setupSettings.setValue(wizardKey + step, true)
        updateSteps()
    }

    Component.onCompleted: {
        updateSteps()
    }

    component StepRow: ItemDelegate {
        id: stepRow
        property int number: 0
        property string subtitle: ""

        Layout.fillWidth: true

        contentItem: RowLayout {
            spacing: 12

            Rectangle {
                implicitWidth: 26
                implicitHeight: 26
                radius: 13
                color: stepRow.checked ? Qt.darker(Utility.getAppHexColor("green"), 3) :
                                         stepRow.highlighted ? Material.accent : Utility.getAppHexColor("normalBackground")
                border.color: stepRow.checked ? Utility.getAppHexColor("green") :
                                                stepRow.highlighted ? Material.accent : Utility.getAppHexColor("disabledText")

                Text {
                    anchors.centerIn: parent
                    text: stepRow.checked ? "✓" : stepRow.number
                    color: Utility.getAppHexColor(stepRow.checked ? "green" : stepRow.highlighted ? "white" : "normalText")
                    font.bold: true
                }
            }

            Image {
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                source: stepRow.icon.source
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Text {
                    text: stepRow.text
                    color: Utility.getAppHexColor(stepRow.checked || stepRow.highlighted ? "white" : "lightText")
                    font.bold: true
                }

                Text {
                    Layout.fillWidth: true
                    text: stepRow.subtitle
                    color: Utility.getAppHexColor("normalText")
                    font.pointSize: 10
                    elide: Text.ElideRight
                }
            }

            Text {
                visible: stepRow.checked
                text: "DONE · REDO"
                color: Utility.getAppHexColor("green")
                font.pointSize: 10
                font.bold: true
                font.letterSpacing: 1
            }

            Rectangle {
                visible: stepRow.highlighted
                implicitWidth: startText.implicitWidth + 24
                implicitHeight: 28
                radius: 2
                color: Material.accent

                Text {
                    id: startText
                    anchors.centerIn: parent
                    text: "START"
                    color: Utility.getAppHexColor("white")
                    font.pointSize: 10
                    font.bold: true
                    font.letterSpacing: 1
                }
            }

            Text {
                visible: !stepRow.checked && !stepRow.highlighted
                text: "›"
                color: Utility.getAppHexColor("disabledText")
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: parent.width
        contentHeight: grid.height + 30
        property int gridItemPreferredWidth: isHorizontal ? parent.width/2.0 - 15 : parent.width - 10
        clip: true

        GridLayout {
            id: grid
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            columns: isHorizontal ? 2 : 1
            anchors.topMargin: 15
            anchors.bottomMargin: 15
            columnSpacing: 10
            rowSpacing: 5

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 500
                Layout.alignment: Qt.AlignTop

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: bannerRow.implicitHeight
                    radius: 10
                    color: Utility.getAppHexColor("darkBackground")
                    border.color: Utility.getAppHexColor("lightestBackground")

                    RowLayout {
                        id: bannerRow
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        spacing: 12

                        Image {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            source: "qrc" + Utility.getThemePath() + "icons/motor_side.png"
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Text {
                                Layout.fillWidth: true
                                text: doneCount === 0 ? "Motor not set up yet" :
                                      doneCount < stepCount ? doneCount + " of " + stepCount + " done" :
                                      "All set"
                                color: Utility.getAppHexColor("white")
                                font.bold: true
                            }

                            Text {
                                text: doneCount === 0 ? stepCount + " steps get it running" :
                                      doneCount < stepCount ? (fwUpdateAvailable ? "Firmware" : motorDone ? "Input" : "Motor") + " next" :
                                      "Redo any step"
                                color: Utility.getAppHexColor("normalText")
                                font.pointSize: 10
                            }
                        }

                        Button {
                            flat: true
                            text: "SKIP TO LIVE ›"
                            font.pointSize: 10
                            Material.foreground: Utility.getAppHexColor("lightAccent")

                            onClicked: {
                                requestSkipToLive()
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    implicitHeight: stepColumn.implicitHeight

                    Rectangle {
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: inputStep.height / 2
                        x: inputStep.leftPadding + 12
                        width: 2
                        color: Utility.getAppHexColor("lightestBackground")
                    }

                    ColumnLayout {
                        id: stepColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 0

                        StepRow {
                            visible: fwUpdateAvailable
                            number: 1
                            text: "Firmware"
                            subtitle: fwUpdateText
                            icon.source: "qrc" + Utility.getThemePath() + "icons/Download-96.png"
                            highlighted: true

                            onClicked: {
                                firmwareDialog.open()
                            }
                        }

                        StepRow {
                            number: fwUpdateAvailable ? 2 : 1
                            text: "Motor"
                            subtitle: "detect & set battery limits"
                            icon.source: "qrc" + Utility.getThemePath() + "icons/motor.png"
                            checked: motorDone
                            highlighted: !fwUpdateAvailable && !motorDone

                            onClicked: {
                                if (!VescIf.isPortConnected()) {
                                    VescIf.emitMessageDialog("FOC Setup Wizard",
                                                             "You are not connected to the VESC. Please connect in order " +
                                                             "to run this wizard.", false, false)
                                } else {
                                    wizardKey = stepKey
                                    wizardFoc.openDialog()
                                }
                            }
                        }

                        StepRow {
                            id: inputStep
                            number: fwUpdateAvailable ? 3 : 2
                            text: "Input"
                            subtitle: "VESC Remote, PPM or ADC"
                            icon.source: "qrc" + Utility.getThemePath() + "icons/Wizard-96.png"
                            checked: inputDone
                            highlighted: !fwUpdateAvailable && motorDone && !inputDone

                            onClicked: {
                                if (!VescIf.isPortConnected()) {
                                    VescIf.emitMessageDialog("Input Setup Wizard",
                                                             "You are not connected to the VESC. Please connect in order " +
                                                             "to run this wizard.", false, false)
                                } else {
                                    // Something in the opendialog function causes a weird glitch, probably
                                    // caused by the eventloop in the can scan function. Disabling the button
                                    // seems to help. TODO: figure out what the actual problem is.
                                    wizardKey = stepKey
                                    enabled = false
                                    wizardInput.openDialog()
                                    enabled = true
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 500

                GroupBox {
                    title: qsTr("Optional")
                    Layout.fillWidth: true

                    GridLayout {
                        anchors.fill: parent
                        columns: 2
                        columnSpacing: 5
                        rowSpacing: 5

                        ImageButton {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredWidth: 500
                            Layout.preferredHeight: 60

                            buttonText: "Setup\nIMU"
                            imageSrc: "qrc" + Utility.getThemePath() + "icons/imu_off.png"

                            onClicked: {
                                if (!VescIf.isPortConnected()) {
                                    VescIf.emitMessageDialog("IMU Setup Wizard",
                                                             "You are not connected to the VESC. Please connect in order " +
                                                             "to run this wizard.", false, false)
                                } else {
                                    wizardIMU.openDialog()
                                }
                            }
                        }

                        ImageButton {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredWidth: 500
                            Layout.preferredHeight: 60

                            buttonText: "Bluetooth\nmodule"
                            imageSrc: "qrc" + Utility.getThemePath() + "icons/bluetooth.png"

                            onClicked: {
                                if (VescIf.getLastFwRxParams().nrfNameSupported &&
                                        VescIf.getLastFwRxParams().nrfPinSupported) {
                                    bleSetupDialog.openDialog()
                                } else {
                                    pairDialog.openDialog()
                                }
                            }
                        }

                        ImageButton {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredWidth: 500
                            Layout.preferredHeight: 60

                            buttonText: "Motor\ndirections"
                            imageSrc: "qrc" + Utility.getThemePath() + "icons/Process-96.png"

                            onClicked: {
                                if (!VescIf.isPortConnected()) {
                                    VescIf.emitMessageDialog("Directions",
                                                             "You are not connected to the VESC. Please connect in order " +
                                                             "to map directions.", false, false)
                                } else {
                                    enabled = false
                                    directionSetupDialog.open()
                                    directionSetup.scanCan()
                                    enabled = true
                                }
                            }
                        }

                        ImageButton {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredWidth: 500
                            Layout.preferredHeight: 60

                            buttonText: "Test motor\nby hand"
                            imageSrc: "qrc" + Utility.getThemePath() + "icons/Controller-96.png"

                            onClicked: {
                                requestOpenControls()
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: canRow.implicitHeight + 20
                    visible: canDevs.length > 0
                    radius: 10
                    color: Utility.getAppHexColor("darkBackground")

                    Canvas {
                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()
                            ctx.setLineDash([4, 4])
                            ctx.strokeStyle = Utility.getAppHexColor("disabledText")
                            ctx.roundedRect(0.5, 0.5, width - 1, height - 1, 10, 10)
                            ctx.stroke()
                        }
                    }

                    RowLayout {
                        id: canRow
                        anchors.fill: parent
                        anchors.margins: 10
                        anchors.leftMargin: 16
                        spacing: 12

                        Image {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            source: "qrc" + Utility.getThemePath() + "icons/can_off.png"
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Steps can include every motor controller on CAN (" + canDevs.length + " found)."
                            color: Utility.getAppHexColor("normalText")
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                GroupBox {
                    title: qsTr("Realtime Data Logging")
                    Layout.fillWidth: true

                    LogBox {
                        anchors.fill: parent
                        dialogParent: topItem
                    }
                }
            }
        }
    }

    SetupWizardFoc {
        id: wizardFoc
        dialogParent: mainSwipeView
        isHorizontal: mainIsHorizontal

        onWizardFinished: {
            setStepDone("motor")
        }
    }

    SetupWizardInput {
        id: wizardInput
        dialogParent: mainSwipeView
        isHorizontal: mainIsHorizontal

        onWizardFinished: {
            setStepDone("input")
        }
    }

    PairingDialog {
        id: pairDialog
        dialogParent: mainSwipeView
    }

    BleSetupDialog {
        id: bleSetupDialog
        dialogParent: mainSwipeView
    }

    SetupWizardIMU {
        id: wizardIMU
        dialogParent: mainSwipeView
    }

    Connections {
        target: VescIf
        function onFwRxChanged() {
            fwUpdateAvailable = false
            updateSteps()
        }

        function onFwUpdateAvailable(major, minor) {
            var params = VescIf.getLastFwRxParams()
            var current = params.major + "." + String(params.minor).padStart(2, "0")
            fwUpdateText = current + " → " + major + "." + String(minor).padStart(2, "0") + ", before detection"
            fwUpdateAvailable = true

            fwUpdateDialog.skipKey = Utility.uuid2Str(params.uuid, false) + "/" + current
            if (motorDone && !setupSettings.value("skippedFw/" + fwUpdateDialog.skipKey, false) &&
                    Date.now() > setupSettings.fwUpdateRemindAfter) {
                fwUpdateDialog.open()
            }
        }
    }

    Settings {
        id: setupSettings
        property double fwUpdateRemindAfter: 0
    }

    Dialog {
        id: directionSetupDialog
        title: "Direction Setup"
        standardButtons: Dialog.Close
        modal: true
        focus: true
        padding: 10

        width: parent.width - 10 - notchLeft - notchRight
        closePolicy: Popup.CloseOnEscape
        x: parent.width/2 - width/2
        y: parent.height / 2 - height / 2
        parent: mainSwipeView

        Overlay.modal: Rectangle {
            color: "#AA000000"
        }

        DirectionSetup {
            id: directionSetup
            anchors.fill: parent
            dialogParent: topItem
        }
    }
    Dialog {
        // HERE Backup
        id: backupConfigDialog
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        focus: true
        width: parent.width - 20 - notchLeft - notchRight
        closePolicy: Popup.CloseOnEscape
        title: "Backup configuration(s)"

        parent: mainSwipeView
        x: parent.width/2 - width/2
        y: topItem.y + topItem.height / 2 - height / 2

        Overlay.modal: Rectangle {
            color: "#AA000000"
        }

        Text {
            color: Utility.getAppHexColor("lightText")
            verticalAlignment: Text.AlignVCenter
            anchors.fill: parent
            wrapMode: Text.WordWrap
            text:
                "This will backup the configuration of the connected VESC, as well as for the VESCs " +
                "connected over CAN-bus. The configurations are stored by VESC UUID. If a backup for a " +
                "VESC UUID already exists it will be overwritten. Continue?"
        }

        onAccepted: {
            progDialog.open()
            workaroundTimerBackup.start()
        }
        Timer {
            id: workaroundTimerBackup
            interval: 0
            repeat: false
            running: false
            onTriggered: {
                VescIf.confStoreBackup(true, "")
                progDialog.close()
            }
        }
    }

    Dialog {
        id: restoreConfigDialog
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        focus: true
        width: parent.width - 20 - notchLeft - notchRight
        closePolicy: Popup.CloseOnEscape
        title: "Restore configuration backup(s)"

        parent: mainSwipeView
        x: parent.width/2 - width/2
        y: topItem.y + topItem.height / 2 - height / 2

        Overlay.modal: Rectangle {
            color: "#AA000000"
        }

        Text {
            color: Utility.getAppHexColor("lightText")
            verticalAlignment: Text.AlignVCenter
            anchors.fill: parent
            wrapMode: Text.WordWrap
            text:
                "This will restore the configuration of the connected VESC, as well as the VESCs connected over CAN bus " +
                "if a backup exists for their UUID in this instance of VESC Tool. If no backup is found for the UUID of " +
                "the VESCs nothing will be changed. Continue?"
        }

        onAccepted: {
            progDialog.open()
            workaroundTimerRestore.start()
        }
        Timer {
            id: workaroundTimerRestore
            interval: 0
            repeat: false
            running: false
            onTriggered: {
                VescIf.confRestoreBackup(true)
                progDialog.close()
            }
        }
    }

    Dialog {
        id: progDialog
        title: "Processing..."
        closePolicy: Popup.NoAutoClose
        modal: true
        focus: true

        width: parent.width - 20 - notchLeft - notchRight
        x: parent.width/2 - width/2
        y: parent.height / 2 - height / 2
        parent: mainSwipeView

        Overlay.modal: Rectangle {
            color: "#AA000000"
        }

        ProgressBar {
            anchors.fill: parent
            indeterminate: visible
        }
    }

    Dialog {
        id: firmwareDialog
        standardButtons: Dialog.Close
        closePolicy: Popup.CloseOnEscape
        modal: true
        focus: true

        width: parent.width - 20 - notchLeft - notchRight
        height: parent.height - 20
        x: parent.width/2 - width/2
        y: parent.height / 2 - height / 2
        parent: mainSwipeView

        Overlay.modal: Rectangle {
            color: "#AA000000"
        }

        contentItem: FwUpdate {
            id: fwUpdate
            anchors.fill: parent
            anchors.bottomMargin: 50
            anchors.topMargin: tabBar.implicitHeight
            pageIndicatorVisible: false
            swipeOrientation: Qt.Horizontal
            currentPage: tabBar.currentIndex
            isHorizontal: topItem.isHorizontal
        }

        header: Rectangle {
            color: Utility.getAppHexColor("lightText")
            height: tabBar.implicitHeight

            TabBar {
                id: tabBar
                currentIndex: fwUpdate.currentPage
                anchors.fill: parent
                clip: true

                background: Rectangle {
                    opacity: 1
                    color: Utility.getAppHexColor("lightestBackground")
                }

                property int buttonWidth: Math.max(120, tabBar.width / (rep.model.length))

                Repeater {
                    id: rep
                    model: ["Included", "Custom", "Bootloader", "Archive"]

                    TabButton {
                        text: modelData
                        width: tabBar.buttonWidth
                    }
                }

                Component.onCompleted: {
                    tabBar.setCurrentIndex(0)
                }
            }
        }
    }

    Dialog {
        id: packageDialog
        standardButtons: Dialog.Close
        closePolicy: Popup.CloseOnEscape
        modal: true
        focus: true

        width: parent.width - 20 - notchLeft - notchRight
        height: parent.height - 20
        x: parent.width / 2 - width / 2
        y: parent.height / 2 - height / 2
        parent: mainSwipeView

        Overlay.modal: Rectangle {
            color: "#AA000000"
        }

        contentItem: Packages {
            anchors.fill: parent
            anchors.margins: 5
            anchors.bottomMargin: 50
            dialogParent: mainSwipeView
        }
    }

    Dialog {
        id: fwUpdateDialog
        property string skipKey: ""
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        title: "Firmware Update Available"

        width: parent.width - 20 - notchLeft - notchRight
        x: (parent.width - width) / 2
        y: (parent.height - height + notchTop) / 2
        parent: mainSwipeView

        Overlay.modal: Rectangle {
            color: "#AA000000"
        }

        Text {
            anchors.fill: parent
            color: Utility.getAppHexColor("lightText")
            wrapMode: Text.WordWrap
            text: "The connected VESC has old, but compatible firmware. This is fine if your setup " +
                  "works properly. Only update when you have time to test everything afterwards."
        }

        footer: DialogButtonBox {
            Button {
                text: "Skip This Version"
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole
            }

            Button {
                text: "Later"
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: "Update"
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }

        onAccepted: {
            firmwareDialog.open()
        }

        onRejected: {
            setupSettings.fwUpdateRemindAfter = Date.now() + 7 * 24 * 3600 * 1000
        }

        onDiscarded: {
            setupSettings.setValue("skippedFw/" + skipKey, true)
            close()
        }
    }
}

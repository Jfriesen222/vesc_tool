/*
    Copyright 2018 - 2019 Benjamin Vedder	benjamin@vedder.se

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

import QtQuick 2.2
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Controls.Universal 2.12
import Qt5Compat.GraphicalEffects
import QtQuick.Controls.Material 2.2
import Vedder.vesc.utility 1.0

Item {
    property double value: 0
    property double outerRadius: width/2
    height: width
    property double labelStepSize: labelStep
    property double tickmarkStepSize: labelStep
    property double labelInset: outerRadius * 0.28 + outerRadius*0.06
    property double tickmarkInset: outerRadius*0.07
    property double minorTickmarkInset: outerRadius*0.07
    property double minimumValueAngle: minAngle
    property double maximumValueAngle: maxAngle
    property real angleRange: maximumValueAngle - minimumValueAngle
    property int tickmarkCount: Math.floor(tickmarkStepSize > 0 ? (maximumValue - minimumValue) / tickmarkStepSize + 1 : 0)
    property int minorTickmarkCount:4
    property int labelCount:tickmarkCount
    property double minimumValue: 0
    property double maximumValue: 100

    onMinimumValueChanged: valueTextModel.update()
    onMaximumValueChanged: valueTextModel.update()
    onTickmarkStepSizeChanged: valueTextModel.update()
    onLabelStepSizeChanged: valueTextModel.update()

    property string unitText: ""
    property string typeText: ""
    property string tickmarkSuffix: ""
    property double labelStep: 10
    property double tickmarkScale: 1
    property color nibColor: {nibColor = Utility.getAppHexColor("tertiary2")}
    property color traceColor: Qt.lighter(nibColor,1.5)
    property double maxAngle: 144
    property double minAngle: -144
    property double precision: 0
    property int isInverted: 1
    property bool tickmarksVisible: true
    property bool centerTextVisible: true

    function valueToAngle(value) {
        var normalised = (value - minimumValue) / (maximumValue - minimumValue);
        return (maximumValueAngle - minimumValueAngle) * normalised + minimumValueAngle;
    }
    function isCovered(value) {
        //var normalised = (value - minimumValue) / (maximumValue - minimumValue);
        return false
    }
    function d2r(degrees) {
        return degrees * (Math.PI / 180.0);
    }

    function toPixels(percentageOfOuterRadius) {
        return percentageOfOuterRadius * outerRadius;
    }

    Rectangle {
        id: gauge
        anchors.centerIn: parent
        width:outerRadius*2
        height:width
        z:0
        radius:outerRadius
        color: {color = Utility.isDarkMode() ? Utility.getAppHexColor("darkBackground") : Utility.getAppHexColor("normalBackground")}
        property double value: parent.value
        Behavior on value {
            NumberAnimation {
                easing.type: Easing.OutCirc
                duration: 100
            }
        }
        Text {
            id: speedLabel
            anchors.centerIn: parent
            visible: centerTextVisible
            text: gauge.value.toFixed(precision)
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: outerRadius * 0.3
            color: {color = Utility.getAppHexColor("lightText") }
            antialiasing: true
            font.family: "Roboto Mono"
        }
        Text {
            id: speedLabelUnit
            text: unitText
            visible: centerTextVisible
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: speedLabel.bottom
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: outerRadius * 0.12
            color: { color = Utility.getAppHexColor("lightText")}
            antialiasing: true
            font.family: "Roboto Mono"
            font.capitalization: Font.AllUppercase
        }

        Text {
            id: typeLabel
            text: typeText
            visible: centerTextVisible
            verticalAlignment: Text.AlignVCenter
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: speedLabel.top
            //anchors.bottomMargin: outerRadius * 0.1
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: outerRadius * 0.12
            color: {color = Utility.getAppHexColor("lightText")}
            antialiasing: true
            font.family: "Roboto Mono"
            font.capitalization: Font.AllUppercase
        }

        Item {
            x: outerRadius - width/2
            y: outerRadius * 0.05
            z:2
            height: outerRadius * 0.22
            width: outerRadius * 0.12
            transform: Rotation {
                id:needleTransform
                origin.x: outerRadius * 0.12/2
                origin.y: outerRadius*(1-0.05)
                angle: valueToAngle(gauge.value)
            }
            Canvas {
                id: pointerNib
                property double value: gauge.value
                property double gAngle: needleTransform.angle
                onValueChanged: requestPaint()
                anchors.fill:parent
                onPaint:{
                    var ctx = getContext("2d");

                    // the triangle
                    ctx.beginPath();
                    ctx.moveTo(parent.width/2, 0);
                    ctx.lineTo(parent.width, parent.height*0.015);
                    ctx.quadraticCurveTo(parent.width*0.7, parent.height/4, 0.6*parent.width , 0.9*parent.height);
                    ctx.quadraticCurveTo(parent.width*0.6, parent.height, parent.width/2, parent.height);
                    ctx.closePath();

                    var gradient = ctx.createLinearGradient(parent.width,parent.height*0.02,parent.width/2,0);
                    gradient.addColorStop(0, Qt.darker(nibColor,1.0 + 0.5* Math.sin(gAngle*3.14/180 - 3.14*0.2)));
                    gradient.addColorStop(0.80, Qt.darker(nibColor,1.0 + 0.5* Math.sin(gAngle*3.14/180 + 3.14*0.0)));
                    gradient.addColorStop(0.92, '#ffffff');
                    gradient.addColorStop(1, '#ffffff');


                    // the fill color
                    ctx.fillStyle = gradient;
                    ctx.fill();

                    // the triangle
                    ctx.beginPath();
                    ctx.moveTo(parent.width/2, 0);
                    ctx.lineTo(0, parent.height*0.015);
                    ctx.quadraticCurveTo(parent.width*0.3, parent.height/4, 0.4*parent.width, 0.9*parent.height);
                    ctx.quadraticCurveTo(parent.width*0.4, parent.height, parent.width/2, parent.height);
                    ctx.closePath();

                    var gradient2 = ctx.createLinearGradient(0,parent.height*0.02,parent.width/2,0);
                    gradient2.addColorStop(0, Qt.darker(nibColor,1.0 + 0.5* Math.sin(gAngle*3.14/180 + 3.14*0.8)));
                    gradient2.addColorStop(0.80, Qt.darker(nibColor,1.0 + 0.5* Math.sin(gAngle*3.14/180 + 3.14*1.0)));
                    gradient2.addColorStop(0.92, '#ffffff');
                    gradient2.addColorStop(1, '#ffffff');
                    // the fill color
                    ctx.fillStyle = gradient2;
                    ctx.fill();
                }
            }
        }
        Canvas {
            id:foregroundpaint
            anchors.fill: parent
            opacity: 1
            z:3
            property double value: gauge.value
            onValueChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                //create radial glow around outside of gauge
                var gradient1 = ctx.createRadialGradient(outerRadius, outerRadius, outerRadius - outerRadius * 0.13, outerRadius, outerRadius, outerRadius- outerRadius * 0.05);
                gradient1.addColorStop(0, '#00000000');
                gradient1.addColorStop(1, traceColor);
                ctx.beginPath();
                ctx.strokeStyle = gradient1
                ctx.lineWidth = outerRadius * 0.2
                ctx.arc(outerRadius,
                        outerRadius,
                        outerRadius - outerRadius * 0.1,
                        d2r(valueToAngle(0.0) - 90),
                        d2r(valueToAngle(gauge.value) - 90),
                        gauge.value*isInverted < 0);
                ctx.stroke();


                //create outer gauge metal bezel effect
                ctx.beginPath();
                var gradient2 = ctx.createLinearGradient(parent.width,0,0 ,parent.height);
                // Add three color stops
                gradient2.addColorStop(1, Utility.getAppHexColor("lightestBackground"));
                gradient2.addColorStop(0.7, Utility.getAppHexColor("darkBackground"));
                gradient2.addColorStop(0.1, Utility.getAppHexColor("lightestBackground"));
                ctx.strokeStyle = gradient2;
                ctx.lineWidth = outerRadius*0.03
                ctx.arc(outerRadius,
                        outerRadius,
                        outerRadius*0.985  ,
                        0, 2 * Math.PI);
                ctx.stroke();
                ctx.beginPath();
                var gradient3 = ctx.createLinearGradient(parent.width,0,0 ,parent.height);
                // Add three color stops
                gradient3.addColorStop(1, Utility.getAppHexColor("darkBackground"));
                gradient3.addColorStop(0.8, Utility.getAppHexColor("lightestBackground"));
                gradient3.addColorStop(0, Utility.getAppHexColor("darkBackground"));
                ctx.strokeStyle = gradient3;
                ctx.lineWidth = outerRadius*0.03
                ctx.arc(outerRadius,
                        outerRadius,
                        0.96*outerRadius ,
                        0, 2 * Math.PI);
                ctx.stroke();

                //small black inset line on inner gauge to give seam
                ctx.beginPath();
                ctx.strokeStyle = 'black';
                ctx.lineWidth = outerRadius*0.002
                ctx.arc(outerRadius,
                        outerRadius,
                        outerRadius*0.942  ,
                        0, 2 * Math.PI);
                ctx.stroke();
            }
        }

        //white circle to overlay glass
        RadialGradient {
            id: glassEffect1
            x: outerRadius - width/2
            y: outerRadius - height/2
            height: 2*outerRadius*0.96
            width: height
            visible: false

            gradient: Gradient {
                GradientStop { position: 1.0; color: "transparent" }
                GradientStop { position: 0.5; color: "transparent" }
                GradientStop { position: 0.495; color: "white" } // Just a part of the canvas
            }
        }
        //Cutout circle for part where glare isn't
        RadialGradient{
            id: glassEffect2
            horizontalOffset: outerRadius*0.8 - width/2
            horizontalRadius: outerRadius*1.9
            verticalOffset: outerRadius*2.3 - height/2
            verticalRadius: outerRadius*1.9
            anchors.fill: glassEffect1
            visible:  false
            z: glassEffect1.z + 1
            gradient: Gradient {
                GradientStop { position: 1.2; color: "#22000000" }
                GradientStop { position: 1.1; color: "#33000000" }
                GradientStop { position: 1.05; color: "#22000000" }
                GradientStop { position: 1; color: "#11000000" }
                GradientStop { position: 0.98; color: "transparent" } // Just a part of the canvas
            }
        }
        //Subtract both
        OpacityMask {
            id: glassEffect3
            anchors.fill: glassEffect1
            source: glassEffect1
            maskSource: glassEffect2
            invert: false
            visible: true
        }

        function isCovered(value) {
            var res = false
            if (gauge.value > 0) {
                if (value <= gauge.value && value >= 0) {
                    res = true
                }
            } else {
                if (value >= gauge.value && value <= 0) {
                    res = true
                }
            }
            return res
        }
        Component{
            id: tickmarkLabel
            Text {
                font.pixelSize: outerRadius * 0.12
                text: parseFloat(value * tickmarkScale).toFixed(0) + tickmarkSuffix
                property color coveredColor: {coveredColor = Utility.getAppHexColor("lightText")}
                property color uncoveredColor: {uncoveredColor = Utility.getAppHexColor("disabledText")}
                color: isCovered(value) ? coveredColor : uncoveredColor
                antialiasing: true
                font.family: "Roboto Mono"
            }
        }
        Component{
            id: tickmark
            Rectangle {
                implicitWidth: outerRadius * 0.02
                implicitHeight: outerRadius * 0.1
                antialiasing: true
                smooth: true
                property color coveredColor: {coveredColor = Utility.getAppHexColor("lightText")}
                property color uncoveredColor: {uncoveredColor = Utility.getAppHexColor("disabledText")}
                color: isCovered(value) ? coveredColor : uncoveredColor
            }
        }
        Component{
            id: minorTickmark
            Rectangle {
                implicitWidth: outerRadius * 0.015
                implicitHeight: outerRadius * 0.07
                antialiasing: true
                smooth: true
                property color coveredColor: {coveredColor = Utility.getAppHexColor("lightText")}
                property color uncoveredColor: {uncoveredColor = Utility.getAppHexColor("disabledText")}
                color: isCovered(value) ? coveredColor : uncoveredColor
            }
        }
        function rangeUsed(count, stepSize) {
            return (((count - 1) * stepSize) / (maximumValue - minimumValue)) * angleRange;
        }

        property real tickmarkSectionSize: rangeUsed(tickmarkCount, tickmarkStepSize) / (tickmarkCount - 1)
        property real tickmarkSectionValue: (maximumValue - minimumValue) / (tickmarkCount - 1)
        property real minorTickmarkSectionSize: tickmarkSectionSize / (minorTickmarkCount + 1)
        property real minorTickmarkSectionValue: tickmarkSectionValue / (minorTickmarkCount + 1)
        property int totalMinorTickmarkCount: {
            // The size of each section within two major tickmarks, expressed as a percentage.
            var minorSectionPercentage = 1 / (minorTickmarkCount + 1);
            // The amount of major tickmarks not able to be displayed; will be 0 if they all fit.
            var tickmarksNotDisplayed = tickmarkCount - tickmarkCount;
            var count = minorTickmarkCount * (tickmarkCount - 1);
            // We'll try to display as many minor tickmarks as we can to fill up the space.
            count + tickmarksNotDisplayed / minorSectionPercentage;
        }
        property real labelSectionSize: rangeUsed(labelCount, labelStepSize) / (labelCount - 1)
        /*!
                    Returns the angle of \a marker (in the range 0 ... n - 1, where n
                    is the amount of markers) on the gauge where sections are of size
                    tickmarkSectionSize.
                */
        function tickmarkAngleFromIndex(tickmarkIndex) {
            return tickmarkIndex * tickmarkSectionSize + minimumValueAngle;
        }

        function labelAngleFromIndex(labelIndex) {
            return labelIndex * labelSectionSize + minimumValueAngle;
        }

        function minorTickmarkAngleFromIndex(minorTickmarkIndex) {
            var baseAngle = tickmarkAngleFromIndex(Math.floor(minorTickmarkIndex / minorTickmarkCount));
            // + minorTickmarkSectionSize because we don't want the first minor tickmark to start on top of its "parent" tickmark.
            var relativeMinorAngle = (minorTickmarkIndex % minorTickmarkCount * minorTickmarkSectionSize) + minorTickmarkSectionSize;
            return baseAngle + relativeMinorAngle;
        }

        function tickmarkValueFromIndex(majorIndex) {
            return (majorIndex * tickmarkSectionValue) + minimumValue;
        }

        function tickmarkValueFromMinorIndex(minorIndex) {
            var majorIndex = Math.floor(minorIndex / minorTickmarkCount);
            var relativeMinorIndex = minorIndex % minorTickmarkCount;
            return tickmarkValueFromIndex(majorIndex) + ((relativeMinorIndex * minorTickmarkSectionValue) + minorTickmarkSectionValue);
        }
        Repeater {
            id: tickmarkRepeater
            anchors.fill: parent
            model: tickmarkCount
            delegate: Loader {
                id: tickmarkLoader
                objectName: "tickmark" + styleData.index
                x: tickmarkRepeater.width / 2
                y: tickmarkRepeater.height / 2
                z:0
                transform: [
                    Translate {
                        y: -outerRadius + tickmarkInset
                    },
                    Rotation {
                        angle: gauge.tickmarkAngleFromIndex(styleData.index) - __tickmarkWidthAsAngle / 2
                    }
                ]

                sourceComponent: tickmark

                property int __index: index
                property QtObject styleData: QtObject {
                    property alias index: tickmarkLoader.__index
                    property real value: gauge.tickmarkValueFromIndex(index)
                }

                property real __tickmarkWidthAsAngle: 180/3.1416*((width / (outerRadius)))
            }
        }
        Repeater {
            id: minorRepeater
            anchors.fill: parent
            model: gauge.totalMinorTickmarkCount
            delegate: Loader {
                id: minorTickmarkLoader
                objectName: "minorTickmark" + styleData.index
                x: minorRepeater.width / 2
                y: minorRepeater.height / 2
                transform: [
                    Translate {
                        y: -outerRadius + minorTickmarkInset
                    },
                    Rotation {
                        angle: gauge.minorTickmarkAngleFromIndex(styleData.index) - __minorTickmarkWidthAsAngle / 2
                    }
                ]
                sourceComponent: minorTickmark
                property int __index: index
                property QtObject styleData: QtObject {
                    property alias index: minorTickmarkLoader.__index
                    property real value: gauge.tickmarkValueFromMinorIndex(index)
                }
                property real __minorTickmarkWidthAsAngle: 180 / 3.14 * ((width / (outerRadius)))
            }
        }
        Repeater {
            id: labelItemRepeater
            anchors.fill: parent
            Component.onCompleted: valueTextModel.update();
            model: ListModel {
                id: valueTextModel
                function update() {
                    if (labelStepSize === 0) {
                        return;
                    }

                    // Make bigger if it's too small and vice versa.
                    // +1 because we want to show 11 values, with, for example: 0, 10, 20... 100.
                    var difference = labelCount - count;
                    if (difference > 0) {
                        for (; difference > 0; --difference) {
                            append({ value: 0 });
                        }
                    } else if (difference < 0) {
                        for (; difference < 0; ++difference) {
                            remove(count - 1);
                        }
                    }

                    var index = 0;
                    for (var value = minimumValue;
                         value <= maximumValue && index < count;
                         value += labelStepSize, ++index) {
                        setProperty(index, "value", value);
                    }
                }
            }
            delegate: Loader {
                id: tickmarkLabelDelegateLoader
                sourceComponent: tickmarkLabel
                x: (outerRadius - width / 2) + ((outerRadius - labelInset) * Math.cos((d2r(gauge.labelAngleFromIndex(index) - 90))))
                y: (outerRadius - height / 2) + ((outerRadius - labelInset) * Math.sin((d2r(gauge.labelAngleFromIndex(index) - 90))))
                property var value: index != -1 ? labelItemRepeater.model.get(index).value : 0
            }
        }
    }
}


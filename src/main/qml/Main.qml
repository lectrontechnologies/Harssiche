import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 900
    height: 520
    title: "Nomad Animator"
    color: "#171717"

    property string activeTool: nomadEngine.activeTool
    property int currentFrame: nomadEngine.currentFrame
    property string statusText: "Motor nativo inicializado"
    Connections { target: nomadEngine; function onActiveToolChanged() { root.activeTool = nomadEngine.activeTool } function onStatusMessage(message) { root.statusText = message } function onTimelineChanged() { root.currentFrame = nomadEngine.currentFrame } }
    property var tools: [
        { label: "Select", glyph: "▧" },
        { label: "Move", glyph: "✥" },
        { label: "Rotate", glyph: "⟳" },
        { label: "Scale", glyph: "⤢" }
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            color: "#222222"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8
                ToolButton { text: "☰"; font.pixelSize: 24; onClicked: menu.open() }
                Rectangle { width: 1; Layout.fillHeight: true; Layout.margins: 10; color: "#454545" }
                Repeater {
                    model: root.tools
                    delegate: ToolButton {
                        required property var modelData
                        text: modelData.glyph
                        font.pixelSize: 23
                        highlighted: root.activeTool === modelData.label
                        onClicked: nomadEngine.setTool(modelData.label)
                        ToolTip.visible: hovered
                        ToolTip.text: modelData.label
                    }
                }
                Item { Layout.fillWidth: true }
                ToolButton { text: "↶"; font.pixelSize: 24; onClicked: nomadEngine.undo(); ToolTip.text: "Undo"; ToolTip.visible: hovered }
                ToolButton { text: "↷"; font.pixelSize: 24; onClicked: nomadEngine.redo(); ToolTip.text: "Redo"; ToolTip.visible: hovered }
                ToolButton { text: "＋"; font.pixelSize: 24; onClicked: nomadEngine.addObject("Cube"); ToolTip.text: "Add object"; ToolTip.visible: hovered }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            NomadViewport {
                id: viewport
                Layout.fillWidth: true
                Layout.fillHeight: true
                focus: true
                Timer { interval: 16; running: true; repeat: true; onTriggered: viewport.requestUpdate() }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    property point pressPoint: Qt.point(0, 0)
                    property point lastPoint: Qt.point(0, 0)
                    property bool dragged: false
                    property bool transforming: false
                    onPressed: function(mouse) {
                        pressPoint = Qt.point(mouse.x, mouse.y)
                        lastPoint = pressPoint
                        dragged = false
                        transforming = nomadEngine.pointerDown(mouse.x / width, mouse.y / height)
                    }
                    onPositionChanged: function(mouse) {
                        if (!(mouse.buttons & Qt.LeftButton)) return
                        var dx = mouse.x - lastPoint.x
                        var dy = mouse.y - lastPoint.y
                        if (Math.abs(mouse.x - pressPoint.x) + Math.abs(mouse.y - pressPoint.y) > 8) dragged = true
                        if (dragged) nomadEngine.pointerMove(dx, dy)
                        lastPoint = Qt.point(mouse.x, mouse.y)
                    }
                    onReleased: function(mouse) {
                        nomadEngine.pointerUp()
                        if (!dragged && !transforming) nomadEngine.pickAt(mouse.x / width, mouse.y / height)
                    }
                }
            }
            Rectangle {
                Layout.preferredWidth: 190; Layout.fillHeight: true; color: "#292929"
                ColumnLayout { anchors.fill: parent; anchors.margins: 12; spacing: 12
                    Label { text: "Scene"; color: "#eeeeee"; font.bold: true }
                    Button { text: "＋  Add cube"; Layout.fillWidth: true; onClicked: nomadEngine.addObject("Cube") }
                    Button { text: "＋  Add sphere"; Layout.fillWidth: true; onClicked: nomadEngine.addObject("Sphere") }
                    Button { text: "−  Remove selected"; Layout.fillWidth: true; onClicked: nomadEngine.removeSelected() }
                    Repeater { model: nomadEngine.sceneObjects; delegate: Label { required property string modelData; text: modelData; color: "#cccccc" } }
                    Item { Layout.fillHeight: true }
                    Label { text: "Tool: " + root.activeTool; color: "#999999"; wrapMode: Text.Wrap }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 92; color: "#242424"
            ColumnLayout { anchors.fill: parent; anchors.margins: 10; spacing: 7
                RowLayout { Layout.fillWidth: true
                    Label { text: "TIMELINE"; color: "#eeeeee"; font.bold: true }
                    Item { Layout.fillWidth: true }
                    ToolButton { text: "|◀"; onClicked: nomadEngine.setFrame(1) }
                    ToolButton { text: nomadEngine.playing ? "Ⅱ" : "▶"; onClicked: nomadEngine.togglePlayback() }
                    ToolButton { text: "▶|"; onClicked: nomadEngine.setFrame(250) }
                    Label { text: "Frame " + root.currentFrame; color: "#bbbbbb" }
                }
                Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; color: "#303030"; radius: 4
                    Row { anchors.fill: parent; anchors.margins: 8; spacing: 20
                        Repeater { model: 12; delegate: Rectangle { required property int index; width: 5; height: 5; radius: 3; color: index % 3 === 0 ? "#e8a84a" : "#656565"; y: index % 2 === 0 ? 5 : 20 } }
                    }
                }
            }
        }
    }
    Menu { id: menu; MenuItem { text: "New project" }; MenuItem { text: "Open" }; MenuItem { text: "Save" }; MenuItem { text: "Settings" } }
}

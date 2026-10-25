import QtQuick

// Controle segmentado (ex.: 480p | 720p | 1080p, Day | Night).
Rectangle {
    id: root
    property real s: 1
    property var labels: []
    property int index: -1
    signal picked(int i)

    color: "#222222"
    radius: 4 * s
    height: 24 * s

    Row {
        anchors.fill: parent
        anchors.margins: 2 * root.s
        spacing: 0
        Repeater {
            model: root.labels
            Rectangle {
                required property var modelData
                required property int index
                width: (root.width - 4 * root.s) / Math.max(1, root.labels.length)
                height: parent.height
                radius: 3 * root.s
                color: root.index === index ? "#4a4a4a" : (ma.pressed ? "#333333" : "transparent")
                Text {
                    anchors.centerIn: parent
                    text: parent.modelData
                    color: "#eeeeee"
                    font.pixelSize: 13 * root.s
                }
                MouseArea {
                    id: ma
                    anchors.fill: parent
                    onClicked: root.picked(parent.index)
                }
            }
        }
    }
}

import QtQuick

Rectangle {
    id: root
    property string icon: ""
    property bool active: false
    property bool dimmed: false
    signal clicked

    implicitWidth: 52
    implicitHeight: 52
    color: active ? "#4a4a4a" : "#282828"

    Icon {
        anchors.centerIn: parent
        name: root.icon
        opacity: root.dimmed ? 0.35 : 1.0
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}

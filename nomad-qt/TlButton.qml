import QtQuick

// Botao cinza em degrade, como os da timeline do Maya.
Rectangle {
    id: root
    property real s: 1
    property var view
    property string icon: ""
    property bool active: false
    property real iconSize: 22 * s
    signal clicked
    signal pressAndHold

    implicitWidth: 27.3 * s
    implicitHeight: 27 * s
    border.color: "#222222"
    border.width: Math.max(1, s * 0.6)
    gradient: Gradient {
        GradientStop { position: 0.0; color: root.active ? "#86a3c4" : (ma.pressed ? "#4a4a4a" : "#636363") }
        GradientStop { position: 1.0; color: root.active ? "#5e7d9f" : (ma.pressed ? "#323232" : "#3d3d3d") }
    }

    TlIcon {
        anchors.centerIn: parent
        name: root.icon
        size: root.iconSize
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        containmentMask: HitMask { target: ma; view: root.view }
        onClicked: root.clicked()
        onPressAndHold: root.pressAndHold()
    }
}

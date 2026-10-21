import QtQuick

// Area de toque de um botao das barras do topo (o icone e desenhado a parte).
Item {
    id: root
    property real s: 1
    signal clicked

    implicitWidth: 46 * s
    implicitHeight: 45 * s

    Rectangle {   // feedback ao tocar
        visible: ma.pressed
        anchors.centerIn: parent
        width: 36 * root.s
        height: 36 * root.s
        radius: 8 * root.s
        color: "white"
        opacity: 0.14
    }
    MouseArea {
        id: ma
        anchors.fill: parent
        onClicked: root.clicked()
    }
}

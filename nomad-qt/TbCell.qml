import QtQuick

// Celula de botao das barras do topo: icone branco sobre o fundo escuro.
Item {
    id: root
    property real s: 1
    property string icon: ""
    property bool active: false
    property bool dimmed: false
    property real iconSize: 28 * s
    signal clicked

    implicitWidth: 46 * s
    implicitHeight: 45 * s

    Rectangle {   // ferramenta ativa
        visible: root.active
        anchors.centerIn: parent
        width: 36 * root.s
        height: 36 * root.s
        radius: 8 * root.s
        color: "#4773b3"
    }
    Rectangle {   // feedback ao tocar
        visible: ma.pressed
        anchors.centerIn: parent
        width: 36 * root.s
        height: 36 * root.s
        radius: 8 * root.s
        color: "white"
        opacity: 0.14
    }
    TlIcon {
        anchors.centerIn: parent
        name: root.icon
        size: root.iconSize
        color: "#f5f5f5"
        opacity: root.dimmed ? 0.35 : 1.0
    }
    MouseArea {
        id: ma
        anchors.fill: parent
        onClicked: root.clicked()
    }
}

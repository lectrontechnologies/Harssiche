import QtQuick
import Nomad

Window {
    id: win
    visible: true
    visibility: Window.FullScreen
    title: "Nomad Animator"
    color: "black"

    // O motor desenha o viewport e a interface atual. Os paineis em QML
    // entram aqui por cima, um de cada vez.
    EngineView {
        id: engine
        anchors.fill: parent
        focus: true
    }

    Component.onCompleted: engine.forceActiveFocus()
}

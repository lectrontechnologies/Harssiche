import QtQuick
import Nomad

Window {
    id: win
    visible: true
    visibility: Window.FullScreen
    title: "Nomad Animator"
    color: "black"

    // O motor desenha o viewport e a interface que ainda nao foi migrada.
    EngineView {
        id: engineView
        anchors.fill: parent
        focus: true
    }

    // Timeline em QML (substitui a que o motor desenhava)
    Timeline {
        anchors.fill: parent
        view: engineView
        visible: engineView.timelineVisible
    }

    // Botoes do conceito (ferramentas, cubo, desfazer/refazer, menu)
    TopBars {
        anchors.fill: parent
        view: engineView
    }

    Component.onCompleted: engineView.forceActiveFocus()
}

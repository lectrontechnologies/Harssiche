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
        rightInset: outliner.insetPx
        visible: engineView.timelineVisible
    }

    // Painel Scene (outliner com parentesco)
    SceneOutliner {
        id: outliner
        anchors.fill: parent
        view: engineView
        visible: engineView.timelineVisible
    }

    // Botoes do conceito (ferramentas, cubo, desfazer/refazer, menu)
    TopBars {
        anchors.fill: parent
        view: engineView
    }

    // Tela de render (aberta pelo menu > Renderizar)
    RenderDialog {
        anchors.fill: parent
        view: engineView
    }

    // o motor precisa saber quanto espaco o Outliner toma a direita (para o painel de transformacao)
    function syncInset() { engineView.command(27, win.width > 0 ? outliner.insetPx / win.width : 0) }
    Connections {
        target: outliner
        function onInsetPxChanged() { win.syncInset() }
    }

    Component.onCompleted: { engineView.forceActiveFocus(); syncInset() }
}

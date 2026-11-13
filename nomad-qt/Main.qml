import QtQuick
import Nomad

Window {
    id: win
    // a janela e mostrada pelo main.cpp (depois de criar a instancia Vulkan)
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
        topInset: topBars.topH + topBars.handleH + 8 * topBars.k
        visible: engineView.timelineVisible
    }

    // Botoes do conceito (ferramentas, cubo, desfazer/refazer, menu)
    TopBars {
        id: topBars
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


    // Aviso se o Vulkan nao estiver disponivel (o app usa so Vulkan) ou se o renderizador falhar.
    Rectangle {
        id: backendChip
        readonly property real k: win.height > 0 ? win.height / 1080 : 1
        visible: engineView.vkFailed || engineView.vkInfo.indexOf("falha") >= 0
        x: 109 * k
        y: 767 * k - height - 10 * k
        width: chipA.implicitWidth + 24
        height: chipA.implicitHeight + 16
        radius: 6
        color: "#d0101010"
        border.color: "#505050"
        Text {
            id: chipA
            anchors.centerIn: parent
            color: "#ff9a8a"
            font.pixelSize: 13
            text: engineView.vkFailed ? "Vulkan indisponível neste aparelho" : "Vulkan: " + engineView.vkInfo.split(" | ").filter(function (p) { return p.indexOf("falha") >= 0 }).join(" | ")
        }
    }

    Component.onCompleted: { engineView.forceActiveFocus(); syncInset() }
}

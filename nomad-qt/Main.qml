import QtQuick
import Nomad

Window {
    id: win
    // a janela e mostrada pelo main.cpp (depois de escolher OpenGL ou Vulkan)
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


    // Diagnostico/escolha do backend grafico (temporario, durante a migracao para Vulkan)
    Rectangle {
        id: backendChip
        anchors.horizontalCenter: parent.horizontalCenter
        y: 4
        width: chipRow.implicitWidth + 24
        height: 30
        radius: 6
        color: "#d0101010"
        border.color: "#505050"
        Row {
  id: chipRow
  anchors.centerIn: parent
  spacing: 10
  Text {
      color: engineView.vkFailed ? "#ff9a8a" : "#dddddd"
      font.pixelSize: 13
      text: engineView.vkFailed ? "Vulkan falhou, voltou ao OpenGL"
          : engineView.backend === "vulkan" ? "Vulkan " + engineView.vkInfo : "OpenGL ES"
  }
  Text {
      color: "#7fd0ff"
      font.pixelSize: 13
      text: engineView.savedBackend !== engineView.backend ? "reabra o app"
          : engineView.backend === "vulkan" ? "[usar OpenGL]" : "[testar Vulkan]"
  }
        }
        MouseArea {
  anchors.fill: parent
  onClicked: engineView.setBackend(engineView.savedBackend === "vulkan" ? "opengl" : "vulkan")
        }
    }

    Component.onCompleted: { engineView.forceActiveFocus(); syncInset() }
}

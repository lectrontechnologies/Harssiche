import QtQuick
import QtQuick.Controls.Basic
import Nomad

ApplicationWindow {
    id: win
    visible: true
    width: 1280
    height: 720
    title: "Nomad Animator"
    color: "#8c8c91"

    // Viewport 3D (OpenGL ES, matcap)
    Viewport {
        id: vp
        anchors.fill: parent
    }

    // Ferramentas de transformacao (canto superior esquerdo)
    Rectangle {
        id: tools
        x: 8
        y: 8
        width: toolRow.width
        height: toolRow.height
        color: "#282828"

        Row {
            id: toolRow
            IconButton { icon: "move"; active: vp.tool === 0; onClicked: vp.tool = 0 }
            IconButton { icon: "rotate"; active: vp.tool === 1; onClicked: vp.tool = 1 }
            IconButton { icon: "scale"; active: vp.tool === 2; onClicked: vp.tool = 2 }
        }
    }

    // Botao de primitivas (abaixo das ferramentas)
    IconButton {
        id: cubeBtn
        x: 8
        y: tools.y + tools.height
        icon: "cube"
        onClicked: primMenu.popup(cubeBtn, cubeBtn.width + 4, 0)
    }

    // Desfazer / refazer / menu (canto superior direito)
    Rectangle {
        id: topRight
        x: win.width - width - 8
        y: 8
        width: trRow.width
        height: trRow.height
        color: "#282828"

        Row {
            id: trRow
            IconButton { icon: "undo"; dimmed: !vp.canUndo; onClicked: vp.undo() }
            IconButton { icon: "redo"; dimmed: !vp.canRedo; onClicked: vp.redo() }
            IconButton { id: menuBtn; icon: "menu"; onClicked: mainMenu.popup(menuBtn, menuBtn.width - 200, menuBtn.height + 4) }
        }
    }

    NomadMenu {
        id: primMenu
        DarkItem { text: "Cubo"; onTriggered: vp.addPrimitive(0) }
        DarkItem { text: "Esfera"; onTriggered: vp.addPrimitive(1) }
        DarkItem { text: "Cilindro"; onTriggered: vp.addPrimitive(2) }
        DarkItem { text: "Cone"; onTriggered: vp.addPrimitive(3) }
        DarkItem { text: "Toroide"; onTriggered: vp.addPrimitive(4) }
        DarkItem { text: "Plano"; onTriggered: vp.addPrimitive(5) }
    }

    NomadMenu {
        id: mainMenu
        DarkItem { text: "Excluir objeto"; onTriggered: vp.deleteSelected() }
        DarkItem { text: "Nova cena"; onTriggered: vp.newScene() }
    }
}

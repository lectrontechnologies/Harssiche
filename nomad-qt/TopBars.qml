import QtQuick
import QtQuick.Shapes

// Barras de botoes do conceito: ferramentas + cubo (canto superior esquerdo)
// e desfazer / refazer / menu (canto superior direito).
// Medidas partem de um desenho de 800x360 e escalam com a altura da tela.
Item {
    id: bars
    required property var view   // EngineView

    readonly property real s: height > 0 ? height / 360 : 1
    property bool addOpen: false

    function pickTool(k) {
        // tocar na ferramenta ativa volta para "selecionar" (ferramenta 3)
        view.command(11, view.toolIndex === k ? 6 : 3 + k)
    }

    // fecha o menu de adicionar ao tocar fora
    MouseArea {
        anchors.fill: parent
        enabled: bars.addOpen
        onClicked: bars.addOpen = false
    }

    // ---------------- ferramentas ----------------
    Shape {
        width: 138
        height: 45
        scale: bars.s
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: "#282828"
            strokeColor: "transparent"
            PathSvg { path: "M0 0 H138 V35 Q138 45 128 45 H0 Z" }
        }
    }
    Row {
        x: 0
        y: 0
        TbCell { s: bars.s; icon: "tmove"; active: bars.view.toolIndex === 0; onClicked: bars.pickTool(0) }
        TbCell { s: bars.s; icon: "trotate"; active: bars.view.toolIndex === 1; onClicked: bars.pickTool(1) }
        TbCell { s: bars.s; icon: "tscale"; active: bars.view.toolIndex === 2; onClicked: bars.pickTool(2) }
    }

    // ---------------- botao de cubo (adicionar) ----------------
    Shape {
        y: 44.5 * bars.s
        width: 52
        height: 51.5
        scale: bars.s
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: "#282828"
            strokeColor: "transparent"
            PathSvg { path: "M0 0 H52 V46.5 Q52 51.5 47 51.5 H0 Z" }
        }
    }
    TbCell {
        x: 0
        y: 45 * bars.s
        width: 52 * bars.s
        height: 51 * bars.s
        s: bars.s
        icon: "tcube"
        active: bars.addOpen
        onClicked: bars.addOpen = !bars.addOpen
    }

    // menu de objetos
    Rectangle {
        visible: bars.addOpen
        x: 58 * bars.s
        y: 48 * bars.s
        width: 112 * bars.s
        height: addCol.height
        color: "#282828"
        radius: 6 * bars.s
        MouseArea { anchors.fill: parent }

        Column {
            id: addCol
            width: parent.width
            Repeater {
                model: [
                    { n: "Cubo", i: 0 }, { n: "Esfera", i: 1 }, { n: "Plano", i: 2 },
                    { n: "Osso", i: 10 }, { n: "Câmera", i: 18 }
                ]
                Rectangle {
                    required property var modelData
                    width: addCol.width
                    height: 34 * bars.s
                    color: rowMa.pressed ? "#444444" : "transparent"
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        x: 12 * bars.s
                        text: parent.modelData.n
                        color: "#f2f2f2"
                        font.pixelSize: 14 * bars.s
                    }
                    MouseArea {
                        id: rowMa
                        anchors.fill: parent
                        onClicked: {
                            bars.view.command(11, parent.modelData.i)
                            bars.addOpen = false
                        }
                    }
                }
            }
        }
    }

    // ---------------- desfazer / refazer / menu ----------------
    Shape {
        x: bars.width - 123 * bars.s
        width: 123
        height: 36
        scale: bars.s
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: "#282828"
            strokeColor: "transparent"
            PathSvg { path: "M0 0 H123 V36 H3 Q0 36 0 33 Z" }
        }
    }
    Row {
        x: bars.width - 123 * bars.s
        y: 0
        TbCell { s: bars.s; width: 41 * bars.s; height: 36 * bars.s; icon: "undo"; dimmed: !bars.view.canUndo; onClicked: bars.view.command(11, 91) }
        TbCell { s: bars.s; width: 41 * bars.s; height: 36 * bars.s; icon: "redo"; dimmed: !bars.view.canRedo; onClicked: bars.view.command(11, 92) }
        TbCell { s: bars.s; width: 41 * bars.s; height: 36 * bars.s; icon: "menu"; onClicked: bars.view.command(11, 60) }
    }
}

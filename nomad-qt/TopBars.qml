import QtQuick
import QtQuick.Shapes

// Barras de botoes do conceito: ferramentas + cubo (canto superior esquerdo)
// e desfazer / refazer / menu (canto superior direito).
// Os icones sao os do proprio conceito (PNG recortado) e ficam nas mesmas posicoes dele.
// Medidas em pixels do conceito (2400x1080) convertidas por "k".
Item {
    id: bars
    required property var view   // EngineView

    readonly property real s: height > 0 ? height / 360 : 1   // unidade de projeto (800x360)
    readonly property real k: s / 3                            // pixel do conceito -> tela
    property bool addOpen: false

    function pickTool(n) {
        // tocar na ferramenta ativa volta para "selecionar" (ferramenta 3)
        view.command(11, view.toolIndex === n ? 6 : 3 + n)
    }

    // fecha o menu de adicionar ao tocar fora
    MouseArea {
        anchors.fill: parent
        enabled: bars.addOpen
        onClicked: bars.addOpen = false
    }

    // ---------------- fundos ----------------
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

    // ---------------- ferramenta ativa (quadrado azul atras do icone) ----------------
    Rectangle { visible: bars.view.toolIndex === 0; x: (76.5 - 54) * bars.k; y: (61 - 54) * bars.k; width: 108 * bars.k; height: 108 * bars.k; radius: 24 * bars.k; color: "#4773b3" }
    Rectangle { visible: bars.view.toolIndex === 1; x: (199.5 - 54) * bars.k; y: (72.5 - 54) * bars.k; width: 108 * bars.k; height: 108 * bars.k; radius: 24 * bars.k; color: "#4773b3" }
    Rectangle { visible: bars.view.toolIndex === 2; x: (338.5 - 54) * bars.k; y: (67 - 54) * bars.k; width: 108 * bars.k; height: 108 * bars.k; radius: 24 * bars.k; color: "#4773b3" }
    Rectangle { visible: bars.addOpen; x: (79.5 - 54) * bars.k; y: (212.5 - 54) * bars.k; width: 108 * bars.k; height: 108 * bars.k; radius: 24 * bars.k; color: "#4773b3" }

    // ---------------- icones (recortes do conceito) ----------------
    Image { source: "icons/tb_move.png";   x: 40 * bars.k;   y: 32 * bars.k;  width: 73 * bars.k; height: 58 * bars.k; smooth: true; mipmap: true }
    Image { source: "icons/tb_rotate.png"; x: 157 * bars.k;  y: 45 * bars.k;  width: 85 * bars.k; height: 55 * bars.k; smooth: true; mipmap: true }
    Image { source: "icons/tb_scale.png";  x: 295 * bars.k;  y: 31 * bars.k;  width: 87 * bars.k; height: 72 * bars.k; smooth: true; mipmap: true }
    Image { source: "icons/tb_cube.png";   x: 35 * bars.k;   y: 169 * bars.k; width: 89 * bars.k; height: 87 * bars.k; smooth: true; mipmap: true }
    Image {
        source: "icons/tb_undo.png"
        x: bars.width + (2066 - 2400) * bars.k; y: 36 * bars.k; width: 85 * bars.k; height: 39 * bars.k
        smooth: true; mipmap: true
        opacity: bars.view.canUndo ? 1.0 : 0.35
    }
    Image {
        source: "icons/tb_redo.png"
        x: bars.width + (2181 - 2400) * bars.k; y: 35 * bars.k; width: 85 * bars.k; height: 39 * bars.k
        smooth: true; mipmap: true
        opacity: bars.view.canRedo ? 1.0 : 0.35
    }
    Image { source: "icons/tb_menu.png"; x: bars.width + (2293 - 2400) * bars.k; y: 25 * bars.k; width: 85 * bars.k; height: 60 * bars.k; smooth: true; mipmap: true }

    // ---------------- areas de toque ----------------
    Row {
        x: 0
        y: 0
        TbCell { s: bars.s; onClicked: bars.pickTool(0) }
        TbCell { s: bars.s; onClicked: bars.pickTool(1) }
        TbCell { s: bars.s; onClicked: bars.pickTool(2) }
    }
    TbCell {
        x: 0
        y: 45 * bars.s
        width: 52 * bars.s
        height: 51 * bars.s
        s: bars.s
        onClicked: bars.addOpen = !bars.addOpen
    }
    Row {
        x: bars.width - 123 * bars.s
        y: 0
        TbCell { s: bars.s; width: 41 * bars.s; height: 36 * bars.s; onClicked: bars.view.command(11, 91) }
        TbCell { s: bars.s; width: 41 * bars.s; height: 36 * bars.s; onClicked: bars.view.command(11, 92) }
        TbCell { s: bars.s; width: 41 * bars.s; height: 36 * bars.s; onClicked: bars.view.command(11, 60) }
    }

    // ---------------- menu de objetos ----------------
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
}

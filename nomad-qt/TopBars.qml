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
    Shape {   // painel de ferramentas do conceito (forma recortada)
        width: 460
        height: 350
        scale: bars.k
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: "#050505"
            strokeColor: "transparent"
            PathSvg { path: "M13 13 H444 V84 L384 136 H271 L132 314 V338 H13 Z" }
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

    // ---------------- icones (vetor; a ferramenta ativa fica em ciano) ----------------
    Glyph { name: "cube";   fg: bars.addOpen ? "#00e5ff" : "#c8c8c8"; x: (72 - 52) * bars.k;  y: (76 - 52) * bars.k;  width: 104 * bars.k; height: 104 * bars.k }
    Glyph { name: "scale";  fg: bars.view.toolIndex === 2 ? "#00e5ff" : "#b4b4b4"; x: (234 - 34) * bars.k; y: (66 - 34) * bars.k; width: 68 * bars.k; height: 68 * bars.k }
    Glyph { name: "cursor"; fg: bars.view.toolIndex === 3 ? "#00e5ff" : "#8c8c8c"; x: (384 - 34) * bars.k; y: (68 - 34) * bars.k; width: 68 * bars.k; height: 68 * bars.k }
    Glyph { name: "rotate"; fg: bars.view.toolIndex === 1 ? "#00e5ff" : "#a8a8a8"; x: (166 - 32) * bars.k; y: (156 - 32) * bars.k; width: 64 * bars.k; height: 64 * bars.k }
    Glyph { name: "move";   fg: bars.view.toolIndex === 0 ? "#00e5ff" : "#c8c8c8"; x: (90 - 40) * bars.k;  y: (230 - 40) * bars.k; width: 80 * bars.k; height: 80 * bars.k }
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
    Repeater {
        // [x0, y0, x1, y1, acao]  em pixels do conceito: 0 mover, 1 rotacionar, 2 escalar, 3 cursor, 9 adicionar
        model: [
            { x0: 13,  y0: 13,  x1: 150, y1: 136, a: 9 },
            { x0: 150, y0: 13,  x1: 309, y1: 136, a: 2 },
            { x0: 309, y0: 13,  x1: 444, y1: 136, a: 3 },
            { x0: 125, y0: 136, x1: 225, y1: 205, a: 1 },
            { x0: 13,  y0: 205, x1: 140, y1: 338, a: 0 }
        ]
        Item {
            id: tcell
            required property var modelData
            x: modelData.x0 * bars.k
            y: modelData.y0 * bars.k
            width: (modelData.x1 - modelData.x0) * bars.k
            height: (modelData.y1 - modelData.y0) * bars.k
            Rectangle { visible: tma.pressed; anchors.fill: parent; color: "white"; opacity: 0.12 }
            MouseArea {
                id: tma
                anchors.fill: parent
                containmentMask: HitMask { target: tma; view: bars.view }
                onClicked: {
                    var a = tcell.modelData.a
                    if (a === 9) bars.addOpen = !bars.addOpen
                    else if (a === 3) bars.view.command(11, 6)   // cursor: navegar (arrastar gira a vista)
                    else bars.pickTool(a)
                }
            }
        }
    }
    Row {
        x: bars.width - 123 * bars.s
        y: 0
        TbCell { view: bars.view; s: bars.s; width: 41 * bars.s; height: 36 * bars.s; onClicked: bars.view.command(11, 91) }
        TbCell { view: bars.view; s: bars.s; width: 41 * bars.s; height: 36 * bars.s; onClicked: bars.view.command(11, 92) }
        TbCell { view: bars.view; s: bars.s; width: 41 * bars.s; height: 36 * bars.s; onClicked: bars.view.command(11, 60) }
    }

    // ---------------- menu de objetos ----------------
    Rectangle {
        visible: bars.addOpen
        x: 460 * bars.k
        y: 20 * bars.k
        width: 112 * bars.s
        height: addCol.height
        color: "#282828"
        radius: 6 * bars.s
        MouseArea {
            id: addPanelMa
            anchors.fill: parent
            containmentMask: HitMask { target: addPanelMa; view: bars.view }
        }

        Column {
            id: addCol
            width: parent.width
            Repeater {
                model: [
                    { n: "Cubo", i: 0 }, { n: "Esfera", i: 1 }, { n: "Plano", i: 2 },
                    { n: "Osso", i: 10 }, { n: "Câmera", i: 18 },
                    { n: "Sol", i: 45 }, { n: "Ponto", i: 46 }, { n: "Spot", i: 47 }
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
                        containmentMask: HitMask { target: rowMa; view: bars.view }
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

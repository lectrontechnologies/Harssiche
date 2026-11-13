import QtQuick
import QtQuick.Shapes

// Barras do topo no estilo do Toolbag (cinzas neutros e verde de destaque), adaptadas ao toque:
// abas de espaco de trabalho, barra de ferramentas (selecionar, mover, girar, escalar), objetos, desfazer/refazer e menu.
// A alca abaixo do canto direito recolhe as abas e deixa so a barra de ferramentas.
// Medidas em pixels de uma tela de 1080 de altura, convertidas por "k".
Item {
    id: bars
    required property var view   // EngineView

    readonly property real k: height > 0 ? height / 1080 : 1

    // ---- paleta (cores medidas na interface do Toolbag) ----
    readonly property color cTabsBg: "#1a1a1a"
    readonly property color cTab: "#434343"
    readonly property color cTabOn: "#515151"
    readonly property color cBar: "#404040"
    readonly property color cEdge: "#2b2b2b"
    readonly property color cAccent: "#93dc3e"
    readonly property color cIcon: "#d6d6d6"
    readonly property color cDim: "#8c8c8c"
    readonly property color cText: "#e8e8e8"
    readonly property color cMenu: "#2e2e2e"

    property bool addOpen: false
    property bool tabsOpen: true
    property int curTab: 0
    property real tabsFrac: tabsOpen ? 1 : 0
    Behavior on tabsFrac { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

    readonly property real tabsH: 62 * k * tabsFrac
    readonly property real barH: 94 * k
    readonly property real handleH: 34 * k
    readonly property real topH: tabsH + barH      // espaco tomado no topo pelas barras

    function pickTool(n) {
        // tocar na ferramenta ativa volta para "selecionar"
        view.command(11, view.toolIndex === n ? 6 : 3 + n)
    }
    function pickTab(i, a) {
        if (a === 61) { view.command(11, 61); return }            // Render: abre a tela de render
        if (a === 84) { curTab = i; view.command(11, 84); return } // Classic e Animate: modo de animacao
        // Setup, Texture e Workspaces: ainda sem funcao
    }

    // fecha o menu de adicionar ao tocar fora
    MouseArea {
        anchors.fill: parent
        enabled: bars.addOpen
        onClicked: bars.addOpen = false
    }

    // ---------------- abas ----------------
    Rectangle {
        id: tabsBg
        visible: bars.tabsH > 1
        width: parent.width
        height: bars.tabsH
        color: bars.cTabsBg
        clip: true
        MouseArea {
            id: tabsBgMa
            anchors.fill: parent
            containmentMask: HitMask { target: tabsBgMa; view: bars.view }
        }
        Row {
            x: 3 * bars.k
            y: 8 * bars.k
            spacing: 10 * bars.k
            Repeater {
                model: [
                    { n: "Classic", a: 84 }, { n: "Setup", a: 0 }, { n: "Texture", a: 0 }, { n: "Render", a: 61 },
                    { n: "Animate", a: 84 }, { n: "Workspace 1", a: 0 }, { n: "Workspace 2", a: 0 }, { n: "+", a: 0 }
                ]
                Item {
                    id: tab
                    required property var modelData
                    required property int index
                    width: tabText.implicitWidth + 44 * bars.k
                    height: 54 * bars.k
                    Rectangle {
                        id: tabBg
                        anchors.fill: parent
                        radius: 8 * bars.k
                        color: bars.curTab === tab.index ? bars.cTabOn : bars.cTab
                    }
                    Rectangle {   // cantos de baixo retos
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: parent.height / 2
                        color: tabBg.color
                    }
                    Rectangle { visible: tabMa.pressed; anchors.fill: parent; color: "white"; opacity: 0.10 }
                    Text {
                        id: tabText
                        anchors.centerIn: parent
                        text: tab.modelData.n
                        color: bars.curTab === tab.index ? "#ffffff" : "#d0d0d0"
                        font.pixelSize: 27 * bars.k
                    }
                    MouseArea {
                        id: tabMa
                        anchors.fill: parent
                        containmentMask: HitMask { target: tabMa; view: bars.view }
                        onClicked: bars.pickTab(tab.index, tab.modelData.a)
                    }
                }
            }
        }
    }

    // ---------------- barra de ferramentas ----------------
    Rectangle {
        id: toolbar
        y: bars.tabsH
        width: parent.width
        height: bars.barH
        color: bars.cBar
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: Math.max(1, 2 * bars.k)
            color: bars.cEdge
        }
        MouseArea {
            id: barMa
            anchors.fill: parent
            containmentMask: HitMask { target: barMa; view: bars.view }
        }

        // selecionar, mover, girar, escalar
        Repeater {
            model: 4
            Item {
                id: tool
                required property int index
                readonly property bool active: index === 0 ? bars.view.toolIndex === 3 : bars.view.toolIndex === index - 1
                x: 8 * bars.k + index * 84 * bars.k
                width: 84 * bars.k
                height: bars.barH
                Rectangle {
                    visible: tool.active
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8 * bars.k
                    width: 52 * bars.k
                    height: 4 * bars.k
                    radius: 2 * bars.k
                    color: bars.cAccent
                }
                Rectangle { visible: toolMa.pressed; anchors.fill: parent; anchors.margins: 6 * bars.k; radius: 8 * bars.k; color: "white"; opacity: 0.12 }
                Shape {   // seta de selecao
                    visible: tool.index === 0
                    anchors.centerIn: parent
                    width: 60
                    height: 60
                    scale: bars.k
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        fillColor: tool.active ? bars.cAccent : bars.cIcon
                        strokeColor: "transparent"
                        PathSvg { path: "M12 5 L12 46 L22 37 L30 56 L38 52 L30 34 L44 34 Z" }
                    }
                }
                Glyph {
                    visible: tool.index > 0
                    anchors.centerIn: parent
                    width: 62 * bars.k
                    height: 62 * bars.k
                    name: tool.index === 1 ? "move" : (tool.index === 2 ? "rotate" : "scale")
                    fg: tool.active ? bars.cAccent : bars.cIcon
                }
                MouseArea {
                    id: toolMa
                    anchors.fill: parent
                    containmentMask: HitMask { target: toolMa; view: bars.view }
                    onClicked: {
                        if (tool.index === 0) bars.view.command(11, 6)   // selecionar: arrastar gira a vista
                        else bars.pickTool(tool.index - 1)
                    }
                }
            }
        }

        Rectangle { x: 358 * bars.k; anchors.verticalCenter: parent.verticalCenter; width: Math.max(1, 2 * bars.k); height: 56 * bars.k; color: bars.cEdge }

        // objetos (abre o menu de adicionar)
        Item {
            id: addBtn
            x: 372 * bars.k
            width: 84 * bars.k
            height: bars.barH
            Rectangle { visible: addMa.pressed; anchors.fill: parent; anchors.margins: 6 * bars.k; radius: 8 * bars.k; color: "white"; opacity: 0.12 }
            Glyph {
                anchors.centerIn: parent
                width: 62 * bars.k
                height: 62 * bars.k
                name: "cube"
                fg: bars.addOpen ? bars.cAccent : bars.cIcon
            }
            MouseArea {
                id: addMa
                anchors.fill: parent
                containmentMask: HitMask { target: addMa; view: bars.view }
                onClicked: bars.addOpen = !bars.addOpen
            }
        }

        // modo de escultura
        Rectangle {
            id: sculptBtn
            x: 470 * bars.k
            anchors.verticalCenter: parent.verticalCenter
            width: sculptText.implicitWidth + 40 * bars.k
            height: 56 * bars.k
            radius: 8 * bars.k
            color: bars.view.sculptMode ? bars.cAccent : "#4d4d4d"
            Text {
                id: sculptText
                anchors.centerIn: parent
                text: "Escultura"
                color: bars.view.sculptMode ? "#1a1a1a" : bars.cText
                font.pixelSize: 25 * bars.k
            }
            Rectangle { visible: sculptMa.pressed; anchors.fill: parent; radius: parent.radius; color: "white"; opacity: 0.14 }
            MouseArea {
                id: sculptMa
                anchors.fill: parent
                containmentMask: HitMask { target: sculptMa; view: bars.view }
                onClicked: bars.view.command(11, 93)
            }
        }

        // desfazer, refazer, menu (logotipo)
        Item {
            id: undoBtn
            x: toolbar.width - 392 * bars.k
            width: 96 * bars.k
            height: bars.barH
            opacity: bars.view.canUndo ? 1.0 : 0.35
            Rectangle { visible: undoMa.pressed; anchors.fill: parent; anchors.margins: 6 * bars.k; radius: 8 * bars.k; color: "white"; opacity: 0.12 }
            Image { anchors.centerIn: parent; source: "icons/tb_undo.png"; width: 70 * bars.k; height: 32 * bars.k; smooth: true; mipmap: true }
            MouseArea {
                id: undoMa
                anchors.fill: parent
                containmentMask: HitMask { target: undoMa; view: bars.view }
                onClicked: bars.view.command(11, 91)
            }
        }
        Item {
            id: redoBtn
            x: toolbar.width - 292 * bars.k
            width: 96 * bars.k
            height: bars.barH
            opacity: bars.view.canRedo ? 1.0 : 0.35
            Rectangle { visible: redoMa.pressed; anchors.fill: parent; anchors.margins: 6 * bars.k; radius: 8 * bars.k; color: "white"; opacity: 0.12 }
            Image { anchors.centerIn: parent; source: "icons/tb_redo.png"; width: 70 * bars.k; height: 32 * bars.k; smooth: true; mipmap: true }
            MouseArea {
                id: redoMa
                anchors.fill: parent
                containmentMask: HitMask { target: redoMa; view: bars.view }
                onClicked: bars.view.command(11, 92)
            }
        }
        Item {
            id: menuBtn
            x: toolbar.width - 190 * bars.k
            width: 110 * bars.k
            height: bars.barH
            Rectangle { visible: menuMa.pressed; anchors.fill: parent; anchors.margins: 6 * bars.k; radius: 8 * bars.k; color: "white"; opacity: 0.12 }
            Image { anchors.centerIn: parent; source: "icons/logo.png"; width: 72 * bars.k; height: 72 * bars.k; smooth: true; mipmap: true }
            MouseArea {
                id: menuMa
                anchors.fill: parent
                containmentMask: HitMask { target: menuMa; view: bars.view }
                onClicked: bars.view.command(11, 60)
            }
        }
    }

    // ---------------- alca: recolhe / expande as abas ----------------
    Item {
        id: handle
        x: bars.width - 122 * bars.k
        y: bars.topH
        width: 122 * bars.k
        height: 56 * bars.k          // area de toque maior que o desenho
        Rectangle {
            width: parent.width
            height: bars.handleH
            color: bars.cEdge
            radius: 6 * bars.k
            Rectangle { width: parent.width; height: parent.radius; color: parent.color }   // canto de cima reto
            Shape {
                anchors.centerIn: parent
                width: 20
                height: 10
                scale: bars.k * 1.2
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: bars.cText
                    strokeWidth: 2.5
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathSvg { path: bars.tabsOpen ? "M1 1 L10 9 L19 1" : "M1 9 L10 1 L19 9" }
                }
            }
        }
        MouseArea {
            id: handleMa
            anchors.fill: parent
            containmentMask: HitMask { target: handleMa; view: bars.view }
            onClicked: { bars.tabsOpen = !bars.tabsOpen; bars.addOpen = false }
        }
    }

    // ---------------- menu de objetos ----------------
    Rectangle {
        visible: bars.addOpen
        x: 372 * bars.k
        y: bars.topH
        width: 300 * bars.k
        height: addCol.height
        color: bars.cMenu
        radius: 8 * bars.k
        border.color: bars.cEdge
        border.width: Math.max(1, bars.k)
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
                    { n: "Sol", i: 45 }, { n: "Ponto", i: 46 }, { n: "Spot", i: 47 }, { n: "Área", i: 48 }
                ]
                Rectangle {
                    required property var modelData
                    width: addCol.width
                    height: 66 * bars.k
                    color: rowMa.pressed ? "#4a4a4a" : "transparent"
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        x: 24 * bars.k
                        text: parent.modelData.n
                        color: bars.cText
                        font.pixelSize: 28 * bars.k
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

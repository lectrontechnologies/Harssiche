import QtQuick

// Outliner (hierarquia) no estilo do conceito: painel docado a direita, redimensionavel e recolhivel.
// Tocar seleciona; segurar uma linha e arrastar sobre outra torna o objeto filho dela;
// soltar abaixo das linhas remove o parentesco. Medidas em pixels do conceito (altura 1080) via "k".
Item {
    id: so
    required property var view   // EngineView

    readonly property real k: height > 0 ? height / 1080 : 1

    // ---- dock ----
    property real wFrac: 0.19                 // largura como fracao da janela (arrastar a borda muda)
    property bool open: true
    property real openFrac: open ? 1 : 0
    Behavior on openFrac { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    readonly property real panelW: width * wFrac
    readonly property real insetPx: panelW * openFrac     // espaco tomado a direita (timeline e motor se ajustam)
    readonly property real topY: 108 * k                  // abaixo da barra de desfazer/refazer/menu

    // ---- dados ----
    property var flat: []
    property var collapsed: ({})
    property string query: ""
    property string menu: ""
    property var showType: ({ "0": true, "1": true, "2": true, "3": true })

    // ---- arrastar para parentear ----
    property bool dragging: false
    property int dragIdx: -1
    property int dropIdx: -2          // -2 invalido, -1 remove o parentesco, >= 0 novo pai
    property real dragX: 0
    property real dragY: 0

    readonly property real rowH: 35.9 * k
    readonly property real listTop: 124.8 * k

    function rows() { return view.sceneRows }

    function isDesc(anc, node) {
        var r = rows()
        var g = 0
        var p = node
        while (p >= 0 && p < r.length && g < 64) {
            if (p === anc) return true
            p = r[p].parent
            g++
        }
        return false
    }

    function rebuild() {
        var r = rows()
        var n = r.length
        var q = query.toLowerCase()
        var kids = []
        var roots = []
        for (var i = 0; i < n; i++) kids.push([])
        for (var j = 0; j < n; j++) {
            var p = r[j].parent
            if (p >= 0 && p < n && p !== j) kids[p].push(j)
            else roots.push(j)
        }
        var out = []
        function visible(idx) {
            if (!showType[String(r[idx].type)]) return false
            if (q.length > 0 && r[idx].name.toLowerCase().indexOf(q) < 0) return false
            return true
        }
        function walk(idx, d) {
            if (d > 48) return
            var vis = visible(idx)
            if (vis) out.push({ idx: idx, depth: d, hasKids: kids[idx].length > 0 })
            if (q.length > 0 || !collapsed[idx])
                for (var c = 0; c < kids[idx].length; c++) walk(kids[idx][c], q.length > 0 ? 0 : (vis ? d + 1 : d))
        }
        for (var t = 0; t < roots.length; t++) walk(roots[t], 0)
        flat = out
    }

    function toggleCollapse(idx) {
        var c = {}
        for (var k2 in collapsed) c[k2] = collapsed[k2]
        c[idx] = !c[idx]
        collapsed = c
        rebuild()
    }
    function expandAll() { collapsed = ({}); rebuild() }
    function collapseAll() {
        var r = rows()
        var c = {}
        for (var i = 0; i < r.length; i++) {
            var p = r[i].parent
            if (p >= 0 && p < r.length) c[p] = true
        }
        collapsed = c
        rebuild()
    }
    function toggleShow(t) {
        var c = {}
        for (var k2 in showType) c[k2] = showType[k2]
        c[String(t)] = !c[String(t)]
        showType = c
        rebuild()
    }

    // linha sob o ponto (x, y) em coordenadas do painel
    function dropAt(px, py) {
        if (px < 0 || px > panel.width || py < listTop) return -2
        var rowIdx = Math.floor((py - listTop + list.contentY) / rowH)
        if (rowIdx < 0 || rowIdx >= flat.length) return -1
        var t = flat[rowIdx].idx
        if (t === dragIdx || isDesc(dragIdx, t)) return -2
        return t
    }

    Connections {
        target: so.view
        function onSceneChanged() { so.rebuild() }
    }
    onQueryChanged: rebuild()
    Component.onCompleted: rebuild()

    // ---------------- painel ----------------
    Rectangle {
        id: panel
        x: so.width - so.insetPx
        y: so.topY
        width: so.panelW
        height: so.height - so.topY
        color: "#383838"
        visible: so.openFrac > 0.001

        MouseArea {   // nao deixa o toque passar para o viewport
            id: bgMa
            anchors.fill: parent
            containmentMask: HitMask { target: bgMa; view: so.view }
        }

        // ---- faixa do topo: aba + recolher ----
        Rectangle { x: 0; y: 0; width: parent.width; height: 39 * so.k; color: "#2a2a2a" }
        Rectangle {
            x: 10.4 * so.k; y: 5.2 * so.k
            width: tabTxt.implicitWidth + 31.2 * so.k
            height: 33.8 * so.k
            color: "#444444"
            Text { id: tabTxt; anchors.centerIn: parent; text: "Outliner"; color: "#e6e6e6"; font.pixelSize: 19.5 * so.k }
        }
        Item {   // botao recolher
            id: collapseBtn
            x: parent.width - 44.2 * so.k; y: 2.6 * so.k
            width: 39 * so.k; height: 33.8 * so.k
            Rectangle { anchors.fill: parent; radius: 3.9 * so.k; color: cbMa.pressed ? "#555555" : "#3a3a3a" }
            Glyph { anchors.centerIn: parent; width: 20.8 * so.k; height: 20.8 * so.k; name: "chev_right"; fg: "#e0e0e0" }
            MouseArea {
                id: cbMa
                anchors.fill: parent
                containmentMask: HitMask { target: cbMa; view: so.view }
                onClicked: { so.menu = ""; so.open = false }
            }
        }

        // ---- menus: Display / Show / Help ----
        Rectangle { x: 0; y: 39 * so.k; width: parent.width; height: 39 * so.k; color: "#464646" }
        Row {
            x: 7.8 * so.k; y: 39 * so.k
            height: 39 * so.k
            Repeater {
                model: ["Display", "Show", "Help"]
                Item {
                    required property var modelData
                    width: mTxt.implicitWidth + 28.6 * so.k
                    height: 39 * so.k
                    Rectangle { anchors.fill: parent; color: "#5a5a5a"; visible: so.menu === parent.modelData || mMa.pressed }
                    Text { id: mTxt; anchors.centerIn: parent; text: parent.modelData; color: "#e2e2e2"; font.pixelSize: 19.5 * so.k }
                    MouseArea {
                        id: mMa
                        anchors.fill: parent
                        containmentMask: HitMask { target: mMa; view: so.view }
                        onClicked: so.menu = (so.menu === parent.modelData ? "" : parent.modelData)
                    }
                }
            }
        }

        // ---- busca ----
        Rectangle { x: 0; y: 78 * so.k; width: parent.width; height: 44.2 * so.k; color: "#464646" }
        Glyph { x: 10.4 * so.k; y: 84.5 * so.k; width: 28.6 * so.k; height: 28.6 * so.k; name: "dashbox"; fg: "#8a8a8a" }
        Rectangle {
            x: 46.8 * so.k; y: 83.2 * so.k
            width: parent.width - 57.2 * so.k; height: 33.8 * so.k
            color: "#2c2c2c"
            Text {
                visible: search.text.length === 0 && !search.activeFocus
                x: 7.8 * so.k; anchors.verticalCenter: parent.verticalCenter
                text: "Search..."; color: "#9a9a9a"; font.pixelSize: 19.5 * so.k
            }
            TextInput {
                id: search
                anchors.fill: parent
                anchors.leftMargin: 7.8 * so.k
                anchors.rightMargin: 33.8 * so.k
                verticalAlignment: TextInput.AlignVCenter
                color: "#f0f0f0"
                font.pixelSize: 19.5 * so.k
                clip: true
                selectByMouse: true
                containmentMask: HitMask { target: search; view: so.view }
                onTextChanged: so.query = text
                onAccepted: { Qt.inputMethod.hide(); so.view.forceActiveFocus() }
            }
            Glyph { x: parent.width - 26 * so.k; y: (parent.height - 18.2 * so.k) / 2; width: 18.2 * so.k; height: 18.2 * so.k; name: "chev_down"; fg: "#cfcfcf" }
        }

        // ---- lista ----
        Rectangle { x: 7.8 * so.k; y: so.listTop; width: parent.width - 15.6 * so.k; height: parent.height - so.listTop - 7.8 * so.k; color: "#333333" }
        Flickable {
            id: list
            x: 7.8 * so.k
            y: so.listTop
            width: panel.width - 15.6 * so.k
            height: panel.height - so.listTop - 7.8 * so.k
            clip: true
            contentWidth: width
            contentHeight: Math.max(height, so.flat.length * so.rowH)
            interactive: !so.dragging && contentHeight > height + 1
            boundsBehavior: Flickable.StopAtBounds

            Repeater {
                model: so.flat
                Rectangle {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property var info: so.rows()[modelData.idx]
                    readonly property bool selected: so.view.selectedObject === modelData.idx
                    readonly property real ind: 10.4 * so.k + modelData.depth * 23.4 * so.k
                    x: 0
                    y: index * so.rowH
                    width: list.width
                    height: so.rowH
                    color: selected ? "#4f79a3" : "transparent"
                    visible: info !== undefined

                    Rectangle {   // alvo do arrastar
                        visible: so.dragging && so.dropIdx === row.modelData.idx
                        anchors.fill: parent
                        color: "#2f6fb5"
                        opacity: 0.55
                    }

                    Glyph {   // seta de expandir
                        visible: row.modelData.hasKids && so.query.length === 0
                        x: row.ind
                        y: (so.rowH - 18.2 * so.k) / 2
                        width: 18.2 * so.k; height: 18.2 * so.k
                        name: so.collapsed[row.modelData.idx] ? "chev_right" : "chev_down"
                        fg: "#bdbdbd"
                    }
                    Glyph {   // icone do tipo
                        x: row.ind + 23.4 * so.k
                        y: (so.rowH - 26 * so.k) / 2
                        width: 26 * so.k; height: 26 * so.k
                        name: row.info ? (row.info.type === 1 ? "o_cam" : row.info.type === 2 ? "o_light" : row.info.type === 3 ? "o_bone" : "cube") : ""
                        fg: row.info && row.info.type === 0 ? "#cfcfcf" : "#e6e6e6"
                    }
                    Text {
                        x: row.ind + 57.2 * so.k
                        width: Math.max(0, parent.width - x - 7.8 * so.k)
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                        text: row.info ? row.info.name : ""
                        color: row.selected ? "#ffffff" : "#d4d4d4"
                        font.pixelSize: 20.8 * so.k
                    }

                    // toque: selecionar; segurar: arrastar para parentear
                    MouseArea {
                        id: rowMa
                        x: row.ind + 20.8 * so.k
                        y: 0
                        width: parent.width - x
                        height: parent.height
                        containmentMask: HitMask { target: rowMa; view: so.view }
                        preventStealing: so.dragging && so.dragIdx === row.modelData.idx
                        onClicked: so.view.command(23, row.modelData.idx)
                        onPressAndHold: {
                            so.dragIdx = row.modelData.idx
                            so.dragging = true
                            so.view.command(23, row.modelData.idx)
                            var p = mapToItem(panel, mouseX, mouseY)
                            so.dragX = p.x; so.dragY = p.y
                            so.dropIdx = so.dropAt(p.x, p.y)
                        }
                        onPositionChanged: function (m) {
                            if (!so.dragging || so.dragIdx !== row.modelData.idx) return
                            var p = mapToItem(panel, m.x, m.y)
                            so.dragX = p.x; so.dragY = p.y
                            so.dropIdx = so.dropAt(p.x, p.y)
                        }
                        function finish() {
                            if (so.dragging && so.dragIdx === row.modelData.idx) {
                                if (so.dropIdx >= -1) so.view.command(24, so.dragIdx, so.dropIdx)
                                so.dragging = false
                                so.dragIdx = -1
                                so.dropIdx = -2
                            }
                        }
                        onReleased: finish()
                        onCanceled: finish()
                    }
                    MouseArea {   // seta de expandir (area maior)
                        id: arrMa
                        x: row.ind - 7.8 * so.k
                        width: 44.2 * so.k
                        height: parent.height
                        enabled: row.modelData.hasKids && so.query.length === 0
                        containmentMask: HitMask { target: arrMa; view: so.view }
                        onClicked: so.toggleCollapse(row.modelData.idx)
                    }
                }
            }
        }

        // barra de rolagem vertical
        Rectangle {
            visible: list.contentHeight > list.height + 1
            x: panel.width - 15.6 * so.k; y: so.listTop
            width: 10.4 * so.k; height: list.height
            color: "#2a2a2a"
            Rectangle {
                width: parent.width
                height: Math.max(31.2 * so.k, parent.height * list.height / list.contentHeight)
                y: (parent.height - height) * (list.contentHeight > list.height ? list.contentY / (list.contentHeight - list.height) : 0)
                color: "#6c6c6c"
                radius: 3.9 * so.k
            }
        }

        // ---- borda esquerda: arrastar para redimensionar ----
        Item {
            id: grip
            x: -13 * so.k; y: 0
            width: 28.6 * so.k; height: parent.height
            property real startX: 0
            property real startW: 0
            Rectangle { x: 13 * so.k; width: Math.max(1, 2.6 * so.k); height: parent.height; color: "#242424" }
            Rectangle {
                x: 7.8 * so.k; y: (parent.height - height) / 2
                width: 13 * so.k; height: 72.8 * so.k; radius: 5.2 * so.k
                color: gMa.pressed ? "#6a6a6a" : "#4a4a4a"
                Column {
                    anchors.centerIn: parent
                    spacing: 6.5 * so.k
                    Repeater { model: 3; Rectangle { width: 3.9 * so.k; height: 3.9 * so.k; radius: width / 2; color: "#b8b8b8" } }
                }
            }
            MouseArea {
                id: gMa
                anchors.fill: parent
                containmentMask: HitMask { target: gMa; view: so.view }
                preventStealing: true
                onPressed: function (m) { grip.startX = mapToItem(so, m.x, m.y).x; grip.startW = so.wFrac }
                onPositionChanged: function (m) {
                    if (!pressed) return
                    var cx = mapToItem(so, m.x, m.y).x
                    var f = grip.startW + (grip.startX - cx) / so.width
                    so.wFrac = Math.max(0.14, Math.min(0.45, f))
                }
            }
        }

        // ---- menus suspensos ----
        MouseArea {   // fecha o menu ao tocar fora
            anchors.fill: parent
            enabled: so.menu !== ""
            visible: so.menu !== ""
            onClicked: so.menu = ""
        }
        Rectangle {
            id: pop
            visible: so.menu !== ""
            x: so.menu === "Display" ? 7.8 * so.k : (so.menu === "Show" ? 111.8 * so.k : 182 * so.k)
            y: 78 * so.k
            width: Math.min(panel.width - x - 7.8 * so.k, 299 * so.k)
            height: popCol.height + 10.4 * so.k
            color: "#2d2d2d"
            border.color: "#555555"
            border.width: Math.max(1, so.k)
            Column {
                id: popCol
                x: 0; y: 5.2 * so.k
                width: parent.width
                Repeater {
                    model: so.menu === "Display" ? [
                        { t: "Expandir tudo", a: "exp" }, { t: "Recolher tudo", a: "col" }
                    ] : so.menu === "Show" ? [
                        { t: "Malhas", a: "t0" }, { t: "Câmeras", a: "t1" }, { t: "Luzes", a: "t2" }, { t: "Ossos", a: "t3" }
                    ] : so.menu === "Help" ? [
                        { t: "Toque: selecionar", a: "" }, { t: "Segure e arraste: parentear", a: "" },
                        { t: "Solte abaixo da lista: soltar do pai", a: "" }
                    ] : []
                    Item {
                        required property var modelData
                        width: popCol.width
                        height: 39 * so.k
                        Rectangle { anchors.fill: parent; color: "#555555"; visible: pMa.pressed && parent.modelData.a !== "" }
                        Text {
                            x: 13 * so.k
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 26 * so.k
                            elide: Text.ElideRight
                            text: (parent.modelData.a.charAt(0) === "t" ? (so.showType[parent.modelData.a.charAt(1)] ? "✓  " : "     ") : "") + parent.modelData.t
                            color: parent.modelData.a === "" ? "#a8a8a8" : "#e6e6e6"
                            font.pixelSize: 19.5 * so.k
                        }
                        MouseArea {
                            id: pMa
                            anchors.fill: parent
                            enabled: parent.modelData.a !== ""
                            onClicked: {
                                var a = parent.modelData.a
                                if (a === "exp") so.expandAll()
                                else if (a === "col") so.collapseAll()
                                else if (a.charAt(0) === "t") so.toggleShow(parseInt(a.charAt(1)))
                                if (a === "exp" || a === "col") so.menu = ""
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- aba para reabrir quando recolhido ----
    Item {
        visible: so.openFrac < 0.999
        x: so.width - 33.8 * so.k * (1 - so.openFrac) - so.insetPx
        y: so.topY + 195 * so.k
        width: 33.8 * so.k; height: 124.8 * so.k
        opacity: 1 - so.openFrac
        Rectangle { anchors.fill: parent; color: tabMa.pressed ? "#555555" : "#3a3a3a"; border.color: "#222222"; border.width: Math.max(1, so.k) }
        Glyph { anchors.centerIn: parent; width: 20.8 * so.k; height: 20.8 * so.k; name: "chev_left"; fg: "#e6e6e6" }
        MouseArea {
            id: tabMa
            anchors.fill: parent
            enabled: !so.open
            containmentMask: HitMask { target: tabMa; view: so.view }
            onClicked: so.open = true
        }
    }

    // ---- etiqueta que segue o dedo no arrastar ----
    Rectangle {
        visible: so.dragging && so.rows()[so.dragIdx] !== undefined
        x: panel.x + so.dragX - width / 2
        y: panel.y + so.dragY - height - 20.8 * so.k
        width: dragTxt.implicitWidth + 39 * so.k
        height: 39 * so.k
        radius: 7.8 * so.k
        color: so.dropIdx === -2 ? "#5a3a3a" : "#2f5f94"
        opacity: 0.95
        Text {
            id: dragTxt
            anchors.centerIn: parent
            text: so.dragging && so.rows()[so.dragIdx] ? so.rows()[so.dragIdx].name : ""
            color: "white"
            font.pixelSize: 20.8 * so.k
        }
    }
}

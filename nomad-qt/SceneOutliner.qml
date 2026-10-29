import QtQuick

// Painel "Scene" (outliner): arvore de objetos com parentesco, olho (visivel) e camera (render).
// Segurar uma linha e arrastar sobre outra torna o objeto filho dela; soltar na "Collection"
// ou fora das linhas remove o parentesco. A unidade "u" e 1% da altura (a mesma do motor).
Item {
    id: so
    required property var view   // EngineView

    readonly property real u: height / 100
    readonly property real panelX: width * 0.96 - 30 * u
    readonly property real panelY: 11.5 * u
    readonly property real panelW: 30 * u
    readonly property real panelH: 16.3 * u
    readonly property real headH: 3.0 * u
    readonly property real rowH: 3.2 * u

    property var flat: []
    property var collapsed: ({})
    property bool allCollapsed: false

    // arrastar para parentear
    property bool dragging: false
    property int dragIdx: -1
    property int dropIdx: -2          // -2 sem alvo valido, -1 remove o parentesco, >= 0 novo pai
    property real dragX: 0
    property real dragY: 0

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
        var kids = []
        var roots = []
        for (var i = 0; i < n; i++) kids.push([])
        for (var j = 0; j < n; j++) {
            var p = r[j].parent
            if (p >= 0 && p < n && p !== j) kids[p].push(j)
            else roots.push(j)
        }
        var out = []
        function walk(idx, d) {
            if (d > 48) return
            out.push({ idx: idx, depth: d, hasKids: kids[idx].length > 0 })
            if (!collapsed[idx]) for (var c = 0; c < kids[idx].length; c++) walk(kids[idx][c], d + 1)
        }
        if (!allCollapsed) for (var q = 0; q < roots.length; q++) walk(roots[q], 0)
        flat = out
    }

    function toggleCollapse(idx) {
        var c = {}
        for (var k in collapsed) c[k] = collapsed[k]
        c[idx] = !c[idx]
        collapsed = c
        rebuild()
    }

    // linha sob o ponto (x, y) em coordenadas do painel; 0 = Collection, 1.. = flat[i - 1]
    function dropAt(px, py) {
        if (px < 0 || px > panelW || py < headH || py > panelH + 6 * u) return -2
        var rowIdx = Math.floor((py - headH + list.contentY) / rowH)
        if (rowIdx <= 0) return -1
        if (rowIdx - 1 >= flat.length) return -1
        var t = flat[rowIdx - 1].idx
        if (t === dragIdx || isDesc(dragIdx, t)) return -2
        return t
    }

    Connections {
        target: so.view
        function onSceneChanged() { so.rebuild() }
    }
    Component.onCompleted: rebuild()

    // ---------------- painel ----------------
    Rectangle {
        id: panel
        x: so.panelX
        y: so.panelY
        width: so.panelW
        height: so.panelH
        color: "#262626"
        MouseArea {
            id: bgMa
            anchors.fill: parent
            containmentMask: HitMask { target: bgMa; view: so.view }
        }

        Text {
            x: 1.4 * so.u
            y: (so.headH - height) / 2
            text: "Scene"
            color: "#a8a8a8"
            font.pixelSize: 1.7 * so.u
        }

        Flickable {
            id: list
            x: 0
            y: so.headH
            width: panel.width
            height: panel.height - so.headH
            clip: true
            contentWidth: width
            contentHeight: (so.flat.length + 1) * so.rowH
            interactive: !so.dragging && contentHeight > height
            boundsBehavior: Flickable.StopAtBounds

            // ---- linha da Collection ----
            Rectangle {
                id: colRow
                x: 0; y: 0
                width: list.width
                height: so.rowH
                color: "#2c2c2c"
                Rectangle {
                    visible: so.dragging && so.dropIdx === -1
                    anchors.fill: parent
                    color: "transparent"
                    border.color: "#4aa3ff"
                    border.width: Math.max(1, 0.2 * so.u)
                }
                Glyph { x: 0.9 * so.u; y: (so.rowH - 1.6 * so.u) / 2; width: 1.6 * so.u; height: 1.6 * so.u; name: so.allCollapsed ? "chev_right" : "chev_down"; fg: "#b0b0b0" }
                Glyph { x: 3.1 * so.u; y: (so.rowH - 2.0 * so.u) / 2; width: 2.0 * so.u; height: 2.0 * so.u; name: "collection"; fg: "#d8d8d8" }
                Text {
                    x: 5.8 * so.u
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Collection"
                    color: "#e4e4e4"
                    font.pixelSize: 1.85 * so.u
                }
                Glyph { x: 22.0 * so.u; y: (so.rowH - 1.8 * so.u) / 2; width: 1.8 * so.u; height: 1.8 * so.u; name: "check_box"; fg: "#e4e4e4" }
                Glyph { x: 25.0 * so.u; y: (so.rowH - 2.0 * so.u) / 2; width: 2.0 * so.u; height: 2.0 * so.u; name: "eye"; fg: "#e4e4e4" }
                Glyph { x: 27.6 * so.u; y: (so.rowH - 2.0 * so.u) / 2; width: 2.0 * so.u; height: 2.0 * so.u; name: "render"; fg: "#e4e4e4" }
                MouseArea {
                    id: colMa
                    x: 0; y: 0
                    width: 3.0 * so.u
                    height: parent.height
                    containmentMask: HitMask { target: colMa; view: so.view }
                    onClicked: { so.allCollapsed = !so.allCollapsed; so.rebuild() }
                }
            }

            // ---- linhas dos objetos ----
            Repeater {
                model: so.flat
                Rectangle {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property var info: so.rows()[modelData.idx]
                    readonly property bool selected: so.view.selectedObject === modelData.idx
                    readonly property real ind: 0.9 * so.u + (modelData.depth + 1) * 2.1 * so.u
                    x: 0
                    y: (index + 1) * so.rowH
                    width: list.width
                    height: so.rowH
                    color: selected ? "#33302a" : (index % 2 ? "#2c2c2c" : "#292929")
                    visible: info !== undefined

                    Rectangle {   // alvo do arrastar
                        visible: so.dragging && so.dropIdx === row.modelData.idx
                        anchors.fill: parent
                        color: "transparent"
                        border.color: "#4aa3ff"
                        border.width: Math.max(1, 0.2 * so.u)
                    }
                    Rectangle {   // linha-guia do parentesco
                        visible: row.modelData.depth > 0
                        x: row.ind - 1.9 * so.u
                        width: Math.max(1, 0.12 * so.u)
                        height: parent.height
                        color: "#4a4a4a"
                    }

                    // seta de expandir
                    Glyph {
                        visible: row.modelData.hasKids
                        x: row.ind
                        y: (so.rowH - 1.5 * so.u) / 2
                        width: 1.5 * so.u; height: 1.5 * so.u
                        name: so.collapsed[row.modelData.idx] ? "chev_right" : "chev_down"
                        fg: "#b0b0b0"
                    }
                    // icone do tipo (laranja)
                    Glyph {
                        x: row.ind + 1.9 * so.u
                        y: (so.rowH - 2.0 * so.u) / 2
                        width: 2.0 * so.u; height: 2.0 * so.u
                        name: row.info ? (row.info.type === 1 ? "o_cam" : row.info.type === 2 ? "o_light" : row.info.type === 3 ? "o_bone" : "o_mesh") : ""
                        fg: "#e8923a"
                    }
                    Text {
                        x: row.ind + 4.6 * so.u
                        width: Math.max(0, 20.5 * so.u - x)
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                        text: row.info ? row.info.name : ""
                        color: row.selected ? "#f5a524" : "#d2d2d2"
                        font.pixelSize: 1.85 * so.u
                        opacity: row.info && row.info.hide ? 0.45 : 1
                    }
                    // icone dos dados (verde)
                    Glyph {
                        x: 20.6 * so.u
                        y: (so.rowH - 1.8 * so.u) / 2
                        width: 1.8 * so.u; height: 1.8 * so.u
                        name: row.info ? (row.info.type === 1 ? "d_cam" : row.info.type === 2 ? "d_light" : "d_mesh") : ""
                        fg: "#26c08a"
                        visible: !!row.info && row.info.type !== 3
                    }
                    Glyph {
                        x: 25.0 * so.u; y: (so.rowH - 2.0 * so.u) / 2; width: 2.0 * so.u; height: 2.0 * so.u
                        name: row.info && row.info.hide ? "eye_off" : "eye"
                        fg: row.info && row.info.hide ? "#7a7a7a" : "#e4e4e4"
                    }
                    Glyph {
                        x: 27.6 * so.u; y: (so.rowH - 2.0 * so.u) / 2; width: 2.0 * so.u; height: 2.0 * so.u
                        name: row.info && row.info.nor ? "render_off" : "render"
                        fg: row.info && row.info.nor ? "#7a7a7a" : "#e4e4e4"
                    }

                    // toque: selecionar; segurar: arrastar para parentear
                    MouseArea {
                        id: rowMa
                        x: 0; y: 0
                        width: 24.2 * so.u
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
                    MouseArea {   // seta de expandir
                        id: arrMa
                        x: row.ind - 0.8 * so.u
                        width: 3.4 * so.u
                        height: parent.height
                        enabled: row.modelData.hasKids
                        containmentMask: HitMask { target: arrMa; view: so.view }
                        onClicked: so.toggleCollapse(row.modelData.idx)
                    }
                    MouseArea {   // olho
                        id: eyeMa
                        x: 24.2 * so.u
                        width: 2.8 * so.u
                        height: parent.height
                        containmentMask: HitMask { target: eyeMa; view: so.view }
                        onClicked: so.view.command(25, row.modelData.idx)
                    }
                    MouseArea {   // render
                        id: renMa
                        x: 27.0 * so.u
                        width: 3.0 * so.u
                        height: parent.height
                        containmentMask: HitMask { target: renMa; view: so.view }
                        onClicked: so.view.command(26, row.modelData.idx)
                    }
                }
            }
        }
    }

    // etiqueta que segue o dedo durante o arrastar
    Rectangle {
        visible: so.dragging && so.rows()[so.dragIdx] !== undefined
        x: so.panelX + so.dragX - width / 2
        y: so.panelY + so.dragY - height - 1.5 * so.u
        width: dragTxt.implicitWidth + 3 * so.u
        height: 3.0 * so.u
        radius: 0.6 * so.u
        color: so.dropIdx === -2 ? "#5a3a3a" : "#2f5f94"
        opacity: 0.95
        Text {
            id: dragTxt
            anchors.centerIn: parent
            text: so.dragging && so.rows()[so.dragIdx] ? so.rows()[so.dragIdx].name : ""
            color: "white"
            font.pixelSize: 1.8 * so.u
        }
    }
}

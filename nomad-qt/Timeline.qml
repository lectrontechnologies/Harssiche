import QtQuick

// Timeline do novo conceito: painel de transporte flutuante (esquerda), painel de keys
// (direita: botao de key, Auto Key, Set Key) e a barra com a regua.
// Medidas em pixels do conceito (2400x1080) convertidas por "k" (= altura / 1080).
Item {
    id: tl
    required property var view   // EngineView

    readonly property real k: height > 0 ? height / 1080 : 1
    property real rightInset: 0                       // espaco tomado pelo Outliner a direita
    readonly property real tw: width - rightInset

    // barra inferior e regua (pixels do conceito)
    readonly property real barTop: height - 222 * k          // y = 858
    readonly property real stripX: 108 * k
    readonly property real stripW: Math.max(40, tw - 233 * k)   // ate x = 2275
    readonly property real stripY: 913 * k
    readonly property real stripH: 105 * k
    readonly property real rulerX: 141 * k                    // x do frame inicial da janela
    readonly property real rulerW: Math.max(10, tw - 270 * k)   // ate x = 2271
    readonly property real span: Math.max(1, view.viewEnd - view.viewStart)
    readonly property real ppf: rulerW / span

    property bool scrubbing: false
    property real scrubFrame: 1
    readonly property real shown: scrubbing ? scrubFrame : view.frame

    function frameToX(f) {
        return rulerX + (f - view.viewStart) / span * rulerW
    }

    function scrubTo(xr) {
        var f = Math.round(view.viewStart + xr / rulerW * span)
        f = Math.max(view.frameStart, Math.min(view.frameEnd, f))
        scrubFrame = f
        view.command(9, f)
    }

    function stepFrame(d) {
        if (view.playing) view.command(2)
        view.command(9, Math.round(view.frame) + d)
    }

    // ---- barra de fundo ----
    Rectangle {
        x: 0
        y: tl.barTop
        width: tl.width
        height: tl.height - tl.barTop
        color: "#242424"
        MouseArea {   // nao deixa o toque passar para o viewport
            id: barMa
            anchors.fill: parent
            containmentMask: HitMask { target: barMa; view: tl.view }
        }
    }

    // ---- regua ----
    Rectangle {
        id: strip
        x: tl.stripX
        y: tl.stripY
        width: tl.stripW
        height: tl.stripH
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#2d2d2d" }
            GradientStop { position: 0.86; color: "#2d2d2d" }
            GradientStop { position: 1.0; color: "#454545" }
        }
    }
    Rectangle {   // linha do intervalo no topo
        x: strip.x
        y: strip.y
        width: strip.width
        height: Math.max(1, 2.5 * tl.k)
        color: "#8a8a2a"
    }
    Rectangle {   // tampa cinza da esquerda
        x: strip.x
        y: strip.y + 9 * tl.k
        width: 16 * tl.k
        height: strip.height - 9 * tl.k
        color: "#454545"
    }

    Canvas {
        id: ruler
        x: strip.x
        y: strip.y
        width: strip.width
        height: strip.height
        renderTarget: Canvas.Image
        property int vs: tl.view.viewStart
        property int ve: tl.view.viewEnd
        onVsChanged: requestPaint()
        onVeChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var k = tl.k
            var ppf = tl.rulerW / Math.max(1, ve - vs)
            var ox = tl.rulerX - tl.stripX
            var steps = [1, 5, 10, 25, 50, 100, 250, 500, 1000]
            var step = 1000
            for (var i = 0; i < steps.length; i++) {
                if (steps[i] * ppf >= 100 * k) { step = steps[i]; break }
            }
            var minor = step >= 5 ? step / 5 : 0
            if (minor > 0 && minor * ppf < 7 * k) minor = 0
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"
            ctx.font = Math.round(34 * k) + "px sans-serif"
            ctx.lineWidth = Math.max(1, 2 * k)
            for (var f = vs; f <= ve; f++) {
                var x = ox + (f - vs) * ppf
                if (f % step === 0) {
                    ctx.strokeStyle = "#c4c4c4"
                    ctx.beginPath()
                    ctx.moveTo(x, 8 * k)
                    ctx.lineTo(x, 50 * k)
                    ctx.stroke()
                    ctx.fillStyle = "#bdbdbd"
                    ctx.fillText(String(f), x, 76 * k)
                } else if (minor > 0 && f % minor === 0) {
                    ctx.strokeStyle = "#8c8c8c"
                    ctx.beginPath()
                    ctx.moveTo(x, 8 * k)
                    ctx.lineTo(x, 38 * k)
                    ctx.stroke()
                }
            }
        }
    }

    // ---- keyframes do objeto selecionado ----
    Repeater {
        model: tl.view.keyFrames
        Rectangle {
            required property var modelData
            visible: modelData >= tl.view.viewStart - 0.5 && modelData <= tl.view.viewEnd + 0.5
            width: Math.max(4 * tl.k, Math.min(10 * tl.k, tl.ppf * 0.5))
            height: 26 * tl.k
            x: tl.frameToX(modelData) - width / 2
            y: strip.y + strip.height - height - 6 * tl.k
            color: "#4fa80a"
        }
    }

    // ---- fim do intervalo (linha amarela) ----
    Rectangle {
        visible: tl.view.frameEnd <= tl.view.viewEnd + 0.01
        width: Math.max(2, 5 * tl.k)
        height: strip.height + 4 * tl.k
        x: tl.frameToX(tl.view.frameEnd) - width / 2
        y: strip.y - 2 * tl.k
        color: "#ffe600"
    }

    // ---- indicador do frame atual ----
    Rectangle {
        visible: tl.shown >= tl.view.viewStart - 0.01 && tl.shown <= tl.view.viewEnd + 0.01
        width: Math.min(Math.max(tl.ppf, 22 * tl.k), 46 * tl.k)
        height: strip.height - 4 * tl.k
        x: tl.frameToX(tl.shown) - width / 2
        y: strip.y + 2 * tl.k
        color: "#a39a2c"
        opacity: 0.85
        Text {
            anchors.centerIn: parent
            text: String(Math.round(tl.shown))
            color: "#fffbe0"
            font.pixelSize: 30 * tl.k
        }
    }

    // ---- toque na regua: 1 dedo move o frame, 2 dedos dao zoom e deslocam a janela ----
    MultiPointTouchArea {
        id: touch
        x: tl.stripX
        y: tl.stripY - 24 * tl.k
        width: tl.stripW
        height: tl.stripH + 48 * tl.k
        minimumTouchPoints: 1
        maximumTouchPoints: 2
        containmentMask: HitMask { target: touch; view: tl.view }
        touchPoints: [ TouchPoint { id: tp1 }, TouchPoint { id: tp2 } ]

        property bool pinching: false
        property bool ignoreRest: false
        property real d0: 1
        property real mid0: 0
        property real vs0: 0
        property real ve0: 1

        onTouchUpdated: function (points) {
            var act = []
            for (var i = 0; i < points.length; i++) {
                if (points[i].pressed) act.push(points[i])
            }
            if (act.length === 0) {
                tl.scrubbing = false
                pinching = false
                ignoreRest = false
                return
            }
            var pad = tl.rulerX - tl.stripX
            if (act.length >= 2) {
                tl.scrubbing = false
                ignoreRest = true
                var a = act[0], b = act[1]
                var dist = Math.max(1, Math.hypot(a.x - b.x, a.y - b.y))
                var mid = (a.x + b.x) / 2
                if (!pinching) {
                    pinching = true
                    d0 = dist
                    mid0 = mid
                    vs0 = tl.view.viewStart
                    ve0 = tl.view.viewEnd
                    return
                }
                var span0 = Math.max(1, ve0 - vs0)
                var full = Math.max(10, tl.view.frameEnd - tl.view.frameStart)
                var sp = Math.max(10, Math.min(full, span0 * d0 / dist))
                var w = tl.rulerW
                var c0 = vs0 + (mid0 - pad) / w * span0
                var nvs = c0 - (mid - pad) / w * sp
                nvs = Math.max(tl.view.frameStart, Math.min(tl.view.frameEnd - sp, nvs))
                tl.view.command(10, Math.round(nvs), Math.round(nvs + sp))
                return
            }
            if (ignoreRest) return
            if (!tl.scrubbing && tl.view.playing) tl.view.command(2)
            tl.scrubbing = true
            tl.scrubTo(act[0].x - pad)
        }
    }

    // ---- painel de transporte ----
    Rectangle {
        x: 109 * tl.k
        y: 767 * tl.k
        width: 456 * tl.k
        height: 91 * tl.k
        color: "#454545"
        MouseArea {   // nao deixa o toque cair no viewport entre os botoes
            id: tpMa
            anchors.fill: parent
            containmentMask: HitMask { target: tpMa; view: tl.view }
        }
    }
    Rectangle { x: 213 * tl.k; y: 783 * tl.k; width: Math.max(1, 3 * tl.k); height: 60 * tl.k; color: "#2b2b2b" }
    Rectangle { x: 462 * tl.k; y: 783 * tl.k; width: Math.max(1, 3 * tl.k); height: 60 * tl.k; color: "#2b2b2b" }
    Rectangle {   // play ativo
        visible: tl.view.playing
        x: 298 * tl.k
        y: 771 * tl.k
        width: 76 * tl.k
        height: 83 * tl.k
        radius: 6 * tl.k
        color: "#4773b3"
        opacity: 0.55
    }
    Glyph { name: "first"; x: (164 - 30) * tl.k; y: (812 - 30) * tl.k; width: 60 * tl.k; height: 60 * tl.k }
    Glyph { name: "prev";  x: (257 - 30) * tl.k; y: (812 - 30) * tl.k; width: 60 * tl.k; height: 60 * tl.k }
    Glyph { name: tl.view.playing ? "pause" : "play"; x: (334 - 32) * tl.k; y: (812 - 32) * tl.k; width: 64 * tl.k; height: 64 * tl.k }
    Glyph { name: "corner"; x: 356 * tl.k; y: 832 * tl.k; width: 12 * tl.k; height: 12 * tl.k }
    Glyph { name: "next";  x: (414 - 30) * tl.k; y: (812 - 30) * tl.k; width: 60 * tl.k; height: 60 * tl.k }
    Glyph { name: "last";  x: (505 - 30) * tl.k; y: (812 - 30) * tl.k; width: 60 * tl.k; height: 60 * tl.k }
    Repeater {
        // [x0, x1, comando]  (x em pixels do conceito)
        model: [
            { x0: 109, x1: 214, c: 0 },
            { x0: 214, x1: 296, c: 1 },
            { x0: 296, x1: 374, c: 2 },
            { x0: 374, x1: 463, c: 3 },
            { x0: 463, x1: 565, c: 4 }
        ]
        Item {
            id: cell
            required property var modelData
            x: modelData.x0 * tl.k
            y: 767 * tl.k
            width: (modelData.x1 - modelData.x0) * tl.k
            height: 91 * tl.k
            Rectangle {
                visible: cma.pressed
                anchors.fill: parent
                color: "white"
                opacity: 0.14
            }
            MouseArea {
                id: cma
                anchors.fill: parent
                containmentMask: HitMask { target: cma; view: tl.view }
                onClicked: {
                    switch (cell.modelData.c) {
                    case 0: tl.view.command(0); break                    // inicio
                    case 1: tl.stepFrame(-1); break                      // frame anterior
                    case 2:                                              // play / pausa
                        if (tl.view.playing) tl.view.command(2)
                        else tl.view.command(3)
                        break
                    case 3: tl.stepFrame(1); break                       // proximo frame
                    case 4: tl.view.command(4); break                    // fim
                    }
                }
                onPressAndHold: {
                    if (cell.modelData.c === 2) tl.view.command(1)       // segurar o play: reproduz ao contrario
                }
            }
        }
    }

    // ---- painel de keys: botao de key, Auto Key, Set Key ----
    Rectangle {
        id: keyPanel
        x: tl.tw - 365 * tl.k
        y: 747 * tl.k
        width: 242 * tl.k
        height: 111 * tl.k
        color: "#454545"
        MouseArea {
            id: kpMa
            anchors.fill: parent
            containmentMask: HitMask { target: kpMa; view: tl.view }
        }
    }
    // botao de key: toque insere, segurar apaga a key do frame
    Rectangle {
        x: tl.tw - (2400 - 2045) * tl.k
        y: 757 * tl.k
        width: 104 * tl.k
        height: 92 * tl.k
        radius: 6 * tl.k
        color: kma.pressed ? "#767676" : "#636363"
        Glyph { anchors.centerIn: parent; name: "key"; fg: "#f0f0f0"; width: 72 * tl.k; height: 72 * tl.k }
        MouseArea {
            id: kma
            anchors.fill: parent
            containmentMask: HitMask { target: kma; view: tl.view }
            onClicked: tl.view.command(7)
            onPressAndHold: tl.view.command(8)
        }
    }
    // Auto Key (liga / desliga)
    Rectangle {
        x: tl.tw - (2400 - 2158) * tl.k
        y: 755 * tl.k
        width: 115 * tl.k
        height: 50 * tl.k
        radius: 5 * tl.k
        color: tl.view.autoKey ? "#4773b3" : (ama.pressed ? "#767676" : "#616161")
        Text { anchors.centerIn: parent; text: "Auto Key"; color: "#f0f0f0"; font.pixelSize: 30 * tl.k }
        MouseArea {
            id: ama
            anchors.fill: parent
            containmentMask: HitMask { target: ama; view: tl.view }
            onClicked: tl.view.command(22)
        }
    }
    // Set Key
    Rectangle {
        x: tl.tw - (2400 - 2158) * tl.k
        y: 808 * tl.k
        width: 115 * tl.k
        height: 41 * tl.k
        radius: 5 * tl.k
        color: sma.pressed ? "#767676" : "#616161"
        Text { anchors.centerIn: parent; text: "Set Key"; color: "#f0f0f0"; font.pixelSize: 30 * tl.k }
        MouseArea {
            id: sma
            anchors.fill: parent
            containmentMask: HitMask { target: sma; view: tl.view }
            onClicked: tl.view.command(7)
        }
    }

    // mantem o indicador dentro da janela visivel (reproducao ou frame digitado)
    Connections {
        target: tl.view
        function onTimelineChanged() {
            if (tl.scrubbing || touch.pinching) return
            var f = tl.view.frame
            if (f < tl.view.viewStart - 0.01 || f > tl.view.viewEnd + 0.01) {
                var sp = tl.view.viewEnd - tl.view.viewStart
                var ns = Math.max(tl.view.frameStart, Math.min(tl.view.frameEnd - sp, Math.round(f - sp / 2)))
                tl.view.command(10, ns, ns + sp)
            }
        }
    }
}

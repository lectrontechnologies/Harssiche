import QtQuick
import QtQuick.Shapes

// Timeline no estilo do conceito: aba elevada com transporte, frame atual e keys,
// e uma regua com keyframes em verde e o indicador do frame.
// Todas as medidas partem de um desenho de 800x360 e escalam com a altura da tela.
Item {
    id: tl
    required property var view   // EngineView

    readonly property real s: height > 0 ? height / 360 : 1
    readonly property real barH: 70 * s
    readonly property real tabH: 37 * s
    readonly property real barTop: height - barH
    readonly property real tabTop: barTop - tabH
    readonly property real btnY: tabTop + 7.3 * s
    readonly property real rulerX: 85 * s
    readonly property real rulerW: Math.max(10, width - 158 * s)
    readonly property real lineY: 38 * s          // linha da regua (relativa ao topo da barra)
    readonly property real span: Math.max(1, view.viewEnd - view.viewStart)

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

    // ---- fundo: barra e aba ----
    Rectangle {
        x: 0
        y: tl.barTop
        width: tl.width
        height: tl.barH
        color: "#2f2f2f"
        MouseArea { anchors.fill: parent }   // nao deixa o toque passar para o viewport
    }

    Shape {
        x: 0
        y: tl.tabTop
        width: 380
        height: 38
        scale: tl.s
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: "#2f2f2f"
            strokeColor: "transparent"
            PathSvg { path: "M 50 38 C 66 38 72 30 75 16 Q 76 0 92 0 L 328 0 Q 344 0 345 16 C 348 30 356 38 372 38 Z" }
        }
    }
    MouseArea {
        x: 60 * tl.s
        y: tl.tabTop
        width: 302 * tl.s
        height: tl.tabH
    }

    // ---- transporte ----
    Row {
        x: 87 * tl.s
        y: tl.btnY
        spacing: 0
        TlButton { s: tl.s; icon: "start"; onClicked: tl.view.command(0) }
        TlButton { s: tl.s; icon: "rev"; active: tl.view.playing && tl.view.playDir < 0; onClicked: tl.view.command(1) }
        TlButton { s: tl.s; icon: "stop"; active: !tl.view.playing; onClicked: tl.view.command(2) }
        TlButton { s: tl.s; icon: "play"; active: tl.view.playing && tl.view.playDir > 0; onClicked: tl.view.command(3) }
        TlButton { s: tl.s; icon: "end"; onClicked: tl.view.command(4) }
    }

    // ---- frame atual (toque para digitar) ----
    Rectangle {
        x: 227 * tl.s
        y: tl.btnY
        width: 46 * tl.s
        height: 27 * tl.s
        color: "#111111"
        border.color: "#222222"
        border.width: Math.max(1, tl.s * 0.6)

        TextInput {
            id: fld
            anchors.fill: parent
            anchors.leftMargin: 5 * tl.s
            verticalAlignment: TextInput.AlignVCenter
            color: "#f2f2f2"
            font.pixelSize: 11.5 * tl.s
            inputMethodHints: Qt.ImhDigitsOnly
            selectByMouse: true
            onAccepted: {
                var f = parseInt(text)
                if (!isNaN(f)) tl.view.command(9, f)
                Qt.inputMethod.hide()
                tl.view.forceActiveFocus()
            }
        }
        Binding {
            target: fld
            property: "text"
            value: String(Math.round(tl.shown))
            when: !fld.activeFocus
        }
    }

    // ---- key anterior / proxima ----
    Row {
        x: 275 * tl.s
        y: tl.btnY
        spacing: 0
        TlButton { s: tl.s; width: 13 * tl.s; iconSize: 14 * tl.s; icon: "prevkey"; onClicked: tl.view.command(5) }
        TlButton { s: tl.s; width: 13 * tl.s; iconSize: 14 * tl.s; icon: "nextkey"; onClicked: tl.view.command(6) }
    }

    // ---- botao de key (toque insere, segurar apaga a key do frame) ----
    Rectangle {
        x: 311.5 * tl.s
        y: tl.btnY
        width: 27 * tl.s
        height: 27 * tl.s
        radius: width / 2
        color: kma.pressed ? "#2d5559" : "#26474a"
        border.color: "#4aa6a6"
        border.width: Math.max(1, tl.s)
        TlIcon { anchors.centerIn: parent; name: "key"; size: 18 * tl.s }
        MouseArea {
            id: kma
            anchors.fill: parent
            onClicked: tl.view.command(7)
            onPressAndHold: tl.view.command(8)
        }
    }

    // ---- regua ----
    Canvas {
        id: ruler
        x: tl.rulerX
        y: tl.barTop
        width: tl.rulerW
        height: tl.barH
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
            var s = tl.s
            var sp = Math.max(1, ve - vs)
            var ppf = width / sp
            var steps = [1, 2, 3, 6, 12, 24, 48, 96, 192, 480, 960, 2400]
            var step = 2400
            for (var i = 0; i < steps.length; i++) {
                if (steps[i] * ppf >= 60 * s) { step = steps[i]; break }
            }
            var minor = step >= 6 ? step / 6 : 0
            var ly = tl.lineY
            ctx.lineWidth = Math.max(1, 1.1 * s)
            ctx.strokeStyle = "#cfcfcf"
            ctx.fillStyle = "#858585"
            ctx.font = Math.round(11 * s) + "px sans-serif"
            ctx.textBaseline = "middle"

            ctx.beginPath()
            ctx.moveTo(0, ly)
            ctx.lineTo(width, ly)
            ctx.stroke()

            ctx.textAlign = "center"
            for (var f = vs; f <= ve; f++) {
                var x = (f - vs) * ppf
                if (f % step === 0) {
                    ctx.beginPath()
                    ctx.moveTo(x, ly - 11 * s)
                    ctx.lineTo(x, ly + 11 * s)
                    ctx.stroke()
                    ctx.fillText(String(f), x, 12 * s)
                } else if (minor > 0 && f % minor === 0) {
                    ctx.beginPath()
                    ctx.moveTo(x, ly - 6.5 * s)
                    ctx.lineTo(x, ly + 6.5 * s)
                    ctx.stroke()
                }
            }
            // primeiro frame visivel, quando nao e multiplo do passo (como no conceito)
            if (vs % step !== 0) {
                var nextMajor = Math.ceil(vs / step) * step
                if ((nextMajor - vs) * ppf > 45 * s) {
                    ctx.beginPath()
                    ctx.moveTo(0, ly - 11 * s)
                    ctx.lineTo(0, ly + 11 * s)
                    ctx.stroke()
                    ctx.textAlign = "left"
                    ctx.fillText(String(vs), 3 * s, 12 * s)
                }
            }
        }
    }

    // ---- keyframes (barras verdes) ----
    Repeater {
        model: tl.view.keyFrames
        Rectangle {
            required property var modelData
            visible: modelData >= tl.view.viewStart - 0.5 && modelData <= tl.view.viewEnd + 0.5
            width: Math.max(3 * tl.s, Math.min(8 * tl.s, tl.rulerW / tl.span * 1.1))
            height: 35 * tl.s
            x: tl.frameToX(modelData) - width / 2
            y: tl.barTop + tl.lineY - height / 2
            color: "#3f8a06"
            opacity: 0.95
        }
    }

    // ---- indicador do frame ----
    Item {
        visible: tl.shown >= tl.view.viewStart - 0.01 && tl.shown <= tl.view.viewEnd + 0.01
        x: tl.frameToX(tl.shown)
        y: tl.barTop

        Rectangle {   // ponta da seta (aparece so a metade de baixo)
            width: 9 * tl.s
            height: 9 * tl.s
            rotation: 45
            x: -width / 2
            y: 27 * tl.s
            color: "#050505"
            border.color: "#f2f2f2"
            border.width: Math.max(1, tl.s * 0.8)
        }
        Rectangle {
            width: Math.max(36 * tl.s, lbl.implicitWidth + 12 * tl.s)
            height: 25 * tl.s
            x: -width / 2
            y: 7 * tl.s
            radius: 2 * tl.s
            color: "#050505"
            border.color: "#f2f2f2"
            border.width: Math.max(1, tl.s * 0.8)
            Text {
                id: lbl
                anchors.centerIn: parent
                text: String(Math.round(tl.shown))
                color: "white"
                font.pixelSize: 15 * tl.s
            }
        }
    }

    // ---- toque na regua: 1 dedo move o frame, 2 dedos dao zoom e deslocam a janela ----
    MultiPointTouchArea {
        id: touch
        x: tl.rulerX - 8 * tl.s
        y: tl.barTop
        width: tl.rulerW + 16 * tl.s
        height: tl.barH
        minimumTouchPoints: 1
        maximumTouchPoints: 2
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
            var pad = 8 * tl.s
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

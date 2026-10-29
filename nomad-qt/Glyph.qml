import QtQuick

// Icones desenhados em codigo (vetor), para ferramentas, transporte e keys.
Canvas {
    id: g
    property string name: ""
    property color fg: "#d6d6d6"
    property color accent: "#2fc4c4"
    renderTarget: Canvas.Image
    onNameChanged: requestPaint()
    onFgChanged: requestPaint()
    onAccentChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function tri(ctx, ax, ay, bx, by, cx, cy) {
        ctx.beginPath(); ctx.moveTo(ax, ay); ctx.lineTo(bx, by); ctx.lineTo(cx, cy); ctx.closePath(); ctx.fill()
    }
    function bar(ctx, x, y, w, h) { ctx.fillRect(x, y, w, h) }

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        var s = Math.min(width, height)
        if (s < 2) return
        ctx.fillStyle = fg
        ctx.strokeStyle = fg
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        var n = name
        if (n === "move") {
            var c = s / 2, L = s * 0.30, hw = s * 0.12, hl = s * 0.17
            ctx.lineWidth = s * 0.10
            ctx.beginPath()
            ctx.moveTo(c - L, c); ctx.lineTo(c + L, c)
            ctx.moveTo(c, c - L); ctx.lineTo(c, c + L)
            ctx.stroke()
            var e = s * 0.47
            tri(ctx, c, c - e, c - hw, c - e + hl, c + hw, c - e + hl)
            tri(ctx, c, c + e, c - hw, c + e - hl, c + hw, c + e - hl)
            tri(ctx, c - e, c, c - e + hl, c - hw, c - e + hl, c + hw)
            tri(ctx, c + e, c, c + e - hl, c - hw, c + e - hl, c + hw)
        } else if (n === "rotate") {
            // duas setas circulares (estilo "sincronizar")
            var cx = s * 0.5, cy = s * 0.5, r = s * 0.31
            ctx.lineWidth = s * 0.07
            var arcs = [[1.08 * Math.PI, 1.86 * Math.PI], [0.08 * Math.PI, 0.86 * Math.PI]]
            for (var q = 0; q < 2; q++) {
                var b0 = arcs[q][0], b1 = arcs[q][1]
                ctx.beginPath(); ctx.arc(cx, cy, r, b0, b1, false); ctx.stroke()
                var ex = cx + r * Math.cos(b1), ey = cy + r * Math.sin(b1)
                var tx = -Math.sin(b1), ty = Math.cos(b1), nx = Math.cos(b1), ny = Math.sin(b1)
                var hh = s * 0.15, ww = s * 0.10
                tri(ctx, ex + tx * hh, ey + ty * hh, ex + nx * ww, ey + ny * ww, ex - nx * ww, ey - ny * ww)
            }
        } else if (n === "scale") {
            // quadrado com um menor preenchido no canto e a diagonal do redimensionar
            ctx.lineWidth = s * 0.07
            ctx.strokeRect(s * 0.14, s * 0.14, s * 0.72, s * 0.72)
            ctx.fillRect(s * 0.14, s * 0.46, s * 0.40, s * 0.40)
            ctx.lineWidth = s * 0.07
            ctx.beginPath()
            ctx.moveTo(s * 0.46, s * 0.54); ctx.lineTo(s * 0.86, s * 0.14)
            ctx.stroke()
        } else if (n === "cursor") {
            ctx.lineWidth = s * 0.07
            ctx.beginPath()
            ctx.moveTo(s * 0.18, s * 0.10)
            ctx.lineTo(s * 0.86, s * 0.50)
            ctx.lineTo(s * 0.54, s * 0.58)
            ctx.lineTo(s * 0.40, s * 0.90)
            ctx.closePath()
            ctx.stroke()
        } else if (n === "cube") {
            var P = function (x, y) { return [x * s, y * s] }
            var top = P(0.5, 0.07), ur = P(0.88, 0.29), lr = P(0.88, 0.71), bt = P(0.5, 0.93), ll = P(0.12, 0.71), ul = P(0.12, 0.29), cc = P(0.5, 0.50)
            var face = function (pts, col) {
                ctx.fillStyle = col
                ctx.beginPath(); ctx.moveTo(pts[0][0], pts[0][1])
                for (var i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1])
                ctx.closePath(); ctx.fill()
            }
            face([top, ur, cc, ul], Qt.lighter(fg, 1.12).toString())
            face([ul, cc, bt, ll], Qt.darker(fg, 1.25).toString())
            face([cc, ur, lr, bt], Qt.darker(fg, 1.7).toString())
            ctx.strokeStyle = "#000000"
            ctx.lineWidth = s * 0.025
            ctx.beginPath()
            ctx.moveTo(ul[0], ul[1]); ctx.lineTo(cc[0], cc[1]); ctx.lineTo(ur[0], ur[1])
            ctx.moveTo(cc[0], cc[1]); ctx.lineTo(bt[0], bt[1])
            ctx.stroke()
        } else if (n === "first" || n === "last") {
            var m = (n === "last")
            var bw = s * 0.09
            var h = s * 0.50
            var y0 = (s - h) / 2
            var p = s * 0.16
            if (!m) {
                bar(ctx, p, y0, bw, h)
                tri(ctx, p + bw + s * 0.26, y0, p + bw + s * 0.26, y0 + h, p + bw, s / 2)
                tri(ctx, p + bw + s * 0.50, y0, p + bw + s * 0.50, y0 + h, p + bw + s * 0.26, s / 2)
            } else {
                bar(ctx, s - p - bw, y0, bw, h)
                tri(ctx, s - p - bw - s * 0.26, y0, s - p - bw - s * 0.26, y0 + h, s - p - bw, s / 2)
                tri(ctx, s - p - bw - s * 0.50, y0, s - p - bw - s * 0.50, y0 + h, s - p - bw - s * 0.26, s / 2)
            }
        } else if (n === "prev") {
            var h2 = s * 0.50, y2 = (s - h2) / 2, b2 = s * 0.12
            tri(ctx, s * 0.14, s / 2, s * 0.14 + s * 0.30, y2, s * 0.14 + s * 0.30, y2 + h2)
            bar(ctx, s * 0.50, y2, b2, h2)
            bar(ctx, s * 0.50 + b2 + s * 0.08, y2, b2, h2)
        } else if (n === "next") {
            var h3 = s * 0.50, y3 = (s - h3) / 2, b3 = s * 0.12
            bar(ctx, s * 0.14, y3, b3, h3)
            bar(ctx, s * 0.14 + b3 + s * 0.08, y3, b3, h3)
            tri(ctx, s * 0.86, s / 2, s * 0.86 - s * 0.30, y3, s * 0.86 - s * 0.30, y3 + h3)
        } else if (n === "play") {
            tri(ctx, s * 0.22, s * 0.14, s * 0.22, s * 0.86, s * 0.84, s * 0.5)
        } else if (n === "pause") {
            bar(ctx, s * 0.22, s * 0.16, s * 0.2, s * 0.68)
            bar(ctx, s * 0.58, s * 0.16, s * 0.2, s * 0.68)
        } else if (n === "chev_down" || n === "chev_right") {
            ctx.lineWidth = s * 0.14
            ctx.beginPath()
            if (n === "chev_down") { ctx.moveTo(s * 0.22, s * 0.36); ctx.lineTo(s * 0.5, s * 0.64); ctx.lineTo(s * 0.78, s * 0.36) }
            else { ctx.moveTo(s * 0.36, s * 0.22); ctx.lineTo(s * 0.64, s * 0.5); ctx.lineTo(s * 0.36, s * 0.78) }
            ctx.stroke()
        } else if (n === "collection") {
            ctx.lineWidth = s * 0.09
            ctx.strokeRect(s * 0.16, s * 0.34, s * 0.68, s * 0.5)
            ctx.fillRect(s * 0.12, s * 0.18, s * 0.76, s * 0.16)
            ctx.fillRect(s * 0.38, s * 0.5, s * 0.24, s * 0.08)
        } else if (n === "check_box") {
            ctx.fillRect(s * 0.14, s * 0.14, s * 0.72, s * 0.72)
            ctx.strokeStyle = "#262626"
            ctx.lineWidth = s * 0.12
            ctx.beginPath(); ctx.moveTo(s * 0.28, s * 0.52); ctx.lineTo(s * 0.44, s * 0.68); ctx.lineTo(s * 0.74, s * 0.32); ctx.stroke()
        } else if (n === "eye" || n === "eye_off") {
            ctx.lineWidth = s * 0.08
            ctx.beginPath()
            ctx.moveTo(s * 0.06, s * 0.5)
            ctx.quadraticCurveTo(s * 0.5, s * 0.08, s * 0.94, s * 0.5)
            ctx.quadraticCurveTo(s * 0.5, s * 0.92, s * 0.06, s * 0.5)
            ctx.stroke()
            ctx.beginPath(); ctx.arc(s * 0.5, s * 0.5, s * 0.15, 0, 2 * Math.PI); ctx.fill()
            if (n === "eye_off") {
                ctx.lineWidth = s * 0.09
                ctx.beginPath(); ctx.moveTo(s * 0.16, s * 0.84); ctx.lineTo(s * 0.84, s * 0.16); ctx.stroke()
            }
        } else if (n === "render" || n === "render_off") {
            ctx.lineWidth = s * 0.09
            ctx.beginPath()
            ctx.moveTo(s * 0.10, s * 0.30); ctx.lineTo(s * 0.32, s * 0.30); ctx.lineTo(s * 0.38, s * 0.18)
            ctx.lineTo(s * 0.62, s * 0.18); ctx.lineTo(s * 0.68, s * 0.30); ctx.lineTo(s * 0.90, s * 0.30)
            ctx.lineTo(s * 0.90, s * 0.80); ctx.lineTo(s * 0.10, s * 0.80); ctx.closePath()
            ctx.stroke()
            ctx.beginPath(); ctx.arc(s * 0.5, s * 0.54, s * 0.17, 0, 2 * Math.PI); ctx.stroke()
            if (n === "render_off") {
                ctx.beginPath(); ctx.moveTo(s * 0.14, s * 0.88); ctx.lineTo(s * 0.86, s * 0.12); ctx.stroke()
            }
        } else if (n === "o_cam") {
            ctx.beginPath(); ctx.arc(s * 0.28, s * 0.26, s * 0.17, 0, 2 * Math.PI); ctx.fill()
            ctx.beginPath(); ctx.arc(s * 0.62, s * 0.26, s * 0.17, 0, 2 * Math.PI); ctx.fill()
            ctx.fillRect(s * 0.10, s * 0.38, s * 0.62, s * 0.44)
            tri(ctx, s * 0.92, s * 0.30, s * 0.92, s * 0.90, s * 0.70, s * 0.60)
        } else if (n === "o_mesh") {
            ctx.fillRect(s * 0.10, s * 0.10, s * 0.80, s * 0.80)
            ctx.fillStyle = "#262626"
            tri(ctx, s * 0.26, s * 0.30, s * 0.74, s * 0.30, s * 0.5, s * 0.74)
        } else if (n === "o_light") {
            ctx.beginPath(); ctx.arc(s * 0.5, s * 0.40, s * 0.28, 0, 2 * Math.PI); ctx.fill()
            ctx.fillRect(s * 0.36, s * 0.70, s * 0.28, s * 0.08)
            ctx.fillRect(s * 0.40, s * 0.82, s * 0.20, s * 0.08)
        } else if (n === "o_bone") {
            ctx.lineWidth = s * 0.10
            ctx.beginPath(); ctx.moveTo(s * 0.25, s * 0.75); ctx.lineTo(s * 0.75, s * 0.25); ctx.stroke()
            ctx.beginPath(); ctx.arc(s * 0.25, s * 0.75, s * 0.14, 0, 2 * Math.PI); ctx.fill()
            ctx.beginPath(); ctx.arc(s * 0.75, s * 0.25, s * 0.14, 0, 2 * Math.PI); ctx.fill()
        } else if (n === "d_cam") {
            ctx.lineWidth = s * 0.10
            ctx.beginPath(); ctx.moveTo(s * 0.62, s * 0.14); ctx.lineTo(s * 0.34, s * 0.14); ctx.lineTo(s * 0.34, s * 0.86); ctx.lineTo(s * 0.62, s * 0.86); ctx.stroke()
        } else if (n === "d_mesh") {
            ctx.lineWidth = s * 0.09
            ctx.beginPath(); ctx.moveTo(s * 0.12, s * 0.22); ctx.lineTo(s * 0.88, s * 0.22); ctx.lineTo(s * 0.5, s * 0.84); ctx.closePath(); ctx.stroke()
            ctx.beginPath(); ctx.arc(s * 0.12, s * 0.22, s * 0.08, 0, 2 * Math.PI); ctx.fill()
            ctx.beginPath(); ctx.arc(s * 0.88, s * 0.22, s * 0.08, 0, 2 * Math.PI); ctx.fill()
            ctx.beginPath(); ctx.arc(s * 0.5, s * 0.84, s * 0.08, 0, 2 * Math.PI); ctx.fill()
        } else if (n === "d_light") {
            ctx.lineWidth = s * 0.09
            ctx.beginPath(); ctx.arc(s * 0.5, s * 0.5, s * 0.36, 0, 2 * Math.PI); ctx.stroke()
            ctx.beginPath(); ctx.arc(s * 0.5, s * 0.5, s * 0.09, 0, 2 * Math.PI); ctx.fill()
        } else if (n === "corner") {
            tri(ctx, s, 0, s, s, 0, s)
        } else if (n === "key") {
            ctx.lineWidth = s * 0.13
            ctx.beginPath()
            ctx.moveTo(s * 0.06, s * 0.34); ctx.lineTo(s * 0.62, s * 0.34)
            ctx.moveTo(s * 0.34, s * 0.06); ctx.lineTo(s * 0.34, s * 0.62)
            ctx.stroke()
            ctx.strokeStyle = accent
            ctx.fillStyle = accent
            ctx.lineWidth = s * 0.07
            ctx.beginPath(); ctx.arc(s * 0.70, s * 0.70, s * 0.10, 0, 2 * Math.PI); ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(s * 0.77, s * 0.77); ctx.lineTo(s * 0.94, s * 0.94)
            ctx.moveTo(s * 0.86, s * 0.86); ctx.lineTo(s * 0.80, s * 0.92)
            ctx.stroke()
        }
    }
}

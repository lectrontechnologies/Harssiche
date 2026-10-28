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
            var cx = s * 0.5, cy = s * 0.54, r = s * 0.30
            var a0 = 0.18 * Math.PI, a1 = 1.72 * Math.PI
            ctx.lineWidth = s * 0.11
            ctx.beginPath(); ctx.arc(cx, cy, r, a0, a1, false); ctx.stroke()
            var ex = cx + r * Math.cos(a1), ey = cy + r * Math.sin(a1)
            var tx = -Math.sin(a1), ty = Math.cos(a1), nx = Math.cos(a1), ny = Math.sin(a1)
            var hh = s * 0.20, ww = s * 0.17
            tri(ctx, ex + tx * hh, ey + ty * hh, ex + nx * ww, ey + ny * ww, ex - nx * ww, ey - ny * ww)
        } else if (n === "scale") {
            ctx.strokeStyle = accent
            ctx.lineWidth = s * 0.07
            ctx.setLineDash([s * 0.12, s * 0.07])
            ctx.strokeRect(s * 0.14, s * 0.14, s * 0.72, s * 0.72)
            ctx.setLineDash([])
            ctx.fillStyle = fg
            ctx.fillRect(s * 0.14, s * 0.50, s * 0.36, s * 0.36)
            ctx.strokeStyle = accent
            ctx.lineWidth = s * 0.07
            ctx.beginPath()
            ctx.moveTo(s * 0.52, s * 0.34); ctx.lineTo(s * 0.66, s * 0.34); ctx.lineTo(s * 0.66, s * 0.48)
            ctx.moveTo(s * 0.66, s * 0.34); ctx.lineTo(s * 0.50, s * 0.50)
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

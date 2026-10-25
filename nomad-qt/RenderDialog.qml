import QtQuick

// Tela de render (conceito do usuario): configuracoes a esquerda, previa e botoes a direita.
// Se a cena tem keyframes, "Renderizar" faz um video (do primeiro ao ultimo key, em 30 fps por padrao);
// se nao tem nenhum, renderiza uma imagem. A camera exporta a imagem e o botao de video exporta o video.
Item {
    id: dlg
    required property var view   // EngineView
    visible: false

    readonly property real s: height > 0 ? height / 360 : 1
    property int resW: 1280
    property int resH: 720
    property real ratio: 16 / 9
    property bool linked: true
    property int fps: 30
    property int fromF: 1
    property int toF: 120
    property bool night: false
    property bool edge: true

    function open() {
        fromF = view.anyKeys ? view.keyFirst : view.frameStart
        toF = view.anyKeys ? view.keyLast : view.frameEnd
        visible = true
    }
    function close() {
        visible = false
        view.forceActiveFocus()
    }
    function preset(i) {
        var w = [854, 1280, 1920][i]
        resW = w
        resH = Math.round(w / ratio / 2) * 2
    }
    function setW(t) {
        var v = parseInt(t)
        if (isNaN(v)) return
        resW = Math.max(64, Math.min(4096, v))
        if (linked) resH = Math.max(64, Math.round(resW / ratio / 2) * 2)
    }
    function setH(t) {
        var v = parseInt(t)
        if (isNaN(v)) return
        resH = Math.max(64, Math.min(4096, v))
        if (linked) resW = Math.max(64, Math.round(resH * ratio / 2) * 2)
    }
    function apply() {
        view.command(12, resW, resH)
        view.command(13, fps)
        view.command(14, fromF, toF)
        view.command(15, night ? 1 : 0)
        view.command(16, edge ? 1 : 0)
    }

    Connections {
        target: dlg.view
        function onRenderRequested() { dlg.open() }
    }

    // bloqueia toques no viewport enquanto a tela esta aberta
    MouseArea { anchors.fill: parent }

    Rectangle {
        id: panel
        width: 455 * dlg.s
        height: 259 * dlg.s
        anchors.centerIn: parent
        color: "#2a2a2a"
        MouseArea { anchors.fill: parent }

        // ================= coluna esquerda (configuracoes) =================
        Flickable {
            id: left
            x: 0
            y: 0
            width: 184 * dlg.s
            height: parent.height
            contentWidth: width
            contentHeight: col.height + 20 * dlg.s
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: col
                x: 18 * dlg.s
                width: 152 * dlg.s
                spacing: 8 * dlg.s
                topPadding: 12 * dlg.s

                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: "Resolução"; color: "#eeeeee"; font.pixelSize: 14 * dlg.s }
                RdSeg {
                    s: dlg.s; width: parent.width
                    labels: ["480p", "720p", "1080p"]
                    index: dlg.resW === 854 ? 0 : (dlg.resW === 1280 ? 1 : (dlg.resW === 1920 ? 2 : -1))
                    onPicked: function (i) { dlg.preset(i) }
                }

                // largura / altura com o botao de vinculo
                Item {
                    width: parent.width
                    height: 58 * dlg.s
                    Text { x: 0; y: 6 * dlg.s; text: "Largura"; color: "#eeeeee"; font.pixelSize: 13 * dlg.s }
                    RdField { s: dlg.s; view: dlg.view; x: 62 * dlg.s; y: 0; width: 64 * dlg.s; height: 26 * dlg.s; text: String(dlg.resW); onCommitted: function (t) { dlg.setW(t) } }
                    Text { x: 0; y: 38 * dlg.s; text: "Altura"; color: "#eeeeee"; font.pixelSize: 13 * dlg.s }
                    RdField { s: dlg.s; view: dlg.view; x: 62 * dlg.s; y: 32 * dlg.s; width: 64 * dlg.s; height: 26 * dlg.s; text: String(dlg.resH); onCommitted: function (t) { dlg.setH(t) } }
                    Rectangle {
                        x: 132 * dlg.s; y: 12 * dlg.s; width: 20 * dlg.s; height: 34 * dlg.s
                        radius: 4 * dlg.s
                        color: dlg.linked ? "#3d3d3d" : "transparent"
                        Image {
                            anchors.centerIn: parent
                            source: "icons/rd_link.png"
                            width: 8 * dlg.s; height: 20 * dlg.s
                            smooth: true; mipmap: true
                            opacity: dlg.linked ? 1.0 : 0.45
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: { dlg.linked = !dlg.linked; if (dlg.linked) dlg.ratio = dlg.resW / Math.max(1, dlg.resH) }
                        }
                    }
                }

                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; topPadding: 6 * dlg.s; text: "Tempo"; color: "#eeeeee"; font.pixelSize: 14 * dlg.s }
                Text {
                    visible: !dlg.view.anyKeys
                    width: parent.width; wrapMode: Text.WordWrap
                    text: "Sem keyframes: será renderizada uma imagem."
                    color: "#9a9a9a"; font.pixelSize: 11 * dlg.s
                }
                Item {
                    width: parent.width; height: 26 * dlg.s
                    Text { x: 0; anchors.verticalCenter: parent.verticalCenter; text: "Taxa de Quadros"; color: "#eeeeee"; font.pixelSize: 12 * dlg.s }
                    RdField { s: dlg.s; view: dlg.view; x: 96 * dlg.s; width: 56 * dlg.s; height: 26 * dlg.s; dimmed: !dlg.view.anyKeys; text: String(dlg.fps)
                        onCommitted: function (t) { var v = parseInt(t); if (!isNaN(v)) dlg.fps = Math.max(1, Math.min(120, v)) } }
                }
                Item {
                    width: parent.width; height: 26 * dlg.s
                    Text { x: 0; anchors.verticalCenter: parent.verticalCenter; text: "A Partir De"; color: "#eeeeee"; font.pixelSize: 12 * dlg.s }
                    RdField { s: dlg.s; view: dlg.view; x: 96 * dlg.s; width: 56 * dlg.s; height: 26 * dlg.s; dimmed: !dlg.view.anyKeys; text: String(dlg.fromF)
                        onCommitted: function (t) { var v = parseInt(t); if (!isNaN(v)) dlg.fromF = Math.max(0, Math.min(v, dlg.toF)) } }
                }
                Item {
                    width: parent.width; height: 26 * dlg.s
                    Text { x: 0; anchors.verticalCenter: parent.verticalCenter; text: "Para"; color: "#eeeeee"; font.pixelSize: 12 * dlg.s }
                    RdField { s: dlg.s; view: dlg.view; x: 96 * dlg.s; width: 56 * dlg.s; height: 26 * dlg.s; dimmed: !dlg.view.anyKeys; text: String(dlg.toF)
                        onCommitted: function (t) { var v = parseInt(t); if (!isNaN(v)) dlg.toF = Math.max(dlg.fromF, Math.min(v, 100000)) } }
                }

                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; topPadding: 6 * dlg.s; text: "Iluminação"; color: "#eeeeee"; font.pixelSize: 14 * dlg.s }
                RdSeg { s: dlg.s; width: 110 * dlg.s; x: 21 * dlg.s; labels: ["Day", "Night"]; index: dlg.night ? 1 : 0; onPicked: function (i) { dlg.night = (i === 1) } }
                Item {
                    width: parent.width; height: 28 * dlg.s
                    Text { x: 0; anchors.verticalCenter: parent.verticalCenter; text: "Sombras de borda"; color: "#eeeeee"; font.pixelSize: 12 * dlg.s }
                    Rectangle {
                        x: 118 * dlg.s; width: 34 * dlg.s; height: 26 * dlg.s; radius: 3 * dlg.s; color: "#363636"
                        Image { anchors.centerIn: parent; source: "icons/rd_check.png"; width: 14 * dlg.s; height: 13 * dlg.s; smooth: true; mipmap: true; visible: dlg.edge }
                        MouseArea { anchors.fill: parent; onClicked: dlg.edge = !dlg.edge }
                    }
                }

                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; topPadding: 6 * dlg.s; text: "Outro"; color: "#eeeeee"; font.pixelSize: 14 * dlg.s }
                Item {
                    width: parent.width; height: 26 * dlg.s
                    Text { x: 0; anchors.verticalCenter: parent.verticalCenter; text: "Taxa de Bits"; color: "#eeeeee"; font.pixelSize: 12 * dlg.s }
                    Rectangle {
                        x: 82 * dlg.s; width: 70 * dlg.s; height: 26 * dlg.s; radius: 3 * dlg.s; color: "#363636"
                        Text { anchors.centerIn: parent; text: "Automático"; color: "#eeeeee"; font.pixelSize: 11 * dlg.s }
                    }
                }
                Text { text: "Formato: MP4"; color: "#eeeeee"; font.pixelSize: 12 * dlg.s }
            }
        }

        // ================= coluna direita (previa e botoes) =================
        Rectangle {
            x: 184 * dlg.s
            y: 0
            width: parent.width - 184 * dlg.s
            height: parent.height
            color: "#1f1f1f"
            MouseArea { anchors.fill: parent }

            // fechar
            Item {
                x: parent.width - 34 * dlg.s; y: 6 * dlg.s; width: 28 * dlg.s; height: 28 * dlg.s
                Image { anchors.centerIn: parent; source: "icons/rd_close.png"; width: 14 * dlg.s; height: 14 * dlg.s; smooth: true; mipmap: true }
                MouseArea { anchors.fill: parent; onClicked: dlg.close() }
            }

            // previa
            Rectangle {
                id: prev
                x: 30 * dlg.s; y: 21 * dlg.s
                width: 211 * dlg.s; height: 120 * dlg.s
                color: "black"
                Image {
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    source: dlg.view.previewUrl
                    cache: false
                    smooth: true
                    visible: dlg.view.previewUrl !== ""
                }
                Text {
                    anchors.centerIn: parent
                    visible: dlg.view.previewUrl === ""
                    text: "Prévia"
                    color: "#555555"
                    font.pixelSize: 13 * dlg.s
                }
                Rectangle {   // progresso do render
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 2 * dlg.s
                    color: "#555555"
                    Rectangle { width: parent.width * Math.max(0, Math.min(1, dlg.view.renderProgress)); height: parent.height; color: "#e8e8e8" }
                }
            }

            // botoes
            Row {
                x: 30 * dlg.s; y: 156 * dlg.s
                spacing: 5 * dlg.s
                Rectangle {   // camera: exporta a imagem
                    width: 34 * dlg.s; height: 32 * dlg.s; radius: 3 * dlg.s
                    color: capMa.pressed ? "#444444" : "#333333"; opacity: dlg.view.rendering ? 0.4 : 1
                    Image { anchors.centerIn: parent; source: "icons/rd_camera.png"; width: 15 * dlg.s; height: 15 * dlg.s; smooth: true; mipmap: true }
                    MouseArea { id: capMa; anchors.fill: parent; enabled: !dlg.view.rendering; onClicked: { dlg.apply(); dlg.view.command(18, 2) } }
                }
                Rectangle {   // video: exporta o video
                    width: 34 * dlg.s; height: 32 * dlg.s; radius: 3 * dlg.s
                    color: vidMa.pressed ? "#444444" : "#333333"; opacity: dlg.view.rendering ? 0.4 : 1
                    Image { anchors.centerIn: parent; source: "icons/rd_video.png"; width: 13 * dlg.s; height: 15 * dlg.s; smooth: true; mipmap: true }
                    MouseArea { id: vidMa; anchors.fill: parent; enabled: !dlg.view.rendering; onClicked: { dlg.apply(); dlg.view.command(19) } }
                }
                Rectangle {   // renderizar (video se houver keyframes, senao imagem) / cancelar
                    width: 77 * dlg.s; height: 32 * dlg.s; radius: 3 * dlg.s
                    color: rndMa.pressed ? "#444444" : "#333333"
                    Text {
                        anchors.centerIn: parent
                        text: dlg.view.rendering ? "Cancelar" : "Renderizar"
                        color: "#eeeeee"; font.pixelSize: 12 * dlg.s
                    }
                    MouseArea {
                        id: rndMa
                        anchors.fill: parent
                        onClicked: {
                            if (dlg.view.rendering) { dlg.view.command(20) }
                            else { dlg.apply(); dlg.view.command(dlg.view.anyKeys ? 19 : 18, 1) }
                        }
                    }
                }
            }

            // modo de render (ja pronto no conceito)
            Rectangle {
                x: 30 * dlg.s; y: 200 * dlg.s
                width: 77 * dlg.s; height: 32 * dlg.s; radius: 3 * dlg.s
                color: "#333333"
                Text { anchors.centerIn: parent; text: "Foundry"; color: "#eeeeee"; font.pixelSize: 13 * dlg.s }
            }
        }
    }
}

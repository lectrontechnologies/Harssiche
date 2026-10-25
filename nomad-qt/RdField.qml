import QtQuick

// Campo numerico da tela de render (fundo cinza escuro, texto claro).
Rectangle {
    id: root
    property real s: 1
    property string text: ""
    property bool dimmed: false
    property var view
    signal committed(string t)

    color: "#363636"
    radius: 3 * s
    opacity: dimmed ? 0.4 : 1.0

    TextInput {
        id: ti
        anchors.fill: parent
        anchors.leftMargin: 8 * root.s
        verticalAlignment: TextInput.AlignVCenter
        color: "#eeeeee"
        font.pixelSize: 13 * root.s
        inputMethodHints: Qt.ImhDigitsOnly
        selectByMouse: true
        enabled: !root.dimmed
        onAccepted: {
            root.committed(text)
            Qt.inputMethod.hide()
            if (root.view) root.view.forceActiveFocus()
        }
        onActiveFocusChanged: if (!activeFocus) root.committed(text)
    }
    Binding {
        target: ti
        property: "text"
        value: root.text
        when: !ti.activeFocus
    }
}

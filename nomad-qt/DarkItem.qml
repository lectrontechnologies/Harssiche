import QtQuick
import QtQuick.Controls.Basic

MenuItem {
    id: control
    implicitHeight: 48

    contentItem: Text {
        text: control.text
        color: "white"
        font.pixelSize: 16
        leftPadding: 12
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        color: control.down || control.highlighted ? "#4a4a4a" : "transparent"
    }
}

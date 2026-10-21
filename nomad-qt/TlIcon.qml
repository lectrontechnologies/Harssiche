import QtQuick
import QtQuick.Shapes

// Icones da timeline (grade 24x24): triangulos e barras cheios, chave em tracos.
Item {
    id: root
    property string name: ""
    property color color: "#ececec"
    property real size: 24
    width: size
    height: size

    readonly property var fills: ({
        "start":   "M2.5 5 H5 V19 H2.5 Z M14 5 L14 19 L6 12 Z M22 5 L22 19 L14 12 Z",
        "rev":     "M17 4 L17 20 L5 12 Z",
        "stop":    "M6.5 6.5 H17.5 V17.5 H6.5 Z",
        "play":    "M7 4 L7 20 L19 12 Z",
        "end":     "M2 5 L2 19 L10 12 Z M10 5 L10 19 L18 12 Z M19 5 H21.5 V19 H19 Z",
        "prevkey": "M5 5 H7.5 V19 H5 Z M19 5 L19 19 L9 12 Z",
        "nextkey": "M16.5 5 H19 V19 H16.5 Z M5 5 L5 19 L15 12 Z"
    })
    readonly property var strokes: ({
        "key": "M12 8 A4 4 0 1 1 4 8 A4 4 0 1 1 12 8 M11 11 L20 20 M16.5 16.5 L19 14 M18.5 18.5 L21 16"
    })

    Shape {
        width: 24
        height: 24
        scale: root.size / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.color
            strokeColor: "transparent"
            PathSvg { path: root.fills[root.name] ? root.fills[root.name] : "M0 0" }
        }
        ShapePath {
            fillColor: "transparent"
            strokeColor: root.strokes[root.name] ? root.color : "transparent"
            strokeWidth: 1.8
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.strokes[root.name] ? root.strokes[root.name] : "M0 0" }
        }
    }
}

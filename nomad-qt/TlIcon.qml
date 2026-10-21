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
        "nextkey": "M16.5 5 H19 V19 H16.5 Z M5 5 L5 19 L15 12 Z",
        "tmove":   "M5 7 H14 V4.2 L20 8.1 L14 12 V9.2 H5 Z M19 14.8 H10 V12 L4 15.9 L10 19.8 V17 H19 Z",
        "trotate": "M8 12.6 L15 12.6 L11.5 19.6 Z",
        "tscale":  "M4 4 h2.2 v2.2 h-2.2 z M7.45 4 h2.2 v2.2 h-2.2 z M10.9 4 h2.2 v2.2 h-2.2 z M14.35 4 h2.2 v2.2 h-2.2 z M17.8 4 h2.2 v2.2 h-2.2 z M17.8 7.45 h2.2 v2.2 h-2.2 z M17.8 10.9 h2.2 v2.2 h-2.2 z M17.8 14.35 h2.2 v2.2 h-2.2 z M17.8 17.8 h2.2 v2.2 h-2.2 z M14.35 17.8 h2.2 v2.2 h-2.2 z M10.9 17.8 h2.2 v2.2 h-2.2 z M4 7.45 h2.2 v2.2 h-2.2 z M4 10.9 h2.2 v2.2 h-2.2 z M4 14.4 H9.4 V20 H5.2 Q4 20 4 18.8 Z",
        "tcube":   "M8 10 L5 5.5 V16.5 L8 21 Z",
        "undo":    "M2.5 13.5 L9.8 6.6 L11 15.2 Z",
        "redo":    "M21.5 13.5 L14.2 6.6 L13 15.2 Z"
    })
    readonly property var strokes: ({
        "key": "M12 8 A4 4 0 1 1 4 8 A4 4 0 1 1 12 8 M11 11 L20 20 M16.5 16.5 L19 14 M18.5 18.5 L21 16",
        "trotate": "M12 7 C17.2 7 21.5 8.7 21.5 10.8 C21.5 12.9 17.2 14.6 12 14.6 C6.8 14.6 2.5 12.9 2.5 10.8 C2.5 8.7 6.8 7 12 7 Z",
        "tcube":   "M8 10 H19 V21 H8 Z M8 10 L5 5.5 H16 L19 10",
        "undo":    "M9 11 C12.5 7.6 19 8.4 21.2 15.6",
        "redo":    "M15 11 C11.5 7.6 5 8.4 2.8 15.6",
        "menu":    "M3.5 6 H20.5 M3.5 12 H20.5 M3.5 18 H20.5"
    })
    readonly property var widths: ({
        "trotate": 2.4, "tcube": 2.0, "undo": 2.6, "redo": 2.6, "menu": 2.4
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
            strokeWidth: root.widths[root.name] ? root.widths[root.name] : 1.8
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.strokes[root.name] ? root.strokes[root.name] : "M0 0" }
        }
    }
}

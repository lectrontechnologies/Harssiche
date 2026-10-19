import QtQuick
import QtQuick.Shapes

// Icones vetoriais em grade 24x24 (tracos brancos), sem arquivos de imagem.
Item {
    id: root
    property string name: ""
    property color color: "white"
    width: 24
    height: 24

    readonly property var paths: ({
        "move":   "M4 8 H20 M16 4 L20 8 L16 12 M20 16 H4 M8 12 L4 16 L8 20",
        "rotate": "M19 12 A7 7 0 1 1 12 5 M9 1.5 L12.5 5 L9 8.5",
        "scale":  "M4 4 H20 V20 H4 Z",
        "cube":   "M12 3 L20 7.5 V16.5 L12 21 L4 16.5 V7.5 Z M4 7.5 L12 12 L20 7.5 M12 12 V21",
        "undo":   "M9 14 L4 9 L9 4 M4 9 H14 A6 6 0 0 1 14 21 H9",
        "redo":   "M15 14 L20 9 L15 4 M20 9 H10 A6 6 0 0 0 10 21 H15",
        "menu":   "M4 6 H20 M4 12 H20 M4 18 H20"
    })

    Shape {
        anchors.fill: parent
        layer.enabled: true
        layer.samples: 4

        ShapePath {
            strokeColor: root.color
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            strokeStyle: root.name === "scale" ? ShapePath.DashLine : ShapePath.SolidLine
            dashPattern: [1.2, 1.2]
            PathSvg { path: root.paths[root.name] ? root.paths[root.name] : "M0 0" }
        }
        // bloco cheio do icone de escala
        ShapePath {
            strokeColor: "transparent"
            strokeWidth: 0
            fillColor: root.name === "scale" ? root.color : "transparent"
            PathSvg { path: "M4 14 H11 V20 H4 Z" }
        }
    }
}

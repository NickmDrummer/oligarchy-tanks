// One key of the game's keyboard, drawn in the same flat pixel language as the
// rest of the UI: dark face, 2px border, a darker strip along the bottom for
// depth, and no rounded corners. Arrows are drawn from a tiny pixel grid
// because the font has no arrow glyphs, so a row of caps looks the same on any
// machine. `face` is also the legend colour, which is what makes a whole row
// fade when it is not the one being chosen.
import QtQuick

Rectangle {
    id: root
    property string label: ""       // "W", "SPACE", "ENTER"
    property string arrow: ""       // "up" | "down" | "left" | "right", or ""
    property color face: "#f2f4f8"  // border and legend
    property color body: "#14141a"
    property int unit: 26           // cap height; a one-letter cap stays square

    // The four arrows on a 5x5 grid, plus the empty shape so the drawing loop
    // never has to check whether an arrow was asked for.
    readonly property var shapes: ({
        "up":    ["..x..", ".xxx.", "xxxxx", "..x..", "..x.."],
        "down":  ["..x..", "..x..", "xxxxx", ".xxx.", "..x.."],
        "left":  ["..x..", ".x...", "xxxxx", ".x...", "..x.."],
        "right": ["..x..", "...x.", "xxxxx", "...x.", "..x.."],
        "":      [".....", ".....", ".....", ".....", "....."]
    })
    function pixel(row, column) {
        return shapes[arrow][row][column] === "x"
    }

    implicitWidth: arrow !== "" ? unit : Math.max(unit, legend.implicitWidth + 18)
    implicitHeight: unit
    color: body
    border.color: face
    border.width: 2

    // The bottom strip that makes it read as a key and not a box.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 2
        height: 2
        color: Qt.darker(body, 1.7)
    }

    Text {
        id: legend
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -1
        visible: root.arrow === ""
        text: root.label
        font.bold: true
        font.pixelSize: Math.max(9, Math.round(root.unit * 0.48))
        color: root.face
    }

    Grid {
        anchors.centerIn: parent
        visible: root.arrow !== ""
        columns: 5
        rows: 5
        Repeater {
            model: 25
            Rectangle {
                width: 2
                height: 2
                color: root.pixel(Math.floor(index / 5), index % 5) ? root.face : "transparent"
            }
        }
    }
}

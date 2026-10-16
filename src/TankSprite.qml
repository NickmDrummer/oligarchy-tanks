// Pixel tank sprite, drawn as a 12x12 block grid facing up.
// The whole sprite rotates to face the direction of travel.
import QtQuick

Item {
    id: root
    property int dir: 0
    property color body: "#4a90ff"
    property color dark: "#1c3f94"
    property color light: "#bcd2ff"
    property color muzzle: "#ffffff"

    property var rows: [
        ".....MM.....",
        ".....CC.....",
        ".....CC.....",
        ".....CC.....",
        "TTTBBBBBBTTT",
        "TtTBBBBBBtTT",
        "TTTBBHHBBTTT",
        "TtTBBHHBBtTT",
        "TTTBBBBBBTTT",
        "TtTBBBBBBtTT",
        "TTTBBBBBBTTT",
        "TTTBBBBBBTTT",
    ]

    function colorFor(ch) {
        if (ch === "B")
            return body
        if (ch === "T" || ch === "C")
            return dark
        if (ch === "t")
            return Qt.darker(dark, 1.35)
        if (ch === "H")
            return light
        if (ch === "M")
            return muzzle
        return "transparent"
    }

    Item {
        anchors.fill: parent
        rotation: [0, 90, 180, 270][root.dir]
        Grid {
            anchors.centerIn: parent
            columns: 12
            rows: 12
            Repeater {
                model: 144
                Rectangle {
                    property int r: Math.floor(index / 12)
                    property int c: index % 12
                    property string ch: root.rows[r][c]
                    width: root.width / 12
                    height: root.height / 12
                    color: root.colorFor(ch)
                }
            }
        }
    }
}

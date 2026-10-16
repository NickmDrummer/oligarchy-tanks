// Battle City style pixel tank: 16x16 block grid facing up.
// Treads use a fixed dark base with one animated chevron row (frame 0/1),
// the hull has a light top edge and dark bottom edge for a fake-3D look.
import QtQuick

Item {
    id: root
    property int dir: 0
    property int frame: 0
    property color body: "#c8a028"
    property color dark: "#6e5410"
    property color light: "#f0d878"

    property var rows: [
        "TTTT...MM...TTTT",
        "Tttt...GG...tttT",
        "TTTT...GG...TTTT",
        "Tttt...GG...tttT",
        "TTTTHHHHHHHHTTTT",
        "TtttSBBBBBBStttT",
        "TTTTSBCCCCBSTTTT",
        "TtttSBCcCCBStttT",
        "TTTTSBCCCCBSTTTT",
        "TtttSBBBBBBStttT",
        "TTTTSSSSSSSSTTTT",
        "TTTT........TTTT",
        "Tttt........tttT",
        "TTTT........TTTT",
        "Tttt........tttT",
        "TTTT........TTTT",
    ]

    // Tread base and chevron stay graphite on every tank, like the original.
    property color treadBase: "#2c2c30"
    property color treadLight: "#6a6e76"

    function colorFor(ch, r) {
        if (ch === ".")
            return "transparent"
        if (ch === "B")
            return body
        if (ch === "H")
            return light
        if (ch === "S" || ch === "G" || ch === "C")
            return dark
        if (ch === "c")
            return light
        if (ch === "M")
            return "#d8d8d8"
        if (ch === "T")
            return treadBase
        if (ch === "t")
            return ((r + frame) % 2 === 0) ? treadLight : treadBase
        return "transparent"
    }

    Item {
        anchors.fill: parent
        rotation: [0, 90, 180, 270][root.dir]
        Grid {
            anchors.centerIn: parent
            columns: 16
            rows: 16
            Repeater {
                model: 256
                Rectangle {
                    property int r: Math.floor(index / 16)
                    property int c: index % 16
                    property string ch: root.rows[r][c]
                    width: Math.ceil(root.width / 16)
                    height: Math.ceil(root.height / 16)
                    color: root.colorFor(ch, r)
                }
            }
        }
    }
}

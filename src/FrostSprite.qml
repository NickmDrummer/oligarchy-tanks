// Frost patch drawn under a frozen enemy tank: celeste + white ice, a bumpy
// pixel silhouette with a few crystal tips, so it does not read as a flat
// circle. Same 16x16 grid language as the tanks and the explosion.
import QtQuick

Item {
    id: root

    readonly property var rows: [
        "................",
        "................",
        "................",
        "....cccCcccc....",
        "....ccCCcCC.....",
        "...cBCCCCCCccc..",
        "..ccCCWCWWCCC...",
        "..BCCWWWWWCCCBc.",
        "..ccCWWWWWWCCcc.",
        "..cCCWWWWWWCcc..",
        "...CCCWWWWWCcc..",
        "...BCCCWCCCcBc..",
        "...ccCCCCCCcc...",
        ".....ccBCccc....",
        ".....c.cc.......",
        "................",
    ]

    function colorFor(ch) {
        if (ch === "W") return "#ffffff"
        if (ch === "C") return "#8fd4ff"
        if (ch === "c") return "#c2e8ff"
        if (ch === "B") return "#eaf7ff"
        return "transparent"
    }

    readonly property int px: Math.max(1, Math.floor(Math.min(width, height) / 16))

    Grid {
        // Whole-pixel placement, so the ice keeps hard pixel edges.
        x: Math.round((root.width - 16 * root.px) / 2)
        y: Math.round((root.height - 16 * root.px) / 2)
        columns: 16
        rows: 16
        Repeater {
            model: 256
            Rectangle {
                width: root.px
                height: root.px
                property int r: Math.floor(index / 16)
                property int c: index % 16
                color: root.colorFor(root.rows[r][c])
            }
        }
    }
}

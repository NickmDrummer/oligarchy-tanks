// Electric shield: two hand-drawn frames of a jagged ring of lightning around
// the tank, alternating back and forth. The ring is uniform (even thickness,
// no stray specks) and 4-fold symmetric — mirrored on both axes — so it reads
// as a steady crackle around the hull rather than a cloud of noise. Corners
// are rounded, not square.
import QtQuick

Item {
    id: root

    // Changes over time; the two frames alternate.
    property int frame: 0
    // Callers can tint the ring: the power-up/grace shield stays white/gray
    // (the default); the tier armour shows in its own colour instead.
    property color tint: "#ffffff"
    property color tintDark: "#c9ced6"

    readonly property var frames: [
        [
            "....................",
            "........WWWW........",
            ".......WW..WW.......",
            "....LLL......LLL....",
            "...LL..........LL...",
            "...L............L...",
            "...L............L...",
            "..W..............W..",
            ".WW..............WW.",
            ".W................W.",
            ".W................W.",
            ".WW..............WW.",
            "..W..............W..",
            "...L............L...",
            "...L............L...",
            "...LL..........LL...",
            "....LLL......LLL....",
            ".......WW..WW.......",
            "........WWWW........",
            "....................",
        ],
        [
            "....................",
            ".........WW.........",
            ".......WW..WW.......",
            "...LLLLL....LLLLL...",
            "...L............L...",
            "...L............L...",
            "...L............L...",
            "..WL............LW..",
            "..W..............W..",
            ".W................W.",
            ".W................W.",
            "..W..............W..",
            "..WL............LW..",
            "...L............L...",
            "...L............L...",
            "...L............L...",
            "...LLLLL....LLLLL...",
            ".......WW..WW.......",
            ".........WW.........",
            "....................",
        ],
    ]

    readonly property var rows: frames[Math.abs(frame) % frames.length]
    readonly property int px: Math.max(1, Math.floor(Math.min(width, height) / 20))

    function colorFor(ch) {
        if (ch === "W") return root.tint
        if (ch === "L") return root.tintDark
        return "transparent"
    }

    Grid {
        // Whole-pixel placement: a fractional origin would antialias every
        // block and turn the lightning into a soft smudge.
        x: Math.round((root.width - 20 * root.px) / 2)
        y: Math.round((root.height - 20 * root.px) / 2)
        columns: 20
        rows: 20
        Repeater {
            model: 400
            Rectangle {
                width: root.px
                height: root.px
                property int r: Math.floor(index / 20)
                property int c: index % 20
                color: root.colorFor(root.rows[r][c])
            }
        }
    }
}

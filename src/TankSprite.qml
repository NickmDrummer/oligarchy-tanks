// Battle City style pixel tank: 16x16 block grid facing up.
//
// Three silhouettes share the same shading language:
//   variant 0  treads   players, basic tanks and carriers
//   variant 1  wheels   the fast tank: same grey as the basic one, but a
//                       wheeled chassis instead of treads, so the two read
//                       apart by shape and not only by speed
//   variant 2  armour   the armoured tank: chunkier plates and a thicker
//                       cannon, only a little longer (never wider, so it still
//                       fits the same lanes)
//
// The tread band is a column of cells, always drawn: one frame lights the even
// cells and the other the odd ones, so the two frames interleave and the whole
// band reads as a moving track. Frame 0 shows the same cells as a still.
import QtQuick

Item {
    id: root
    property int dir: 0
    property int frame: 0
    property int variant: 0
    property color body: "#c8a028"
    property color dark: "#6e5410"
    property color light: "#f0d878"
    // The tank's contour. A neutral grey on purpose: the board is near black
    // (#050507) and a darkened hull colour (the old trace) is lost on it, so
    // the tread band - most of the silhouette - read as black and the body's
    // outline could not be told from the background. Grey reads on black
    // whatever colour the hull is, and it is lighter than any hull's dark
    // shade, so the body stays outlined against it.
    property color outline: "#6b7280"

    readonly property var treadRows: [
        "Tttt...MM...tttT",
        "Tttt...GG...tttT",
        "Tttt...GG...tttT",
        "Tttt...GG...tttT",
        "TtttHHHHHHHHtttT",
        "TtttSBBBBBBStttT",
        "TtttSBCCCCBStttT",
        "TtttSBCcCCBStttT",
        "TtttSBCCCCBStttT",
        "TtttSBBBBBBStttT",
        "TtttSSSSSSSStttT",
        "Tttt........tttT",
        "Tttt........tttT",
        "Tttt........tttT",
        "Tttt........tttT",
        "Tttt........tttT",
    ]

    // The wheeled chassis, drawn whole by the user: three wheels a side (a one
    // cell column that swells into a three cell bar with the hub on its middle
    // row), a flared cannon, a light front plate, a hatch window with a raised
    // centre (rows 8-12) and a two row dark bottom. The wheels do not animate.
    // The sketch is centred on column 8, so one column of margin is left at the
    // left of the sprite; do not "centre" it, that would move every feature.
    readonly property var wheelRows: [
        "......LLLLL.....",
        ".......DDD......",
        ".......DDD......",
        "......DDDDD.....",
        ".....LLLLLLL....",
        "..B..LLLLLLL..B.",
        ".BLBDBBBBBBBDBLB",
        "..B.DBBBBBBBD.B.",
        "....DBB...BBD...",
        "..B.DBB.B.BBD.B.",
        ".BLBDBB.B.BBDBLB",
        "..B.DBB...BBD.B.",
        "....DBBBBBBBD...",
        "..B.DBBBBBBBD.B.",
        ".BLBDDDDDDDDDBLB",
        "..B....DDD....B.",
    ]

    readonly property var armorRows: [
        "Tttt..HHHH..tttT",
        "Tttt..GGGG..tttT",
        "Tttt..GGGG..tttT",
        "Tttt..GGGG..tttT",
        "Tttt..GGGG..tttT",
        "TtttHHHHHHHHtttT",
        "TtttSSSSSSSStttT",
        "TtttSBBBBBBStttT",
        "TtttSBCCCCBStttT",
        "TtttSBCcCCBStttT",
        "TtttSBCCCCBStttT",
        "TtttSBBBBBBStttT",
        "TtttSSSSSSSStttT",
        "Tttt........tttT",
        "Tttt........tttT",
        "Tttt........tttT",
    ]

    property var rows: variant === 1 ? wheelRows : variant === 2 ? armorRows : treadRows

    // Monochrome like the original: treads and cannon are darker and lighter
    // shades of the hull color, never graphite black. The tread band is the
    // exception: its trace is the neutral grey above, not a darkened hull
    // colour, because that trace is the tank's outline against the black board.
    function colorFor(ch, r) {
        if (ch === ".")
            return "transparent"
        if (ch === "B")
            return body
        // Hub of a wheel: the tank's own highlight, never black.
        if (ch === "L")
            return light
        if (ch === "H" || ch === "c" || ch === "M")
            return light
        if (ch === "S" || ch === "G" || ch === "C" || ch === "D")
            return dark
        if (ch === "T")
            return outline
        // One tread cell per row: even rows light in frame 0, odd rows light in
        // frame 1. Both frames together cover the band, so the cells are always
        // visible instead of appearing only on the moving frame.
        if (ch === "t")
            return ((r + frame) % 2 === 0) ? body : outline
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

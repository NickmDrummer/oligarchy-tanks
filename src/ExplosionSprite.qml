// Battle City style tank death, drawn as pixel grids so the silhouette is
// a bumpy cloud and never an anti-aliased circle.
//
// The effect runs in three overlapping phases driven by `progress` (0..1):
//   1. a big white/gray spark flash (the "chispazo"),
//   2. a black/gray smoke cloud that takes over,
//   3. fire licking up inside the smoke as it burns out.
//
// Each phase is its own 16x16 grid, gated with a Loader so only the grids
// that are actually on screen exist (the item-count lesson, AGENTS.md 7).
import QtQuick

Item {
    id: root

    // 0 at the start of the effect, 1 at the end.
    property real progress: 0

    // Phase windows (fractions of the whole effect): the flash comes up
    // instantly and is gone by 0.30; the smoke takes over from 0.12 and
    // fades out over the last quarter; the fire burns the middle out.
    readonly property real flashAlpha: Math.max(0, Math.min(1, 1.0 - progress / 0.30))
    readonly property real smokeAlpha: Math.min(
        Math.min(1, Math.max(0, (progress - 0.12) / 0.12)),
        Math.max(0, Math.min(1, (1.0 - progress) / 0.25)))
    readonly property real fireAlpha: Math.min(
        Math.min(1, Math.max(0, (progress - 0.45) / 0.18)),
        Math.max(0, Math.min(1, (1.0 - progress) / 0.18)))

    // One pixel, kept integral so the blocks stay crisp while the cloud grows.
    readonly property int px: Math.max(1, Math.floor(Math.min(width, height) / 16))

    // White/gray spark burst, with a few purple speckles for the arcade feel.
    readonly property var sparkRows: [
        "................",
        ".......PD.......",
        "..D....LL....D..",
        "...D.P.LL.P.D...",
        "....D..LL..D....",
        ".....DLLLLD.....",
        "..DP.LWLLWLDP.D.",
        "DDD.LWLWWLWL.DDD",
        "DDD.LWLWWLWL.DDD",
        "..D..LWLLWLD..D.",
        ".....DLLLLD.....",
        "....DP.LL.PD....",
        "...D...LL...D...",
        "..D....LL....D..",
        ".......DD.......",
        "................",
    ]

    // Black/gray smoke cloud, bumpy on purpose.
    readonly property var smokeRows: [
        "................",
        "......L.........",
        "......DLDL......",
        "...L.LLDKDL.....",
        "...LLDLDDLDLD...",
        "..LLDDDKDDDDDLL.",
        "..DDKDDKKKDKDLL.",
        "..LLKDKKKKDKDL..",
        ".LLLDDKKKKDDLKL.",
        ".LDKLDKKKKKDLKLL",
        "..LDLKKDKDKKDL..",
        "..L.LKKKDLDDDL..",
        "....LLLKDDLLL...",
        "....LLLLLL.L....",
        "......LLL.......",
        "........L.......",
    ]

    // The fire that burns inside the smoke during the last phase.
    readonly property var fireRows: [
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "......F.........",
        ".....FFF........",
        "....FFFFF.......",
        "...FFFFFFF......",
        "...FFFFFFFF.....",
        "....FF.FFFFF....",
        "....FF...FFF....",
        ".....F...F......",
        "................",
        "................",
    ]

    function sparkColor(ch) {
        if (ch === "W") return "#ffffff"
        if (ch === "L") return "#c9ced6"
        if (ch === "D") return "#5a5e68"
        if (ch === "P") return "#9b30c0"
        return "transparent"
    }
    function smokeColor(ch) {
        if (ch === "K") return "#101014"
        if (ch === "D") return "#2b2f37"
        if (ch === "L") return "#5c6068"
        return "transparent"
    }
    function fireColor(r, c) {
        if ((r + c) % 3 === 0) return "#ffd05a"
        if ((r + c) % 2 === 0) return "#ff8a2a"
        return "#d02810"
    }

    // Phase 1: the spark flash.
    Loader {
        anchors.fill: parent
        active: root.flashAlpha > 0.01
        opacity: root.flashAlpha
        sourceComponent: Component {
            Grid {
                anchors.centerIn: parent
                columns: 16
                rows: 16
                Repeater {
                    model: 256
                    Rectangle {
                        width: root.px
                        height: root.px
                        property int r: Math.floor(index / 16)
                        property int c: index % 16
                        color: root.sparkColor(root.sparkRows[r][c])
                    }
                }
            }
        }
    }

    // Phase 2: the smoke cloud.
    Loader {
        anchors.fill: parent
        active: root.smokeAlpha > 0.01
        opacity: root.smokeAlpha
        sourceComponent: Component {
            Grid {
                anchors.centerIn: parent
                columns: 16
                rows: 16
                Repeater {
                    model: 256
                    Rectangle {
                        width: root.px
                        height: root.px
                        property int r: Math.floor(index / 16)
                        property int c: index % 16
                        color: root.smokeColor(root.smokeRows[r][c])
                    }
                }
            }
        }
    }

    // Phase 3: the fire inside the smoke.
    Loader {
        anchors.fill: parent
        active: root.fireAlpha > 0.01
        opacity: root.fireAlpha
        sourceComponent: Component {
            Grid {
                anchors.centerIn: parent
                columns: 16
                rows: 16
                Repeater {
                    model: 256
                    Rectangle {
                        width: root.px
                        height: root.px
                        property int r: Math.floor(index / 16)
                        property int c: index % 16
                        color: root.fireRows[r][c] === "F" ? root.fireColor(r, c) : "transparent"
                    }
                }
            }
        }
    }
}

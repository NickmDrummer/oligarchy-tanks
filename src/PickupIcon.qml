// Pixel bonus icons shared by the board pickup and the help legend.
// All pattern reads go through the root id: delegates must not rely on
// scope lookup for string grids.
import QtQuick

Item {
    id: root
    property int kind: 0
    property real side: 40

    property var shield: ["XXXXX", "XXXXX", "XXXXX", ".XXX.", "..X..", "..X.."]
    property var bolt: ["...XX", "..XX.", "..XX.", ".XX..", ".XX..", "XX...", "XX..."]
    property var wall: ["BBBBBBB", "BBDBBDB", "BBBBBBB", "BDBBDBB", "BBBBBBB"]

    // 0 STAR: rapid fire.
    Text {
        visible: root.kind === 0
        anchors.centerIn: parent
        text: "★"
        font.pixelSize: root.side * 0.8
        color: "#edc400"
    }
    // 1 SHIELD: pixel badge, blocks one hit wave for 10s.
    Grid {
        visible: root.kind === 1
        anchors.centerIn: parent
        columns: 5
        Repeater {
            model: 30
            Rectangle {
                property int r: Math.floor(index / 5)
                property int c: index % 5
                property string ch: root.shield[r][c]
                width: root.side / 6
                height: root.side / 6
                color: ch === "X" ? "#57c8d8" : "transparent"
            }
        }
    }
    // 2 SPEED: lightning bolt, +35% speed for 20s.
    Grid {
        visible: root.kind === 2
        anchors.centerIn: parent
        columns: 5
        Repeater {
            model: 35
            Rectangle {
                property int r: Math.floor(index / 5)
                property int c: index % 5
                property string ch: root.bolt[r][c]
                width: root.side / 7
                height: root.side / 7
                color: ch === "X" ? "#5da24a" : "transparent"
            }
        }
    }
    // 3 WALL: brick courses, steel base guard for 25s.
    Grid {
        visible: root.kind === 3
        anchors.centerIn: parent
        columns: 7
        Repeater {
            model: 35
            Rectangle {
                property int r: Math.floor(index / 7)
                property int c: index % 7
                property string ch: root.wall[r][c]
                width: root.side / 7
                height: root.side / 7
                color: ch === "B" ? "#9e5230" : "#2e1a10"
            }
        }
    }
    // 4 LIFE: mini red tank, one extra life.
    TankSprite {
        visible: root.kind === 4
        anchors.centerIn: parent
        width: root.side
        height: root.side
        dir: 0
        frame: 0
        body: "#b03a2e"
        dark: "#5e1f18"
        light: "#e8a49c"
    }
}

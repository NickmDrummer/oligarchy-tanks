// Pixel bonus icons shared by the board pickup and the help legend.
// All pattern reads go through the root id: delegates must not rely on
// scope lookup for string grids.
import QtQuick

Item {
    id: root
    property int kind: 0
    property real side: 40

    property var bolt: ["...XX", "..XX.", "..XX.", ".XX..", ".XX..", "XX...", "XX..."]
    property var shield: [".XXXXX.", "XXXXXXX", "XXXXXXX", "XXXXXXX", ".XXXXX.", "..XXX..", "...X..."]
    property var wall: [".BBBBB.", ".B...B.", ".B.O.B.", ".B...B.", ".B...B."]
    property var grenade: ["..FFFF.", ".BBBBB.", "BLBBBBB", "BBBBBBB", "BBBBBBB", "BBBBBBB", ".BBBBB."]
    property var cannon: ["..T..", "..T..", ".TTT.", ".TTT.", ".CCC.", ".CCC.", ".CCC."]
    property var clock: ["...W...", ".WWWWW.", "WWWWWWW", "WWWHWWW", "WWWHWWW", ".WWWWW."]

    // 0 SPEED: yellow lightning bolt, +35% move speed for 20s.
    Grid {
        visible: root.kind === 0
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
                color: ch === "X" ? "#edc400" : "transparent"
            }
        }
    }
    // 1 SHIELD: plain gray heater shield, blocks shots for 10s.
    Grid {
        visible: root.kind === 1
        anchors.centerIn: parent
        columns: 7
        Repeater {
            model: 49
            Rectangle {
                property int r: Math.floor(index / 7)
                property int c: index % 7
                property string ch: root.shield[r][c]
                width: root.side / 7
                height: root.side / 7
                color: ch === "X" ? "#8d8d94" : "transparent"
            }
        }
    }
    // 2 WALL: brick courses in an inverted U, steel base guard for 25s.
    Grid {
        visible: root.kind === 2
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
                color: ch === "B" ? "#9e5230" : ch === "O" ? "#edc400" : "transparent"
            }
        }
    }
    // 3 LIFE: pale gray tank, one extra life.
    TankSprite {
        visible: root.kind === 3
        anchors.centerIn: parent
        width: root.side
        height: root.side
        dir: 0
        frame: 0
        body: "#c9ced6"
        dark: "#6a6e76"
        light: "#f2f4f8"
    }
    // 4 GRENADE: wipes every enemy tank off the map.
    Grid {
        visible: root.kind === 4
        anchors.centerIn: parent
        columns: 7
        Repeater {
            model: 49
            Rectangle {
                property int r: Math.floor(index / 7)
                property int c: index % 7
                property string ch: root.grenade[r][c]
                width: root.side / 7
                height: root.side / 7
                color: ch === "B" ? "#2e6b28" : ch === "L" ? "#6aa84f" : ch === "F" ? "#8d8d94" : "transparent"
            }
        }
    }
    // 5 CANNON: brass shell, cracks any armor in one hit for 20s.
    Grid {
        visible: root.kind === 5
        anchors.centerIn: parent
        columns: 5
        Repeater {
            model: 35
            Rectangle {
                property int r: Math.floor(index / 5)
                property int c: index % 5
                property string ch: root.cannon[r][c]
                width: root.side / 7
                height: root.side / 7
                color: ch === "C" ? "#c8963c" : ch === "T" ? "#d8d8d8" : "transparent"
            }
        }
    }
    // 6 FREEZE: stops enemy tanks for 8s.
    Grid {
        visible: root.kind === 6
        anchors.centerIn: parent
        columns: 7
        Repeater {
            model: 42
            Rectangle {
                property int r: Math.floor(index / 7)
                property int c: index % 7
                property string ch: root.clock[r][c]
                width: root.side / 7
                height: root.side / 7
                color: ch === "W" ? "#e8f4ff" : ch === "H" ? "#57c8d8" : "transparent"
            }
        }
    }
}

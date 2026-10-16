// Pixel bonus icons shared by the board pickup and the help legend.
// All pattern reads go through the root id: delegates must not rely on
// scope lookup for string grids.
import QtQuick

Item {
    id: root
    property int kind: 0
    property real side: 40

    property var bolt: ["..KKK..", "..KYYK.", "..KYYK.", ".KYYYYK", ".KYYYYK", "..KYYOK", "..KYYOK", ".KYYOK.", ".KYOK.."]
    property var shield: [".XXXXX.", "XXXXXXX", "XXXXXXX", "XXXXXXX", ".XXXXX.", "..XXX..", "...X..."]
    property var wall: [".CCCCC.", ".Cc.Cc.", ".CcOCc.", ".Cc.Cc.", ".cC.cC.", ".Cc.Cc.", ".Cc.Cc."]
    property var grenade: ["..FFFF.", ".BBBBB.", "BXBXBXB", "BBBBBBB", "BXBXBXB", "BBBBBBB", ".BBBBB."]
    property var cannon: [".....KKK.", "....KCCCK", "...KCCCDK", "..KCLCDDK", "..KCLDDK.", ".KCLDDDK.", ".KCLDDK..", "KLLDDK...", "KLLK....."]
    property var snow: ["..X.X..", "X..X..X", ".XXXXX.", "XXXXXXX", ".XXXXX.", "X..X..X", "..X.X.."]

    // 0 SPEED: chunky bolt, black outline, yellow body, orange shade.
    Grid {
        visible: root.kind === 0
        anchors.centerIn: parent
        columns: 7
        Repeater {
            model: 63
            Rectangle {
                property int r: Math.floor(index / 7)
                property int c: index % 7
                property string ch: root.bolt[r][c]
                width: root.side / 7
                height: root.side / 7
                color: ch === "K" ? "#1a1a22" : ch === "Y" ? "#ffe14d" : ch === "O" ? "#f5a623" : "transparent"
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
    // 2 WALL: chunky cement U of pavers, steel base guard for 25s.
    Grid {
        visible: root.kind === 2
        anchors.centerIn: parent
        columns: 7
        Repeater {
            model: 49
            Rectangle {
                property int r: Math.floor(index / 7)
                property int c: index % 7
                property string ch: root.wall[r][c]
                width: root.side / 7
                height: root.side / 7
                color: ch === "B" ? "#9e5230" : ch === "O" ? "#edc400" : ch === "C" ? "#8d8d94" : ch === "c" ? "#6a6e76" : "transparent"
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
    // 4 GRENADE: frag crosshatch, wipes every enemy tank off the map.
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
                color: ch === "B" ? "#2e6b28" : ch === "X" ? "#1d3d1a" : ch === "F" ? "#8d8d94" : "transparent"
            }
        }
    }
    // 5 CANNON: brass slug with black outline, cracks any armor in one
    // hit for 20s.
    Grid {
        visible: root.kind === 5
        anchors.centerIn: parent
        columns: 9
        Repeater {
            model: 81
            Rectangle {
                property int r: Math.floor(index / 9)
                property int c: index % 9
                property string ch: root.cannon[r][c]
                width: root.side / 9
                height: root.side / 9
                color: ch === "K" ? "#14141a" : ch === "C" ? "#c8862e" : ch === "D" ? "#7a4d1a" : ch === "L" ? "#ffd94d" : "transparent"
            }
        }
    }
    // 6 FREEZE: snowflake, stops enemy tanks for 8s.
    Grid {
        visible: root.kind === 6
        anchors.centerIn: parent
        columns: 7
        Repeater {
            model: 49
            Rectangle {
                property int r: Math.floor(index / 7)
                property int c: index % 7
                property string ch: root.snow[r][c]
                width: root.side / 7
                height: root.side / 7
                color: ch === "X" ? "#bfe8ff" : "transparent"
            }
        }
    }
}

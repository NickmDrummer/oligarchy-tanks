// Pixel bonus icons shared by the board pickup and the help legend.
// All pattern reads go through the root id: delegates must not rely on
// scope lookup for string grids.
import QtQuick

Item {
    id: root
    property int kind: 0
    property real side: 40

    property var bolt: ["...KKKK..", "..KWYYK..", ".KWYYYK..", ".KWYYK...", "KWYYK....", "KWYKKKKK.", "KWYYYYYK.", "KKKYYOYK.", "...KYOYK.", "...KYYK..", "..KYOK...", "..KKK...."]
    property var shield: ["XXXXXXX", "XWXXXXX", "XXXXXXX", "XXXXXXX", ".XXXXX.", ".XXXXX.", "..XXX..", "...X..."]
    property var pick: ["..PPPPP..", ".PPPPPPP.", "PP..p..PP", "P...H...P", "....H....", "....H....", "....H....", "....H....", "....H...."]
    property var grenade: ["..FFFF.", ".BBBBB.", "BXBXBXB", "BBBBBBB", "BXBXBXB", "BBBBBBB", ".BBBBB."]
    property var cannon: ["..KK....", ".KBBBK..", "KWBBBBBK.", "KWBBBDDBK", ".KBBBK..", "..KK...."]
    property var snow: ["..X.X..", "X..X..X", ".XXXXX.", "XXXXXXX", ".XXXXX.", "X..X..X", "..X.X.."]

    // Only the icon the pickup actually is gets built. Each Grid used to be
    // instantiated for every icon and merely hidden, which cost thousands of
    // items across the help legend and the menu legend.
    // 0 SPEED: sharp zigzag bolt with a hard middle step, black outline,
    // yellow body, orange shade on the tail, white highlight on the edge.
    Loader {
        anchors.fill: parent
        active: root.kind === 0
        sourceComponent: boltPart
    }
    Component {
        id: boltPart
        Grid {
            anchors.centerIn: parent
            columns: 9
            Repeater {
                model: 108
                Rectangle {
                    property int r: Math.floor(index / 9)
                    property int c: index % 9
                    property string ch: root.bolt[r][c]
                    width: Math.max(1, Math.floor(root.side / 12))
                    height: Math.max(1, Math.floor(root.side / 12))
                    color: ch === "K" ? "#1a1a22" : ch === "Y" ? "#ffdf3d" : ch === "O" ? "#f5a623" : ch === "W" ? "#ffffff" : "transparent"
                }
            }
        }
    }
    // 1 SHIELD: plain gray heater shield, flat top and pointed bottom.
    Loader {
        anchors.fill: parent
        active: root.kind === 1
        sourceComponent: shieldPart
    }
    Component {
        id: shieldPart
        Grid {
            anchors.centerIn: parent
            columns: 7
            Repeater {
                model: 56
                Rectangle {
                    property int r: Math.floor(index / 7)
                    property int c: index % 7
                    property string ch: root.shield[r][c]
                    width: Math.max(1, Math.floor(root.side / 8))
                    height: Math.max(1, Math.floor(root.side / 8))
                    color: ch === "X" ? "#8d8d94" : ch === "W" ? "#d8dbe2" : "transparent"
                }
            }
        }
    }
    // 2 PICK: mining pick with an arched head and a wood handle. Smashes
    // through steel for 20s.
    Loader {
        anchors.fill: parent
        active: root.kind === 2
        sourceComponent: pickPart
    }
    Component {
        id: pickPart
        Grid {
            anchors.centerIn: parent
            columns: 9
            Repeater {
                model: 81
                Rectangle {
                    property int r: Math.floor(index / 9)
                    property int c: index % 9
                    property string ch: root.pick[r][c]
                    width: Math.max(1, Math.floor(root.side / 9))
                    height: Math.max(1, Math.floor(root.side / 9))
                    color: ch === "P" ? "#c9ced6" : ch === "p" ? "#8b9099" : ch === "H" ? "#8a5a32" : "transparent"
                }
            }
        }
    }
    // 3 LIFE: pale gray tank, one extra life.
    Loader {
        anchors.fill: parent
        active: root.kind === 3
        sourceComponent: lifePart
    }
    Component {
        id: lifePart
        TankSprite {
            anchors.centerIn: parent
            width: root.side
            height: root.side
            dir: 0
            frame: 0
            body: "#c9ced6"
            dark: "#6a6e76"
            light: "#f2f4f8"
        }
    }
    // 4 GRENADE: frag crosshatch, wipes every enemy tank off the map.
    Loader {
        anchors.fill: parent
        active: root.kind === 4
        sourceComponent: grenadePart
    }
    Component {
        id: grenadePart
        Grid {
            anchors.centerIn: parent
            columns: 7
            Repeater {
                model: 49
                Rectangle {
                    property int r: Math.floor(index / 7)
                    property int c: index % 7
                    property string ch: root.grenade[r][c]
                    width: Math.max(1, Math.floor(root.side / 7))
                    height: Math.max(1, Math.floor(root.side / 7))
                    color: ch === "B" ? "#2e6b28" : ch === "X" ? "#1d3d1a" : ch === "F" ? "#8d8d94" : "transparent"
                }
            }
        }
    }
    // 5 CANNON: bronze shell, black outline, white shine. Cracks any
    // armor in one hit for 20s.
    Loader {
        anchors.fill: parent
        active: root.kind === 5
        sourceComponent: cannonPart
    }
    Component {
        id: cannonPart
        Grid {
            anchors.centerIn: parent
            columns: 7
            Repeater {
                model: 63
                Rectangle {
                    property int r: Math.floor(index / 7)
                    property int c: index % 7
                    property string ch: root.cannon[r][c]
                    width: Math.max(1, Math.floor(root.side / 9))
                    height: Math.max(1, Math.floor(root.side / 9))
                    color: ch === "K" ? "#14141a" : ch === "B" ? "#c8862e" : ch === "D" ? "#7a4d1a" : ch === "W" ? "#ffffff" : "transparent"
                }
            }
        }
    }
    // 6 FREEZE: snowflake, stops enemy tanks for 8s.
    Loader {
        anchors.fill: parent
        active: root.kind === 6
        sourceComponent: freezePart
    }
    Component {
        id: freezePart
        Grid {
            anchors.centerIn: parent
            columns: 7
            Repeater {
                model: 49
                Rectangle {
                    property int r: Math.floor(index / 7)
                    property int c: index % 7
                    property string ch: root.snow[r][c]
                    width: Math.max(1, Math.floor(root.side / 7))
                    height: Math.max(1, Math.floor(root.side / 7))
                    color: ch === "X" ? "#bfe8ff" : "transparent"
                }
            }
        }
    }
}

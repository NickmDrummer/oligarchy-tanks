// Pixel bonus icons shared by the board pickup and the help legend.
// All pattern reads go through the root id: delegates must not rely on
// scope lookup for string grids.
//
// Every part is an Item wrapping the sprite. The Loader that shows one icon
// resizes the root item of the component it loads, so a Grid as the root gets
// pinned to (0,0) and the icon sits in the corner of its box instead of in the
// middle of it: one level down, the sprite keeps the geometry it declares.
import QtQuick

Item {
    id: root
    property int kind: 0
    property real side: 40

    property var bolt: ["...KKKK..", "..KWYYK..", ".KWYYYK..", ".KWYYK...", "KWYYK....", "KWYKKKKK.", "KWYYYYYK.", "KKKYYOYK.", "...KYOYK.", "...KYYK..", "..KYOK...", "..KKK...."]
    property var shield: ["XXXXXXX", "XWXXXXX", "XXXXXXX", "XXXXXXX", ".XXXXX.", ".XXXXX.", "..XXX..", "...X..."]
    property var pick: ["..PPPPP..", ".PPPPPPP.", "PP..p..PP", "P...H...P", "....H....", "....H....", "....H....", "....H....", "....H...."]
    property var nuke: ["..DDDDDDDDD..", "..DGGGGGGGD..", "....DGGGD....", "..DGGGGGGGD..", ".DLGGGGGGGGD.", "DLGGGGGGGGGGD", "DLGGGGGGGGGGD", "GLGGYKKKYGGGG", "GGGYYKKKYYGGG", "GGGYKKYKKYGGG", "GGGGYKKKYGGGD", "DDLGGGGGGGGDD", "DDLGGGGGGGGDD", ".DDLGGGGGGDD.", "...DDGGGDD..."]
    property var cannon: [".........", "...BBBBD.", "..BBBBBDD", ".BBBBBBDD", "BBWWWWWDD", ".BBBBBBDD", "..BBBBBDD", "...BBBBD.", "........."]
    property var snow: ["....X....", "...XXX...", "..X.W.X..", ".X..W..X.", "XXXWWWXXX", ".X..W..X.", "..X.W.X..", "...XXX...", "....X...."]

    // Whole-pixel origin for every sprite: a half-pixel one antialiases the
    // grid, and a blurred pixel icon is exactly what "barely legible" looks
    // like (AGENTS.md section 10).
    // Only the icon the pickup actually is gets built. Each Grid used to be
    // instantiated for every icon and merely hidden, which cost thousands of
    // items across the help legend and the menu legend.
    // 0 SPEED: sharp zigzag bolt with a hard middle step, in yellows only.
    // The old outline was near-black and made the bolt read as a yellow shape
    // inside a black shell, so the rim is amber now, the body yellow, the tail
    // orange and the edge a pale yellow.
    Loader {
        anchors.fill: parent
        active: root.kind === 0
        sourceComponent: boltPart
    }
    Component {
        id: boltPart
        Item {
            Grid {
                x: Math.round((parent.width - width) / 2)
                y: Math.round((parent.height - height) / 2)
                columns: 9
                Repeater {
                    model: 108
                    Rectangle {
                        property int r: Math.floor(index / 9)
                        property int c: index % 9
                        property string ch: root.bolt[r][c]
                        width: Math.max(1, Math.floor(root.side / 12))
                        height: Math.max(1, Math.floor(root.side / 12))
                        color: ch === "K" ? "#d9a11b" : ch === "Y" ? "#ffdf3d" : ch === "O" ? "#f5a623" : ch === "W" ? "#fff3bd" : "transparent"
                    }
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
        Item {
            Grid {
                x: Math.round((parent.width - width) / 2)
                y: Math.round((parent.height - height) / 2)
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
    }
    // 2 PICK: mining pick with an arched head and a wood handle. It is the
    // Battle City shovel, not a steel breaker: it turns the base wall into
    // concrete for 15 s.
    Loader {
        anchors.fill: parent
        active: root.kind === 2
        sourceComponent: pickPart
    }
    Component {
        id: pickPart
        Item {
            Grid {
                x: Math.round((parent.width - width) / 2)
                y: Math.round((parent.height - height) / 2)
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
    }
    // 3 LIFE: the player's own tank, a little smaller than the board tank so
    // the box shows it as an icon rather than a sprite.
    Loader {
        anchors.fill: parent
        active: root.kind === 3
        sourceComponent: lifePart
    }
    Component {
        id: lifePart
        Item {
            TankSprite {
                // The tank's own grid is 16 cells of ceil(width/16), so the
                // drawn sprite can overhang this box by a pixel; that is why the
                // icon box is a little bigger than the sprite.
                x: Math.round((parent.width - width) / 2)
                y: Math.round((parent.height - height) / 2)
                width: root.side * 0.78
                height: root.side * 0.78
                dir: 0
                frame: 0
                body: "#c9ced6"
                dark: "#6a6e76"
                light: "#f2f4f8"
            }
        }
    }
    // 4 NUKE: the user's cartoon bomb — a fat military-green zeppelin body
    // with a yellow tail and fins, nose down, carrying a radiation trefoil on
    // a yellow disc. The trefoil is black on yellow the way the reference is:
    // the grey-contour rule (AGENTS.md section 18.3) is about a dark outline
    // lost against the black board, and this ink sits inside the disc.
    Loader {
        anchors.fill: parent
        active: root.kind === 4
        sourceComponent: nukePart
    }
    Component {
        id: nukePart
        Item {
            Grid {
                x: Math.round((parent.width - width) / 2)
                y: Math.round((parent.height - height) / 2)
                columns: 13
                Repeater {
                    model: 195
                    Rectangle {
                        property int r: Math.floor(index / 13)
                        property int c: index % 13
                        property string ch: root.nuke[r][c]
                        width: Math.max(1, Math.floor(root.side / 13))
                        height: Math.max(1, Math.floor(root.side / 13))
                        color: ch === "G" ? "#6d7d4a" : ch === "D" ? "#464f2c" : ch === "L" ? "#9cb073" : ch === "Y" ? "#e8c53c" : ch === "y" ? "#a3822a" : ch === "K" ? "#1b1c20" : "transparent"
                    }
                }
            }
        }
    }
    // 5 CANNON: a brass shell, muzzle to the left, and no outline at all: the
    // old near-black one swallowed the shape it was meant to draw. Fires shells
    // that break armour in one hit for 15 s.
    Loader {
        anchors.fill: parent
        active: root.kind === 5
        sourceComponent: cannonPart
    }
    Component {
        id: cannonPart
        Item {
            Grid {
                x: Math.round((parent.width - width) / 2)
                y: Math.round((parent.height - height) / 2)
                columns: 9
                Repeater {
                    model: 81
                    Rectangle {
                        property int r: Math.floor(index / 9)
                        property int c: index % 9
                        property string ch: root.cannon[r][c]
                        width: Math.max(1, Math.floor(root.side / 10))
                        height: Math.max(1, Math.floor(root.side / 10))
                        color: ch === "B" ? "#c8862e" : ch === "D" ? "#8a5a20" : ch === "W" ? "#f6e8cc" : "transparent"
                    }
                }
            }
        }
    }
    // 6 FREEZE: a snowflake. Eight arms (vertical, horizontal, both diagonals)
    // with their branch ticks and a white diamond core, so it reads as a
    // crystal and not as a plain asterisk. Stops enemy tanks for 8 s.
    Loader {
        anchors.fill: parent
        active: root.kind === 6
        sourceComponent: freezePart
    }
    Component {
        id: freezePart
        Item {
            Grid {
                x: Math.round((parent.width - width) / 2)
                y: Math.round((parent.height - height) / 2)
                columns: 9
                Repeater {
                    model: 81
                    Rectangle {
                        property int r: Math.floor(index / 9)
                        property int c: index % 9
                        property string ch: root.snow[r][c]
                        width: Math.max(1, Math.floor(root.side / 10))
                        height: Math.max(1, Math.floor(root.side / 10))
                        color: ch === "X" ? "#9fd8ff" : ch === "W" ? "#ffffff" : "transparent"
                    }
                }
            }
        }
    }
}

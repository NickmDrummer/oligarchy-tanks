import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

ApplicationWindow {
    id: win
    width: 1040
    height: 800
    minimumWidth: 840
    minimumHeight: 680
    visible: true
    title: "Tanks"
    color: "#0b0b10"

    property bool helpVisible: false
    property int cell: Math.max(30, Math.floor(Math.min(boardArea.width / backend.cols, boardArea.height / backend.rows)))
    // Retro palette.
    property color p1body: "#4a90ff"
    property color p1dark: "#1c3f94"
    property color p1light: "#bcd2ff"
    property color p2body: "#e5484d"
    property color p2dark: "#7a1f22"
    property color p2light: "#ffc9cb"

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; onActivated: backend.togglePause() }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; onActivated: { backend.startGame(); keys.forceActiveFocus() } }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; onActivated: backend.soundEnabled = !backend.soundEnabled }

    SoundEffect { id: sndShoot; source: "qrc:/sounds/shoot.wav"; volume: 0.5 }
    SoundEffect { id: sndEShoot; source: "qrc:/sounds/eshoot.wav"; volume: 0.4 }
    SoundEffect { id: sndHit; source: "qrc:/sounds/hit.wav"; volume: 0.5 }
    SoundEffect { id: sndBoom; source: "qrc:/sounds/explode.wav"; volume: 0.7 }

    Connections {
        target: backend
        function onPlaySound(name) {
            if (!backend.soundEnabled)
                return
            if (name === "shoot")
                sndShoot.play()
            else if (name === "eshoot")
                sndEShoot.play()
            else if (name === "hit")
                sndHit.play()
            else if (name === "explode")
                sndBoom.play()
        }
        function onStateChanged() { keys.forceActiveFocus() }
    }

    // Keyboard state forwarding. Held keys set direction flags in C++,
    // edge presses fire. Same path a gamepad will use later.
    Item {
        id: keys
        anchors.fill: parent
        focus: true
        Keys.onPressed: (e) => {
            if (e.isAutoRepeat && (e.key === Qt.Key_Space || e.key === Qt.Key_Return || e.key === Qt.Key_Enter))
                return
            switch (e.key) {
            case Qt.Key_W: backend.setP1Move(0, -1, true); e.accepted = true; break
            case Qt.Key_S: backend.setP1Move(0, 1, true); e.accepted = true; break
            case Qt.Key_A: backend.setP1Move(-1, 0, true); e.accepted = true; break
            case Qt.Key_D: backend.setP1Move(1, 0, true); e.accepted = true; break
            case Qt.Key_Up: backend.setP2Move(0, -1, true); e.accepted = true; break
            case Qt.Key_Down: backend.setP2Move(0, 1, true); e.accepted = true; break
            case Qt.Key_Left: backend.setP2Move(-1, 0, true); e.accepted = true; break
            case Qt.Key_Right: backend.setP2Move(1, 0, true); e.accepted = true; break
            case Qt.Key_Space: backend.fireP1(); e.accepted = true; break
            case Qt.Key_Return:
            case Qt.Key_Enter: backend.fireP2(); e.accepted = true; break
            }
        }
        Keys.onReleased: (e) => {
            if (e.isAutoRepeat)
                return
            switch (e.key) {
            case Qt.Key_W: backend.setP1Move(0, -1, false); break
            case Qt.Key_S: backend.setP1Move(0, 1, false); break
            case Qt.Key_A: backend.setP1Move(-1, 0, false); break
            case Qt.Key_D: backend.setP1Move(1, 0, false); break
            case Qt.Key_Up: backend.setP2Move(0, -1, false); break
            case Qt.Key_Down: backend.setP2Move(0, 1, false); break
            case Qt.Key_Left: backend.setP2Move(-1, 0, false); break
            case Qt.Key_Right: backend.setP2Move(1, 0, false); break
            }
        }
    }

    Timer {
        id: blink
        interval: 530
        running: true
        repeat: true
        onTriggered: blinkDot.visible = !blinkDot.visible
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        // Pixel HUD bar.
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Item {
                width: 190; height: 56
                Text {
                    x: 3; y: 4
                    text: "TANKS"
                    font.family: "Monospace"; font.bold: true; font.pixelSize: 34; font.letterSpacing: 3
                    color: "#000000"
                }
                Text {
                    text: "TANKS"
                    font.family: "Monospace"; font.bold: true; font.pixelSize: 34; font.letterSpacing: 3
                    color: theme.accent
                }
            }
            // P1 panel.
            Rectangle {
                width: 220; height: 56
                color: "#101014"; border.color: win.p1body; border.width: 2
                Column {
                    anchors.centerIn: parent
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "P1 BLUE"
                        font.family: "Monospace"; font.bold: true; font.pixelSize: 13; font.letterSpacing: 2
                        color: win.p1body
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "SC " + (backend.players.length > 0 ? backend.players[0].score : 0) + "  x" + (backend.players.length > 0 ? backend.players[0].lives : 0)
                        font.family: "Monospace"; font.bold: true; font.pixelSize: 15; font.letterSpacing: 1
                        color: "#f2f4f8"
                    }
                }
            }
            // P2 panel.
            Rectangle {
                width: 220; height: 56
                color: "#101014"; border.color: win.p2body; border.width: 2
                Column {
                    anchors.centerIn: parent
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "P2 RED"
                        font.family: "Monospace"; font.bold: true; font.pixelSize: 13; font.letterSpacing: 2
                        color: win.p2body
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "SC " + (backend.players.length > 1 ? backend.players[1].score : 0) + "  x" + (backend.players.length > 1 ? backend.players[1].lives : 0)
                        font.family: "Monospace"; font.bold: true; font.pixelSize: 15; font.letterSpacing: 1
                        color: "#f2f4f8"
                    }
                }
            }
            Item { Layout.fillWidth: true }
            Text {
                id: blinkDot
                text: "● REC"
                font.family: "Monospace"; font.bold: true; font.pixelSize: 13; font.letterSpacing: 2
                color: "#ff705a"
            }
            Rectangle {
                width: 150; height: 56
                color: "#101014"; border.color: "#edc400"; border.width: 2
                Column {
                    anchors.centerIn: parent
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "ENEMY"
                        font.family: "Monospace"; font.bold: true; font.pixelSize: 13; font.letterSpacing: 2
                        color: "#edc400"
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "LEFT " + backend.enemiesLeft
                        font.family: "Monospace"; font.bold: true; font.pixelSize: 15
                        color: "#f2f4f8"
                    }
                }
            }
        }

        Item {
            id: boardArea
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                id: board
                width: win.cell * backend.cols
                height: win.cell * backend.rows
                anchors.centerIn: parent
                color: "#050507"
                border.color: "#3a3a44"
                border.width: 4
                clip: true

                // Pixel tiles.
                Repeater {
                    model: backend.cols * backend.rows
                    Item {
                        property int cx: index % backend.cols
                        property int cy: Math.floor(index / backend.cols)
                        property string t: backend.board.length > index ? backend.board[index] : "."
                        x: cx * win.cell
                        y: cy * win.cell
                        width: win.cell
                        height: win.cell
                        // Brick: stepped two-tone pixel courses.
                        Grid {
                            visible: t === "B"
                            anchors.fill: parent
                            anchors.margins: 1
                            columns: 4
                            rows: 4
                            Repeater {
                                model: 16
                                Rectangle {
                                    property int r: Math.floor(index / 4)
                                    width: win.cell / 4; height: win.cell / 4
                                    color: (r % 2 === 0) ? (index % 2 === 0 ? "#c05a1e" : "#7a3512") : (index % 2 === 0 ? "#7a3512" : "#c05a1e")
                                    border.color: "#3d1c08"; border.width: 1
                                }
                            }
                        }
                        // Steel: plate with corner rivets.
                        Rectangle {
                            visible: t === "S"
                            anchors.fill: parent
                            anchors.margins: 1
                            color: "#6e747e"
                            border.color: "#2c2f36"; border.width: 2
                        }
                        Grid {
                            visible: t === "S"
                            anchors.fill: parent
                            anchors.margins: 4
                            columns: 2
                            rows: 2
                            Repeater {
                                model: 4
                                Item {
                                    width: (win.cell - 8) / 2; height: (win.cell - 8) / 2
                                    Rectangle {
                                        x: 1; y: 1; width: 5; height: 5
                                        color: "#c9ced6"
                                    }
                                }
                            }
                        }
                        // Base star.
                        Rectangle {
                            visible: t === "X" || t === "x"
                            anchors.fill: parent
                            anchors.margins: 1
                            color: t === "X" ? "#edc400" : "#3d1010"
                            border.color: t === "X" ? "#7a5c00" : "#7a241b"
                            border.width: 2
                        }
                        Text {
                            visible: t === "X" || t === "x"
                            anchors.centerIn: parent
                            text: "★"
                            font.pixelSize: win.cell * 0.62
                            color: t === "X" ? "#1a1400" : "#ff705a"
                        }
                    }
                }

                // Enemies by tier: 0 green weak, 1 yellow fast, 2 gray armored.
                Repeater {
                    model: backend.enemies
                    Item {
                        property var e: backend.enemies[index]
                        x: e.x * win.cell - width / 2
                        y: e.y * win.cell - height / 2
                        width: win.cell * 0.94
                        height: win.cell * 0.94
                        TankSprite {
                            anchors.fill: parent
                            dir: e.dir
                            body: e.kind === 2 ? "#9aa0a8" : e.kind === 1 ? "#edc400" : "#57d977"
                            dark: e.kind === 2 ? "#4a4e55" : e.kind === 1 ? "#7a5c00" : "#1f7a33"
                            light: e.kind === 2 ? "#d5d9de" : e.kind === 1 ? "#ffef9e" : "#c9f2d4"
                            muzzle: "#ffffff"
                        }
                        // Armor pips for gray tanks.
                        Row {
                            visible: e.kind === 2
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 2
                            Repeater {
                                model: e.hp
                                Rectangle { width: 6; height: 6; color: "#ffffff"; border.color: "black"; border.width: 1 }
                            }
                        }
                    }
                }

                // Players: P1 blue, P2 red.
                Repeater {
                    model: backend.players
                    Item {
                        property var p: backend.players[index]
                        visible: p.alive
                        x: p.x * win.cell - width / 2
                        y: p.y * win.cell - height / 2
                        width: win.cell * 0.94
                        height: win.cell * 0.94
                        TankSprite {
                            anchors.fill: parent
                            dir: p.dir
                            body: index === 0 ? win.p1body : win.p2body
                            dark: index === 0 ? win.p1dark : win.p2dark
                            light: index === 0 ? win.p1light : win.p2light
                            muzzle: "#ffffff"
                        }
                        Text {
                            anchors.centerIn: parent
                            y: win.cell * 0.12
                            text: index === 0 ? "1" : "2"
                            font.family: "Monospace"; font.bold: true
                            font.pixelSize: Math.max(11, win.cell * 0.3)
                            color: "white"
                            style: Text.Outline
                            styleColor: "black"
                        }
                    }
                }

                // Pixel bullets: glow block + white core, no rounded corners.
                Repeater {
                    model: backend.bullets
                    Item {
                        property var b: backend.bullets[index]
                        property real s: Math.max(8, win.cell * 0.24)
                        x: b.x * win.cell - s / 2
                        y: b.y * win.cell - s / 2
                        width: s; height: s
                        Rectangle {
                            anchors.fill: parent
                            color: b.owner === 2 ? "#ff705a" : "#ffd60a"
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width / 2; height: parent.height / 2
                            color: "#ffffff"
                        }
                    }
                }

                // CRT scanlines on top, pointer-transparent.
                Column {
                    anchors.fill: parent
                    spacing: 0
                    z: 10
                    enabled: false
                    Repeater {
                        model: backend.rows * 2
                        Rectangle { width: board.width; height: 2; color: index % 2 ? "#00000000" : "#14ffffff" }
                    }
                }
                // Vignette corners.
                Rectangle {
                    anchors.fill: parent
                    z: 11
                    enabled: false
                    color: "transparent"
                    border.color: "#000000"
                    border.width: 6
                    opacity: 0.55
                }

                // State overlay: pixel panel, no Material.
                Rectangle {
                    anchors.fill: parent
                    visible: backend.state !== "playing"
                    color: "#d8050507"
                    z: 20
                    Rectangle {
                        anchors.centerIn: parent
                        width: panelCol.implicitWidth + 56
                        height: panelCol.implicitHeight + 44
                        color: "#0d0d12"
                        border.color: theme.accent
                        border.width: 4
                        Rectangle {
                            x: 8; y: 8
                            width: parent.width; height: parent.height
                            color: "#000000"
                            z: -1
                        }
                        Column {
                            id: panelCol
                            anchors.centerIn: parent
                            spacing: 12
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: backend.state === "menu" ? "TANKS" : backend.state === "paused" ? "PAUSED" : backend.state === "victory" ? "VICTORY" : "GAME OVER"
                                font.family: "Monospace"; font.bold: true
                                font.pixelSize: 46; font.letterSpacing: 4
                                color: theme.accent
                                style: Text.Outline
                                styleColor: "black"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: backend.state === "menu"
                                text: "2P CO-OP VS AI - DEFEND THE STAR"
                                font.family: "Monospace"; font.bold: true
                                font.pixelSize: 14; font.letterSpacing: 1
                                color: "#f2f4f8"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: backend.state !== "menu"
                                text: backend.message.toUpperCase()
                                font.family: "Monospace"; font.bold: true
                                font.pixelSize: 14; font.letterSpacing: 1
                                color: "#f2f4f8"
                            }
                            PixelButton {
                                anchors.horizontalCenter: parent.horizontalCenter
                                accent: theme.accent
                                text: backend.state === "menu" ? ">> START <<" : ">> RETRY (R) <<"
                                onClicked: { backend.startGame(); keys.forceActiveFocus() }
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "P1 WASD+SPACE - P2 ARROWS+ENTER"
                                font.family: "Monospace"; font.bold: true
                                font.pixelSize: 12; font.letterSpacing: 1
                                color: "#8a8f98"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "GREEN 100 - YELLOW 200 - GRAY 300"
                                font.family: "Monospace"; font.bold: true
                                font.pixelSize: 12; font.letterSpacing: 1
                                color: "#57d977"
                            }
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "P PAUSE - R RESTART - M SOUND " + (backend.soundEnabled ? "ON" : "OFF") + " - ? HELP - Q QUIT"
            font.family: "Monospace"; font.bold: true
            font.pixelSize: 12; font.letterSpacing: 1
            color: "#5a5e68"
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: win.helpVisible
        color: "#e6050507"
        Rectangle {
            anchors.centerIn: parent
            width: helpCol.implicitWidth + 56
            height: helpCol.implicitHeight + 44
            color: "#0d0d12"
            border.color: theme.accent
            border.width: 4
            Column {
                id: helpCol
                anchors.centerIn: parent
                spacing: 8
                Text { text: "TANKS - MANUAL"; font.family: "Monospace"; font.bold: true; font.pixelSize: 22; font.letterSpacing: 2; color: theme.accent }
                Text { text: "P1 MOVE: W A S D - FIRE: SPACE"; font.family: "Monospace"; font.bold: true; font.pixelSize: 14; color: "white" }
                Text { text: "P2 MOVE: ARROWS - FIRE: ENTER"; font.family: "Monospace"; font.bold: true; font.pixelSize: 14; color: "white" }
                Text { text: "P PAUSE - R RESTART - M SOUND - Q QUIT"; font.family: "Monospace"; font.bold: true; font.pixelSize: 14; color: "white" }
                Text { text: "GREEN WEAK - YELLOW FAST - GRAY 2 HITS"; font.family: "Monospace"; font.bold: true; font.pixelSize: 14; color: "#57d977" }
                Text { text: "DEFEND THE STAR. 8 ENEMIES PER SECTOR."; font.family: "Monospace"; font.bold: true; font.pixelSize: 14; color: "#8a8f98" }
                PixelButton { accent: theme.accent; text: "<< CLOSE (?)"; onClicked: { win.helpVisible = false; keys.forceActiveFocus() } }
            }
        }
    }

    Component.onCompleted: keys.forceActiveFocus()
}

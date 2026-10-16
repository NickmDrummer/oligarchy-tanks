import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtMultimedia

ApplicationWindow {
    id: win
    width: 920
    height: 720
    minimumWidth: 760
    minimumHeight: 600
    visible: true
    title: "Tanks"
    color: "#080203"

    Material.theme: Material.Dark
    Material.accent: theme.accent

    property bool helpVisible: false
    property int cell: 32

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; onActivated: backend.togglePause() }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; onActivated: backend.startGame() }
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            Label {
                text: "TANKS"
                font.pixelSize: 26
                font.bold: true
                color: theme.accent
            }
            Label {
                text: "P1  score " + (backend.players.length > 0 ? backend.players[0].score : 0) + "  lives " + (backend.players.length > 0 ? backend.players[0].lives : 0)
                font.pixelSize: 14
                color: "#dbe5f9"
            }
            Label {
                text: "P2  score " + (backend.players.length > 1 ? backend.players[1].score : 0) + "  lives " + (backend.players.length > 1 ? backend.players[1].lives : 0)
                font.pixelSize: 14
                color: "#92baff"
            }
            Item { Layout.fillWidth: true }
            Label {
                text: "Enemies " + backend.enemiesLeft
                font.pixelSize: 14
                color: "#edc400"
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                id: board
                width: win.cell * backend.cols
                height: win.cell * backend.rows
                anchors.centerIn: parent
                color: "#0a0a0c"
                border.color: "#2a2a2e"
                border.width: 2

                // Retro scanlines overlay.
                Column {
                    anchors.fill: parent
                    spacing: 0
                    Repeater {
                        model: backend.rows * 2
                        Rectangle { width: board.width; height: 2; color: index % 2 ? "#00000000" : "#0dffffff" }
                    }
                }

                Repeater {
                    model: backend.cols * backend.rows
                    Rectangle {
                        property int cx: index % backend.cols
                        property int cy: Math.floor(index / backend.cols)
                        property string t: backend.board.length > index ? backend.board[index] : "."
                        x: cx * win.cell
                        y: cy * win.cell
                        width: win.cell
                        height: win.cell
                        color: "transparent"
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: t === "." ? 0 : 2
                            visible: t !== "."
                            color: t === "B" ? "#b3541e" : t === "S" ? "#8a8f98" : t === "X" ? "#dfb700" : "#5c1414"
                            border.color: t === "B" ? "#7a3a14" : "transparent"
                            border.width: 1
                        }
                        // Pixel detail on bricks.
                        Rectangle {
                            visible: t === "B"
                            x: 6; y: parent.height / 2 - 1
                            width: parent.width - 12; height: 2
                            color: "#7a3a14"
                        }
                        // Eagle glyph as text for the base.
                        Text {
                            visible: t === "X" || t === "x"
                            anchors.centerIn: parent
                            text: "★"
                            font.pixelSize: 20
                            color: t === "X" ? "black" : "#ff705a"
                        }
                    }
                }

                Repeater {
                    model: backend.enemies
                    Item {
                        property var e: backend.enemies[index]
                        x: (e.x + 0.5) * win.cell - width / 2
                        y: (e.y + 0.5) * win.cell - height / 2
                        width: win.cell * 0.86
                        height: win.cell * 0.86
                        Rectangle {
                            anchors.fill: parent
                            color: "#d15040"
                            border.color: "#7a241b"
                            border.width: 2
                            radius: 3
                        }
                        Rectangle {
                            // Barrel rotated by direction.
                            width: 6
                            height: parent.height / 2
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.verticalCenter
                            color: "#7a241b"
                            transform: Rotation {
                                origin.x: 3
                                origin.y: parent.height / 4 + parent.height / 4
                                angle: [0, 90, 180, 270][e.dir]
                            }
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: 10; height: 10
                            color: "#f2d8d3"
                            radius: 2
                        }
                    }
                }

                Repeater {
                    model: backend.players
                    Item {
                        property var p: backend.players[index]
                        visible: p.alive
                        x: (p.x + 0.5) * win.cell - width / 2
                        y: (p.y + 0.5) * win.cell - height / 2
                        width: win.cell * 0.86
                        height: win.cell * 0.86
                        Rectangle {
                            anchors.fill: parent
                            color: index === 0 ? theme.accent : "#7098f2"
                            border.color: index === 0 ? theme.accentForeground : "#1c2c55"
                            border.width: 2
                            radius: 3
                        }
                        Rectangle {
                            width: 6
                            height: parent.height / 2
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.verticalCenter
                            color: index === 0 ? theme.accentForeground : "#1c2c55"
                            transform: Rotation {
                                origin.x: 3
                                origin.y: parent.height / 4 + parent.height / 4
                                angle: [0, 90, 180, 270][p.dir]
                            }
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: 10; height: 10
                            color: index === 0 ? theme.accentForeground : "white"
                            radius: 2
                        }
                        Text {
                            anchors.centerIn: parent
                            text: index === 0 ? "1" : "2"
                            font.pixelSize: 12
                            font.bold: true
                            color: index === 0 ? theme.accent : "#1c2c55"
                        }
                    }
                }

                Repeater {
                    model: backend.bullets
                    Rectangle {
                        property var b: backend.bullets[index]
                        x: (b.x + 0.5) * win.cell - 3
                        y: (b.y + 0.5) * win.cell - 3
                        width: 6; height: 6
                        radius: 1
                        color: b.owner === 2 ? "#ff705a" : "#ffffff"
                        border.color: "black"
                        border.width: 1
                    }
                }

                // State overlays.
                Rectangle {
                    anchors.fill: parent
                    visible: backend.state !== "playing"
                    color: "#cc060202"
                    Column {
                        anchors.centerIn: parent
                        spacing: 10
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: backend.state === "menu" ? "TANKS" : backend.state === "paused" ? "PAUSED" : backend.state === "victory" ? "VICTORY" : "GAME OVER"
                            font.pixelSize: 40
                            font.bold: true
                            color: theme.accent
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: backend.state === "menu"
                            text: "2 players co-op vs AI — defend the star base"
                            color: "#dbe5f9"
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: backend.state !== "menu"
                            text: backend.message
                            color: "#dbe5f9"
                        }
                        Button {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: backend.state === "menu" ? "Start game" : "Restart (R)"
                            highlighted: true
                            onClicked: { backend.startGame(); keys.forceActiveFocus() }
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "P1 WASD + Space   ·   P2 Arrows + Enter"
                            color: "#a4acbb"
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: keys.forceActiveFocus()
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "P pause · R restart · M sound " + (backend.soundEnabled ? "on" : "off") + " · ? help · Q quit"
            color: "#696263"
            font.pixelSize: 12
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: win.helpVisible
        color: "#e6080203"
        Column {
            anchors.centerIn: parent
            spacing: 8
            Label { text: "TANKS — shortcuts"; font.pixelSize: 22; font.bold: true; color: theme.accent }
            Label { text: "P1 move: W A S D — fire: Space"; color: "white" }
            Label { text: "P2 move: Arrow keys — fire: Enter"; color: "white" }
            Label { text: "P pause · R restart · M sound · Q quit"; color: "white" }
            Label { text: "Defend the star base. 8 enemies per sector."; color: "#a4acbb" }
            Button { text: "Close (?)"; onClicked: win.helpVisible = false }
        }
        MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }
    }

    Component.onCompleted: keys.forceActiveFocus()
}

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
    property bool helpPausedGame: false
    property int cell: Math.max(30, Math.floor(Math.min(boardArea.width / backend.cols, boardArea.height / backend.rows)))
    property int treadFrame: 0

    // Opening the manual pauses the game; closing it resumes only if the
    // manual caused the pause.
    onHelpVisibleChanged: {
        if (helpVisible && backend.state === "playing") {
            backend.togglePause()
            helpPausedGame = true
        } else if (!helpVisible && helpPausedGame) {
            helpPausedGame = false
            if (backend.state === "paused")
                backend.togglePause()
        }
    }
    // Muted Battle City palette. Players are told apart by color only.
    property color p1body: "#c8a028"
    property color p1dark: "#6e5410"
    property color p1light: "#f0d878"
    property color p2body: "#b03a2e"
    property color p2dark: "#5e1f18"
    property color p2light: "#e8a49c"

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; onActivated: backend.togglePause() }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; onActivated: { if (backend.state === "victory") backend.nextLevel(); else backend.startGame(); keys.forceActiveFocus() } }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; onActivated: backend.soundEnabled = !backend.soundEnabled }

    SoundEffect { id: sndShoot; source: "qrc:/sounds/shoot.wav"; volume: 0.5 }
    SoundEffect { id: sndEShoot; source: "qrc:/sounds/eshoot.wav"; volume: 0.4 }
    SoundEffect { id: sndHit; source: "qrc:/sounds/hit.wav"; volume: 0.5 }
    SoundEffect { id: sndBoom; source: "qrc:/sounds/explode.wav"; volume: 0.7 }
    SoundEffect { id: sndPower; source: "qrc:/sounds/power.wav"; volume: 0.6 }
    SoundEffect { id: sndNuke; source: "qrc:/sounds/nuke.wav"; volume: 0.6 }
    // 8-bit engine putter, loops only while a player tank is rolling.
    // Note: SoundEffect.playing is read-only, so a timer drives play/stop.
    SoundEffect {
        id: sndEngine
        source: "qrc:/sounds/engine.wav"
        loops: SoundEffect.Infinite
        volume: 0.22
    }
    Timer {
        interval: 150
        running: true
        repeat: true
        onTriggered: {
            var on = backend.state === "playing" && ((backend.players.length > 0 && backend.players[0].moving) || (backend.players.length > 1 && backend.players[1].moving))
            if (on && !sndEngine.playing)
                sndEngine.play()
            else if (!on && sndEngine.playing)
                sndEngine.stop()
        }
    }

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
            else if (name === "power")
                sndPower.play()
            else if (name === "nuke")
                sndNuke.play()
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

    // Tread animation clock. Sprites advance a chevron only while moving.
    Timer {
        interval: 140
        running: true
        repeat: true
        onTriggered: win.treadFrame = (win.treadFrame + 1) % 2
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
                width: 220; height: 70
                color: "#101014"; border.color: win.p1body; border.width: 2
                Column {
                    anchors.centerIn: parent
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "P1 YELLOW"
                        font.family: "Monospace"; font.bold: true; font.pixelSize: 13; font.letterSpacing: 2
                        color: win.p1body
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "SC " + (backend.players.length > 0 ? backend.players[0].score : 0) + "  x" + (backend.players.length > 0 ? backend.players[0].lives : 0)
                        font.family: "Monospace"; font.bold: true; font.pixelSize: 15; font.letterSpacing: 1
                        color: "#f2f4f8"
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: backend.players.length > 0 ? ((backend.players[0].shield ? "SHIELD " : "") + (backend.players[0].speed ? "SPEED " : "") + (backend.players[0].cannon ? "CANNON" : "")) : ""
                        font.family: "Monospace"; font.bold: true; font.pixelSize: 11; font.letterSpacing: 1
                        color: "#ffd60a"
                    }
                }
            }
            // P2 panel.
            Rectangle {
                width: 220; height: 70
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
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: backend.players.length > 1 ? ((backend.players[1].shield ? "SHIELD " : "") + (backend.players[1].speed ? "SPEED " : "") + (backend.players[1].cannon ? "CANNON" : "")) : ""
                        font.family: "Monospace"; font.bold: true; font.pixelSize: 11; font.letterSpacing: 1
                        color: "#ffd60a"
                    }
                }
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: 150; height: 70
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
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: backend.frozen ? "FROZEN" : "SEC " + backend.level
                        font.family: "Monospace"; font.bold: true; font.pixelSize: 11; font.letterSpacing: 1
                        color: backend.frozen ? "#9fd8ff" : "#edc400"
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

                // Brick wall, Battle City style: dark mortar courses with
                // offset bricks, muted clay colors, highlight on top edge.
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
                        Rectangle {
                            visible: t === "B"
                            anchors.fill: parent
                            color: "#2e1a10"
                        }
                        Column {
                            visible: t === "B"
                            anchors.fill: parent
                            anchors.margins: 1
                            spacing: 2
                            Repeater {
                                model: 4
                                Item {
                                    property bool odd: index % 2 === 1
                                    width: win.cell - 2
                                    height: (win.cell - 2 - 6) / 4
                                    // Even course: two whole bricks.
                                    Row {
                                        visible: !odd
                                        anchors.fill: parent
                                        spacing: 2
                                        Repeater {
                                            model: 2
                                            Rectangle {
                                                width: (win.cell - 2 - 2) / 2
                                                height: parent.height
                                                color: "#9e5230"
                                                Rectangle {
                                                    width: parent.width; height: 2
                                                    color: "#b96a3e"
                                                }
                                                Rectangle {
                                                    y: parent.height - 2
                                                    width: parent.width; height: 2
                                                    color: "#5e2f18"
                                                }
                                            }
                                        }
                                    }
                                    // Odd course: half + whole + half, offset joints.
                                    Row {
                                        visible: odd
                                        anchors.fill: parent
                                        spacing: 2
                                        Rectangle {
                                            width: (win.cell - 2 - 4) / 4
                                            height: parent.height
                                            color: "#9e5230"
                                            Rectangle { width: parent.width; height: 2; color: "#b96a3e" }
                                            Rectangle { y: parent.height - 2; width: parent.width; height: 2; color: "#5e2f18" }
                                        }
                                        Rectangle {
                                            width: (win.cell - 2 - 4) / 2
                                            height: parent.height
                                            color: "#9e5230"
                                            Rectangle { width: parent.width; height: 2; color: "#b96a3e" }
                                            Rectangle { y: parent.height - 2; width: parent.width; height: 2; color: "#5e2f18" }
                                        }
                                        Rectangle {
                                            width: (win.cell - 2 - 4) / 4
                                            height: parent.height
                                            color: "#9e5230"
                                            Rectangle { width: parent.width; height: 2; color: "#b96a3e" }
                                            Rectangle { y: parent.height - 2; width: parent.width; height: 2; color: "#5e2f18" }
                                        }
                                    }
                                }
                            }
                        }
                        // Concrete pavers: gray blocks with dark grout joints.
                        Rectangle {
                            visible: t === "S"
                            anchors.fill: parent
                            color: "#3a3a3a"
                        }
                        Grid {
                            visible: t === "S"
                            anchors.fill: parent
                            anchors.margins: 2
                            columns: 2
                            rows: 2
                            spacing: 2
                            Repeater {
                                model: 4
                                Rectangle {
                                    width: (win.cell - 4 - 2) / 2
                                    height: (win.cell - 4 - 2) / 2
                                    color: "#8d8d8d"
                                    Rectangle {
                                        width: parent.width; height: 2
                                        color: "#b5b5b5"
                                    }
                                    Rectangle {
                                        x: parent.width - 8; y: parent.height - 8
                                        width: 6; height: 6
                                        color: "#6a6a6a"
                                    }
                                }
                            }
                        }
                        // Base: the official Omarchy wordmark in pixels. Defend it.
                        Item {
                            id: baseTile
                            visible: t === "X" || t === "x"
                            anchors.fill: parent
                            property bool dead: t === "x"
                            // 3x5 micro font spelling "omarchy", 27x5 blocks.
                            property var word: [
                                "XXX X.X .X. XX. .XX X.. X.X",
                                "X.X XXX X.X X.X X.. X.. X.X",
                                "X.X X.X XXX X.. X.. XXX XXX",
                                "X.X X.X X.X X.. X.. X.X .X.",
                                "XXX X.X X.X X.. .XX X.X .X.",
                            ]
                            property real px: Math.max(1.5, win.cell / 30)
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 1
                                color: baseTile.dead ? "#3d1010" : "#0d0d12"
                                border.color: baseTile.dead ? "#7a241b" : theme.accent
                                border.width: 2
                            }
                            Grid {
                                anchors.centerIn: parent
                                columns: 27
                                rows: 5
                                Repeater {
                                    model: 135
                                    Rectangle {
                                        property int r: Math.floor(index / 27)
                                        property int c: index % 27
                                        property string ch: baseTile.word[r][c]
                                        width: baseTile.px
                                        height: baseTile.px
                                        color: ch === "X" ? (baseTile.dead ? "#7a3535" : theme.accent) : "transparent"
                                    }
                                }
                            }
                        }
                    }
                }

                // Enemies: 0 gray basic, 1 green fast, 2 steel armored.
                // Fixed model: delegates persist, only properties update.
                Repeater {
                    model: 6
                    Item {
                        property var e: backend.enemyAt(index, backend.frameTick)
                        visible: e.alive
                        x: e.x * win.cell - width / 2
                        y: e.y * win.cell - height / 2
                        width: win.cell * 0.94
                        height: win.cell * 0.94
                        TankSprite {
                            anchors.fill: parent
                            dir: e.dir
                            frame: e.moving ? win.treadFrame : 0
                            body: e.kind === 2 ? "#6a6e76" : e.kind === 1 ? "#5da24a" : "#8d8d94"
                            dark: e.kind === 2 ? "#2c2e34" : e.kind === 1 ? "#2e5b26" : "#4a4d55"
                            light: e.kind === 2 ? "#c9ced6" : e.kind === 1 ? "#bfe3b4" : "#d5d9de"
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

                // Players told apart by color only: P1 yellow, P2 red.
                Repeater {
                    model: 2
                    Item {
                        property var p: backend.playerAt(index, backend.frameTick)
                        visible: p.alive
                        x: p.x * win.cell - width / 2
                        y: p.y * win.cell - height / 2
                        width: win.cell * 0.94
                        height: win.cell * 0.94
                        TankSprite {
                            anchors.fill: parent
                            dir: p.dir
                            frame: p.moving ? win.treadFrame : 0
                            body: index === 0 ? win.p1body : win.p2body
                            dark: index === 0 ? win.p1dark : win.p2dark
                            light: index === 0 ? win.p1light : win.p2light
                        }
                        // Shield bubble while protected.
                        Rectangle {
                            visible: p.shield
                            anchors.fill: parent
                            anchors.margins: -4
                            color: "transparent"
                            border.color: win.treadFrame === 0 ? "#ffffff" : "#8d8d94"
                            border.width: 3
                        }
                    }
                }

                // Pixel bullets, brass cannon shells while powered.
                Repeater {
                    model: 16
                    Item {
                        property var b: backend.bulletAt(index, backend.frameTick)
                        visible: b.active
                        property real s: b.big ? Math.max(12, win.cell * 0.38) : Math.max(8, win.cell * 0.24)
                        x: b.x * win.cell - s / 2
                        y: b.y * win.cell - s / 2
                        width: s; height: s
                        Rectangle {
                            anchors.fill: parent
                            color: b.big ? "#c8963c" : b.owner === 2 ? "#ff705a" : "#ffd60a"
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width / 2; height: parent.height / 2
                            color: "#ffffff"
                        }
                    }
                }

                // Bonus pickups in the Battle City frame: white border,
                // blue hard shadow, dark navy interior, word label below.
                Item {
                    id: pickupBox
                    property var k: backend.pickupAt(backend.frameTick)
                    visible: k.active
                    property real s: win.cell * 1.1
                    property var cols: ["#edc400", "#c9ced6", "#8d8d94", "#e8e8e8", "#4e8a42", "#c8963c", "#9fd8ff"]
                    property var labels: ["SPEED", "SHIELD", "WALL", "1UP", "GRENADE", "CANNON", "FREEZE"]
                    x: k.x * win.cell - s / 2
                    y: k.y * win.cell - (s + 18) / 2
                    width: s; height: s + 18
                    opacity: (k.age > 12 && win.treadFrame === 0) ? 0.35 : 1
                    Rectangle {
                        x: 4; y: 4
                        width: parent.s; height: parent.s
                        color: "#2a4a8c"
                    }
                    Rectangle {
                        width: parent.s; height: parent.s
                        color: "#0a1030"
                        border.color: "white"
                        border.width: 2
                    }
                    PickupIcon {
                        y: 4
                        width: parent.width
                        height: parent.s - 8
                        kind: parent.k.kind
                        side: Math.min(parent.width - 14, parent.s - 16)
                    }
                    Text {
                        y: parent.s + 2
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: parent.labels[parent.k.kind]
                        font.family: "Monospace"; font.bold: true
                        font.pixelSize: 11; font.letterSpacing: 1
                        color: parent.cols[parent.k.kind]
                        style: Text.Outline
                        styleColor: "black"
                    }
                }

                // Floating kill scores in pixel type.
                Repeater {
                    model: 8
                    Text {
                        property var f: backend.floaterAt(index, backend.frameTick)
                        visible: f.active
                        x: f.x * win.cell - 24
                        y: f.y * win.cell - 12 - f.age * 34
                        width: 72
                        horizontalAlignment: Text.AlignHCenter
                        text: f.text
                        font.family: "Monospace"; font.bold: true
                        font.pixelSize: 19
                        color: "#ffd60a"
                        style: Text.Outline
                        styleColor: "black"
                        opacity: Math.max(0, 1 - f.age / 1.2)
                        z: 9
                    }
                }

                // Forest canopy above everything: hides tanks like the original.
                // Each tile stamps a round leafy blob spilling past its edges,
                // so neighbours merge into one ragged canopy with holes.
                Repeater {
                    model: backend.cols * backend.rows
                    Item {
                        id: forestTile
                        property int cx: index % backend.cols
                        property int cy: Math.floor(index / backend.cols)
                        property string t: backend.board.length > index ? backend.board[index] : "."
                        property int seed: index
                        visible: t === "F"
                        x: cx * win.cell
                        y: cy * win.cell
                        width: win.cell
                        height: win.cell
                        z: 8
                        Grid {
                            x: -0.3 * win.cell
                            y: -0.3 * win.cell
                            columns: 8
                            rows: 8
                            Repeater {
                                model: 64
                                Rectangle {
                                    property int r: Math.floor(index / 8)
                                    property int c: index % 8
                                    property real dist: Math.max(Math.abs(c - 3.5), Math.abs(r - 3.5))
                                    property int h: (forestTile.seed * 13 + r * 7 + c * 17) % 100
                                    width: win.cell * 0.2; height: win.cell * 0.2
                                    color: {
                                        if (dist > 3.5)
                                            return "transparent"
                                        if (dist > 2.5) {
                                            if (h > 50)
                                                return "transparent"
                                            return h < 50 ? "#2f6630" : "#4e8a42"
                                        }
                                        if (h < 6)
                                            return "transparent"
                                        if (h < 42)
                                            return "#2f6630"
                                        if (h < 80)
                                            return "#1d3d1a"
                                        if (h < 95)
                                            return "#4e8a42"
                                        return "#6aa84f"
                                    }
                                }
                            }
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
                                text: "2P CO-OP VS AI - DEFEND THE BASE"
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
                                text: backend.state === "menu" ? ">> START <<" : backend.state === "victory" ? ">> NEXT SECTOR (R) <<" : ">> RETRY (R) <<"
                                onClicked: { if (backend.state === "victory") backend.nextLevel(); else backend.startGame(); keys.forceActiveFocus() }
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
                                text: "GRAY 100 - GREEN 200 - STEEL 300"
                                font.family: "Monospace"; font.bold: true
                                font.pixelSize: 12; font.letterSpacing: 1
                                color: "#8d8d94"
                            }
                            // Bonus preview so the pickups are known before playing.
                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: 10
                                Repeater {
                                    model: 7
                                    PickupIcon {
                                        width: 26; height: 26
                                        kind: index
                                        side: 24
                                    }
                                }
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "GREEN DROPS BONUS - FOREST HIDES TANKS"
                                font.family: "Monospace"; font.bold: true
                                font.pixelSize: 12; font.letterSpacing: 1
                                color: "#4e8a42"
                            }
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "P PAUSE - R RESTART - M SOUND " + (backend.soundEnabled ? "ON" : "OFF") + " - ? HELP - Q QUIT - GREEN DROPS BONUS"
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
                Text { text: "GRAY BASIC - GREEN FAST - STEEL 2 HITS"; font.family: "Monospace"; font.bold: true; font.pixelSize: 14; color: "#8d8d94" }
                Text { text: "BONUS DROP FROM GREEN TANKS - ONE AT A TIME"; font.family: "Monospace"; font.bold: true; font.pixelSize: 14; color: "#edc400" }
                Grid {
                    anchors.horizontalCenter: parent.horizontalCenter
                    columns: 4
                    columnSpacing: 16
                    rowSpacing: 8
                    Repeater {
                        model: [["SPEED", "#edc400"], ["SHIELD", "#c9ced6"], ["WALL", "#8d8d94"], ["1UP", "#e8e8e8"], ["GRENADE", "#4e8a42"], ["CANNON", "#c8963c"], ["FREEZE", "#9fd8ff"]]
                        Column {
                            spacing: 2
                            PickupIcon {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 36; height: 36
                                kind: index
                                side: 32
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData[0]
                                font.family: "Monospace"; font.bold: true; font.pixelSize: 11
                                color: modelData[1]
                            }
                        }
                    }
                }
                Text { text: "DEFEND THE BASE. LATER SECTORS: MORE TANKS,"; font.family: "Monospace"; font.bold: true; font.pixelSize: 14; color: "#8a8f98" }
                Text { text: "FASTER SPECIALS, TOUGHER ARMOR."; font.family: "Monospace"; font.bold: true; font.pixelSize: 14; color: "#8a8f98" }
                PixelButton { accent: theme.accent; text: "<< CLOSE (?)"; onClicked: { win.helpVisible = false; keys.forceActiveFocus() } }
            }
        }
    }

    Component.onCompleted: keys.forceActiveFocus()
}

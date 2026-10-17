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

    // Tile kinds, matching the non-empty entries of Backend::Tile. An empty
    // cell matches none of them and simply draws nothing.
    readonly property int tileBrick: 1
    readonly property int tileSteel: 2
    readonly property int tileBase: 3
    readonly property int tileDead: 4
    readonly property int tileForest: 5

    // True while a brick strip has been shot away. Strips 0-3 are the
    // vertical ones, left to right; 4-7 the horizontal ones, top to bottom.
    // backend.boardRev is what makes the binding re-run, and only when the
    // board really changes: pulling a string property from QML every frame
    // is what used to cost ~100 ms a frame (AGENTS.md section 7).
    function brickGone(cx, cy, strip) {
        return backend.brickStripGone(cx, cy, strip, backend.boardRev)
    }

    // Gamepads. The keyboard is always live for both players, so this is a
    // label, not a lock: it names the device that drives each tank once a pad
    // has pressed START to join. With two tanks the first pad takes player 2
    // (player 1 stays on the keyboard) and a second pad takes player 1; with a
    // single tank the pad takes that one.
    function padLabel(player) {
        const list = backend.pads
        for (let i = 0; i < list.length; ++i)
            if (list[i].player === player)
                return "PAD " + (list[i].index + 1)
        return "KEY"
    }
    // Any pad at all: decides whether the join hints mention START.
    readonly property bool padConnected: backend.pads.length > 0

    // The hint that blinks under a tank until its player has used a control: the
    // keys that join it, as key caps — the keyboard key that drives it, plus
    // START while there is a pad around. The keyboard is always live, so this
    // only ever invites: the caps give way to the KEY / PAD label once the
    // player has actually played.
    function joinKeys(player) {
        const keys = [{ label: player === 0 ? "SPACE" : "ENTER" }]
        // In a two-player game the first pad takes player 2, so only a
        // one-player game invites player 1 with START.
        if (padConnected && (player === 1 || backend.playerCount === 1))
            keys.push({ label: "START" })
        return keys
    }

    // The keys that play each mode, in the order the modes are shown: player 1
    // on the keyboard, player 2 on the arrows.
    readonly property var modeKeys: [
        [{ label: "W" }, { label: "A" }, { label: "S" }, { label: "D" }, { label: "SPACE" }],
        [{ arrow: "up" }, { arrow: "left" }, { arrow: "down" }, { arrow: "right" }, { label: "ENTER" }]
    ]

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; onActivated: backend.togglePause() }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; onActivated: { if (backend.state === "select") backend.menuConfirm(); else if (backend.state === "victory") backend.nextLevel(); else backend.toSelect(); keys.forceActiveFocus() } }
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
        // SELECT / Share on a pad opens the manual, exactly like "?".
        function onHelpRequested() { win.helpVisible = !win.helpVisible }
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

        // Pixel HUD bar: player 1 on the left, the game name in the centre,
        // player 2 on the right. The enemy count moved to the side panel.
        Item {
            Layout.fillWidth: true
            height: 112
            RowLayout {
                anchors.fill: parent
                spacing: 10
                // P1, with left padding (50% of the box width).
                Rectangle {
                    Layout.leftMargin: 120
                    width: 240; height: 112
                    color: "#101014"; border.color: win.p1body; border.width: 2
                    Column {
                        anchors.centerIn: parent
                        spacing: 4
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 10
                            TankSprite {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 30; height: 30
                                dir: 0; frame: 0
                                body: win.p1body; dark: win.p1dark; light: win.p1light
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "P1"
                                font.bold: true; font.pixelSize: 18; font.letterSpacing: 2
                                color: win.p1body
                            }
                            Item { width: 8; height: 1 }
                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 10
                                Repeater {
                                    model: 5
                                    TankSprite {
                                        width: 12; height: 12
                                        dir: 0; frame: 0
                                        body: win.p1body; dark: win.p1dark; light: win.p1light
                                        visible: backend.players.length > 0
                                                 && index < backend.players[0].lives
                                    }
                                }
                            }
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "SCORE " + (backend.players.length > 0 ? backend.players[0].score : 0)
                            font.bold: true; font.pixelSize: 19; font.letterSpacing: 1
                            color: "#f2f4f8"
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: backend.players.length > 0
                                  ? ((backend.players[0].shield ? "SHIELD " : "")
                                     + (backend.players[0].speed ? "SPEED " : "")
                                     + (backend.players[0].cannon ? "CANNON " : ""))
                                  : ""
                            font.bold: true; font.pixelSize: 14; font.letterSpacing: 1
                            color: "#ffd60a"
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: backend.players.length > 0
                                     && backend.players[0].lives === 0
                            text: "KILLED"
                            font.bold: true; font.pixelSize: 14; font.letterSpacing: 2
                            color: "#e85050"
                        }
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 4
                            visible: backend.state !== "select"
                                     && backend.players.length > 0
                                     && !backend.players[0].joined
                            Repeater {
                                model: win.joinKeys(0)
                                KeyCap { unit: 23; label: modelData.label; face: theme.accent }
                            }
                            SequentialAnimation on opacity {
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.3; duration: 550 }
                                NumberAnimation { to: 1.0; duration: 550 }
                            }
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                // P2, with right padding (50% of the box width). Hidden in a one-player game.
                Rectangle {
                    Layout.rightMargin: 120
                    width: 240; height: 112
                    visible: backend.playerCount === 2
                    color: "#101014"; border.color: win.p2body; border.width: 2
                    Column {
                        anchors.centerIn: parent
                        spacing: 4
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 10
                            TankSprite {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 30; height: 30
                                dir: 0; frame: 0
                                body: win.p2body; dark: win.p2dark; light: win.p2light
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "P2"
                                font.bold: true; font.pixelSize: 18; font.letterSpacing: 2
                                color: win.p2body
                            }
                            Item { width: 8; height: 1 }
                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 10
                                Repeater {
                                    model: 5
                                    TankSprite {
                                        width: 12; height: 12
                                        dir: 0; frame: 0
                                        body: win.p2body; dark: win.p2dark; light: win.p2light
                                        visible: backend.players.length > 1
                                                 && index < backend.players[1].lives
                                    }
                                }
                            }
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "SCORE " + (backend.players.length > 1 ? backend.players[1].score : 0)
                            font.bold: true; font.pixelSize: 19; font.letterSpacing: 1
                            color: "#f2f4f8"
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: backend.players.length > 1
                                  ? ((backend.players[1].shield ? "SHIELD " : "")
                                     + (backend.players[1].speed ? "SPEED " : "")
                                     + (backend.players[1].cannon ? "CANNON " : ""))
                                  : ""
                            font.bold: true; font.pixelSize: 14; font.letterSpacing: 1
                            color: "#ffd60a"
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: backend.players.length > 1
                                     && backend.players[1].lives === 0
                            text: "KILLED"
                            font.bold: true; font.pixelSize: 14; font.letterSpacing: 2
                            color: "#e85050"
                        }
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 4
                            visible: backend.state !== "select"
                                     && backend.players.length > 1
                                     && !backend.players[1].joined
                            Repeater {
                                model: win.joinKeys(1)
                                KeyCap { unit: 23; label: modelData.label; face: theme.accent }
                            }
                            SequentialAnimation on opacity {
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.3; duration: 550 }
                                NumberAnimation { to: 1.0; duration: 550 }
                            }
                        }
                    }
                }
            }
            // Game name, centred over the bar.
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                width: 190; height: 56
                z: 1
                Text {
                    x: 3; y: 4
                    text: "TANKS"
                    font.bold: true; font.pixelSize: 34; font.letterSpacing: 3
                    color: "#000000"
                }
                Text {
                    text: "TANKS"
                    font.bold: true; font.pixelSize: 34; font.letterSpacing: 3
                    color: theme.accent
                }
            }
        }

        Item {
            id: boardArea
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Battle City style side panel: stage and remaining enemies.
            // Content is shown only while playing; the panel stays in
            // place so the board never jumps when a game starts.
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 160
                color: "#0d0d12"
                Column {
                    anchors.centerIn: parent
                    spacing: 13
                    visible: backend.state === "playing"
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "STAGE " + backend.level
                        font.bold: true; font.pixelSize: 20; font.letterSpacing: 2
                        color: "#edc400"
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "ENEMIES"
                        font.bold: true; font.pixelSize: 17; font.letterSpacing: 2
                        color: "#edc400"
                    }
                    Grid {
                        anchors.horizontalCenter: parent.horizontalCenter
                        columns: 2
                        spacing: 20
                        Repeater {
                            model: 32
                            TankSprite {
                                width: 20; height: 20
                                dir: 0; frame: 0
                                body: "#8d8d8d"; dark: "#4a4a4a"; light: "#b5b5b5"
                                visible: index < backend.enemiesLeft
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: board
                width: win.cell * backend.cols
                height: win.cell * backend.rows
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: -80
                anchors.verticalCenter: parent.verticalCenter
                color: "#050507"
                border.color: "#3a3a44"
                border.width: 4
                clip: true

                // Brick wall, Battle City style: dark mortar courses with
                // offset bricks, muted clay colors, highlight on top edge.
                Repeater {
                    model: backend.cols * backend.rows
                    Item {
                        id: tileItem
                        property int cx: index % backend.cols
                        property int cy: Math.floor(index / backend.cols)
                        // Kind comes from an int query keyed on boardRev, so
                        // it is only recomputed when the map changes.
                        property int kind: backend.tileKind(cx, cy, backend.boardRev)
                        x: cx * win.cell
                        y: cy * win.cell
                        width: win.cell
                        height: win.cell
                        // Only the kind this tile actually is gets instantiated.
                        // `visible: false` still builds every child, which cost
                        // tens of thousands of items across the 15x15 board.
                        Loader {
                            anchors.fill: parent
                            active: tileItem.kind === win.tileBrick
                            sourceComponent: brickPart
                        }
                        // Brick wall, Battle City style: dark mortar courses
                        // with offset bricks and a highlight on the top edge.
                        // A shot shears off the strip on the face it came
                        // from; the gap shows the board behind, so the
                        // brickwork keeps exactly its original look.
                        Component {
                            id: brickPart
                            Item {
                                anchors.fill: parent
                                Rectangle {
                                    anchors.fill: parent
                                    color: "#2e1a10"
                                }
                                Column {
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
                                                        Rectangle { width: parent.width; height: 2; color: "#b96a3e" }
                                                        Rectangle { y: parent.height - 2; width: parent.width; height: 2; color: "#5e2f18" }
                                                    }
                                                }
                                            }
                                            // Odd course: half, whole, half, so the
                                            // joints are offset like real brickwork.
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
                                // Sheared strips: 0-3 vertical, left to right;
                                // 4-7 horizontal, top to bottom. The gap is
                                // painted over the whole cell slice, edges
                                // included, so nothing of the wall is left
                                // behind: no mortar rim, no lighter fill.
                                Repeater {
                                    model: 8
                                    Rectangle {
                                        property bool vertical: index < 4
                                        property int slot: index % 4
                                        // Integer edges, so two neighbouring
                                        // strips cannot leave a hairline seam.
                                        property real e0: Math.round(slot * win.cell / 4)
                                        property real e1: Math.round((slot + 1) * win.cell / 4)
                                        visible: win.brickGone(tileItem.cx, tileItem.cy, index)
                                        x: vertical ? e0 : 0
                                        y: vertical ? 0 : e0
                                        width: vertical ? e1 - e0 : win.cell
                                        height: vertical ? win.cell : e1 - e0
                                        color: "#050507"
                                    }
                                }
                            }
                        }
                        Loader {
                            anchors.fill: parent
                            active: tileItem.kind === win.tileSteel
                            sourceComponent: steelPart
                        }
                        // Concrete pavers: gray blocks with dark grout joints.
                        Component {
                            id: steelPart
                            Item {
                                anchors.fill: parent
                                Rectangle {
                                    anchors.fill: parent
                                    color: "#3a3a3a"
                                }
                                Grid {
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
                            }
                        }
                        Loader {
                            anchors.fill: parent
                            active: tileItem.kind === win.tileBase || tileItem.kind === win.tileDead
                            sourceComponent: basePart
                        }
                        // Base: the official Omarchy wordmark in pixels. Defend it.
                        Component {
                            id: basePart
                            Item {
                                id: baseTile
                                anchors.fill: parent
                                property bool dead: tileItem.kind === win.tileDead
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
                                model: Math.max(0, e.hp)
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
                    property var cols: ["#edc400", "#c9ced6", "#b9c0c9", "#e8e8e8", "#4e8a42", "#c8963c", "#9fd8ff"]
                    property var labels: ["SPEED", "SHIELD", "PICK", "1UP", "GRENADE", "CANNON", "FREEZE"]
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
                        font.bold: true
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
                        font.bold: true
                        font.pixelSize: 19
                        color: "#ffd60a"
                        style: Text.Outline
                        styleColor: "black"
                        opacity: Math.max(0, 1 - f.age / 1.2)
                        z: 9
                    }
                }

                // Forest canopy above everything: hides tanks like the original.
                // A dithered leaf field keyed on global cell coordinates, so
                // neighbouring forest tiles join with no visible seams. Clear
                // cells are real holes you can see through and soft cells are
                // translucent leaves; the rim of a copse thins out into a
                // ragged edge instead of a hard square.
                Repeater {
                    model: backend.cols * backend.rows
                    Item {
                        id: forestTile
                        property int cx: index % backend.cols
                        property int cy: Math.floor(index / backend.cols)
                        property int kind: backend.tileKind(cx, cy, backend.boardRev)
                        x: cx * win.cell
                        y: cy * win.cell
                        width: win.cell
                        height: win.cell
                        z: 8

                        // The 8x8 leaf grid is only built on real forest tiles,
                        // never on the other cells of the board.
                        Loader {
                            anchors.fill: parent
                            active: forestTile.kind === win.tileForest
                            sourceComponent: canopyPart
                        }

                        Component {
                            id: canopyPart
                            Item {
                                id: canopyTile
                                anchors.fill: parent
                                property bool rimUp: tileAt(forestTile.cx, forestTile.cy - 1) !== win.tileForest
                                property bool rimDown: tileAt(forestTile.cx, forestTile.cy + 1) !== win.tileForest
                                property bool rimLeft: tileAt(forestTile.cx - 1, forestTile.cy) !== win.tileForest
                                property bool rimRight: tileAt(forestTile.cx + 1, forestTile.cy) !== win.tileForest

                                // Int query, not a string one: this is also
                                // what keeps the canopy from re-reading the
                                // whole board every frame.
                                function tileAt(x, y) {
                                    return backend.tileKind(x, y, backend.boardRev)
                                }
                                // Stable per-cell hash on global cell coordinates.
                                function leafNoise(gx, gy) {
                                    let h = (gx * 374761393 + gy * 668265263) | 0
                                    h = h ^ (h >>> 15)
                                    h = Math.imul(h, 2246822519)
                                    h = h ^ (h >>> 13)
                                    return (h >>> 0) % 100
                                }
                                function canopy(gx, gy, r, c) {
                                    const n = leafNoise(gx, gy)
                                    if (n < 18)
                                        return "transparent"
                                    if (r === 0 && rimUp || r === 7 && rimDown || c === 0 && rimLeft || c === 7 && rimRight) {
                                        if (n < 55)
                                            return "transparent"
                                    }
                                    if (n < 32)
                                        return "#b0336a29"
                                    const clump = leafNoise(gx >> 1, gy >> 1)
                                    if (clump < 20)
                                        return "#1d3d1a"
                                    if (clump < 45)
                                        return "#2f662c"
                                    if (clump < 80)
                                        return "#468a35"
                                    return "#6fae4a"
                                }
                                function colorAt(r, c) {
                                    return canopy(forestTile.cx * 8 + c, forestTile.cy * 8 + r, r, c)
                                }

                                Grid {
                                    anchors.fill: parent
                                    columns: 8
                                    rows: 8
                                    Repeater {
                                        model: 64
                                        Rectangle {
                                            property int r: Math.floor(index / 8)
                                            property int c: index % 8
                                            width: Math.ceil(forestTile.width / 8)
                                            height: Math.ceil(forestTile.height / 8)
                                            color: canopyTile.colorAt(r, c)
                                        }
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
                            // The select screen breathes: it is the one screen
                            // with room for it.
                            spacing: backend.state === "select" ? 22 : 12
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: backend.state === "select" ? "TANKS"
                                      : backend.state === "gameover" ? (backend.ending === "base" ? "BASE DESTROYED" : "GAME OVER")
                                      : backend.state === "paused" ? "PAUSED" : "VICTORY"
                                font.bold: true
                                font.pixelSize: 46; font.letterSpacing: 4
                                color: theme.accent
                                style: Text.Outline
                                styleColor: "black"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: backend.state === "select"
                                text: "1 OR 2 PLAYERS - CO-OP VS AI - DEFEND THE BASE"
                                font.bold: true
                                font.pixelSize: 14; font.letterSpacing: 1
                                color: "#f2f4f8"
                            }
                            // What the run was worth, on the game over screen.
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: backend.message.length > 0
                                text: backend.message.toUpperCase()
                                font.bold: true
                                font.pixelSize: backend.state === "gameover" ? 22 : 14
                                font.letterSpacing: 2
                                color: "#f2f4f8"
                            }
                            // Mode select, the arcade 1 PLAYER / 2 PLAYERS
                            // screen: each mode's name with a little tank as its
                            // marker, and under it the keys that play that mode.
                            // Left/right moves the highlight, Space, Enter or
                            // START confirms it (the mouse can click either).
                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: backend.state === "select"
                                spacing: 56
                                Repeater {
                                    model: 2
                                    Column {
                                        spacing: 18
                                        Item {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            // Only as wide as the name: the marker
                                            // hangs off to its left, so the name
                                            // and the keys below it share one axis.
                                            width: optionText.implicitWidth
                                            height: Math.max(optionText.implicitHeight, marker.height)
                                            Text {
                                                id: optionText
                                                anchors.centerIn: parent
                                                text: index === 0 ? "1 PLAYER" : "2 PLAYERS"
                                                font.bold: true
                                                font.pixelSize: 22; font.letterSpacing: 2
                                                color: backend.menuIndex === index ? theme.accent : "#6a6e78"
                                            }
                                            // Where the chevron used to be: player
                                            // 1's tank, treads running.
                                            TankSprite {
                                                id: marker
                                                anchors.right: optionText.left
                                                anchors.rightMargin: 8
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: 22; height: 22
                                                dir: 1
                                                frame: win.treadFrame
                                                body: win.p1body
                                                dark: win.p1dark
                                                light: win.p1light
                                                visible: backend.menuIndex === index
                                            }
                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: backend.menuChoose(index)
                                            }
                                        }
                                        // The keys of this mode: player 1's under
                                        // 1 PLAYER, player 2's under 2 PLAYERS.
                                        // The row that is not chosen fades, so
                                        // the eye goes to what the highlighted
                                        // mode needs.
                                        Row {
                                            id: keysRow
                                            // Which mode these keys play: `index` inside
                                            // the Repeater below belongs to the individual
                                            // key, so the option's own index is kept here.
                                            property int option: index
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            spacing: 5
                                            Repeater {
                                                model: win.modeKeys[index]
                                                KeyCap {
                                                    unit: 24
                                                    label: modelData.label !== undefined ? modelData.label : ""
                                                    arrow: modelData.arrow !== undefined ? modelData.arrow : ""
                                                    face: backend.menuIndex === keysRow.option ? "#f2f4f8" : "#6a6e78"
                                                    body: backend.menuIndex === keysRow.option ? "#16161c" : "#0e0e12"
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            // Pad controls as pictures, centred under the two
                            // modes because a pad can drive either tank. Only
                            // there when there is a pad to use.
                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: backend.state === "select" && backend.pads.length > 0
                                spacing: 8
                                Text {
                                    height: 24
                                    verticalAlignment: Text.AlignVCenter
                                    text: "MOVE"
                                    font.bold: true
                                    font.pixelSize: 11; font.letterSpacing: 1
                                    color: "#8a8f98"
                                }
                                PadIcon { kind: "stick"; unit: 24; face: "#8a8f98" }
                                PadIcon { kind: "dpad"; unit: 24; face: "#8a8f98" }
                                Item { width: 10; height: 1 }
                                Text {
                                    height: 24
                                    verticalAlignment: Text.AlignVCenter
                                    text: "FIRE"
                                    font.bold: true
                                    font.pixelSize: 11; font.letterSpacing: 1
                                    color: "#8a8f98"
                                }
                                PadIcon { kind: "trigger"; label: "L2"; unit: 24; face: "#8a8f98" }
                                PadIcon { kind: "trigger"; label: "R2"; unit: 24; face: "#8a8f98" }
                                PadIcon { kind: "face"; label: "X"; unit: 24; face: "#8a8f98" }
                            }
                            // Game over: one way out, and it is the arcade one.
                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: backend.state === "gameover"
                                spacing: 8
                                KeyCap { unit: 26; label: "START"; face: theme.accent }
                                KeyCap { unit: 26; label: "SPACE"; face: theme.accent }
                                KeyCap { unit: 26; label: "ENTER"; face: theme.accent }
                                SequentialAnimation on opacity {
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.25; duration: 550 }
                                    NumberAnimation { to: 1.0; duration: 550 }
                                }
                            }
                            // The mouse needs a way out of every overlay.
                            PixelButton {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: backend.state !== "select"
                                accent: theme.accent
                                text: backend.state === "victory" ? ">> NEXT SECTOR (R) <<"
                                      : backend.state === "gameover" ? ">> CONTINUE <<" : ">> RESUME (P) <<"
                                onClicked: {
                                    if (backend.state === "victory")
                                        backend.nextLevel()
                                    else if (backend.state === "gameover")
                                        backend.toSelect()
                                    else
                                        backend.togglePause()
                                    keys.forceActiveFocus()
                                }
                            }
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "P PAUSE - R MENU - M SOUND " + (backend.soundEnabled ? "ON" : "OFF") + " - ? HELP - Q QUIT - GREEN DROPS BONUS - BUILD " + buildId
            font.bold: true
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
                Text { text: "TANKS - MANUAL"; font.bold: true; font.pixelSize: 22; font.letterSpacing: 2; color: theme.accent }
                Text { text: "P1 MOVE: W A S D - FIRE: SPACE"; font.bold: true; font.pixelSize: 14; color: "white" }
                Text { text: "P2 MOVE: ARROWS - FIRE: ENTER"; font.bold: true; font.pixelSize: 14; color: "white" }
                Text { text: "1 OR 2 PLAYERS: CHOSEN ON THE SELECT SCREEN"; font.bold: true; font.pixelSize: 14; color: "white" }
                Text { text: "P PAUSE - R MENU - M SOUND - Q QUIT"; font.bold: true; font.pixelSize: 14; color: "white" }
                Text { text: "PAD: START JOINS OR PAUSES - SHARE: MANUAL"; font.bold: true; font.pixelSize: 14; color: "white" }
                Text { text: "PAD: STICK OR DPAD MOVE - L2/R2 OR SQUARE-X FIRE"; font.bold: true; font.pixelSize: 14; color: "white" }
                Text { text: "1 PLAYER: THE KEYBOARD UNLESS A PAD PRESSES START"; font.bold: true; font.pixelSize: 14; color: "#8a8f98" }
                Text { text: "2 PLAYERS: KEYBOARD P1, FIRST PAD P2, 2ND PAD P1"; font.bold: true; font.pixelSize: 14; color: "#8a8f98" }
                Text { text: "GAME OVER: START OR SPACE GOES BACK TO THE SELECT SCREEN"; font.bold: true; font.pixelSize: 14; color: "#8a8f98" }
                Text { text: "GRAY BASIC - GREEN FAST - STEEL 2 HITS"; font.bold: true; font.pixelSize: 14; color: "#8d8d94" }
                Text { text: "BONUS DROP FROM GREEN TANKS - ONE AT A TIME"; font.bold: true; font.pixelSize: 14; color: "#edc400" }
                Grid {
                    anchors.horizontalCenter: parent.horizontalCenter
                    columns: 4
                    columnSpacing: 16
                    rowSpacing: 8
                    Repeater {
                        model: [["SPEED", "#edc400"], ["SHIELD", "#c9ced6"], ["PICK", "#b9c0c9"], ["1UP", "#e8e8e8"], ["GRENADE", "#4e8a42"], ["CANNON", "#c8963c"], ["FREEZE", "#9fd8ff"]]
                        Column {
                            spacing: 2
                            PickupIcon {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 40; height: 40
                                kind: index
                                side: 36
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData[0]
                                font.bold: true; font.pixelSize: 11
                                color: modelData[1]
                            }
                        }
                    }
                }
                Text { text: "PICK: BASE WALL BECOMES CONCRETE FOR 20 S"; font.bold: true; font.pixelSize: 14; color: "#b9c0c9" }
                Text { text: "DEFEND THE BASE. LATER SECTORS: MORE TANKS,"; font.bold: true; font.pixelSize: 14; color: "#8a8f98" }
                Text { text: "FASTER SPECIALS, TOUGHER ARMOR."; font.bold: true; font.pixelSize: 14; color: "#8a8f98" }
                PixelButton { accent: theme.accent; text: "<< CLOSE (?)"; onClicked: { win.helpVisible = false; keys.forceActiveFocus() } }
            }
        }
    }

    Component.onCompleted: keys.forceActiveFocus()
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtMultimedia

ApplicationWindow {
    id: win
    // Opens fullscreen: the pixel-art layout is resolution-bound, so the
    // window always renders 1:1. F11 toggles between fullscreen and windowed.
    visibility: Window.FullScreen
    width: 1040
    height: 800
    minimumWidth: 840
    minimumHeight: 680
    visible: true
    title: "Oligarchy Tanks"
    color: "#0b0b10"

    property bool helpVisible: false
    property bool helpPausedGame: false
    // The display is one centred block: board plus side panel, like the
    // screen of an arcade cabinet. The cell is sized from what that block
    // really has (the area minus the panel, the gap between them, the
    // bezel reserve around the block and the board's own 4px margins on
    // both axes), never from the full area, so the whole cabinet fits and
    // the board never leans on the panel.
    readonly property int panelW: 160
    readonly property int displayGap: 14
    // Thickness of the cabinet bezel that frames the display block.
    readonly property int bezW: 20
    property int cell: Math.max(30, Math.floor(Math.min(
        (boardArea.width - win.panelW - win.displayGap - 8 - 2 * win.bezW) / backend.cols,
        (boardArea.height - 8 - 2 * win.bezW) / backend.rows)))
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
    property color p2body: "#5da24a"
    property color p2dark: "#2e5b26"
    property color p2light: "#bfe3b4"

    // Enemy palette. The basic tank is grey and the carrier flashes that same
    // grey with red. The fast tank shares the basic one's chassis family but is
    // a pastel metallic yellow. The armoured tank walks purple -> cyan -> grey
    // as it takes hits, so its last stage is the grey of the basic one.
    readonly property color foeGray: "#a8adb8"
    readonly property color foeGrayDark: "#5d626c"
    readonly property color foeGrayLight: "#e4e8ee"
    readonly property color foeYellow: "#d8c878"
    readonly property color foeYellowDark: "#8c7a3c"
    readonly property color foeYellowLight: "#f6f2d8"
    readonly property color foeRed: "#b03a2e"
    readonly property color foeRedDark: "#5e1f18"
    readonly property color foeRedLight: "#e8a49c"

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
    // F11: fullscreen <-> windowed. Windowed keeps the design size, so the
    // pixel layout reads the same in both states.
    Shortcut { sequence: "F11"; context: Qt.ApplicationShortcut; onActivated: win.visibility = (win.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen) }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; onActivated: backend.togglePause() }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; onActivated: { if (backend.state === "select") backend.menuConfirm(); else if (backend.state === "victory") backend.nextLevel(); else backend.toSelect(); keys.forceActiveFocus() } }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; onActivated: backend.soundEnabled = !backend.soundEnabled }

    SoundEffect { id: sndShoot; source: "qrc:/sounds/shoot.wav"; volume: 0.7 }
    SoundEffect { id: sndHit; source: "qrc:/sounds/hit.wav"; volume: 0.6 }
    SoundEffect { id: sndBoom; source: "qrc:/sounds/explode.wav"; volume: 1.0 }
    SoundEffect { id: sndPower; source: "qrc:/sounds/power.wav"; volume: 0.6 }
    SoundEffect { id: sndNuke; source: "qrc:/sounds/nuke.wav"; volume: 1.0 }
    SoundEffect { id: sndAirSupport; source: "qrc:/sounds/air-support.wav"; volume: 0.8 }
    SoundEffect { id: sndBlastShoot; source: "qrc:/sounds/blast_missile-shoot.wav"; volume: 0.805 }
    SoundEffect { id: sndBlastHit; source: "qrc:/sounds/blast_missile-hit.wav"; volume: 0.96 }
    // 8-bit engine putter, loops only while a player tank is rolling.
    // Note: SoundEffect.playing is read-only, so a timer drives play/stop.
    SoundEffect {
        id: sndEngine
        source: "qrc:/sounds/engine.wav"
        loops: SoundEffect.Infinite
        volume: 0.40
    }
    // Faster gallop for when a tank is actually rolling.
    SoundEffect {
        id: sndEngineMove
        source: "qrc:/sounds/engine_move.wav"
        loops: SoundEffect.Infinite
        volume: 0.0
    }
    Timer {
        interval: 100
        running: true
        repeat: true
        onTriggered: {
            var anyAlive = (backend.players.length > 0 && backend.players[0].alive)
                           || (backend.players.length > 1 && backend.players[1].alive)
            var playing = backend.state === "playing" && anyAlive
            var moving = playing && ((backend.players.length > 0 && backend.players[0].moving) || (backend.players.length > 1 && backend.players[1].moving))
            if (playing) {
                if (!sndEngine.playing)
                    sndEngine.play()
                if (!sndEngineMove.playing)
                    sndEngineMove.play()
                sndEngine.volume = moving ? 0.0 : 0.40
                sndEngineMove.volume = moving ? 0.50 : 0.0
            } else {
                if (sndEngine.playing)
                    sndEngine.stop()
                if (sndEngineMove.playing)
                    sndEngineMove.stop()
            }
        }
    }

    Connections {
        target: backend
        function onPlaySound(name) {
            if (!backend.soundEnabled)
                return
            if (name === "shoot")
                sndShoot.play()
            else if (name === "hit")
                sndHit.play()
            else if (name === "explode")
                sndBoom.play()
            else if (name === "power")
                sndPower.play()
            else if (name === "nuke")
                sndNuke.play()
            else if (name === "air-support")
                sndAirSupport.play()
            else if (name === "blast_missile-shoot")
                sndBlastShoot.play()
            else if (name === "blast_missile-hit")
                sndBlastHit.play()
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

    // Tread animation clock: two frames, so a tread cell lights up on one and
    // its neighbour on the next.
    Timer {
        interval: 90
        running: true
        repeat: true
        onTriggered: win.treadFrame = (win.treadFrame + 1) % 2
    }

    // The cabinet wall mural: a game map of brick and steel tiles (175% of
    // the screen's own tile size) fills the window from the top edge down to
    // just above the footer, so the HUD bar, the game name and the display
    // block all sit on top of it. The map is one Canvas painted per resize,
    // so nothing here costs time in play; the decorative tanks live next to
    // the display, inside boardArea. Hidden when the window leaves no room.
    Item {
        id: wallmap
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        // The layout sits at a 14 px margin; the footer row is its last child.
        height: 14 + footerRow.y
        visible: parent.width > 1100

        Canvas {
            anchors.fill: parent
            enabled: false
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                var B = wallmap.big
                for (var sy = 0; sy * B < height; ++sy) {
                    for (var sx = 0; sx * B < width; ++sx) {
                        var r = (sx * 13 + sy * 7) % 9
                        if (r === 2)
                            continue        // street gap
                        if (r === 6)
                            wallmap.paintSteel(ctx, sx * B, sy * B, B)
                        else
                            wallmap.paintBrick(ctx, sx * B, sy * B, B)
                    }
                }
            }
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            Component.onCompleted: requestPaint()
        }
        readonly property real big: win.cell * 1.75
        // One brick tile in four offset courses, same inks as the game's own
        // bricks.
        function paintBrick(ctx, x, y, B) {
            ctx.save()
            ctx.beginPath()
            ctx.rect(x, y, B, B)
            ctx.clip()
            ctx.fillStyle = "#2e1a10"
            ctx.fillRect(x, y, B, B)
            var mh = Math.max(2, Math.round(B / 22))
            var cH = (B - 3 * mh) / 4
            for (var r = 0; r < 4; ++r) {
                var cy = y + r * (cH + mh)
                var off = (r % 2) * B / 4
                for (var bx = x + off - B / 2; bx < x + B; bx += B / 2) {
                    ctx.fillStyle = "#9e5230"
                    ctx.fillRect(bx + mh / 2, cy, B / 2 - mh, cH)
                    ctx.fillStyle = "#b96a3e"
                    ctx.fillRect(bx + mh / 2, cy, B / 2 - mh, 3)
                    ctx.fillStyle = "#5e2f18"
                    ctx.fillRect(bx + mh / 2, cy + cH - 3, B / 2 - mh, 3)
                }
            }
            ctx.restore()
        }
        // One steel tile: four grey plates with top highlights and dark
        // rivet corners, same construction as the game's steel.
        function paintSteel(ctx, x, y, B) {
            ctx.save()
            ctx.beginPath()
            ctx.rect(x, y, B, B)
            ctx.clip()
            ctx.fillStyle = "#2a2a2a"
            ctx.fillRect(x, y, B, B)
            var g = 4
            var pw = (B - 3 * g) / 2
            for (var py = 0; py < 2; ++py)
                for (var px = 0; px < 2; ++px) {
                    var qx = x + g + px * (pw + g)
                    var qy = y + g + py * (pw + g)
                    ctx.fillStyle = "#8d8d8d"
                    ctx.fillRect(qx, qy, pw, pw)
                    ctx.fillStyle = "#b5b5b5"
                    ctx.fillRect(qx, qy, pw, 3)
                    var rs = Math.max(4, Math.round(B / 22))
                    ctx.fillStyle = "#6a6a6a"
                    ctx.fillRect(qx + pw - rs, qy + pw - rs, rs, rs)
                }
            ctx.restore()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        // Pixel HUD bar: player 1 on the left, the game name in the centre,
        // player 2 on the right. The enemy count moved to the side panel.
        Item {
            id: hudRow
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
                    // Black frame around the panel, so the panel cuts against
                    // the mural behind the HUD bar.
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -10
                        color: "#000000"
                        z: -1
                    }
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
                                     + (backend.players[0].cannon ? "CANNON " : "")
                                     + (backend.players[0].blast ? "BLAST " : "")
                                     + (backend.players[0].tier > 0 ? ("LV " + backend.players[0].tier + " ") : ""))
                                  : ""
                            font.bold: true; font.pixelSize: 14; font.letterSpacing: 1
                            color: "#ffd60a"
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: backend.players.length > 0
                                     && backend.players[0].lives <= 0
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
                    // Black frame around the panel, so the panel cuts against
                    // the mural behind the HUD bar.
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -10
                        color: "#000000"
                        z: -1
                    }
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
                                     + (backend.players[1].cannon ? "CANNON " : "")
                                     + (backend.players[1].blast ? "BLAST " : "")
                                     + (backend.players[1].tier > 0 ? ("LV " + backend.players[1].tier + " ") : ""))
                                  : ""
                            font.bold: true; font.pixelSize: 14; font.letterSpacing: 1
                            color: "#ffd60a"
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: backend.players.length > 1
                                     && backend.players[1].lives <= 0
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

            // The game name, centred between the two panels on a black plate,
            // so it cuts against the mural behind the HUD bar.
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                width: titleAccent.contentWidth + 64
                height: 104
                Rectangle {
                    anchors.fill: parent
                    color: "#000000"
                }
                Text {
                    id: titleShadow
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.horizontalCenterOffset: 4
                    anchors.verticalCenterOffset: 4
                    width: hudRow.width - 2 * (120 + 240 + 32)
                    text: "OLIGARCHY TANKS"
                    // The Omarchy wordmark font: the block-built letters of
                    // the official logo, for the cabinet marquee only.
                    font.family: "Omarchy Font"; font.pixelSize: 56
                    fontSizeMode: Text.HorizontalFit; minimumPixelSize: 22
                    horizontalAlignment: Text.AlignHCenter
                    color: "#000000"
                }
                Text {
                    id: titleAccent
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    width: titleShadow.width
                    text: "OLIGARCHY TANKS"
                    font.family: "Omarchy Font"; font.pixelSize: 56
                    fontSizeMode: Text.HorizontalFit; minimumPixelSize: 22
                    horizontalAlignment: Text.AlignHCenter
                    color: theme.accent
                }
            }
        }

        Item {
            id: boardArea
            Layout.fillWidth: true
            Layout.fillHeight: true

            // The mural's decorative tanks: one of every kind, staged on the
            // perimeter roads beside the display, sized like the wall tiles
            // (175% of a screen tile). dir points at the display; the carrier
            // shows its red. The tile pattern itself paints in wallmap.
            Item {
                id: muralTanks
                anchors.fill: parent
                visible: parent.width > 1100
                readonly property real tp: wallmap.big * 0.94

                TankSprite {
                    x: 48; y: bezel.y + 40
                    width: parent.tp; height: parent.tp
                    dir: 1; frame: win.treadFrame
                    body: win.p1body; dark: win.p1dark; light: win.p1light
                }
                TankSprite {
                    x: 48; y: board.y + board.height * 0.58
                    width: parent.tp; height: parent.tp
                    dir: 1; frame: win.treadFrame
                    body: win.foeGray; dark: win.foeGrayDark; light: win.foeGrayLight
                }
                TankSprite {
                    x: 168; y: board.y + board.height * 0.78
                    width: parent.tp; height: parent.tp
                    dir: 1; frame: win.treadFrame
                    variant: 2
                    body: "#9b7ebd"; dark: "#5a4a6e"; light: "#d4c4e8"
                }
                TankSprite {
                    x: parent.width - 48 - parent.tp; y: bezel.y + 40
                    width: parent.tp; height: parent.tp
                    dir: 3; frame: win.treadFrame
                    body: win.p2body; dark: win.p2dark; light: win.p2light
                }
                TankSprite {
                    x: parent.width - 48 - parent.tp; y: board.y + board.height * 0.58
                    width: parent.tp; height: parent.tp
                    dir: 3; frame: win.treadFrame
                    variant: 1
                    body: win.foeYellow; dark: win.foeYellowDark; light: win.foeYellowLight
                }
                TankSprite {
                    x: parent.width - 168 - parent.tp; y: board.y + board.height * 0.78
                    width: parent.tp; height: parent.tp
                    dir: 3; frame: win.treadFrame
                    body: win.foeRed; dark: win.foeRedDark; light: win.foeRedLight
                }
            }

            // The cabinet bezel: a matte frame around the whole display
            // block (board + side panel), with a brass lip hugging the
            // picture and a screw in every corner, so the screen reads as a
            // coin-op monitor set into a cabinet. Pure Sizes; nothing here
            // covers the playfield pixels.
            Item {
                id: bezel
                x: board.x - win.bezW
                y: board.y - win.bezW
                width: board.width + 2 * win.bezW + win.displayGap + win.panelW
                height: board.height + 2 * win.bezW

                // Frame face.
                Rectangle {
                    anchors.fill: parent
                    color: "#171720"
                }
                // Light from the picture: a top bevel and a bottom drop.
                Rectangle {
                    anchors { left: parent.left; top: parent.top; right: parent.right }
                    height: 2
                    color: "#262631"
                }
                Rectangle {
                    anchors { left: parent.left; bottom: parent.bottom; right: parent.right }
                    height: 4
                    color: "#0b0b0f"
                }
                // Brass lip around the picture opening.
                Rectangle {
                    x: board.x - 2 - bezel.x
                    y: board.y - 2 - bezel.y
                    width: (board.x + board.width + win.displayGap + win.panelW)
                           - board.x + 4
                    height: 2
                    color: "#c8862e"
                }
                Rectangle {
                    x: board.x - 2 - bezel.x
                    y: board.y + board.height - bezel.y
                    width: (board.x + board.width + win.displayGap + win.panelW)
                           - board.x + 4
                    height: 2
                    color: "#c8862e"
                }
                Rectangle {
                    x: board.x - 2 - bezel.x
                    y: board.y - 2 - bezel.y
                    width: 2
                    height: board.height + 4
                    color: "#c8862e"
                }
                Rectangle {
                    x: (board.x + board.width + win.displayGap + win.panelW) - bezel.x
                    y: board.y - 2 - bezel.y
                    width: 2
                    height: board.height + 4
                    color: "#c8862e"
                }
                // One screw per corner: a metal dome with a cross slot.
                Repeater {
                    model: 4
                    Item {
                        x: index % 2 === 0 ? 9 : bezel.width - 21
                        y: index < 2 ? 9 : bezel.height - 21
                        width: 12; height: 12
                        Rectangle {
                            anchors.fill: parent
                            radius: 6
                            color: "#23232d"
                            border.color: "#45454f"
                            border.width: 1
                        }
                        Rectangle {
                            x: 2; y: 5
                            width: 8; height: 2
                            color: "#0c0c11"
                        }
                        Rectangle {
                            x: 5; y: 2
                            width: 2; height: 8
                            color: "#0c0c11"
                        }
                    }
                }
                // Bezel sticker on the bottom rail, the way instruction
                // cards sit on a real cabinet.
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height - win.bezW / 2 - height / 2
                    text: "DEFEND THE OLIGARCHY BASE"
                    font.pixelSize: 11
                    font.letterSpacing: 2
                    color: "#5a5a66"
                }
            }

            // Battle City style side panel: stage and remaining enemies.
            // It rides with the board inside the centred display block and
            // matches its height; its content shows only while playing, the
            // panel itself stays in place so the board never jumps when a
            // game starts.
            Rectangle {
                x: board.x + board.width + win.displayGap
                y: board.y
                width: win.panelW
                height: board.height
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
                                body: win.foeGray; dark: win.foeGrayDark; light: win.foeGrayLight
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
                // Centering the block, not the board: shift left by half the
                // panel column (panel + gap), so board + panel read as one
                // centred screen with the leftover space split evenly.
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: -(win.panelW + win.displayGap) / 2
                anchors.verticalCenter: parent.verticalCenter
                color: "#050507"
                clip: true

                // The picture the tube shows: everything the playfield
                // holds is rendered into a texture and drawn back through
                // the CRT shader below, so the glass can curve, darken and
                // fringe it. The item itself is not drawn.
                Item {
                    id: playfield
                    anchors.fill: parent
                    anchors.margins: 4
                    // The playfield draws through a layer: everything below is
                    // captured to a texture every frame and redrawn through the
                    // CRT effect, in place. (A ShaderEffectSource over a hidden
                    // item froze its texture at the first frame and lost every
                    // dynamic sprite; layer is the supported path.)
                    layer {
                        enabled: true
                        smooth: false
                        effect: ShaderEffect {
                            vertexShader: "qrc:/shaders/crt.vert.qsb"
                            fragmentShader: "qrc:/shaders/crt.frag.qsb"
                        }
                    }

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
                                    // Sheared steel strips: same 4+4 mask as brick,
                                    // so a CANNON shell leaves the same kind of gap.
                                    Repeater {
                                        model: 8
                                        Rectangle {
                                            property bool vertical: index < 4
                                            property int slot: index % 4
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
                                active: tileItem.kind === win.tileBase || tileItem.kind === win.tileDead
                                sourceComponent: basePart
                            }
                            // Base: the official Omarchy icon — the square mark with
                            // no wordmark. Defend it.
                            Component {
                                id: basePart
                                Item {
                                    id: baseTile
                                    anchors.fill: parent
                                    property bool dead: tileItem.kind === win.tileDead
                                    // The square Omarchy logo, 27x13 blocks: the
                                    // official mark (icon.txt) collapsed 2x2, hard
                                    // coded so the game needs nothing from the host.
                                    // Plate on, glyph cut in.
                                    property var mark: [
                                        "XXXXXXXXXXXXXXXXXXXXXXXXXXX",
                                        "XX..........XXX..........XX",
                                        "XX..XXXXXXXXXXX....XXXX..XX",
                                        "XX..XX...............XX..XX",
                                        "XX..XX...............XX..XX",
                                        "XX..XX...............XX..XX",
                                        "XXXXXX...............XX..XX",
                                        "XX..XX...............XX..XX",
                                        "XX..XX...............XX..XX",
                                        "XX..XX...............XX..XX",
                                        "XX..XXXXXXXXXXXXXXXXXXX..XX",
                                        "XX..........XXX..........XX",
                                        "XXXXXXXXXXXXXXX..XXXXXXXXXX",
                                    ]
                                    property string plate: dead ? "#7a3535" : "#9ece6a"
                                    property string glyph: "transparent"
                                    // The mark is 2:1 by design, so uniform blocks
                                    // leave half the tile empty and read squashed:
                                    // stretch the vertical block ~1.7x so the plate
                                    // fills the tile without breaking the shape.
                                    property real px: Math.max(1, win.cell / 29)
                                    property real py: Math.max(1.5, win.cell / 17)
                                    Rectangle {
                                        anchors.fill: parent
                                        anchors.margins: 1
                                        color: baseTile.dead ? "#3d1010" : "#0d0d12"
                                    }
                                    Grid {
                                        anchors.centerIn: parent
                                        columns: 27
                                        rows: 13
                                        Repeater {
                                            model: 351
                                            Rectangle {
                                                property int r: Math.floor(index / 27)
                                                property int c: index % 27
                                                property string ch: baseTile.mark[r][c]
                                                width: baseTile.px
                                                height: baseTile.py
                                                color: ch === "X" ? baseTile.plate : baseTile.glyph
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Enemies: 0 gray basic, 1 yellow wheeled fast, 2 steel armored.
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
                            // FREEZE blinks while it is about to run out, so the
                            // thaw is not a surprise.
                            opacity: (backend.freezeEnding
                                      && Math.floor(backend.frameTick / 20) % 2 === 0) ? 0.25 : 1
                            // Armored tanks shift colour with each hit:
                            // purple (full) -> cyan (1 hit) -> gray (vulnerable).
                            // Carriers flash red <-> gray.
                            property bool carrierFlash: e.kind === 3
                                                        && Math.floor(backend.frameTick / 6) % 2 === 0
                            property color enemyBody: {
                                if (e.kind === 3)
                                    return carrierFlash ? win.foeRed : win.foeGray
                                if (e.kind === 2) {
                                    if (e.hp >= 3) return "#9b7ebd"
                                    if (e.hp === 2) return "#7eb8c4"
                                    return win.foeGray
                                }
                                if (e.kind === 1)
                                    return win.foeYellow
                                return win.foeGray
                            }
                            property color enemyDark: {
                                if (e.kind === 3)
                                    return carrierFlash ? win.foeRedDark : win.foeGrayDark
                                if (e.kind === 2) {
                                    if (e.hp >= 3) return "#5a4a6e"
                                    if (e.hp === 2) return "#3a5a60"
                                    return win.foeGrayDark
                                }
                                if (e.kind === 1)
                                    return win.foeYellowDark
                                return win.foeGrayDark
                            }
                            property color enemyLight: {
                                if (e.kind === 3)
                                    return carrierFlash ? win.foeRedLight : win.foeGrayLight
                                if (e.kind === 2) {
                                    if (e.hp >= 3) return "#d4c4e8"
                                    if (e.hp === 2) return "#c4e4ea"
                                    return win.foeGrayLight
                                }
                                if (e.kind === 1)
                                    return win.foeYellowLight
                                return win.foeGrayLight
                            }
                            // Basic and carrier use treads, the fast tank wheels and
                            // the armoured tank the chunkier plated hull.
                            property int enemyVariant: e.kind === 1 ? 1 : (e.kind === 2 ? 2 : 0)
                            // Frost while time is frozen: a single big iced patch
                            // under the tank. Built only while frozen.
                            Loader {
                                active: backend.frozen
                                anchors.fill: parent
                                z: -1
                                sourceComponent: Component {
                                    Item {
                                        anchors.fill: parent
                                        FrostSprite {
                                            anchors.centerIn: parent
                                            width: parent.width * 1.9
                                            height: parent.height * 1.9
                                        }
                                    }
                                }
                            }
                            TankSprite {
                                anchors.fill: parent
                                dir: e.dir
                                frame: e.moving ? win.treadFrame : 0
                                variant: enemyVariant
                                body: enemyBody
                                dark: enemyDark
                                light: enemyLight
                            }
                        }
                    }

                    // Death explosions: a big white/gray spark flash that turns
                    // into a black/gray smoke cloud with fire burning inside it.
                    Repeater {
                        model: 8
                        Item {
                            property var ex: index < backend.explosions.length ? backend.explosions[index] : null
                            visible: ex !== null
                            x: ex ? ex.x * win.cell - width / 2 : 0
                            y: ex ? ex.y * win.cell - height / 2 : 0
                            property real boom: ex ? Math.min(1.0, ex.age / 0.5) : 0
                            width: win.cell * (1.3 + 1.0 * boom)
                            height: width
                            ExplosionSprite {
                                anchors.fill: parent
                                progress: parent.boom
                            }
                        }
                    }

                    // Players told apart by colour: P1 yellow, P2 green.
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
                            // Armour shields: three white pixel outlines drawn
                            // on the tank sprite boundary, all inscribed in the
                            // same box so each one reaches the tank's perimeter:
                            // a square (the boundary itself), a diamond with its
                            // vertices on the side midpoints, and an inscribed
                            // circle. On a hit the circle is lost first, then
                            // the diamond; the square is the last to go.
                            Canvas {
                                id: armourCanvas
                                x: -(width - parent.width) / 2
                                y: -(height - parent.height) / 2
                                width: parent.width * 1.2
                                height: parent.height * 1.2
                                rotation: [-90, 0, 90, 180][p.dir]
                                visible: p.armorHP > 0
                                property int seenArmor: p.armorHP
                                onSeenArmorChanged: requestPaint()
                                Component.onCompleted: requestPaint()
                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.reset();
                                    ctx.clearRect(0, 0, width, height);
                                    ctx.fillStyle = "#ffffff";
                                    var ps = Math.max(2, Math.round(parent.width / 20));
                                    var cx = width / 2;
                                    var cy = height / 2;
                                    // Half-size of the tank's bounding box, snapped
                                    // to the pixel grid.
                                    var hw = Math.round(parent.width * 0.5 / ps) * ps;
                                    var layers = Math.min(3, Math.floor(p.armorHP));
                                    // Pixel-stair segment between two points.
                                    function line(x0, y0, x1, y1) {
                                        var steps = Math.max(1, Math.round(Math.abs(x1 - x0) / ps)
                                                               + Math.round(Math.abs(y1 - y0) / ps));
                                        for (var k = 0; k <= steps; ++k) {
                                            var t = k / steps;
                                            var x = Math.round((x0 + (x1 - x0) * t) / ps) * ps;
                                            var y = Math.round((y0 + (y1 - y0) * t) / ps) * ps;
                                            ctx.fillRect(x, y, ps, ps);
                                        }
                                    }
                                    if (layers >= 1) {
                                        // Square: outline right on the sprite boundary.
                                        line(cx - hw, cy - hw, cx + hw, cy - hw);
                                        line(cx + hw, cy - hw, cx + hw, cy + hw);
                                        line(cx + hw, cy + hw, cx - hw, cy + hw);
                                        line(cx - hw, cy + hw, cx - hw, cy - hw);
                                    }
                                    if (layers >= 2) {
                                        // Diamond: vertices on the midpoints of the
                                        // tank's sides, so it spans the whole perimeter.
                                        line(cx, cy - hw, cx + hw, cy);
                                        line(cx + hw, cy, cx, cy + hw);
                                        line(cx, cy + hw, cx - hw, cy);
                                        line(cx - hw, cy, cx, cy - hw);
                                    }
                                    if (layers >= 3) {
                                        // Circle: inscribed in the same box, drawn as
                                        // a pixel stair (not a smooth stroke).
                                        var csteps = Math.max(24, Math.round(6 * hw / ps));
                                        for (var a = 0; a < csteps; ++a) {
                                            var ang = 2 * Math.PI * a / csteps;
                                            var x = Math.round((cx + hw * Math.cos(ang)) / ps) * ps;
                                            var y = Math.round((cy + hw * Math.sin(ang)) / ps) * ps;
                                            ctx.fillRect(x, y, ps, ps);
                                        }
                                    }
                                    ctx.fillStyle = "#ffffff";
                                }
                            }
                            // Shield: static crackling around the hull as two
                            // alternating frames of a jagged, rounded ring of
                            // lightning (see ShieldSprite.qml).
                            Loader {
                                // Hidden while armour shields are up: the white
                                // crackling ring would visually collide with the
                                // armour outlines around the tank.
                                active: p.shield && p.armorHP <= 0
                                anchors.fill: parent
                                anchors.margins: -Math.round(parent.width * 0.35)
                                sourceComponent: Component {
                                    ShieldSprite {
                                        anchors.fill: parent
                                        frame: Math.floor(backend.frameTick / 6)
                                    }
                                }
                            }
                        }
                    }

                    // Pixel bullets, brass cannon shells while powered.
                    Repeater {
                        model: 16
                        Item {
                            property var b: backend.bulletAt(index, backend.frameTick)
                            visible: b.active
                            property real s: b.blast ? Math.max(14, win.cell * 0.4) : (b.big ? Math.max(12, win.cell * 0.38) : Math.max(8, win.cell * 0.24))
                            property real mw: b.blast ? s * 0.4 : s
                            property real mh: b.blast ? s * 1.8 : s
                            x: b.x * win.cell - mw / 2
                            y: b.y * win.cell - mh / 2
                            width: mw; height: mh
                            rotation: b.blast ? (b.dir === 0 ? 0 : b.dir === 1 ? 90 : b.dir === 2 ? 180 : 270) : 0
                            // Missile body (dark gray).
                            Rectangle {
                                anchors.fill: parent
                                color: b.blast ? "#4a4a4a" : (b.big ? "#c8963c" : (b.owner === 2 ? "#ff705a" : "#ffd60a"))
                            }
                            // Missile tip (diamond).
                            Rectangle {
                                visible: b.blast
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.top
                                anchors.topMargin: -width / 2
                                width: parent.width * 1.4
                                height: width
                                rotation: 45
                                color: "#7a7a7a"
                            }
                            // Fire trail behind the missile.
                            Rectangle {
                                visible: b.blast
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.bottom
                                width: parent.width * 0.8
                                height: parent.height * 0.5
                                color: "#ff6b35"
                            }
                            Rectangle {
                                visible: b.blast
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.bottom
                                anchors.topMargin: parent.height * 0.3
                                width: parent.width * 0.5
                                height: parent.height * 0.4
                                color: "#ffd700"
                            }
                            Rectangle {
                                visible: !b.blast
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
                        property var cols: ["#edc400", "#c9ced6", "#b9c0c9", "#e8e8e8", "#ffd93b", "#c8963c", "#9fd8ff", "#ffd700", "#ff6b6b"]
                        property var labels: ["SPEED", "SHIELD", "PICK", "LIFE", "NUKE", "CANNON", "FREEZE", "LEVELUP", "BLAST"]
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


                    // NUKE: a semi-transparent yellow blast that grows out of the
                    // spot where the bonus was picked up until it covers the whole
                    // board, then fades (Backend::nuke()).
                    Item {
                        id: nukeFlash
                        property var n: backend.nuke
                        property real p: n.active ? n.progress : 0
                        visible: n.active
                        z: 12
                        x: n.active ? n.x * win.cell : 0
                        y: n.active ? n.y * win.cell : 0
                        // A radius of one board diagonal reaches every corner from
                        // any spot on the board.
                        property real reach: win.cell * Math.sqrt(backend.cols * backend.cols
                                                                 + backend.rows * backend.rows)
                        Rectangle {
                            anchors.centerIn: parent
                            width: nukeFlash.reach * 2 * nukeFlash.p
                            height: width
                            radius: width / 2
                            color: "#ffd93b"
                            opacity: 0.45 * Math.min(1, (1 - nukeFlash.p) / 0.3)
                        }
                    }

                    // BLAST MISSILE impact: a small orange circle that grows out of
                    // the hit spot (Backend::blastImpact()).
                    Item {
                        id: blastFlash
                        property var b: backend.blastImpact
                        property real p: b.active ? b.progress : 0
                        visible: b.active
                        z: 12
                        x: b.active ? b.x * win.cell : 0
                        y: b.active ? b.y * win.cell : 0
                        Rectangle {
                            anchors.centerIn: parent
                            width: win.cell * 3 * blastFlash.p
                            height: width
                            radius: width / 2
                            color: "#ff6b35"
                            opacity: 0.5 * Math.min(1, (1 - blastFlash.p) / 0.5)
                        }
                    }

                    // Air support (LEVELUP tier 4) impact: a white/blue circle that
                    // grows out of the strike spot, 1 s after the sound starts.
                    Item {
                        id: airSupportFlash
                        property var a: backend.airSupportImpact
                        property real p: a.active ? a.progress : 0
                        visible: a.active
                        z: 12
                        x: a.active ? a.x * win.cell : 0
                        y: a.active ? a.y * win.cell : 0
                        Rectangle {
                            anchors.centerIn: parent
                            width: win.cell * 4 * airSupportFlash.p
                            height: width
                            radius: width / 2
                            color: "#a0d8ff"
                            opacity: 0.5 * Math.min(1, (1 - airSupportFlash.p) / 0.4)
                        }
                    }
                }

                // The CRT tube: the playfield texture drawn back through a
                // curved-glass shader — barrel distortion, screen-space
                // scanlines, a corner vignette, a chroma split that grows
                // toward the edges, and a dark rim that follows the
                // curvature (the straight frame lives in the shader now).
                // Needs the GL scene graph; under software rendering the
                // straight picture is not replaced.


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
                                text: backend.state === "select" ? "OLIGARCHY TANKS"
                                      : backend.state === "gameover" ? (backend.ending === "base" ? "BASE DESTROYED" : "GAME OVER")
                                      : backend.state === "paused" ? "PAUSED" : "VICTORY"
                                font.bold: true
                                // The long name takes a smaller face than the
                                // short state words, so the panel never fights
                                // the board for width.
                                font.pixelSize: backend.state === "select" ? 30 : 46
                                font.letterSpacing: backend.state === "select" ? 3 : 4
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
                                text: backend.state === "victory" ? (backend.pads.length > 0 ? ">> NEXT SECTOR (START) <<" : ">> NEXT SECTOR (R) <<")
                                      : backend.state === "gameover" ? ">> CONTINUE <<" : (backend.pads.length > 0 ? ">> RESUME (START) <<" : ">> RESUME (P) <<")
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

        Row {
            id: footerRow
            Layout.alignment: Qt.AlignHCenter
            spacing: 6
            KeyCap { unit: 20; label: "P" }
            Text { text: "PAUSE"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 13; color: "#5a5e68"; anchors.verticalCenter: parent.verticalCenter }
            Text { text: "-"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 13; color: "#5a5e68"; anchors.verticalCenter: parent.verticalCenter }
            KeyCap { unit: 20; label: "R" }
            Text { text: "MENU"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 13; color: "#5a5e68"; anchors.verticalCenter: parent.verticalCenter }
            Text { text: "-"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 13; color: "#5a5e68"; anchors.verticalCenter: parent.verticalCenter }
            KeyCap { unit: 20; label: "M" }
            Text { text: "TOGGLE SOUND"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 13; color: "#5a5e68"; anchors.verticalCenter: parent.verticalCenter }
            Text { text: "-"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 13; color: "#5a5e68"; anchors.verticalCenter: parent.verticalCenter }
            KeyCap { unit: 20; label: "?" }
            Text { text: "HELP"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 13; color: "#5a5e68"; anchors.verticalCenter: parent.verticalCenter }
            Text { text: "-"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 13; color: "#5a5e68"; anchors.verticalCenter: parent.verticalCenter }
            KeyCap { unit: 20; label: "Q" }
            Text { text: "QUIT"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 13; color: "#5a5e68"; anchors.verticalCenter: parent.verticalCenter }
            Text { text: "-"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 13; color: "#5a5e68"; anchors.verticalCenter: parent.verticalCenter }
            Text { text: buildId; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 13; color: "#5a5e68"; anchors.verticalCenter: parent.verticalCenter }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: win.helpVisible
        color: "#e6050507"
        Rectangle {
            anchors.centerIn: parent
            width: helpCol.implicitWidth + 56
            height: Math.min(helpCol.implicitHeight + 44, parent.height - 40)
            color: "#0d0d12"
            border.color: theme.accent
            border.width: 4
            Flickable {
                id: helpFlick
                anchors.fill: parent
                anchors.margins: 22
                contentWidth: width
                contentHeight: helpCol.implicitHeight
                clip: true
                Column {
                    id: helpCol
                    width: helpFlick.width
                    spacing: 20
                // Title.
                Item {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: helpTitle.implicitWidth + 6
                    height: helpTitle.implicitHeight + 6
                    Text {
                        x: 3; y: 3
                        text: "OLIGARCHY TANKS - MANUAL"
                        font.bold: true; font.pixelSize: 20; font.letterSpacing: 3
                        color: "#000000"
                    }
                    Text {
                        id: helpTitle
                        text: "OLIGARCHY TANKS - MANUAL"
                        font.bold: true; font.pixelSize: 20; font.letterSpacing: 3
                        color: theme.accent
                    }
                }
                // ---- CONTROLS ----
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "CONTROLS"
                    font.bold: true; font.pixelSize: 20; font.letterSpacing: 3
                    color: theme.accent
                }
                Column {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 10
                    // Keyboard.
                    Text { text: "KEYBOARD"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 11; font.letterSpacing: 2; color: "#8a8f98" }
                    Row {
                        spacing: 10
                        Text { text: "P1"; font.bold: true; font.pixelSize: 13; color: win.p1body; anchors.verticalCenter: parent.verticalCenter }
                        KeyCap { unit: 22; label: "W" }
                        KeyCap { unit: 22; label: "A" }
                        KeyCap { unit: 22; label: "S" }
                        KeyCap { unit: 22; label: "D" }
                        KeyCap { unit: 22; label: "SPACE" }
                    }
                    Row {
                        spacing: 10
                        Text { text: "P2"; font.bold: true; font.pixelSize: 13; color: win.p2body; anchors.verticalCenter: parent.verticalCenter }
                        KeyCap { unit: 22; arrow: "up" }
                        KeyCap { unit: 22; arrow: "left" }
                        KeyCap { unit: 22; arrow: "down" }
                        KeyCap { unit: 22; arrow: "right" }
                        KeyCap { unit: 22; label: "ENTER" }
                    }
                    // Gamepad.
                    Text { text: "PAD"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 11; font.letterSpacing: 2; color: "#8a8f98" }
                    Row {
                        spacing: 8
                        Text { text: "MOVE"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 11; color: "#8a8f98"; anchors.verticalCenter: parent.verticalCenter }
                        PadIcon { kind: "stick"; unit: 22; face: "#f2f4f8" }
                        PadIcon { kind: "dpad"; unit: 22; face: "#f2f4f8" }
                        Item { width: 8; height: 1 }
                        Text { text: "FIRE"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 11; color: "#8a8f98"; anchors.verticalCenter: parent.verticalCenter }
                        PadIcon { kind: "trigger"; label: "L2"; unit: 22; face: "#f2f4f8" }
                        PadIcon { kind: "trigger"; label: "R2"; unit: 22; face: "#f2f4f8" }
                        PadIcon { kind: "face"; label: "X"; unit: 22; face: "#f2f4f8" }
                    }
                    Text { text: "START JOINS OR PAUSES - SHARE: MANUAL"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 12; color: "#8a8f98" }
                    Text { text: "1 PLAYER: KEYBOARD UNLESS A PAD PRESSES START"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 12; color: "#8a8f98" }
                    Text { text: "2 PLAYERS: KEYBOARD P1, FIRST PAD P2, 2ND PAD P1"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 12; color: "#8a8f98" }
                    Text { text: "GAME OVER: START, SPACE OR ENTER GOES BACK TO THE SELECT SCREEN"; font.family: "Departure Mono"; font.bold: true; font.pixelSize: 12; color: "#8a8f98" }
                }
                // ---- GAME ----
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "GAME"
                    font.bold: true; font.pixelSize: 20; font.letterSpacing: 3
                    color: theme.accent
                }
                Column {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 6
                    width: 400
                    Text { text: "DEFEND THE BASE. DESTROY ALL 16 ENEMIES PER SECTOR."; font.bold: true; font.pixelSize: 13; color: "#f2f4f8"; wrapMode: Text.WordWrap; width: parent.width }
                    Text { text: "ENEMIES: GRAY BASIC (1 HP), YELLOW FAST (1 HP, 1.35X SPEED),"; font.bold: true; font.pixelSize: 13; color: "#f2f4f8"; wrapMode: Text.WordWrap; width: parent.width }
                    Text { text: "ARMORED (3 HP, PURPLE>CYAN>GRAY), CARRIER (RED/GRAY, DROPS POWER-UPS)."; font.bold: true; font.pixelSize: 13; color: "#f2f4f8"; wrapMode: Text.WordWrap; width: parent.width }
                    Text { text: "SCORE: BASIC 100, FAST 200, ARMORED 300. LIVES: 3 (CAP 5)."; font.bold: true; font.pixelSize: 13; color: "#f2f4f8"; wrapMode: Text.WordWrap; width: parent.width }
                    Text { text: "SECTOR: MIX CHANGES (MORE FAST/ARMORED), ENEMIES GET FASTER."; font.bold: true; font.pixelSize: 13; color: "#f2f4f8"; wrapMode: Text.WordWrap; width: parent.width }
                    Text { text: "BRICKS: 4 STRIPS PER TILE. UNDER 2 GONE IT BLOCKS WHOLE;"; font.bold: true; font.pixelSize: 13; color: "#f2f4f8"; wrapMode: Text.WordWrap; width: parent.width }
                    Text { text: "FROM 2 GONE ONLY SURVIVING STRIPS BLOCK (50% LAW)."; font.bold: true; font.pixelSize: 13; color: "#f2f4f8"; wrapMode: Text.WordWrap; width: parent.width }
                    Text { text: "PICK REBUILDS THE BASE WALL AS STEEL (15 S). BASE WALL NEVER BREAKS."; font.bold: true; font.pixelSize: 13; color: "#f2f4f8"; wrapMode: Text.WordWrap; width: parent.width }
                    Text { text: "DEATH RESETS LEVELUP TIER. TIERS SURVIVE SECTOR CHANGE."; font.bold: true; font.pixelSize: 13; color: "#f2f4f8"; wrapMode: Text.WordWrap; width: parent.width }
                }
                // ---- POWER-UPS ----
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "POWER-UPS"
                    font.bold: true; font.pixelSize: 20; font.letterSpacing: 3
                    color: theme.accent
                }
                Column {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8
                    Repeater {
                        model: [
                            ["SPEED", "#edc400", "+35% MOVEMENT FOR 20 S"],
                            ["SHIELD", "#c9ced6", "BLOCKS SHOTS FOR 10 S"],
                            ["PICK", "#b9c0c9", "BASE WALL BECOMES CONCRETE FOR 15 S"],
                            ["LIFE", "#e8e8e8", "+1 LIFE (CAP 5), OR REVIVES A PARTNER WHO IS OUT"],
                            ["NUKE", "#ffd93b", "KILLS ALL ENEMIES"],
                            ["CANNON", "#c8963c", "ONE-SHOT ARMOUR (BIG SHELL) FOR 15 S"],
                            ["FREEZE", "#9fd8ff", "ENEMIES STOP FOR 8 S, BLINKING AS THEY THAW"],
                            ["LEVELUP", "#ffd700", "RAISES OFFENSIVE TIER (SPEED, DOUBLE SHOT, ARMOUR, AIR SUPPORT)"],
                            ["BLAST", "#ff6b6b", "BLAST MISSILE (5 SHOTS): SLOWER SHOT, AREA DAMAGE (2 HP), PASSES THROUGH BLOCKS"]
                        ]
                        Row {
                            spacing: 16
                            Column {
                                spacing: 2
                                width: 90
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
                            Text {
                                width: 260
                                text: modelData[2]
                                font.family: "Departure Mono"; font.bold: true; font.pixelSize: 13
                                color: "#f2f4f8"
                                wrapMode: Text.WordWrap
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
                }
                // Footer (outside the scroll area, always visible).
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 28
                    text: "BUILD " + buildId
                    font.family: "Departure Mono"
                    font.bold: true; font.pixelSize: 11; font.letterSpacing: 1
                    color: "#5a5e68"
                }
            }
            PixelButton {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 4
                accent: theme.accent; text: "<< CLOSE (?)"
                onClicked: { win.helpVisible = false; keys.forceActiveFocus() }
            }
        }
    }

    Component.onCompleted: keys.forceActiveFocus()
}

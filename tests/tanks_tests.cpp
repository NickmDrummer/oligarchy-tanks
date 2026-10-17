#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <vector>

#include "backend.h"
#include "padmap.h"
#include "theme.h"

// Deterministic access to Backend internals so bonus behaviour and block
// damage can be asserted without rigging the map at runtime.
struct TanksTestAccess {
    static constexpr int Empty = 0;
    static constexpr int Brick = 1;
    static constexpr int Steel = 2;
    static constexpr int Base = 3;
    static constexpr int BaseDead = 4;
    static constexpr int Forest = 5;
    static void putTile(Backend &b, int x, int y, int tile) {
        b.m_grid[y * Backend::COLS + x] = tile;
        b.m_brickQ[y * Backend::COLS + x] = (tile == TanksTestAccess::Brick) ? 0xFF : 0;
        b.bumpBoard();
    }
    static int tile(const Backend &b, int x, int y) {
        return b.m_grid[y * Backend::COLS + x];
    }
    // Surviving strips on one axis of a brick tile: 0 = vertical, 1 = horizontal.
    static int strips(const Backend &b, int x, int y, int axis) {
        const int m = b.m_brickQ[y * Backend::COLS + x] >> (axis * 4);
        int n = 0;
        for (int i = 0; i < 4; ++i)
            if (m & (1 << i))
                ++n;
        return n;
    }
    static void chip(Backend &b, int x, int y, int dir) { b.chipBrick(x, y, dir); }
    // Terrain test for a single point, the same test tanks and shots use.
    static bool solid(const Backend &b, double x, double y) { return b.solidAt(x, y, 0.01); }
    // Slots, so the PICK crush can be asserted: a tank parked in the wall.
    static void putPlayer(Backend &b, int i, double x, double y) {
        b.m_p[i].x = x;
        b.m_p[i].y = y;
        b.m_p[i].alive = true;
    }
    static void putEnemy(Backend &b, double x, double y) {
        Backend::Enemy e;
        e.x = x;
        e.y = y;
        e.alive = true;
        e.hp = 1;
        b.m_foes.push_back(e);
    }
    static bool playerAlive(const Backend &b, int i) { return b.m_p[i].alive; }
    static int enemiesAlive(const Backend &b) { return b.aliveEnemies(); }
    static void takePickup(Backend &b, int kind) {
        b.m_pickup.kind = kind;
        b.m_pickup.active = true;
        b.applyPickup(0);
    }
    static bool baseGuardActive(const Backend &b) { return b.m_baseGuardUntil > 0; }
    // Jump past the guard timer and run the expiry check.
    static void lapseBaseGuard(Backend &b) {
        b.m_time = b.m_baseGuardUntil + 1;
        b.updateBaseGuard();
    }
    // Which player a pad drives (-1 while it has not joined).
    static int padPlayer(const Backend &b, int pad) { return b.playerForPad(pad); }
    // Take a player out of the game (no lives left) and run the end check.
    static void outPlayer(Backend &b, int i) {
        b.m_p[i].alive = false;
        b.m_p[i].lives = 0;
    }
    static void endCheck(Backend &b) { b.checkEnd(); }
    static double playerX(const Backend &b, int i) { return b.m_p[i].x; }
    // A cleared square, so an input test measures input and not the random map.
    static void clearArea(Backend &b, int x0, int y0, int x1, int y1) {
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                putTile(b, x, y, Empty);
    }
};

// Records what the mapper made of a pad, so the mapping rules can be asserted
// with no Backend and no controller in the room.
struct PadCalls {
    struct Move {
        int pad, dx, dy;
        bool pressed;
    };
    std::vector<Move> moves;
    std::vector<int> fires, starts, selects;
    // Whether a direction is held right now, following the call sequence.
    bool moving(int pad, int dx, int dy) const {
        bool on = false;
        for (const Move &m : moves)
            if (m.pad == pad && m.dx == dx && m.dy == dy)
                on = m.pressed;
        return on;
    }
    struct Sink : PadSink {
        explicit Sink(PadCalls *c) : calls(c) {}
        void padMove(int pad, int dx, int dy, bool pressed) override { calls->moves.push_back({pad, dx, dy, pressed}); }
        void padFire(int pad) override { calls->fires.push_back(pad); }
        void padStart(int pad) override { calls->starts.push_back(pad); }
        void padSelect(int pad) override { calls->selects.push_back(pad); }
        PadCalls *calls;
    };
};

class TanksTests : public QObject {
    Q_OBJECT

private slots:
    // The game opens on the mode select, with a generated map already behind it.
    void startsOnTheSelectScreen() {
        Backend b;
        QCOMPARE(b.state(), QString("select"));
        QCOMPARE(b.cols(), 15);
        QCOMPARE(b.rows(), 15);
        QCOMPARE(b.board().size(), 15 * 15);
    }

    void startGameEntersPlaying() {
        Backend b;
        b.startGame();
        QCOMPARE(b.state(), QString("playing"));
        QCOMPARE(b.players().size(), 2);
        QVERIFY(b.enemiesLeft() > 0);
    }

    void movementFlagsDoNotCrash() {
        Backend b;
        b.startGame();
        b.setP1Move(0, -1, true);
        b.setP1Move(0, -1, false);
        b.setP2Move(1, 0, true);
        b.fireP1();
        b.fireP2();
        QVERIFY(b.bullets().size() >= 0);
    }

    void pauseToggles() {
        Backend b;
        b.startGame();
        b.togglePause();
        QCOMPARE(b.state(), QString("paused"));
        b.togglePause();
        QCOMPARE(b.state(), QString("playing"));
    }

    void nextLevelKeepsScoreAndScales() {
        Backend b;
        b.startGame();
        QCOMPARE(b.level(), 1);
        QCOMPARE(b.enemiesLeft(), 8);
        b.nextLevel();
        QCOMPARE(b.level(), 2);
        QCOMPARE(b.state(), QString("playing"));
        QCOMPARE(b.enemiesLeft(), 10);
    }

    // The int queries QML drives its tiles with.
    void tileQueriesReportTheBoard() {
        Backend b;
        b.startGame();
        TanksTestAccess::putTile(b, 4, 4, TanksTestAccess::Brick);
        const int rev = b.boardRev();
        QCOMPARE(b.tileKind(4, 4, rev), int(TanksTestAccess::Brick));
        QCOMPARE(b.tileKind(-1, 4, rev), int(TanksTestAccess::Empty));
        QVERIFY(!b.brickStripGone(4, 4, 0, rev));
        TanksTestAccess::chip(b, 4, 4, 1); // shot from the left
        QVERIFY(b.brickStripGone(4, 4, 0, b.boardRev()));
        QVERIFY(!b.brickStripGone(4, 4, 3, b.boardRev()));
        QVERIFY(b.boardRev() > rev);
    }

    // Each shot shears off the strip on the face it hits: a horizontal
    // shot takes a vertical quarter of the width, a vertical shot takes a
    // horizontal quarter of the height. The tile disappears only when a
    // whole axis is used up.
    void shotsChipSlices() {
        Backend b;
        b.startGame();
        TanksTestAccess::putTile(b, 5, 5, TanksTestAccess::Brick);
        QCOMPARE(TanksTestAccess::strips(b, 5, 5, 0), 4);
        QCOMPARE(TanksTestAccess::strips(b, 5, 5, 1), 4);
        TanksTestAccess::chip(b, 5, 5, 1); // shot from the left
        QCOMPARE(TanksTestAccess::strips(b, 5, 5, 0), 3);
        TanksTestAccess::chip(b, 5, 5, 3); // shot from the right
        QCOMPARE(TanksTestAccess::strips(b, 5, 5, 0), 2);
        TanksTestAccess::chip(b, 5, 5, 2); // shot from above
        QCOMPARE(TanksTestAccess::strips(b, 5, 5, 1), 3);
        TanksTestAccess::chip(b, 5, 5, 0); // shot from below
        QCOMPARE(TanksTestAccess::strips(b, 5, 5, 1), 2);
        QCOMPARE(TanksTestAccess::tile(b, 5, 5), int(TanksTestAccess::Brick));
    }

    // Under half destroyed the tile blocks as if it were whole: the first
    // shot changes how it looks, not how it stops a tank.
    void oneChipStillBlocksTheWholeTile() {
        Backend b;
        b.startGame();
        TanksTestAccess::putTile(b, 5, 5, TanksTestAccess::Brick);
        TanksTestAccess::chip(b, 5, 5, 1);
        QCOMPARE(TanksTestAccess::strips(b, 5, 5, 0), 3);
        QVERIFY(TanksTestAccess::solid(b, 5.1, 5.5));
        QVERIFY(TanksTestAccess::solid(b, 5.5, 5.5));
        QVERIFY(TanksTestAccess::solid(b, 5.9, 5.5));
    }

    // Half gone: the tile stops blocking as a whole block and only the
    // masonry that is left stops anything, so a tank rolls up to the 50%
    // line. The same happens whichever face the shots came from.
    void halfGoneOpensTheTileUpToTheFiftyPercentLine() {
        Backend b;
        b.startGame();
        TanksTestAccess::putTile(b, 5, 5, TanksTestAccess::Brick);
        TanksTestAccess::chip(b, 5, 5, 1);
        TanksTestAccess::chip(b, 5, 5, 1);
        QCOMPARE(TanksTestAccess::strips(b, 5, 5, 0), 2);
        QVERIFY(!TanksTestAccess::solid(b, 5.1, 5.5));
        QVERIFY(!TanksTestAccess::solid(b, 5.4, 5.5));
        QVERIFY(TanksTestAccess::solid(b, 5.6, 5.5));
        QVERIFY(TanksTestAccess::solid(b, 5.9, 5.5));

        // Same from above: the top half opens, the bottom half holds.
        TanksTestAccess::putTile(b, 7, 5, TanksTestAccess::Brick);
        TanksTestAccess::chip(b, 7, 5, 2);
        TanksTestAccess::chip(b, 7, 5, 2);
        QVERIFY(!TanksTestAccess::solid(b, 7.5, 5.1));
        QVERIFY(!TanksTestAccess::solid(b, 7.5, 5.4));
        QVERIFY(TanksTestAccess::solid(b, 7.5, 5.6));
    }

    // Four shots from the same face use the tile up; then it is gone.
    void fourShotsFromOneFaceClearTile() {
        Backend b;
        b.startGame();
        TanksTestAccess::putTile(b, 5, 5, TanksTestAccess::Brick);
        for (int i = 0; i < 3; ++i)
            TanksTestAccess::chip(b, 5, 5, 1);
        QCOMPARE(TanksTestAccess::tile(b, 5, 5), int(TanksTestAccess::Brick));
        QCOMPARE(TanksTestAccess::strips(b, 5, 5, 0), 1);
        TanksTestAccess::chip(b, 5, 5, 1);
        QCOMPARE(TanksTestAccess::tile(b, 5, 5), TanksTestAccess::Empty);
    }

    // A real shot stops at the brick and shears off exactly one strip,
    // perpendicular to the direction it came from.
    void shotChipsOneSlice() {
        Backend b;
        b.startGame();
        TanksTestAccess::putTile(b, 3, 12, TanksTestAccess::Brick);
        QCOMPARE(TanksTestAccess::strips(b, 3, 12, 1), 4);
        b.fireP1(); // P1 faces up, so the shot arrives from below
        QTest::qWait(500);
        QCOMPARE(TanksTestAccess::strips(b, 3, 12, 1), 3);
        QCOMPARE(TanksTestAccess::strips(b, 3, 12, 0), 4);
    }

    // The rule the user asked for: with half the tile gone a tank rolls past
    // the tile edge and stops with its hull on the 50% line, because the
    // masonry that is left still holds.
    void tankRollsIntoTheGapUpToTheFiftyPercentLine() {
        Backend b;
        b.startGame();
        TanksTestAccess::putTile(b, 3, 12, TanksTestAccess::Brick);
        TanksTestAccess::chip(b, 3, 12, 0); // shots from below eat H3...
        TanksTestAccess::chip(b, 3, 12, 0); // ...and H2: half the tile is gone
        QCOMPARE(TanksTestAccess::strips(b, 3, 12, 1), 2);
        // P1 starts one cell below the brick, facing up.
        TanksTestAccess::putPlayer(b, 0, 3.5, 13.5);
        b.setP1Move(0, -1, true);
        QTest::qWait(400);
        b.setP1Move(0, -1, false);
        const QVariantMap p = b.players().at(0).toMap();
        QVERIFY(p.value("alive").toBool());
        // The hull is 0.8 tall, so its top edge sits 0.4 above the centre:
        // the centre stops at 12.9, which is exactly the 50% line at 12.5.
        QVERIFY2(p.value("y").toDouble() > 12.85, qPrintable(QString("y=%1").arg(p.value("y").toDouble())));
        QVERIFY2(p.value("y").toDouble() < 12.95, qPrintable(QString("y=%1").arg(p.value("y").toDouble())));
    }

    // The lane only opens right through when the tile is used up.
    void laneOpensOnceTheTileIsGone() {
        Backend b;
        b.startGame();
        TanksTestAccess::putTile(b, 3, 12, TanksTestAccess::Brick);
        TanksTestAccess::putTile(b, 3, 11, TanksTestAccess::Brick);
        for (int i = 0; i < 4; ++i) {
            QTest::qWait(450);
            b.fireP1();
        }
        QTest::qWait(500);
        QCOMPARE(TanksTestAccess::tile(b, 3, 12), TanksTestAccess::Empty);
        // Now the fifth shot flies through the cleared cell to the next brick.
        QTest::qWait(450);
        b.fireP1();
        QTest::qWait(700);
        QCOMPARE(TanksTestAccess::strips(b, 3, 11, 1), 3);
    }

    // Steel shrugs shells off. PICK does not change that any more.
    void shotsNeverBreakSteel() {
        Backend b;
        b.startGame();
        TanksTestAccess::putTile(b, 3, 12, TanksTestAccess::Steel);
        TanksTestAccess::takePickup(b, 2); // Pick
        for (int i = 0; i < 3; ++i) {
            QTest::qWait(450);
            b.fireP1();
        }
        QTest::qWait(500);
        QCOMPARE(TanksTestAccess::tile(b, 3, 12), int(TanksTestAccess::Steel));
    }

    // PICK rebuilds the whole wall around the base, whatever state it was
    // left in, and lapses back to whole brick.
    void pickRebuildsTheWholeBaseWall() {
        Backend b;
        b.startGame();
        const int bx = Backend::COLS / 2, by = Backend::ROWS - 1;
        static const int offs[5][2] = {{-1, 0}, {1, 0}, {-1, -1}, {0, -1}, {1, -1}};
        TanksTestAccess::putTile(b, bx - 1, by - 1, TanksTestAccess::Empty);
        TanksTestAccess::putTile(b, bx + 1, by, TanksTestAccess::Empty);
        TanksTestAccess::takePickup(b, 2); // Pick
        QVERIFY(TanksTestAccess::baseGuardActive(b));
        for (const auto &o : offs)
            QCOMPARE(TanksTestAccess::tile(b, bx + o[0], by + o[1]), int(TanksTestAccess::Steel));
        QCOMPARE(TanksTestAccess::tile(b, bx, by), int(TanksTestAccess::Base));

        TanksTestAccess::lapseBaseGuard(b);
        QVERIFY(!TanksTestAccess::baseGuardActive(b));
        for (const auto &o : offs)
            QCOMPARE(TanksTestAccess::tile(b, bx + o[0], by + o[1]), int(TanksTestAccess::Brick));
        // Whole again, not the half-eaten brick that was there before.
        QCOMPARE(TanksTestAccess::strips(b, bx - 1, by - 1, 0), 4);
        QCOMPARE(TanksTestAccess::tile(b, bx, by), int(TanksTestAccess::Base));
    }

    // Nothing survives where the concrete lands: a tank caught in the gap
    // is crushed, and no score is handed out for it.
    void pickCrushesTanksInTheWall() {
        Backend b;
        b.startGame();
        const int bx = Backend::COLS / 2, by = Backend::ROWS - 1;
        TanksTestAccess::putTile(b, bx + 1, by, TanksTestAccess::Empty);
        TanksTestAccess::putPlayer(b, 0, bx + 1.5, by + 0.5); // in the gap
        TanksTestAccess::putPlayer(b, 1, bx - 3.5, by - 1.5); // well clear
        TanksTestAccess::putEnemy(b, bx - 0.5, by + 0.5);     // in the other gap
        TanksTestAccess::takePickup(b, 2);                    // Pick
        QVERIFY(!TanksTestAccess::playerAlive(b, 0));
        QVERIFY(TanksTestAccess::playerAlive(b, 1));
        QCOMPARE(TanksTestAccess::enemiesAlive(b), 0);
        QCOMPARE(TanksTestAccess::tile(b, bx + 1, by), int(TanksTestAccess::Steel));
        QCOMPARE(b.players().at(0).toMap().value("score").toInt(), 0);
    }

    // --- Gamepads -----------------------------------------------------------
    // The mapper is device-free, so the rules are asserted without a pad:
    // the stick has to leave the deadzone, the d-pad is digital, a diagonal
    // sets both axes, and fire/start/select fire once per press.

    void padMapperDeadzoneAndDirections() {
        PadCalls calls;
        PadCalls::Sink sink(&calls);
        PadMapper mapper;
        PadState s;
        s.index = 0;
        s.lx = 0.2; // inside the deadzone
        s.ly = 0.2;
        mapper.apply(s, sink);
        QCOMPARE(int(calls.moves.size()), 0);

        s.ly = -0.9;
        mapper.apply(s, sink);
        QCOMPARE(int(calls.moves.size()), 1);
        QVERIFY(calls.moving(0, 0, -1));

        // Diagonal: pushing the same stick right adds the second axis.
        s.lx = 0.9;
        mapper.apply(s, sink);
        QVERIFY(calls.moving(0, 1, 0));
        QVERIFY(calls.moving(0, 0, -1));

        s.lx = 0.0;
        s.ly = 0.0;
        mapper.apply(s, sink);
        QVERIFY(!calls.moving(0, 1, 0));
        QVERIFY(!calls.moving(0, 0, -1));
        const int settled = int(calls.moves.size());
        mapper.apply(s, sink);
        QCOMPARE(int(calls.moves.size()), settled); // nothing to report while centred

        // The d-pad is reported as held flags and works the same way.
        s.right = true;
        mapper.apply(s, sink);
        QVERIFY(calls.moving(0, 1, 0));
        s.right = false;
        mapper.apply(s, sink);
        QVERIFY(!calls.moving(0, 1, 0));
    }

    void padMapperEdgeTriggersFireAndStart() {
        PadCalls calls;
        PadCalls::Sink sink(&calls);
        PadMapper mapper;
        PadState s;
        s.index = 3;
        s.west = true; // X / Square
        mapper.apply(s, sink);
        QCOMPARE(int(calls.fires.size()), 1);
        mapper.apply(s, sink); // still held: no auto-repeat
        QCOMPARE(int(calls.fires.size()), 1);

        s.west = false;
        mapper.apply(s, sink);
        s.lt = 0.5; // either trigger past halfway fires
        mapper.apply(s, sink);
        QCOMPARE(int(calls.fires.size()), 2);
        s.lt = 0.0;
        mapper.apply(s, sink);
        s.rt = 0.9;
        mapper.apply(s, sink);
        QCOMPARE(int(calls.fires.size()), 3);
        // A trigger resting in its own noise band is not a press.
        s.rt = 0.1;
        mapper.apply(s, sink);
        QCOMPARE(int(calls.fires.size()), 3);

        s.start = true;
        s.select = true;
        mapper.apply(s, sink);
        QCOMPARE(int(calls.starts.size()), 1);
        QCOMPARE(int(calls.selects.size()), 1);
        mapper.apply(s, sink);
        QCOMPARE(int(calls.starts.size()), 1);
        QCOMPARE(int(calls.selects.size()), 1);
    }

    void padMapperReleaseLetsGoOfHeldDirections() {
        PadCalls calls;
        PadCalls::Sink sink(&calls);
        PadMapper mapper;
        PadState s;
        s.index = 1;
        s.left = true;
        s.lx = -1.0;
        mapper.apply(s, sink);
        QVERIFY(calls.moving(1, -1, 0));
        // Unplugging must not leave a direction stuck down.
        mapper.release(1, sink);
        QVERIFY(!calls.moving(1, -1, 0));
    }

    // The first pad to press START takes player 2 (player 1 is the keyboard's)
    // and joining from the menu starts the game, like a cabinet.
    // The first screen is the mode select, exactly like an arcade cabinet.
    void selectScreenPicksTheMode() {
        Backend b;
        QCOMPARE(b.state(), QString("select"));
        QCOMPARE(b.menuIndex(), 0);
        b.menuMove(1);
        QCOMPARE(b.menuIndex(), 1);
        b.menuMove(1); // the highlight wraps
        QCOMPARE(b.menuIndex(), 0);
        b.menuMove(-1);
        QCOMPARE(b.menuIndex(), 1);
        b.menuConfirm();
        QCOMPARE(b.state(), QString("playing"));
        QCOMPARE(b.playerCount(), 2);
    }

    // One player means one tank: no parked second tank to keep the run alive.
    void onePlayerModeLeavesOneTank() {
        Backend b;
        b.menuChoose(0);
        QCOMPARE(b.playerCount(), 1);
        QCOMPARE(b.state(), QString("playing"));
        const QVariantList ps = b.players();
        QCOMPARE(ps.at(0).toMap().value("active").toBool(), true);
        QCOMPARE(ps.at(1).toMap().value("active").toBool(), false);
        QCOMPARE(ps.at(1).toMap().value("alive").toBool(), false);
        QCOMPARE(ps.at(1).toMap().value("lives").toInt(), 0);
    }

    void twoPlayerModeKeepsBothTanks() {
        Backend b;
        b.menuChoose(1);
        QCOMPARE(b.playerCount(), 2);
        const QVariantList ps = b.players();
        QCOMPARE(ps.at(1).toMap().value("active").toBool(), true);
        QCOMPARE(ps.at(1).toMap().value("alive").toBool(), true);
    }

    // Losing the only player ends the run; with two, the other one carries on.
    void dyingAsASinglePlayerEndsTheRun() {
        Backend b;
        b.menuChoose(0);
        TanksTestAccess::outPlayer(b, 0);
        TanksTestAccess::endCheck(b);
        QCOMPARE(b.state(), QString("gameover"));
        QCOMPARE(b.ending(), QString("over"));
        // START, Space or Enter carries on to the select screen.
        b.fireP1();
        QCOMPARE(b.state(), QString("select"));

        Backend two;
        two.menuChoose(1);
        TanksTestAccess::outPlayer(two, 0);
        TanksTestAccess::endCheck(two);
        QCOMPARE(two.state(), QString("playing"));
        TanksTestAccess::outPlayer(two, 1);
        TanksTestAccess::endCheck(two);
        QCOMPARE(two.state(), QString("gameover"));
        QCOMPARE(two.ending(), QString("over"));
    }

    // The base going down ends the run too, and says so.
    void baseDestroyedEndsTheRun() {
        Backend b;
        b.menuChoose(1);
        TanksTestAccess::putTile(b, 7, 14, TanksTestAccess::BaseDead);
        TanksTestAccess::endCheck(b);
        QCOMPARE(b.state(), QString("gameover"));
        QCOMPARE(b.ending(), QString("base"));
        b.padConnected(0, "Pad");
        b.padStart(0); // any pad carries on, joined or not
        QCOMPARE(b.state(), QString("select"));
    }

    // The game over screen is the only way back, and it leaves an empty board.
    void gameOverLeadsToAnEmptySelectScreen() {
        Backend b;
        b.menuChoose(0);
        TanksTestAccess::outPlayer(b, 0);
        TanksTestAccess::endCheck(b);
        QCOMPARE(b.state(), QString("gameover"));
        b.toSelect();
        QCOMPARE(b.state(), QString("select"));
        QCOMPARE(b.message(), QString());
        QCOMPARE(b.ending(), QString());
        const QVariantList ps = b.players();
        QCOMPARE(ps.at(0).toMap().value("alive").toBool(), false);
        QCOMPARE(ps.at(1).toMap().value("alive").toBool(), false);
    }

    // R mid-game is a way out of the run, not a hidden restart.
    void leavingARunGoesBackToTheSelectScreen() {
        Backend b;
        b.menuChoose(1);
        QCOMPARE(b.state(), QString("playing"));
        b.toSelect();
        QCOMPARE(b.state(), QString("select"));
        QCOMPARE(b.playerCount(), 2); // the mode is remembered
    }

    // A pad that already played drives the next select screen too: this is the
    // bug where only the keyboard could pick a mode after a one-pad run.
    void anAssignedPadDrivesTheSelectScreen() {
        Backend b;
        b.menuChoose(0);
        b.padConnected(0, "Pad");
        b.padStart(0);
        QCOMPARE(TanksTestAccess::padPlayer(b, 0), 0);
        b.toSelect();
        QCOMPARE(b.menuIndex(), 0);
        b.padMove(0, 1, 0, true);
        b.padMove(0, 1, 0, false);
        QCOMPARE(b.menuIndex(), 1);
        b.padMove(0, 0, -1, true); // the d-pad works on the other axis too
        b.padMove(0, 0, -1, false);
        QCOMPARE(b.menuIndex(), 0);
    }

    // A new game hands the pads back to waiting: the pad that played the last
    // one-player run must not stay glued to player 1 in a two-player game.
    void confirmingAModeReleasesThePadsOfTheLastRun() {
        Backend b;
        b.menuChoose(0);
        b.padConnected(0, "Pad");
        b.padStart(0);
        QCOMPARE(TanksTestAccess::padPlayer(b, 0), 0);
        TanksTestAccess::outPlayer(b, 0);
        TanksTestAccess::endCheck(b);
        b.toSelect();
        b.menuMove(1); // highlight 2 PLAYERS
        b.menuConfirm();
        QCOMPARE(b.playerCount(), 2);
        QCOMPARE(TanksTestAccess::padPlayer(b, 0), -1); // free again
        b.padStart(0); // and it takes P2, the keyboard keeps P1
        QCOMPARE(TanksTestAccess::padPlayer(b, 0), 1);
        QCOMPARE(b.state(), QString("playing"));
    }

    // The same via START on the pad, which is the arcade path.
    void aPadConfirmingTheModeTakesItsTank() {
        Backend b;
        b.padConnected(0, "Pad");
        b.padMove(0, 1, 0, true); // the pad picks 2 PLAYERS
        b.padMove(0, 1, 0, false);
        b.padStart(0);
        QCOMPARE(b.playerCount(), 2);
        QCOMPARE(TanksTestAccess::padPlayer(b, 0), 1);
        QCOMPARE(b.state(), QString("playing"));
    }

    // An unassigned pad drives the select screen: left/right, then confirm.
    void unassignedPadPicksTheMode() {
        Backend b;
        b.padConnected(0, "Test Pad");
        QCOMPARE(TanksTestAccess::padPlayer(b, 0), -1);
        b.padMove(0, 1, 0, true); // no tank yet: this moves the highlight
        QCOMPARE(b.menuIndex(), 1);
        b.padMove(0, 1, 0, false);
        b.padStart(0);
        QCOMPARE(b.state(), QString("playing"));
        QCOMPARE(b.playerCount(), 2);
        QCOMPARE(TanksTestAccess::padPlayer(b, 0), 1);
    }

    // With a single tank, the first pad to press START takes that one.
    void padTakesTheOnlyTankInASinglePlayerGame() {
        Backend b;
        b.menuChoose(0); // the keyboard picks one player
        QCOMPARE(b.playerCount(), 1);
        b.padConnected(0, "Pad");
        b.padStart(0);
        QCOMPARE(TanksTestAccess::padPlayer(b, 0), 0);
        QCOMPARE(b.state(), QString("playing")); // joining must not pause
        b.padConnected(1, "Second");
        b.padStart(1);
        QCOMPARE(TanksTestAccess::padPlayer(b, 1), -1); // no tank left to claim
        QCOMPARE(b.state(), QString("playing"));
    }

    // Switching back to one player releases a pad that held the second tank.
    void singlePlayerModeReleasesTheSecondPad() {
        Backend b;
        b.menuChoose(1);
        b.padConnected(0, "Pad");
        b.padStart(0);
        QCOMPARE(TanksTestAccess::padPlayer(b, 0), 1);
        TanksTestAccess::outPlayer(b, 0);
        TanksTestAccess::outPlayer(b, 1);
        TanksTestAccess::endCheck(b); // game over, then on to the select screen
        QCOMPARE(b.state(), QString("gameover"));
        b.toSelect();
        QCOMPARE(b.state(), QString("select"));
        b.menuMove(-1); // highlight 1 PLAYER, left where it was on 2
        b.menuConfirm();
        QCOMPARE(b.playerCount(), 1);
        QCOMPARE(TanksTestAccess::padPlayer(b, 0), -1); // its tank is gone
        b.padStart(0); // and it can claim the only tank
        QCOMPARE(TanksTestAccess::padPlayer(b, 0), 0);
    }

    void secondPadJoinsPlayerOne() {
        Backend b;
        b.startGame();
        b.padConnected(0, "First");
        b.padConnected(1, "Second");
        b.padStart(0);
        QCOMPARE(TanksTestAccess::padPlayer(b, 0), 1);
        // A pad joining mid-game must not pause the game.
        QCOMPARE(b.state(), QString("playing"));
        b.padStart(1);
        QCOMPARE(TanksTestAccess::padPlayer(b, 1), 0);
        // A third pad has nowhere to go and leaves the game alone.
        b.padConnected(2, "Third");
        b.padStart(2);
        QCOMPARE(TanksTestAccess::padPlayer(b, 2), -1);
        QCOMPARE(b.state(), QString("playing"));
    }

    void padStartPausesOnceThePadHasAPlayer() {
        Backend b;
        b.startGame();
        b.padConnected(0, "Pad");
        b.padStart(0); // joins
        QCOMPARE(b.state(), QString("playing"));
        b.padStart(0); // now START means pause
        QCOMPARE(b.state(), QString("paused"));
        b.padStart(0);
        QCOMPARE(b.state(), QString("playing"));
    }

    void padMovesOnlyItsOwnPlayer() {
        Backend b;
        b.startGame();
        TanksTestAccess::clearArea(b, 5, 6, 10, 8);
        TanksTestAccess::putPlayer(b, 1, 6.5, 7.5);
        const double p1x = TanksTestAccess::playerX(b, 0);
        b.padConnected(0, "Pad");
        b.padStart(0); // player 2
        b.padMove(0, 1, 0, true);
        QTest::qWait(300);
        b.padMove(0, 1, 0, false);
        QVERIFY2(TanksTestAccess::playerX(b, 1) > 7.2,
                 qPrintable(QString("p2x=%1").arg(TanksTestAccess::playerX(b, 1))));
        QCOMPARE(TanksTestAccess::playerX(b, 0), p1x);
    }

    // Both sources drive the same tank; letting go of one must not cancel the
    // other (which is the whole reason the flags are kept per source).
    void keyboardAndPadDoNotCancelEachOther() {
        Backend b;
        b.startGame();
        TanksTestAccess::clearArea(b, 5, 6, 12, 8);
        TanksTestAccess::putPlayer(b, 1, 6.5, 7.5);
        b.padConnected(0, "Pad");
        b.padStart(0);
        b.setP2Move(1, 0, true); // keyboard holds right
        b.padMove(0, 1, 0, true); // pad holds right too
        QTest::qWait(200);
        const double both = TanksTestAccess::playerX(b, 1);
        b.setP2Move(1, 0, false); // the key is released, the stick is not
        QTest::qWait(200);
        const double padOnly = TanksTestAccess::playerX(b, 1);
        QVERIFY2(padOnly > both + 0.2, qPrintable(QString("both=%1 padOnly=%2").arg(both).arg(padOnly)));
        b.padMove(0, 1, 0, false); // now nobody holds it
        QTest::qWait(200);
        QCOMPARE(TanksTestAccess::playerX(b, 1), padOnly);
    }

    void padFireShootsForItsPlayer() {
        Backend b;
        b.startGame();
        b.padConnected(0, "Pad");
        b.padStart(0); // player 2
        b.padFire(0);
        const QVariantList shots = b.bullets();
        QCOMPARE(shots.size(), 1);
        QCOMPARE(shots.at(0).toMap().value("owner").toInt(), 1);
        // A pad with no player is inert.
        b.padConnected(1, "Idle");
        b.padFire(1);
        QCOMPARE(b.bullets().size(), 1);
    }

    void padDisconnectGivesTheSlotBack() {
        Backend b;
        b.startGame();
        b.padConnected(0, "Pad");
        b.padStart(0);
        QCOMPARE(TanksTestAccess::padPlayer(b, 0), 1);
        b.padDisconnected(0);
        QCOMPARE(b.pads().size(), 0);
        TanksTestAccess::clearArea(b, 5, 6, 10, 8);
        TanksTestAccess::putPlayer(b, 1, 6.5, 7.5);
        b.padMove(0, 1, 0, true); // ignored: that pad is gone
        QTest::qWait(200);
        QCOMPARE(TanksTestAccess::playerX(b, 1), 6.5);
    }

    void padDisconnectDoesNotLeaveADirectionHeld() {
        Backend b;
        b.startGame();
        b.padConnected(0, "Pad");
        b.padStart(0);
        b.padMove(0, 1, 0, true);
        b.padDisconnected(0);
        TanksTestAccess::clearArea(b, 5, 6, 10, 8);
        TanksTestAccess::putPlayer(b, 1, 6.5, 7.5);
        QTest::qWait(200);
        QCOMPARE(TanksTestAccess::playerX(b, 1), 6.5);
    }

    void themeFallsBack() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }
};

QTEST_MAIN(TanksTests)
#include "tanks_tests.moc"

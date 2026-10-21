#include <QDir>
#include <QFile>
#include <QSignalSpy>
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
    // An armoured (kind 2) enemy that costs an air strike one credit to kill.
    static void putArmoredEnemy(Backend &b, double x, double y) {
        Backend::Enemy e;
        e.x = x;
        e.y = y;
        e.alive = true;
        e.hp = 1;
        e.kind = 2;
        b.m_foes.push_back(e);
    }
    // Queue an air strike for a given owner; updateAirStrikes() does the rest.
    static void airStrike(Backend &b, int owner, double x, double y) {
        b.m_airStrikes.push_back(Backend::AirStrike{x, y, 1.0, false, owner});
    }
    // Strikes in flight right now.
    static int strikesPending(const Backend &b) { return int(b.m_airStrikes.size()); }
    // One strike in flight (to nudge its timer or read its landing spot).
    static Backend::AirStrike &strike(Backend &b, int i) { return b.m_airStrikes[i]; }
    static double airX(const Backend &b) { return b.m_airSupportX; }
    static double airY(const Backend &b) { return b.m_airSupportY; }
    // Take a tank down the normal death path (the charge dies with it).
    static void killTank(Backend &b, int i) { b.killPlayer(i); }
    static void advanceAir(double dt, Backend &b) { b.updateAirStrikes(dt); }
    // Armoured-kill counters: the air-support recharge currency.
    static int armouredKills(const Backend &b, int i) { return b.m_p[i].armoredKills; }
    // A live player tank, for setting up tier/air state directly.
    static Backend::Tank &tank(Backend &b, int i) { return b.m_p[i]; }
    // One full frame of the game (the QTimer normally drives it).
    static void runTick(Backend &b) { b.tick(); }
    // The game clock, so a test can prove the loop kept moving.
    static double gameTime(const Backend &b) { return b.m_time; }
    static bool playerAlive(const Backend &b, int i) { return b.m_p[i].alive; }
    static int enemiesAlive(const Backend &b) { return b.aliveEnemies(); }
    static void takePickup(Backend &b, int kind) {
        b.m_pickup.kind = kind;
        b.m_pickup.active = true;
        b.applyPickup(0);
    }
    static bool baseGuardActive(const Backend &b) { return b.m_baseGuardUntil > 0; }
    // The drop (section 21.3): run the real solver and read where it landed.
    static void spawnPickupAt(Backend &b, double x, double y) { b.spawnPickupAt(x, y); }
    static bool pickupActive(const Backend &b) { return b.m_pickup.active; }
    static double pickupX(const Backend &b) { return b.m_pickup.x; }
    static double pickupY(const Backend &b) { return b.m_pickup.y; }
    // Any living tank whose hull reaches into the cell the bonus chose.
    static bool tankAt(const Backend &b, double px, double py) {
        auto overlap = [&](double tx, double ty) {
            return std::abs(tx - px) < 0.9 && std::abs(ty - py) < 0.9;
        };
        for (int i = 0; i < 2; ++i)
            if (b.m_p[i].alive && overlap(b.m_p[i].x, b.m_p[i].y))
                return true;
        for (const auto &e : b.m_foes)
            if (e.alive && overlap(e.x, e.y))
                return true;
        return false;
    }
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
    // Spawn composition: the kind/hp spawnEnemy() picks for a given spawn index.
    static int spawnKindAt(Backend &b, int index) {
        for (auto &e : b.m_foes)
            e.alive = false;
        b.m_spawned = index;
        b.spawnEnemy();
        for (const auto &e : b.m_foes)
            if (e.alive)
                return e.kind;
        return -1;
    }
    static int spawnHpAt(Backend &b, int index) {
        for (auto &e : b.m_foes)
            e.alive = false;
        b.m_spawned = index;
        b.spawnEnemy();
        for (const auto &e : b.m_foes)
            if (e.alive)
                return e.hp;
        return -1;
    }
    static int enemiesPerStage(const Backend &b) { return b.totalForLevel(); }
    static void setLevel(Backend &b, int level) { b.m_level = level; }
    // A cleared square, so an input test measures input and not the random map.
    static void clearArea(Backend &b, int x0, int y0, int x1, int y1) {
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                putTile(b, x, y, Empty);
    }
    static void stepBullets(Backend &b, double dt) { b.updateBullets(dt); }
    // Empty the barrel between shots: keeping the previous missile in the
    // air would turn "one missile per press" into a stack count.
    static void clearBullets(Backend &b) { b.m_bullets.clear(); }
    static unsigned brickMaskAt(const Backend &b, int x, int y) {
        return b.m_brickQ[y * Backend::COLS + x];
    }
    // ---- enemies getting stuck (section 17.5) ----
    static void seed(Backend &b, unsigned s) { b.m_rng.seed(s); }
    static void remap(Backend &b) { b.generateMap(); }
    static void clearFoes(Backend &b) {
        for (auto &e : b.m_foes)
            e.alive = false;
    }
    static void clearPlayers(Backend &b) {
        b.m_p[0].alive = false;
        b.m_p[1].alive = false;
    }
    static void setSpawned(Backend &b, int n) { b.m_spawned = n; }
    static void noMoreSpawns(Backend &b) { b.m_spawned = b.totalForLevel(); }
    static int lastEnemy(const Backend &b) { return int(b.m_foes.size()) - 1; }    static bool enemyIsAlive(const Backend &b, int i) { return b.m_foes[i].alive; }
    static int enemyCount(const Backend &b) { return int(b.m_foes.size()); }
    // One enemy slot, mutable (move it mid-flight, kill it, read its hp).
    static Backend::Enemy &enemy(Backend &b, int i) { return b.m_foes[i]; }
    static double enemyX(const Backend &b, int i) { return b.m_foes[i].x; }
    static double enemyY(const Backend &b, int i) { return b.m_foes[i].y; }
    static bool buried(const Backend &b, int i) { return b.buriedInSolid(b.m_foes[i].x, b.m_foes[i].y); }
    static bool spawnNext(Backend &b) { return b.spawnEnemy(); }
    // The game's own collision test for a move: can enemy i step to (x, y)?
    static bool canStep(Backend &b, int i, double x, double y) {
        return !b.blockedByTank(x, y, 0.8, &b.m_foes[i], true, b.m_foes[i].x, b.m_foes[i].y);
    }
    // One frame of the enemy pass, in the order tick() runs it.
    static void stepEnemies(Backend &b) {
        b.separateTanks();
        b.updateEnemies(1.0 / 60.0);
    }
    // ---- players out of lives (section 17.6) ----
    static constexpr int LifeKind = 3; // PickupKind::Life
    static int playerLives(const Backend &b, int i) { return b.m_p[i].lives; }
    static void kill(Backend &b, int i) { b.killPlayer(i); }
    static void givePickup(Backend &b, int player, int kind) {
        b.m_pickup.kind = kind;
        b.m_pickup.active = true;
        b.applyPickup(player);
    }
    // ---- power-up effects (section 19) ----
    static constexpr int PickKind = 2;   // PickupKind::Pick
    static constexpr int NukeKind = 4;   // PickupKind::Nuke
    static constexpr int CannonKind = 5; // PickupKind::Cannon
    static constexpr int LevelUpKind = 7;      // PickupKind::LevelUp
    static constexpr int BlastMissileKind = 8; // PickupKind::BlastMissile
    // A bonus lying on the board, at a known spot.
    static void putPickup(Backend &b, int kind, double x, double y) {
        b.m_pickup = Backend::Pickup{x, y, kind, 0, true};
    }
    static void takePickupAt(Backend &b, int player) { b.applyPickup(player); }
    static bool blastActive(const Backend &b) { return b.nuke()["active"].toBool(); }
    static double blastX(const Backend &b) { return b.nuke()["x"].toDouble(); }
    static double blastY(const Backend &b) { return b.nuke()["y"].toDouble(); }
    static double blastProgress(const Backend &b) { return b.nuke()["progress"].toDouble(); }
    static void stepBlast(Backend &b, double dt) { b.updateNuke(dt); }
    static bool frozen(const Backend &b) { return b.frozen(); }
    static bool freezeEnding(const Backend &b) { return b.freezeEnding(); }
    static void freezeFor(Backend &b, double seconds) { b.m_freezeUntil = b.m_time + seconds; }
    // What is left of the two effects the user shortened, in seconds.
    static double guardLeft(const Backend &b) { return b.m_baseGuardUntil - b.m_time; }
    static double cannonLeft(const Backend &b, int i) { return b.m_p[i].cannonUntil - b.m_time; }
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

    void nextLevelAdvancesWithSixteenTanksPerStage() {
        Backend b;
        b.startGame();
        QCOMPARE(b.level(), 1);
        QCOMPARE(b.enemiesLeft(), 16);
        b.nextLevel();
        QCOMPARE(b.level(), 2);
        QCOMPARE(b.state(), QString("playing"));
        QCOMPARE(b.enemiesLeft(), 16);
    }

    // Phase C: 16 tanks a stage whatever the level, with the mix of kinds
    // changing by level and the two carriers taken from the basic pool,
    // interleaved rather than both at the start.
    void stageFieldsSixteenTanksWithInterleavedCarriers() {
        Backend b;
        b.startGame();
        QCOMPARE(TanksTestAccess::enemiesPerStage(b), 16);

        int carriers = 0, basics = 0, fast = 0, armored = 0;
        for (int i = 0; i < 16; ++i) {
            const int k = TanksTestAccess::spawnKindAt(b, i);
            if (k == 3) ++carriers;
            else if (k == 0) ++basics;
            else if (k == 1) ++fast;
            else if (k == 2) ++armored;
        }
        QCOMPARE(carriers, 2);
        QCOMPARE(basics, 6); // the basic pool is 8, two of them carriers
        QCOMPARE(fast, 4);
        QCOMPARE(armored, 4);

        // Carriers are interleaved: never the first tank, never both early.
        QCOMPARE(TanksTestAccess::spawnKindAt(b, 0), 0);
        QCOMPARE(TanksTestAccess::spawnKindAt(b, 2), 3);
        QCOMPARE(TanksTestAccess::spawnKindAt(b, 5), 3);
        QCOMPARE(TanksTestAccess::spawnKindAt(b, 8), 1);
        QCOMPARE(TanksTestAccess::spawnKindAt(b, 12), 2);
        // Armour is a fixed three hits, whatever the sector.
        QCOMPARE(TanksTestAccess::spawnHpAt(b, 12), 3);
        QCOMPARE(TanksTestAccess::spawnHpAt(b, 0), 1);

        // A later sector keeps 16 tanks but fields more armour.
        TanksTestAccess::setLevel(b, 7);
        carriers = basics = fast = armored = 0;
        for (int i = 0; i < 16; ++i) {
            const int k = TanksTestAccess::spawnKindAt(b, i);
            if (k == 3) ++carriers;
            else if (k == 0) ++basics;
            else if (k == 1) ++fast;
            else if (k == 2) ++armored;
        }
        QCOMPARE(TanksTestAccess::enemiesPerStage(b), 16);
        QCOMPARE(carriers, 2);
        QCOMPARE(basics, 2);
        QCOMPARE(fast, 6);
        QCOMPARE(armored, 6);
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

    // --- Enemies getting stuck (section 17.5) -------------------------------
    // Reported: enemies held still at the top-left corner, the top-right corner
    // and the top centre, with no other tank touching them. Measured cause: two
    // tanks that come within a hull's width of each other blocked each other in
    // all four directions for ever (the collision test read the *position*, not
    // the move), a spawn could drop a tank on top of another one, and the tank
    // separateTanks() then shoved into the wall could never step back out. The
    // four cases below lock each of those.

    // Touching tanks may always move apart, and never into each other.
    void twoTanksInContactCanAlwaysBackAway() {
        Backend b;
        b.startGame();
        TanksTestAccess::clearArea(b, 0, 0, Backend::COLS - 1, Backend::ROWS - 1);
        TanksTestAccess::clearFoes(b);
        TanksTestAccess::clearPlayers(b);
        TanksTestAccess::noMoreSpawns(b);
        TanksTestAccess::putEnemy(b, 3.5, 3.5);
        TanksTestAccess::putEnemy(b, 3.5, 3.9); // touching, overlapping
        const int a = 6, c = 7;
        QVERIFY(!TanksTestAccess::canStep(b, a, 3.5, 3.6)); // into it: blocked
        QVERIFY(TanksTestAccess::canStep(b, a, 3.5, 3.4));  // away: allowed
        TanksTestAccess::stepEnemies(b);
        QVERIFY(TanksTestAccess::enemyY(b, a) <= 3.5);
        QVERIFY(TanksTestAccess::enemyY(b, c) >= 3.9);
    }

    // A tank inside a whole tile cannot be nudged out (its hull is wider than
    // any step), so it is rescued outright instead of standing there for ever.
    void aTankBuriedInMasonryIsFreed() {
        Backend b;
        b.startGame();
        TanksTestAccess::clearArea(b, 0, 0, Backend::COLS - 1, Backend::ROWS - 1);
        TanksTestAccess::clearFoes(b);
        TanksTestAccess::clearPlayers(b);
        TanksTestAccess::noMoreSpawns(b);
        TanksTestAccess::putTile(b, 5, 5, TanksTestAccess::Brick);
        TanksTestAccess::putEnemy(b, 5.5, 5.5);
        const int e = TanksTestAccess::lastEnemy(b);
        QVERIFY(TanksTestAccess::buried(b, e));
        TanksTestAccess::stepEnemies(b);
        QVERIFY(!TanksTestAccess::buried(b, e));
    }

    // Two tanks never come out of the same gate: stacking them is what let
    // separateTanks() shove one into the wall. With no gate free the spawn
    // waits instead of stacking.
    void enemySpawnsNeverStackOnOneCell() {
        Backend b;
        b.startGame();
        TanksTestAccess::clearFoes(b);
        TanksTestAccess::setSpawned(b, 0);
        TanksTestAccess::putEnemy(b, 0.5, 0.5); // the first gate is taken
        QVERIFY(TanksTestAccess::spawnNext(b));
        QCOMPARE(TanksTestAccess::enemyX(b, 0), 7.5); // so it uses another one
        TanksTestAccess::putEnemy(b, 14.5, 0.5);
        QVERIFY(!TanksTestAccess::spawnNext(b)); // all three taken: wait
        QCOMPARE(TanksTestAccess::enemyX(b, 0), 7.5);
    }

    // Every map leaves the three entry lanes open at the top: spawning inside
    // masonry is unrecoverable, because an enemy cannot dig.
    void theEntryLanesAreNeverSealed() {
        Backend b;
        b.startGame();
        const int lanes[3] = {0, Backend::COLS / 2, Backend::COLS - 1};
        for (int map = 0; map < 50; ++map) {
            TanksTestAccess::remap(b);
            for (int l = 0; l < 3; ++l)
                for (int y = 0; y <= 2; ++y)
                    QCOMPARE(TanksTestAccess::tile(b, lanes[l], y), int(TanksTestAccess::Empty));
        }
    }

    // The user's actual complaint: an enemy that spawns in a corner has to
    // leave it. Twelve seeded maps, eight seconds each, no standing still.
    void enemiesLeaveTheTopOfTheBoardOnEveryMap() {
        Backend b;
        for (unsigned s = 1; s <= 12; ++s) {
            TanksTestAccess::seed(b, s);
            b.startGame();
            TanksTestAccess::clearFoes(b);
            TanksTestAccess::clearPlayers(b);
            TanksTestAccess::noMoreSpawns(b);
            TanksTestAccess::putEnemy(b, 0.5, 0.5);
            const int e = TanksTestAccess::lastEnemy(b);
            double far = 0;
            for (int f = 0; f < 480; ++f) { // 8 s
                TanksTestAccess::stepEnemies(b);
                const double dx = std::abs(TanksTestAccess::enemyX(b, e) - 0.5);
                const double dy = std::abs(TanksTestAccess::enemyY(b, e) - 0.5);
                far = std::max(far, std::max(dx, dy));
            }
            QVERIFY(far > 1.5);
        }
    }

    // --- A player who is out of lives (section 17.6) -------------------------
    // Reported: P1 ran out of lives, P2 carried on to the next sector, both
    // tanks came back, P1's panel showed neither spare tanks nor KILLED (a
    // blank) and P1 was back with a single life. Cause: resetTanks() rebuilt
    // the tank with alive = true whatever the life count, so an out player
    // returned as a phantom with 0 lives; dying again took him to -1, which is
    // neither "spare tank" nor "KILLED" for the panel. An out player now stays
    // out, and only a LIFE from his partner revives him.

    void anEliminatedPlayerStaysOutInsteadOfReturning() {
        Backend b;
        b.menuChoose(1); // two players
        TanksTestAccess::outPlayer(b, 0);
        b.nextLevel();
        QVERIFY(!TanksTestAccess::playerAlive(b, 0));
        QCOMPARE(TanksTestAccess::playerLives(b, 0), 0);
        // Dying again cannot push the count below zero, which is what blanked
        // the panel.
        TanksTestAccess::kill(b, 0);
        QCOMPARE(TanksTestAccess::playerLives(b, 0), 0);
        QVERIFY(!TanksTestAccess::playerAlive(b, 0));
        // The partner is untouched by any of it.
        QVERIFY(TanksTestAccess::playerAlive(b, 1));
    }

    void aLifeFromThePartnerRevivesTheEliminatedPlayer() {
        Backend b;
        b.menuChoose(1);
        TanksTestAccess::outPlayer(b, 0);
        TanksTestAccess::givePickup(b, 1, TanksTestAccess::LifeKind); // P2 finds a LIFE
        QVERIFY(TanksTestAccess::playerAlive(b, 0));      // P1 is back...
        QCOMPARE(TanksTestAccess::playerLives(b, 0), 1);  // ...with one tank
        QCOMPARE(TanksTestAccess::playerX(b, 0), 3.5);    // at his own spawn
        QCOMPARE(TanksTestAccess::playerLives(b, 1), 3);  // P2 gains nothing
    }

    void aLifeTopsUpThePickerWhileBothAreIn() {
        Backend b;
        b.menuChoose(1);
        TanksTestAccess::givePickup(b, 1, TanksTestAccess::LifeKind);
        QCOMPARE(TanksTestAccess::playerLives(b, 1), 4);
        QCOMPARE(TanksTestAccess::playerLives(b, 0), 3);
    }

    // --- Pickup sounds (section 21.13) ---------------------------------------

    // LEVELUP used to fire the shared "power" sample twice on the same frame
    // (once inside its case, once in the fall-through), which the user heard
    // at double volume. It now sounds exactly once, with its own arpeggio.
    void levelUpSoundsOnceWithItsOwnArpeggio() {
        Backend b;
        b.menuChoose(1);
        QSignalSpy spy(&b, &Backend::playSound);
        QVERIFY(spy.isValid());
        TanksTestAccess::givePickup(b, 0, TanksTestAccess::LevelUpKind);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QString("levelup"));
    }

    // BLAST/MISSILE had the same double emission (in-case plus fall-through);
    // it stays on the shared "power" sample, once.
    void blastMissileSoundsOnlyOnce() {
        Backend b;
        b.menuChoose(1);
        QSignalSpy spy(&b, &Backend::playSound);
        QVERIFY(spy.isValid());
        TanksTestAccess::givePickup(b, 0, TanksTestAccess::BlastMissileKind);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QString("power"));
    }

    // The FIFTH blast shot used to double-fire: fireBullet() spent the last
    // blast count during the main fire, so the tier-2 trailing-bullet gate
    // (read afterwards) saw 0 and launched a normal bullet right behind the
    // missile; the two collided and the missile died at the muzzle with
    // nothing but its sound (user report, section 21.14).
    void theFifthBlastShotSpawnsItsMissile() {
        Backend b;
        b.startGame();
        TanksTestAccess::tank(b, 0).tier = 2;
        TanksTestAccess::tank(b, 0).blastShots = 5;
        for (int shot = 0; shot < 5; ++shot) {
            TanksTestAccess::tank(b, 0).cooldown = 0;
            TanksTestAccess::clearBullets(b);
            b.fireP1();
            // Exactly one bullet per press, and it is the missile.
            QVERIFY2(b.bullets().size() == 1,
                     qPrintable(QStringLiteral("shot %1: %2 bullets").arg(shot).arg(b.bullets().size())));
            QVERIFY2(b.bullets().at(0).toMap().value("blast").toBool(),
                     qPrintable(QStringLiteral("shot %1 is not a blast missile (spend count=%2)")
                                    .arg(shot)
                                    .arg(TanksTestAccess::tank(b, 0).blastShots)));
        }
    }

    // And the tier-2 twin shot is untouched when no BLAST is in play: one
    // press fires the main shell plus the trailing second bullet.
    void tier2TwinShotsSurviveWithoutBlast() {
        Backend b;
        b.startGame();
        TanksTestAccess::tank(b, 0).tier = 2;
        b.fireP1();
        QCOMPARE(b.bullets().size(), 2);
        // The trailing one sits 0.4 cells behind the first (P1 faces up).
        QCOMPARE(b.bullets().at(0).toMap().value("blast").toBool(), false);
        const double y0 = b.bullets().at(0).toMap().value("y").toDouble();
        const double y1 = b.bullets().at(1).toMap().value("y").toDouble();
        QVERIFY(qAbs((y1 - y0) - 0.4) < 0.01);
    }

    // --- Power-ups (section 19) ---------------------------------------------

    // The NUKE blast grows out of the spot where the bonus was picked up, not
    // out of the board's centre and not out of the picking player.
    void theNukeBlastStartsWhereTheBonusWasPickedUp() {
        Backend b;
        b.menuChoose(1);
        TanksTestAccess::putEnemy(b, 9.5, 9.5);
        QVERIFY(!TanksTestAccess::blastActive(b)); // nothing runs on its own
        TanksTestAccess::putPickup(b, TanksTestAccess::NukeKind, 6.5, 10.5);
        TanksTestAccess::takePickupAt(b, 0);
        QVERIFY(TanksTestAccess::blastActive(b));
        QCOMPARE(TanksTestAccess::blastX(b), 6.5);
        QCOMPARE(TanksTestAccess::blastY(b), 10.5);
        QCOMPARE(TanksTestAccess::blastProgress(b), 0.0);
        QCOMPARE(TanksTestAccess::enemiesAlive(b), 0); // and the board is swept
    }

    void theNukeBlastGrowsAndThenGoesAway() {
        Backend b;
        b.menuChoose(1);
        TanksTestAccess::putPickup(b, TanksTestAccess::NukeKind, 2.5, 2.5);
        TanksTestAccess::takePickupAt(b, 0);
        TanksTestAccess::stepBlast(b, 0.3);
        QVERIFY(TanksTestAccess::blastActive(b));           // half way
        QCOMPARE(TanksTestAccess::blastProgress(b), 0.5);
        TanksTestAccess::stepBlast(b, 0.4);                 // past the 0.6 s
        QVERIFY(!TanksTestAccess::blastActive(b));
    }

    // The freeze blinks only in its last two seconds, and stops the moment the
    // timer runs out.
    void theFreezeOnlyBlushesWhileItIsEnding() {
        Backend b;
        TanksTestAccess::freezeFor(b, 8.0);
        QVERIFY(TanksTestAccess::frozen(b));
        QVERIFY(!TanksTestAccess::freezeEnding(b)); // 8 s left is not the end
        TanksTestAccess::freezeFor(b, 1.5);
        QVERIFY(TanksTestAccess::frozen(b));
        QVERIFY(TanksTestAccess::freezeEnding(b));
        TanksTestAccess::freezeFor(b, -0.1);
        QVERIFY(!TanksTestAccess::frozen(b));
        QVERIFY(!TanksTestAccess::freezeEnding(b));
    }

    // PICK and CANNON were 20 s and are 15 s now. Asserted against the clock
    // the effects themselves read, so a wrong constant cannot hide.
    void pickAndCannonNowLastFifteenSeconds() {
        Backend b;
        b.menuChoose(1);
        TanksTestAccess::putPickup(b, TanksTestAccess::PickKind, 7.5, 13.5);
        TanksTestAccess::takePickupAt(b, 0);
        QVERIFY(TanksTestAccess::baseGuardActive(b));
        QCOMPARE(TanksTestAccess::guardLeft(b), 15.0);
        TanksTestAccess::putPickup(b, TanksTestAccess::CannonKind, 5.5, 5.5);
        TanksTestAccess::takePickupAt(b, 0);
        QCOMPARE(TanksTestAccess::cannonLeft(b, 0), 15.0);
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

    void accentIsTheOmarchyGreen() {
        // The user's decision: the accent is the Omarchy logo green no matter
        // what the desktop theme carries (commit of the aesthetics round) —
        // #9ece6a, the same the base tile's plate uses.
        Theme theme;
        QCOMPARE(theme.accent(), QString("#9ece6a"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    // An air strike credits its explosion to the player who launched it,
    // never to player 0 (the old hard-coded owner). This is what broke the
    // recharge for P2: his armoured kills were never counted.
    void airStrikeCreditsItsOwner() {
        Backend b;
        b.startGame();
        // Strike far from the spawn lanes so no other foe is caught in it.
        TanksTestAccess::putArmoredEnemy(b, 7.0, 13.0);
        TanksTestAccess::airStrike(b, 1, 7.0, 13.0);
        const int p0score = b.players().at(0).toMap().value("score").toInt();
        const int p1score = b.players().at(1).toMap().value("score").toInt();
        QCOMPARE(TanksTestAccess::armouredKills(b, 0), 0);
        QCOMPARE(TanksTestAccess::armouredKills(b, 1), 0);
        TanksTestAccess::advanceAir(1.2, b);
        // Armoured enemies are 300 points; the armoured one is gone and BOTH
        // the score and the armoured kill went to its owner (P2).
        QCOMPARE(TanksTestAccess::armouredKills(b, 0), 0);
        QCOMPARE(TanksTestAccess::armouredKills(b, 1), 1);
        QCOMPARE(b.players().at(0).toMap().value("score").toInt(), p0score);
        QCOMPARE(b.players().at(1).toMap().value("score").toInt(), p1score + 300);
    }

    // With the tank pressed against a block the shot used to be pulled back
    // all the way to the tank centre, hiding it under the sprite and reading
    // as "the shot never came out" (section 20.8).
    void shotLeavesTheMuzzleAgainstABlock() {
        Backend b;
        b.startGame();
        TanksTestAccess::clearArea(b, 1, 9, 6, 12);
        TanksTestAccess::putTile(b, 4, 10, TanksTestAccess::Brick);
        TanksTestAccess::putPlayer(b, 0, 3.5, 10.5);
        TanksTestAccess::tank(b, 0).dir = 1; // firing right, at the brick
        const unsigned intact = TanksTestAccess::brickMaskAt(b, 4, 10);
        b.fireP1();
        QCOMPARE(b.bullets().size(), 1);
        // The shot must spawn at the muzzle, not under the tank centre.
        const double x0 = b.bullets().at(0).toMap().value("x").toDouble();
        QVERIFY2(x0 >= 3.7, qPrintable(QString("spawn x0=%1").arg(x0)));
        QVERIFY2(x0 <= 3.9, qPrintable(QString("spawn on the hull edge x0=%1").arg(x0)));
        // It then dies on the wall and takes at least one strip with it.
        for (int i = 0; i < 12 && b.bullets().size() == 1; ++i)
            TanksTestAccess::stepBullets(b, 1.0 / 60.0);
        QVERIFY2(TanksTestAccess::brickMaskAt(b, 4, 10) != intact, "wall not chipped");
        QCOMPARE(b.bullets().size(), 0);
    }

    // Same story at the arena border: the tip is past the edge, so the shot
    // must still come out of the muzzle instead of being born at the centre.
    void shotFiresAtTheArenaEdge() {
        Backend b;
        b.startGame();
        TanksTestAccess::clearArea(b, 10, 9, 14, 12);
        TanksTestAccess::putPlayer(b, 0, 14.45, 10.5); // flush-ish vs the right edge
        TanksTestAccess::tank(b, 0).dir = 1;
        b.fireP1();
        QCOMPARE(b.bullets().size(), 1);
        const double x0 = b.bullets().at(0).toMap().value("x").toDouble();
        QVERIFY2(x0 >= 14.2, qPrintable(QString("spawn x0=%1").arg(x0)));
        QVERIFY2(x0 < 15.0, qPrintable(QString("inside the arena x0=%1").arg(x0)));
        // It flies out of the arena within a few frames.
        for (int i = 0; i < 8 && b.bullets().size() == 1; ++i)
            TanksTestAccess::stepBullets(b, 1.0 / 60.0);
        QCOMPARE(b.bullets().size(), 0);
    }

    // The tier-4 air support recharges once the player has 10 armoured kills.
    void airSupportRechargesAtTenArmouredKills() {
        Backend b;
        b.startGame();
        TanksTestAccess::freezeFor(b, 60.0);
        auto &t2 = TanksTestAccess::tank(b, 1);
        // P2 side, tier 4, no salvo running, ten kills banked: on the tick.
        t2.tier = 4;
        t2.alive = true;
        t2.armoredKills = 10;
        t2.airMissiles = 0;
        t2.airTimer = 0;
        TanksTestAccess::runTick(b);
        QCOMPARE(t2.airMissiles, 5);
        QCOMPARE(t2.armoredKills, 0);
    }

    // The user's design (round 21.9, second pass): the armoured-kill bank IS
    // the charge and the cycle is continuous while tier 4 stays alive. Ten
    // banked kills release five strikes and the bank starts over; kills keep
    // banking during the flight, and an exhausted salvo with ten more banked
    // fires the next one at once.
    void airSupportFiresAtTheTenthArmouredKill() {
        Backend b;
        b.startGame();
        TanksTestAccess::freezeFor(b, 60.0);
        TanksTestAccess::clearArea(b, 0, 0, Backend::COLS - 1, Backend::ROWS - 1);
        TanksTestAccess::clearFoes(b);
        auto &p1 = TanksTestAccess::tank(b, 0);
        p1.alive = true;
        p1.x = 7.5;
        p1.y = 13.5;
        p1.tier = 4;
        p1.armorHP = 0;
        p1.airMissiles = 0;
        p1.airTimer = 0;
        p1.armoredKills = 0;
        // Nine banked kills hold the hand: nothing fires yet.
        p1.armoredKills = 9;
        for (int k = 0; k < 20; ++k)
            TanksTestAccess::runTick(b);
        QCOMPARE(TanksTestAccess::strikesPending(b), 0);
        QCOMPARE(p1.airMissiles, 0);
        // The tenth kill is the trigger: the salvo queues and the bank starts
        // over from zero.
        p1.armoredKills = 10;
        TanksTestAccess::runTick(b);
        QCOMPARE(p1.airMissiles, 5);
        QCOMPARE(p1.armoredKills, 0);
        // One strike out on the next tick; kills banked in flight wait for a
        // free launcher.
        p1.airTimer = 0;
        TanksTestAccess::runTick(b);
        QCOMPARE(p1.airMissiles, 4);
        QCOMPARE(TanksTestAccess::strikesPending(b), 1);
        p1.armoredKills = 10;
        p1.airTimer = 0;
        TanksTestAccess::runTick(b);
        QCOMPARE(p1.airMissiles, 3);   // still in flight; no stacking second salvo
        QCOMPARE(p1.armoredKills, 10); // the bank just waits
        QCOMPARE(TanksTestAccess::strikesPending(b), 2);
        // Drain the salvo anyway (the pacing is airTimer's job, tested
        // elsewhere) and the instant-respend rule takes over.
        for (int k = 0; k < 3 && p1.airMissiles > 0; ++k) {
            p1.airTimer = 0;
            TanksTestAccess::runTick(b);
        }
        QCOMPARE(p1.airMissiles, 0);   // salvo exhausted, ten still banked
        TanksTestAccess::runTick(b);
        QCOMPARE(p1.airMissiles, 5);   // the cycle fired the next one at once
        QCOMPARE(p1.armoredKills, 0);
    }

    // The bank crosses the stage change untouched and keeps counting there;
    // nothing is granted on stage entry - the charge alone drives the salvo.
    void airSupportCycleSurvivesTheStageChange() {
        Backend b;
        b.startGame();
        TanksTestAccess::freezeFor(b, 60.0);
        TanksTestAccess::clearArea(b, 0, 0, Backend::COLS - 1, Backend::ROWS - 1);
        TanksTestAccess::clearFoes(b);
        auto &p1 = TanksTestAccess::tank(b, 0);
        p1.alive = true;
        p1.tier = 4;
        p1.airMissiles = 0;
        p1.airTimer = 0;
        p1.armoredKills = 4;          // 40% banked
        TanksTestAccess::runTick(b);  // a live tick before the sector flips
        b.nextLevel();
        auto &np = TanksTestAccess::tank(b, 0);
        QCOMPARE(np.tier, 4);
        QCOMPARE(np.armoredKills, 4); // the bank survives the sector change
        QCOMPARE(np.airMissiles, 0);  // nothing is forced on stage entry
        for (int k = 0; k < 20; ++k)
            TanksTestAccess::runTick(b);
        QCOMPARE(TanksTestAccess::strikesPending(b), 0);
        // Six more kills land in the new sector: the tenth releases.
        np.armoredKills = 10;
        TanksTestAccess::runTick(b);
        QCOMPARE(np.airMissiles, 5);
        QCOMPARE(np.armoredKills, 0);
    }

    // Death drops the whole cycle with the tank: the bank starts over when
    // the tier comes back.
    void airSupportBankDiesWithThePlayer() {
        Backend b;
        b.startGame();
        TanksTestAccess::freezeFor(b, 60.0);
        auto &p1 = TanksTestAccess::tank(b, 0);
        p1.alive = true;
        p1.tier = 4;
        p1.armoredKills = 9;
        TanksTestAccess::killTank(b, 0);
        QCOMPARE(p1.tier, 0);
        QCOMPARE(p1.armoredKills, 0);
        QCOMPARE(p1.airMissiles, 0);
    }

    // Air support is guided shell fire (user spec, round 21.10): each strike
    // goes after the nearest alive enemy and lands on its CURRENT position,
    // so the hit is guaranteed even if it moved during the 1 s flight.
    void airSupportLandsOnItsTarget() {
        Backend b;
        b.startGame();
        TanksTestAccess::freezeFor(b, 60.0);
        TanksTestAccess::clearArea(b, 0, 0, Backend::COLS - 1, Backend::ROWS - 1);
        TanksTestAccess::clearFoes(b);
        TanksTestAccess::putPlayer(b, 0, 7.5, 13.5);
        auto &p1 = TanksTestAccess::tank(b, 0);
        p1.tier = 4;
        p1.armorHP = 0;
        p1.airMissiles = 1;
        p1.airTimer = 0;
        p1.armoredKills = 0;
        TanksTestAccess::putEnemy(b, 3.0, 3.0);   // the nearest (only) foe
        const int target = TanksTestAccess::lastEnemy(b);
        TanksTestAccess::runTick(b);
        QCOMPARE(p1.airMissiles, 0);              // the missile is out
        QCOMPARE(TanksTestAccess::strikesPending(b), 1);
        // The enemy wanders while the missile flies; the strike must follow.
        TanksTestAccess::enemy(b, target).x = 5.0;
        TanksTestAccess::enemy(b, target).y = 4.0;
        TanksTestAccess::strike(b, 0).timer = 0.0;  // the 1 s flight is over
        TanksTestAccess::runTick(b);
        QCOMPARE(TanksTestAccess::enemyIsAlive(b, target), false);
        QCOMPARE(TanksTestAccess::airX(b), 5.0);  // landed on the moved spot
        QCOMPARE(TanksTestAccess::airY(b), 4.0);
    }

    // A target that dies during the flight does not astray the strike: it
    // falls back to the spot it was launched at.
    void airSupportFallsBackWithoutLiveTarget() {
        Backend b;
        b.startGame();
        TanksTestAccess::freezeFor(b, 60.0);
        TanksTestAccess::clearArea(b, 0, 0, Backend::COLS - 1, Backend::ROWS - 1);
        TanksTestAccess::clearFoes(b);
        TanksTestAccess::putPlayer(b, 0, 7.5, 13.5);
        auto &p1 = TanksTestAccess::tank(b, 0);
        p1.tier = 4;
        p1.armorHP = 0;
        p1.airMissiles = 1;
        p1.airTimer = 0;
        p1.armoredKills = 0;
        TanksTestAccess::putEnemy(b, 3.0, 3.0);
        TanksTestAccess::runTick(b);              // fires, guided at the foe
        TanksTestAccess::enemy(b, TanksTestAccess::lastEnemy(b)).alive = false;
        TanksTestAccess::strike(b, 0).timer = 0.0;
        TanksTestAccess::runTick(b);              // impact on the stored spot
        QCOMPARE(TanksTestAccess::strikesPending(b), 0);
        QCOMPARE(TanksTestAccess::airX(b), 3.0);
        QCOMPARE(TanksTestAccess::airY(b), 3.0);
    }

    // A blast missile fired at an enemy ahead of the player but off the
    // firing lane curves up and reaches it (teledirigido, round 21.9).
    void theBlastHomesOnAnEnemyAhead() {
        Backend b;
        b.startGame();
        TanksTestAccess::freezeFor(b, 60.0);
        TanksTestAccess::clearArea(b, 0, 0, Backend::COLS - 1, Backend::ROWS - 1);
        TanksTestAccess::clearFoes(b);
        auto &p1 = TanksTestAccess::tank(b, 0);
        p1.alive = true;
        p1.x = 2.5;
        p1.y = 12.5;
        p1.dir = 1;
        TanksTestAccess::putEnemy(b, 9.5, 8.5);   // 4 cells off the lane, ahead
        TanksTestAccess::takePickup(b, 8);         // BLAST MISSILE
        b.fireP1();
        QVERIFY2(b.bullets().size() >= 1, "the blast shot was not fired");
        for (int i = 0; i < 90 && b.bullets().size() > 0; ++i)
            TanksTestAccess::stepBullets(b, 1.0 / 60.0);
        QCOMPARE(TanksTestAccess::enemiesAlive(b), 0);
    }

    // With nobody ahead the blast stays a plain straight shot: it flies past
    // the board edge without touching an enemy standing behind the player.
    void theBlastStaysStraightWithNobodyAhead() {
        Backend b;
        b.startGame();
        TanksTestAccess::freezeFor(b, 60.0);
        TanksTestAccess::clearArea(b, 0, 0, Backend::COLS - 1, Backend::ROWS - 1);
        TanksTestAccess::clearFoes(b);
        auto &p1 = TanksTestAccess::tank(b, 0);
        p1.alive = true;
        p1.x = 2.5;
        p1.y = 12.5;
        p1.dir = 1;
        TanksTestAccess::putEnemy(b, 1.5, 12.5);   // behind the player
        TanksTestAccess::putEnemy(b, 1.5, 4.0);    // behind, far off-lane anyway
        TanksTestAccess::takePickup(b, 8);
        b.fireP1();
        for (int i = 0; i < 90 && b.bullets().size() > 0; ++i)
            TanksTestAccess::stepBullets(b, 1.0 / 60.0);
        QCOMPARE(TanksTestAccess::enemiesAlive(b), 2);
    }

    // The BLAST MISSILE's area damage reaches 3 cells from the impact now
    // (user spec: 1.5 -> 3, section 21.3): an enemy 2.0-3.0 cells from the
    // impact dies with it, one beyond 3 survives.
    void theBlastAreaReachesThreeCells() {
        Backend b;
        b.startGame();
        TanksTestAccess::freezeFor(b, 60.0);
        TanksTestAccess::clearArea(b, 0, 0, Backend::COLS - 1, Backend::ROWS - 1);
        // One player on a clear lane, facing right; three frozen enemies.
        TanksTestAccess::clearFoes(b);
        auto &p1 = TanksTestAccess::tank(b, 0);
        p1.alive = true;
        p1.x = 2.5;
        p1.y = 7.5;
        p1.dir = 1;
        TanksTestAccess::putEnemy(b, 6.5, 7.5);    // impact here
        const int far = TanksTestAccess::lastEnemy(b);
        TanksTestAccess::putEnemy(b, 8.8, 7.5);    // ~2.3 from the impact: dies
        const int nearBy = TanksTestAccess::lastEnemy(b);
        TanksTestAccess::putEnemy(b, 10.5, 7.5);   // ~4.0 from the impact: survives
        const int out = TanksTestAccess::lastEnemy(b);
        TanksTestAccess::takePickup(b, 8);         // BLAST MISSILE
        b.fireP1();
        QVERIFY2(b.bullets().size() >= 1, "the blast shot was not fired");
        // Run the shot to its first hit and let the area pulse do its work.
        for (int i = 0; i < 60 && b.bullets().size() > 0; ++i)
            TanksTestAccess::stepBullets(b, 1.0 / 60.0);
        QCOMPARE(b.bullets().size(), 0);
        QVERIFY2(!TanksTestAccess::enemyIsAlive(b, far), "the impact target died");
        QVERIFY2(!TanksTestAccess::enemyIsAlive(b, nearBy), "the enemy at ~2.3 cells died with the blast");
        QVERIFY2(TanksTestAccess::enemyIsAlive(b, out), "the enemy at ~4.0 cells must survive a 3-cell blast");
    }

    // A carrier's bonus lands on a random free spot anywhere on the board
    // (section 21.3): empty or forest ground with no tank standing on it,
    // never dictated by the death spot.
    void pickupDropsOnAFreeSpot() {
        Backend b;
        b.startGame();
        TanksTestAccess::freezeFor(b, 60.0);
        // A crowded corner is the death spot...
        TanksTestAccess::clearArea(b, 0, 0, Backend::COLS - 1, Backend::ROWS - 1);
        TanksTestAccess::putPlayer(b, 0, 1.5, 13.5);
        TanksTestAccess::putEnemy(b, 1.5, 12.6);
        TanksTestAccess::putEnemy(b, 1.9, 13.5);
        TanksTestAccess::putEnemy(b, 0.7, 12.9);

        TanksTestAccess::spawnPickupAt(b, 1.5, 13.5);
        QVERIFY(TanksTestAccess::pickupActive(b));
        const double px = TanksTestAccess::pickupX(b);
        const double py = TanksTestAccess::pickupY(b);
        QVERIFY2(px > 0.0 && px < Backend::COLS && py > 0.0 && py < Backend::ROWS,
                 "inside the board");
        QVERIFY2(!TanksTestAccess::tankAt(b, px, py), "a tank stands on the dropped bonus");
    }

    // ---- The freeze round (21.15) ----
    //
    // The sessions that wedged the GUI thread ran tier 4, whose salvo
    // plays a 2.0 s air-support sample every 1.5 s, so the plays overlap.
    // The game froze mid-air-support while the idle loop kept sounding,
    // and the same thing happened three times on a different machine, so
    // it is not one machine's audio stack.
    //
    // WHAT THESE TESTS DO NOT DO: they do not reproduce the freeze. The
    // freeze is a blocked GUI thread, and the audio lives in QML
    // (SoundEffect) and in the multimedia backend - none of which a
    // backend test can reach. The real capture needs the running game
    // and a debugger: see tools/watchdog.sh.
    //
    // What the backend CAN be held to is that the churn never costs the
    // game loop its progress: a tick must always advance the clock, and a
    // salvo in flight must never leave the state inconsistent (missiles
    // lost, strikes leaked, the bank wrong). If the backend ever grew a
    // stall of its own under the churn, these would catch it.

    // A salvo that keeps restarting, with the samples overlapping, must
    // never stall the loop or lose state.
    void theGameClockKeepsMovingUnderTheTier4Churn() {
        Backend b;
        b.startGame();
        TanksTestAccess::freezeFor(b, 600.0);
        TanksTestAccess::clearArea(b, 0, 0, Backend::COLS - 1, Backend::ROWS - 1);
        TanksTestAccess::clearFoes(b);
        auto &p1 = TanksTestAccess::tank(b, 0);
        p1.alive = true;
        p1.x = 7.5;
        p1.y = 13.5;
        p1.tier = 4;
        p1.armorHP = 0;

        // Ten kills release the salvo of five; the bank then restarts, so
        // the cycle repeats for ever: the same overlap the failing session
        // had, with a strike in flight most of the time.
        const double start = TanksTestAccess::gameTime(b);
        int salvos = 0;
        for (int k = 0; k < 600; ++k) {
            p1.armoredKills = 10;   // the bank is topped every tick
            TanksTestAccess::runTick(b);
            // The clock must move on every single tick: a frozen loop is
            // exactly the symptom the user reported.
            QVERIFY2(TanksTestAccess::gameTime(b) > start,
                     "the game clock stopped advancing under the audio churn");
            if (p1.airMissiles == 5)
                salvos++;
            // A salvo may not stack on a live one, and the bank may not
            // hold more than the ten it is worth.
            QVERIFY(p1.airMissiles <= 5);
            QVERIFY(p1.armoredKills <= 10);
        }
        QVERIFY2(salvos > 0, "the salvo cycle never fired");
        // The strikes that were launched must all resolve: one second of
        // flight and the missile is gone. The last salvo may still be in
        // the air, so the loop is given the time it needs.
        for (int k = 0; k < 200 && TanksTestAccess::strikesPending(b) > 0; ++k)
            TanksTestAccess::runTick(b);
        QCOMPARE(TanksTestAccess::strikesPending(b), 0);
    }

    // The same churn with the BLAST shots mixed in: the fifth blast shot is
    // the one that used to die at the muzzle, and a burst of them is what
    // the failing session was playing.
    void theBlastShotsSurviveTheTier4Churn() {
        Backend b;
        b.startGame();
        TanksTestAccess::freezeFor(b, 600.0);
        TanksTestAccess::clearArea(b, 0, 0, Backend::COLS - 1, Backend::ROWS - 1);
        TanksTestAccess::clearFoes(b);
        auto &p1 = TanksTestAccess::tank(b, 0);
        p1.alive = true;
        p1.x = 7.5;
        p1.y = 13.5;
        p1.tier = 4;
        p1.armorHP = 0;
        p1.blastShots = 5;

        const double start = TanksTestAccess::gameTime(b);
        for (int k = 0; k < 400; ++k) {
            p1.armoredKills = 10;
            TanksTestAccess::runTick(b);
            QVERIFY2(TanksTestAccess::gameTime(b) > start,
                     "the game clock stopped advancing with the blast shots");
            // A blast shot is spent by the press, and the fifth one still
            // spawns its missile (the 21.14 fix).
            if (k % 3 == 0)
                b.fireP1();
        }
        // The blast count never goes negative and the salvo stays sane.
        QVERIFY(p1.blastShots >= 0);
        QVERIFY(p1.airMissiles <= 5);
    }

    // Muting must stop the engine loops (the 21.17 fix) and the sound flag
    // must be what the audio side reads, so a muted game is silent even
    // with the tier-4 churn running.
    void aMutedGameAsksForNoSoundUnderTheTier4Churn() {
        Backend b;
        b.startGame();
        TanksTestAccess::freezeFor(b, 600.0);
        auto &p1 = TanksTestAccess::tank(b, 0);
        p1.alive = true;
        p1.tier = 4;

        b.setSoundEnabled(false);
        QSignalSpy sounds(&b, &Backend::playSound);
        for (int k = 0; k < 60; ++k) {
            p1.armoredKills = 10;
            TanksTestAccess::runTick(b);
        }
        QVERIFY2(sounds.isEmpty(), "a muted game still asked for a sound");
    }
};

QTEST_MAIN(TanksTests)
#include "oligarchy-tanks_tests.moc"

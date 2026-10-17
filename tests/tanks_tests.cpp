#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
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
};

class TanksTests : public QObject {
    Q_OBJECT

private slots:
    void startsPausedInMenu() {
        Backend b;
        QCOMPARE(b.state(), QString("menu"));
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

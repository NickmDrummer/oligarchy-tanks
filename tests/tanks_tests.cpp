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
    static void putTile(Backend &b, int x, int y, int tile) {
        b.m_grid[y * Backend::COLS + x] = tile;
        if (tile == TanksTestAccess::Brick)
            b.m_brickQ[y * Backend::COLS + x] = 0xF;
    }
    static int tile(const Backend &b, int x, int y) {
        return b.m_grid[y * Backend::COLS + x];
    }
    static void chip(Backend &b, double x, double y) { b.damageBrickAt(x, y); }
    static void grantPick(Backend &b, int player) { b.m_p[player].pickUntil = b.m_time + 20; }
    static bool hasPick(const Backend &b, int player) { return b.m_time < b.m_p[player].pickUntil; }
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
        QCOMPARE(b.boardQ().size(), 30 * 30);
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

    // Surviving quarters of one tile, read from the 2x2-collapsed boardQ.
    static int quartersOf(const Backend &b, int x, int y) {
        const QString q = b.boardQ();
        int n = 0;
        for (int dy = 0; dy < 2; ++dy)
            for (int dx = 0; dx < 2; ++dx)
                if (q[(y * 2 + dy) * Backend::COLS * 2 + (x * 2 + dx)] == QChar('B'))
                    ++n;
        return n;
    }

    // Each hit chips exactly the quarter it lands in; the tile only turns
    // empty once all four quarters are gone.
    void shotsChipQuarters() {
        Backend b;
        b.startGame();
        TanksTestAccess::putTile(b, 5, 5, TanksTestAccess::Brick);
        QCOMPARE(quartersOf(b, 5, 5), 4);
        TanksTestAccess::chip(b, 5.25, 5.25); // top-left
        QCOMPARE(quartersOf(b, 5, 5), 3);
        QCOMPARE(TanksTestAccess::tile(b, 5, 5), int(TanksTestAccess::Brick));
        TanksTestAccess::chip(b, 5.75, 5.25); // top-right
        QCOMPARE(quartersOf(b, 5, 5), 2);
        TanksTestAccess::chip(b, 5.25, 5.75); // bottom-left
        QCOMPARE(quartersOf(b, 5, 5), 1);
        TanksTestAccess::chip(b, 5.75, 5.75); // bottom-right
        QCOMPARE(quartersOf(b, 5, 5), 0);
        QCOMPARE(TanksTestAccess::tile(b, 5, 5), TanksTestAccess::Empty);
    }

    // A real shot stops at the brick and removes a single quarter.
    void shotChipsOneQuarter() {
        Backend b;
        b.startGame();
        TanksTestAccess::putTile(b, 3, 12, TanksTestAccess::Brick);
        QCOMPARE(quartersOf(b, 3, 12), 4);
        b.fireP1();
        QTest::qWait(500);
        QCOMPARE(quartersOf(b, 3, 12), 3);
    }

    // A shot chips the quarter in its lane; once both quarters on that side
    // are gone, later shots fly through the gap to the tile behind.
    void shotsPassThroughBrokenQuarter() {
        Backend b;
        b.startGame();
        TanksTestAccess::putTile(b, 3, 12, TanksTestAccess::Brick);
        TanksTestAccess::putTile(b, 3, 11, TanksTestAccess::Brick);
        b.fireP1();
        QTest::qWait(500);
        QCOMPARE(quartersOf(b, 3, 12), 3);
        QTest::qWait(400);
        b.fireP1();
        QTest::qWait(500);
        QCOMPARE(quartersOf(b, 3, 12), 2);
        QTest::qWait(400);
        b.fireP1();
        QTest::qWait(600);
        QCOMPARE(quartersOf(b, 3, 12), 2);
        QCOMPARE(quartersOf(b, 3, 11), 3);
    }

    // The real in-game case: PICK active, a brick in the lane and steel
    // behind it. Repeated shots chew through the brick and then break steel.
    void pickReachesSteelBehindBrick() {
        Backend b;
        b.startGame();
        TanksTestAccess::putTile(b, 3, 12, TanksTestAccess::Brick);
        TanksTestAccess::putTile(b, 3, 11, TanksTestAccess::Empty);
        TanksTestAccess::putTile(b, 3, 10, TanksTestAccess::Steel);
        TanksTestAccess::grantPick(b, 0);
        for (int i = 0; i < 4; ++i) {
            QTest::qWait(450);
            b.fireP1();
        }
        QTest::qWait(600);
        QCOMPARE(quartersOf(b, 3, 12), 2);
        QCOMPARE(TanksTestAccess::tile(b, 3, 10), TanksTestAccess::Empty);
    }

    // PICK is what lets a shot smash steel: without it steel shrugs the
    // hit off, with it the tile is destroyed.
    void pickBonusBreaksSteel() {
        Backend b;
        b.startGame();
        TanksTestAccess::putTile(b, 3, 12, TanksTestAccess::Steel);
        b.fireP1();
        QTest::qWait(500);
        QCOMPARE(TanksTestAccess::tile(b, 3, 12), int(TanksTestAccess::Steel));

        TanksTestAccess::grantPick(b, 0);
        QVERIFY(TanksTestAccess::hasPick(b, 0));
        QTest::qWait(1500); // let the fire cooldown expire
        b.fireP1();
        QTest::qWait(500);
        QCOMPARE(TanksTestAccess::tile(b, 3, 12), TanksTestAccess::Empty);
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

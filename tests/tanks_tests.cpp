#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "theme.h"

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

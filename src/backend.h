#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <random>
#include <vector>

// Backend owns all game state. QML only renders and forwards key events.
// Input is abstracted per player (direction flags + fire edge), and the
// gamepad reader in gamepad.cpp drives the same path through the pad*
// invokables below. The keyboard is always live for both players.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    // Board state has its own notification: tile queries must only re-run
    // when the map really changes. Wired to frameChanged they re-evaluated
    // 60 times a second, and reading a QString property that often is what
    // made the game crawl (see AGENTS.md section 7).
    Q_PROPERTY(QString board READ board NOTIFY boardChanged)
    Q_PROPERTY(int boardRev READ boardRev NOTIFY boardChanged)
    Q_PROPERTY(int cols READ cols CONSTANT)
    Q_PROPERTY(int rows READ rows CONSTANT)
    Q_PROPERTY(QVariantList players READ players NOTIFY frameChanged)
    Q_PROPERTY(QVariantList enemies READ enemies NOTIFY frameChanged)
    Q_PROPERTY(QVariantList bullets READ bullets NOTIFY frameChanged)
    Q_PROPERTY(int enemiesLeft READ enemiesLeft NOTIFY frameChanged)
    Q_PROPERTY(QVariantList floaters READ floaters NOTIFY frameChanged)
    Q_PROPERTY(QVariantList pickups READ pickups NOTIFY frameChanged)
    Q_PROPERTY(int frameTick READ frameTick NOTIFY frameChanged)
    Q_PROPERTY(int level READ level NOTIFY stateChanged)
    Q_PROPERTY(bool frozen READ frozen NOTIFY frameChanged)
    Q_PROPERTY(bool soundEnabled READ soundEnabled WRITE setSoundEnabled NOTIFY soundChanged)
    // Connected pads and the player each one drives (-1 until it joins).
    Q_PROPERTY(QVariantList pads READ pads NOTIFY padsChanged)

public:
    static constexpr int COLS = 15;
    static constexpr int ROWS = 15;
    static constexpr int TOTAL_ENEMIES = 8;

    explicit Backend(QObject *parent = nullptr);

    QString state() const { return m_state; }
    QString message() const { return m_message; }
    QString board() const;
    int boardRev() const { return m_boardRev; }
    // Tile kind at (x, y), matching the Tile enum: 0 empty, 1 brick,
    // 2 steel, 3 base, 4 dead base, 5 forest. Out of bounds reads empty.
    // QML passes boardRev as `rev`, so the binding is only re-run when the
    // board changes instead of every frame.
    Q_INVOKABLE int tileKind(int x, int y, int rev) const;
    // True while strip `strip` of the brick tile at (x, y) has been shot
    // away. Strips 0-3 are the vertical quarters, left to right; 4-7 the
    // horizontal ones, top to bottom. `rev` works like in tileKind().
    Q_INVOKABLE bool brickStripGone(int x, int y, int strip, int rev) const;
    int cols() const { return COLS; }
    int rows() const { return ROWS; }
    QVariantList players() const;
    QVariantList enemies() const;
    QVariantList bullets() const;
    QVariantList floaters() const;
    QVariantList pickups() const;
    int enemiesLeft() const;
    int frameTick() const { return m_frameTick; }
    int level() const { return m_level; }
    bool frozen() const { return m_time < m_freezeUntil; }
    int totalForLevel() const { return TOTAL_ENEMIES + 2 * (m_level - 1); }

    // Stable per-slot getters for QML. Delegates are created once with a
    // fixed count and only update properties, so the scene graph is never
    // rebuilt mid-game. Pass frameTick (ignored) to refresh each frame.
    Q_INVOKABLE QVariantMap playerAt(int i, int tick) const;
    Q_INVOKABLE QVariantMap enemyAt(int i, int tick) const;
    Q_INVOKABLE QVariantMap bulletAt(int i, int tick) const;
    Q_INVOKABLE QVariantMap floaterAt(int i, int tick) const;
    Q_INVOKABLE QVariantMap pickupAt(int tick) const;
    bool soundEnabled() const { return m_soundEnabled; }
    void setSoundEnabled(bool v);

    // Movement flags from held keys. dx/dy is -1/0/1, pressed tracks key state.
    Q_INVOKABLE void setP1Move(int dx, int dy, bool pressed);
    Q_INVOKABLE void setP2Move(int dx, int dy, bool pressed);
    Q_INVOKABLE void fireP1();
    Q_INVOKABLE void fireP2();
    Q_INVOKABLE void startGame();
    Q_INVOKABLE void nextLevel();
    Q_INVOKABLE void togglePause();

    // Gamepads, driven by the reader in gamepad.cpp. The keyboard stays live
    // for both players; a pad joins by pressing START, and the flags are kept
    // per source so releasing a key never cancels a stick.
    //
    // Who gets which player: player 1 is the keyboard's, so the *first* pad to
    // press START takes player 2 and a second pad takes player 1. That way one
    // pad means one human on it, and two pads give each player their own stick.
    Q_INVOKABLE void padConnected(int pad, const QString &name);
    Q_INVOKABLE void padDisconnected(int pad);
    Q_INVOKABLE void padMove(int pad, int dx, int dy, bool pressed);
    Q_INVOKABLE void padFire(int pad);
    Q_INVOKABLE void padStart(int pad);
    Q_INVOKABLE void padSelect(int pad);
    QVariantList pads() const;

signals:
    void stateChanged();
    void frameChanged();
    // Emitted whenever the map changes: new map, brick damage, base guard.
    void boardChanged();
    void soundChanged();
    void playSound(const QString &name);
    // A pad asked for the manual (SELECT / Share).
    void helpRequested();
    void padsChanged();

    // Test-only access to internal state, so behaviour like bonus effects
    // can be asserted deterministically without rigging the map at runtime.
    friend struct TanksTestAccess;

private:
    struct Tank {
        double x = 0, y = 0;
        int dir = 0; // 0 up, 1 right, 2 down, 3 left
        bool alive = true;
        bool moving = false;
        int lives = 3;
        int score = 0;
        double cooldown = 0;
        double respawn = 0;
        double shieldUntil = -1;
        double speedUntil = -1;
        double cannonUntil = -1;
    };
    struct Enemy {
        double x = 0, y = 0;
        int dir = 2;
        bool alive = false;
        bool moving = false;
        double cooldown = 0;
        double think = 0;
        int kind = 0; // 0 gray basic, 1 green fast, 2 steel armored
        int hp = 1;
    };
    struct Pickup {
        double x = 0, y = 0;
        int kind = 0;
        double age = 0;
        bool active = false;
    };
    struct Floater {
        double x = 0, y = 0;
        int score = 0;
        double age = 0;
    };
    struct Bullet {
        double x = 0, y = 0;
        int dir = 0;
        int owner = 2; // 0 p1, 1 p2, 2 enemy
        bool active = false;
        double speed = 0;
        bool big = false;
    };

    enum Tile { Empty = 0, Brick = 1, Steel = 2, Base = 3, BaseDead = 4, Forest = 5 };
    enum PickupKind { Speed = 0, Shield = 1, Pick = 2, Life = 3, Grenade = 4, Cannon = 5, Freeze = 6 };

    void setState(const QString &s, const QString &msg = {});
    void generateMap();
    void resetTanks(bool fullStats);
    void tick();
    void updateTank(Tank &t, bool up, bool down, bool left, bool right, double dt, double speed);
    void updateEnemies(double dt);
    void updateBullets(double dt);
    void spawnEnemy();
    void fireBullet(double x, double y, int dir, int owner, double speed);
    double enemySpeedMult() const { return 1.0 + 0.1 * (m_level - 1); }
    bool solidAt(double x, double y, double size) const;
    // Solid terrain test for a single point. Brick is special: under half
    // destroyed the tile blocks as a whole block, from the halfway mark on
    // only the surviving strips block.
    bool tileBlocksPoint(double x, double y) const;
    bool hitsTank(double bx, double by, double tx, double ty) const;
    bool blockedByTank(double x, double y, double size, const void *self, bool isEnemy) const;
    int tileAt(int cx, int cy) const;
    // Shears one strip off a brick tile, on the face the shot came from.
    void chipBrick(int cx, int cy, int dir);
    // PICK turns the wall around the base into concrete, then back to brick.
    // Anything standing where the concrete lands is crushed.
    void setBaseGuard(bool steel);
    void updateBaseGuard();
    // Kills every tank overlapping one board cell, so PICK can rebuild the
    // base wall without sealing a tank inside it.
    void crushTanksIn(int cx, int cy);
    static bool cellHitsTank(int cx, int cy, double tx, double ty);
    // Board mutations funnel through here: the cached string form is
    // dropped and QML is told to re-run its tile queries.
    void bumpBoard();
    void killPlayer(int idx);
    void checkEnd();
    int aliveEnemies() const;
    void spawnPickupAt(double x, double y);
    void applyPickup(int player);
    void updatePickups(double dt);
    void updateFloaters(double dt);

    // Gamepad bookkeeping. A pad is a connected device plus the player it
    // drives (-1 until it presses START).
    struct Pad {
        int index = 0;
        QString name;
        int player = -1;
    };
    const Pad *padAt(int index) const;
    int playerForPad(int index) const;
    // Claims the first free player slot, player 2 before player 1.
    bool joinPad(int index);

    QString m_state = "menu";
    QString m_message;
    std::vector<int> m_grid;
    // Per-tile brick strip mask, bits 0-3 vertical V0..V3 (left to right),
    // bits 4-7 horizontal H0..H3 (top to bottom). 0xFF is a whole tile.
    std::vector<uint8_t> m_brickQ;
    // Bumped on every board change; QML passes it back to the tile queries
    // so they re-run only when something actually moved.
    int m_boardRev = 0;
    // Cached string form of m_grid for board() (tests and probes only).
    mutable QString m_boardCache;
    mutable bool m_boardCacheValid = false;
    Tank m_p[2];
    std::vector<Enemy> m_foes;
    std::vector<Bullet> m_bullets;
    Pickup m_pickup;
    std::vector<Floater> m_floaters;
    double m_freezeUntil = -1;
    // Concrete wall around the base, granted by PICK. Negative means off.
    double m_baseGuardUntil = -1;
    std::vector<int> m_baseGuard;
    double m_time = 0;
    int m_frameTick = 0;
    int m_level = 1;
    QElapsedTimer m_clock;
    bool m_p1Up = false, m_p1Down = false, m_p1Left = false, m_p1Right = false;
    bool m_p2Up = false, m_p2Down = false, m_p2Left = false, m_p2Right = false;
    // Directions held by a pad, indexed [player][0 up, 1 right, 2 down, 3 left]
    // and ORed with the keyboard's flags when a tank moves.
    bool m_padHeld[2][4] = {};
    std::vector<Pad> m_pads;
    int m_spawned = 0;
    double m_spawnTimer = 0;
    bool m_soundEnabled = true;
    QTimer m_timer;
    std::mt19937 m_rng{std::random_device{}()};
};

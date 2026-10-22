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
    // Why the last run ended, for the game over screen: "over" or "base".
    Q_PROPERTY(QString ending READ ending NOTIFY stateChanged)
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
    Q_PROPERTY(QVariantList explosions READ explosions NOTIFY frameChanged)
    Q_PROPERTY(QVariantList pickups READ pickups NOTIFY frameChanged)
    Q_PROPERTY(int frameTick READ frameTick NOTIFY frameChanged)
    Q_PROPERTY(int level READ level NOTIFY stateChanged)
    Q_PROPERTY(bool frozen READ frozen NOTIFY frameChanged)
    // True during the last two seconds of a freeze: QML blinks the frozen
    // enemies so the player can see the freeze about to lift.
    Q_PROPERTY(bool freezeEnding READ freezeEnding NOTIFY frameChanged)
    // The NUKE blast: a circle that grows out of the spot where the bonus was
    // picked up until it covers the board. {x, y, progress, active}.
    Q_PROPERTY(QVariantMap nuke READ nuke NOTIFY frameChanged)
    // The BLAST MISSILE impact: a small circle that grows out of the hit spot.
    // {x, y, progress, active}.
    Q_PROPERTY(QVariantMap blastImpact READ blastImpact NOTIFY frameChanged)
    // Air support (LEVELUP tier 4) impact: a circle that grows out of the
    // strike spot, 1 s after the sound starts. {x, y, progress, active}.
    Q_PROPERTY(QVariantMap airSupportImpact READ airSupportImpact NOTIFY frameChanged)
    Q_PROPERTY(bool soundEnabled READ soundEnabled WRITE setSoundEnabled NOTIFY soundChanged)
    // Vibration is its own switch (the user's call: sound and rumble are
    // separate things). V toggles it; the footer keeps an icon for each.
    Q_PROPERTY(bool rumbleEnabled READ rumbleEnabled WRITE setRumbleEnabled NOTIFY rumbleChanged)
    // Connected pads and the player each one drives (-1 until it joins).
    Q_PROPERTY(QVariantList pads READ pads NOTIFY padsChanged)
    // How many tanks are in the game, picked on the select screen: 1 or 2.
    Q_PROPERTY(int playerCount READ playerCount NOTIFY playerCountChanged)
    // Highlighted option on the select screen: 0 is one player, 1 is two.
    Q_PROPERTY(int menuIndex READ menuIndex NOTIFY menuChanged)

public:
    static constexpr int COLS = 15;
    static constexpr int ROWS = 15;
    // Tanks per stage: fixed at 16 (phase C). What scales with the level is the
    // mix of kinds, not the count. See spawnEnemy() and AGENTS.md section 17.
    static constexpr int TOTAL_ENEMIES = 16;

    explicit Backend(QObject *parent = nullptr);

    QString state() const { return m_state; }
    QString message() const { return m_message; }
    QString ending() const { return m_ending; }
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
    QVariantList explosions() const;
    QVariantList pickups() const;
    int enemiesLeft() const;
    int frameTick() const { return m_frameTick; }
    int level() const { return m_level; }
    int playerCount() const { return m_playerCount; }
    int menuIndex() const { return m_menuIndex; }
    bool frozen() const { return m_time < m_freezeUntil; }
    bool freezeEnding() const { return m_time < m_freezeUntil && m_freezeUntil - m_time <= 2.0; }
    QVariantMap nuke() const;
    QVariantMap blastImpact() const;
    QVariantMap airSupportImpact() const;
    int totalForLevel() const { return TOTAL_ENEMIES; }

    // Stable per-slot getters for QML. Delegates are created once with a
    // fixed count and only update properties, so the scene graph is never
    // rebuilt mid-game. Pass frameTick (ignored) to refresh each frame.
    Q_INVOKABLE QVariantMap playerAt(int i, int tick) const;
    Q_INVOKABLE QVariantMap enemyAt(int i, int tick) const;
    Q_INVOKABLE QVariantMap bulletAt(int i, int tick) const;
    Q_INVOKABLE QVariantMap floaterAt(int i, int tick) const;
    Q_INVOKABLE QVariantMap explosionAt(int i, int tick) const;
    Q_INVOKABLE QVariantMap pickupAt(int tick) const;
    bool soundEnabled() const { return m_soundEnabled; }
    void setSoundEnabled(bool v);
    bool rumbleEnabled() const { return m_rumbleEnabled; }
    void setRumbleEnabled(bool v);
    Q_INVOKABLE void toggleRumble();

    // Movement flags from held keys. dx/dy is -1/0/1, pressed tracks key state.
    Q_INVOKABLE void setP1Move(int dx, int dy, bool pressed);
    Q_INVOKABLE void setP2Move(int dx, int dy, bool pressed);
    Q_INVOKABLE void fireP1();
    Q_INVOKABLE void fireP2();
    Q_INVOKABLE void startGame();
    Q_INVOKABLE void nextLevel();
    Q_INVOKABLE void togglePause();
    // Back to the select screen: what a game over leads to once the player
    // presses START, and what R does mid-game (a clean way out of a run).
    Q_INVOKABLE void toSelect();

    // The select screen, the first screen of the game and where a game over
    // returns to. Keyboard keys and pad controls both land here, so QML keeps
    // forwarding input blindly (see setP1Move / fireP1 below).
    Q_INVOKABLE void menuMove(int delta);
    Q_INVOKABLE void menuConfirm();
    Q_INVOKABLE void menuChoose(int mode);

    // Gamepads, driven by the reader in gamepad.cpp. The keyboard stays live
    // for both players; a pad joins by pressing START, and the flags are kept
    // per source so releasing a key never cancels a stick.
    //
    // Who gets which player: with two tanks the keyboard keeps player 1, so the
    // *first* pad to press START takes player 2 and a second pad takes player 1.
    // With a single tank (one-player game) the pad takes that tank. Either way
    // one pad means one human on it.
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
    void rumbleChanged();
    // A pad asked for the manual (SELECT / Share).
    void menuRequested();
    void padsChanged();
    void playerCountChanged();
    void menuChanged();

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
        double blastUntil = -1;
        // BLAST MISSILE: number of blast shots remaining (5 per pickup).
        int blastShots = 0;
        // Offensive tier from LEVELUP: 0 default, 1..4. Resets on death,
        // survives a sector change.
        int tier = 0;
        // Armour layers: 3 at tier 3+, plus 1 while CANNON is active (max).
        int armorHP = 0;
        // Armored tanks killed by this player, feeding the air-support
        // recharge at tier 4.
        int armoredKills = 0;
        // Air support (tier 4): missiles left in the current salvo and the
        // timer to the next one. The armoured-kill bank (`armoredKills`)
        // IS the charge: ten banked kills release the salvo, and it starts
        // counting over while tier 4 stays alive.
        int airMissiles = 0;
        double airTimer = -1;
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
    struct Explosion {
        double x = 0, y = 0;
        double age = 0;
    };
    struct Bullet {
        double x = 0, y = 0;
        int dir = 0;
        int owner = 2; // 0 p1, 1 p2, 2 enemy
        bool active = false;
        double speed = 0;
        bool big = false;
        bool blast = false;
        // Homing target (enemy index) for blast shots; -1 flies straight.
        int target = -1;
    };
    // Pending air-support strike: the sound plays immediately, the explosion
    // (and radio circle) happen 1 s later.
    struct AirStrike {
        double x = 0, y = 0;
        double timer = 1.0;
        bool done = false;
        int owner = 0;   // player who launched it; score/kill credit goes here
        int target = -1; // enemy index the strike tracks; -1 = aimless spot
    };

    enum Tile { Empty = 0, Brick = 1, Steel = 2, Base = 3, BaseDead = 4, Forest = 5 };
    enum PickupKind { Speed = 0, Shield = 1, Pick = 2, Life = 3, Nuke = 4, Cannon = 5, Freeze = 6, LevelUp = 7, BlastMissile = 8 };

    void setState(const QString &s, const QString &msg = {});
    void generateMap();
    // An enemy cannot dig, so a spawn lane sealed by masonry would trap it for
    // the whole stage: carve a way down only when the scatter really cut it off.
    void openSpawnLanes();
    void resetTanks(bool fullStats);
    void tick();
    void updateTank(Tank &t, bool up, bool down, bool left, bool right, double dt, double speed);
    void updateEnemies(double dt);
    void updateBullets(double dt);
    // Spawns the next tank of the wave in a free lane. Returns false when
    // every entry lane is taken, so the caller can retry instead of stacking
    // two tanks on one cell (which separateTanks() would then shove into the
    // wall).
    bool spawnEnemy();
    bool cellBusy(double x, double y) const;
    void fireBullet(double x, double y, int dir, int owner, double speed, bool playSnd = true);
    double enemySpeedMult() const { return 1.0 + 0.1 * (m_level - 1); }
    bool solidAt(double x, double y, double size) const;
    // Solid terrain test for a single point. Brick is special: under half
    // destroyed the tile blocks as a whole block, from the halfway mark on
    // only the surviving strips block.
    bool tileBlocksPoint(double x, double y) const;
    bool hitsTank(double bx, double by, double tx, double ty) const;
    // Would a tank at (nx, ny) run into another one? Contact that already
    // exists only blocks a move that closes the gap, so two tanks that end up
    // touching can always back away instead of locking each other in place.
    // (curX, curY) is where the moving tank is now; `self` is skipped.
    bool blockedByTank(double nx, double ny, double size, const void *self, bool isEnemy,
                       double curX, double curY) const;
    // Nudge overlapping tanks apart so they cannot stay locked together.
    void separateTanks();
    // A tank that ends up inside a whole tile or off the board cannot step out
    // (its 0.8 hull is wider than any step), so it is rescued to the nearest
    // cell whose hull is free. The escape nudge handles damaged bricks, which
    // is the legitimate case of a tank standing on the 50% line.
    bool buriedInSolid(double x, double y) const;
    bool nearestFreeSpot(double x, double y, double &outX, double &outY) const;
    // How far a tank could travel in a direction before hitting anything, in
    // cells. Used to pick a way out of a corner instead of re-rolling blind
    // into the wall it just hit.
    double freeRun(double x, double y, int dir, const void *self, bool isEnemy) const;
    int bestDir(double x, double y, const void *self, bool isEnemy, int fallback);
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
    void pushExplosion(double x, double y);
    static bool cellHitsTank(int cx, int cy, double tx, double ty);
    // Board mutations funnel through here: the cached string form is
    // dropped and QML is told to re-run its tile queries.
    void bumpBoard();
    void killPlayer(int idx);
    // Bring a player who is out back with a single tank, at his spawn point and
    // with the usual 3 s grace. Used by the LIFE bonus on a partner.
    void revivePlayer(int idx);
    void checkEnd();
    int aliveEnemies() const;
    void spawnPickupAt(double x, double y);
    void applyPickup(int player);
    void updatePickups(double dt);
    // Armour layers granted by the current tier (3 at tier 3+, else 0).
    int tierArmor(const Tank &t) const { return t.tier >= 3 ? 3 : 0; }
    // Effective armour: tier armour or the CANNON one-shot, whichever is
    // higher (they do not stack).
    int armorLayers(const Tank &t) const {
        return std::max(tierArmor(t), (m_time < t.cannonUntil) ? 1 : 0);
    }
    // The NUKE blast that QML draws: it grows out of the spot where the bonus
    // was picked up and is gone NUKE_FLASH_TIME later.
    void updateNuke(double dt);
    // The BLAST MISSILE impact circle.
    void updateBlast(double dt);
    // Pending air-support strikes: sound first, explosion 1 s later.
    void updateAirStrikes(double dt);
    void updateFloaters(double dt);
    void updateExplosions(double dt);

    // Gamepad bookkeeping. A pad is a connected device plus the player it
    // drives (-1 until it presses START).
    struct Pad {
        int index = 0;
        QString name;
        int player = -1;
    };
    const Pad *padAt(int index) const;
    int playerForPad(int index) const;
    // Claims the first free tank: the only one with a single player game,
    // player 2 before player 1 with two.
    bool joinPad(int index);
    // Is tank i in this game at all? A one-player game has no second tank.
    bool playerActive(int i) const { return i < m_playerCount; }
    // The player has used their control at least once in this game, so the
    // join hint under their tank gives way to the KEY / PAD label.
    void markJoined(int player);
    // Starts the game with the highlighted mode. `pad` is the pad that
    // confirmed, if any: it claims the tank the rules give it.
    void confirmSelect(int pad);
    // Hands every pad back to waiting and lets go of the directions it held, so
    // a new game picks its pads from scratch (START decides, nothing carries
    // over from the run before).
    void releasePads(int fromPlayer = 0);
    // Takes every actor off the board, for when the select screen comes back.
    void clearRun();

    QString m_state = "select";
    QString m_message;
    QString m_ending;
    // 1 or 2 tanks, picked on the select screen.
    int m_playerCount = 2;
    int m_menuIndex = 0;
    bool m_joined[2] = {};
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
    std::vector<Explosion> m_explosions;
    double m_freezeUntil = -1;
    // The NUKE blast, drawn by QML: where it started and how long it has been
    // running. A negative age means no blast is on screen.
    double m_nukeX = 0, m_nukeY = 0;
    double m_nukeAge = -1;
    // BLAST MISSILE impact circle.
    double m_blastX = 0, m_blastY = 0;
    double m_blastAge = -1;
    // Air support pending strikes and impact circle.
    std::vector<AirStrike> m_airStrikes;
    double m_airSupportX = 0, m_airSupportY = 0;
    double m_airSupportAge = -1;
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
    bool m_rumbleEnabled = true;
    // Debug: OLIGARCHY_TANKS_TIER=N starts the player at that tier (air support testing).
    int m_debugTier = -1;
    QTimer m_timer;
    std::mt19937 m_rng{std::random_device{}()};
};

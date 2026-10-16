#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <random>
#include <vector>

// Backend owns all game state. QML only renders and forwards key events.
// Input is abstracted per player (direction flags + fire edge) so a future
// gamepad (DualSense/Xbox via SDL/evdev) can drive the same path.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    Q_PROPERTY(QString board READ board NOTIFY frameChanged)
    Q_PROPERTY(int cols READ cols CONSTANT)
    Q_PROPERTY(int rows READ rows CONSTANT)
    Q_PROPERTY(QVariantList players READ players NOTIFY frameChanged)
    Q_PROPERTY(QVariantList enemies READ enemies NOTIFY frameChanged)
    Q_PROPERTY(QVariantList bullets READ bullets NOTIFY frameChanged)
    Q_PROPERTY(int enemiesLeft READ enemiesLeft NOTIFY frameChanged)
    Q_PROPERTY(bool soundEnabled READ soundEnabled WRITE setSoundEnabled NOTIFY soundChanged)

public:
    static constexpr int COLS = 15;
    static constexpr int ROWS = 15;
    static constexpr int TOTAL_ENEMIES = 8;

    explicit Backend(QObject *parent = nullptr);

    QString state() const { return m_state; }
    QString message() const { return m_message; }
    QString board() const;
    int cols() const { return COLS; }
    int rows() const { return ROWS; }
    QVariantList players() const;
    QVariantList enemies() const;
    QVariantList bullets() const;
    int enemiesLeft() const;
    bool soundEnabled() const { return m_soundEnabled; }
    void setSoundEnabled(bool v);

    // Movement flags from held keys. dx/dy is -1/0/1, pressed tracks key state.
    Q_INVOKABLE void setP1Move(int dx, int dy, bool pressed);
    Q_INVOKABLE void setP2Move(int dx, int dy, bool pressed);
    Q_INVOKABLE void fireP1();
    Q_INVOKABLE void fireP2();
    Q_INVOKABLE void startGame();
    Q_INVOKABLE void togglePause();

signals:
    void stateChanged();
    void frameChanged();
    void soundChanged();
    void playSound(const QString &name);

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
    struct Bullet {
        double x = 0, y = 0;
        int dir = 0;
        int owner = 2; // 0 p1, 1 p2, 2 enemy
        bool active = false;
    };

    enum Tile { Empty = 0, Brick = 1, Steel = 2, Base = 3, BaseDead = 4 };

    void setState(const QString &s, const QString &msg = {});
    void generateMap();
    void resetTanks();
    void tick();
    void updateTank(Tank &t, bool up, bool down, bool left, bool right, double dt, double speed);
    void updateEnemies(double dt);
    void updateBullets(double dt);
    void spawnEnemy();
    void fireBullet(double x, double y, int dir, int owner);
    bool solidAt(double x, double y, double size) const;
    bool hitsTank(double bx, double by, double tx, double ty) const;
    bool blockedByTank(double x, double y, double size, const void *self, bool isEnemy) const;
    int tileAt(int cx, int cy) const;
    void damageTile(int cx, int cy);
    void killPlayer(int idx);
    void checkEnd();
    int aliveEnemies() const;

    QString m_state = "menu";
    QString m_message;
    std::vector<int> m_grid;
    Tank m_p[2];
    std::vector<Enemy> m_foes;
    std::vector<Bullet> m_bullets;
    bool m_p1Up = false, m_p1Down = false, m_p1Left = false, m_p1Right = false;
    bool m_p2Up = false, m_p2Down = false, m_p2Left = false, m_p2Right = false;
    int m_spawned = 0;
    double m_spawnTimer = 0;
    bool m_soundEnabled = true;
    QTimer m_timer;
    std::mt19937 m_rng{std::random_device{}()};
};

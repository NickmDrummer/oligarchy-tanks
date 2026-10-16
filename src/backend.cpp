#include "backend.h"

#include <algorithm>

namespace {
constexpr double TANK_SIZE = 0.8;
constexpr double PLAYER_SPEED = 4.2;
constexpr double ENEMY_SPEED = 2.4;
constexpr double BULLET_SPEED = 8.0;
constexpr double FIRE_GAP = 0.35;
}

Backend::Backend(QObject *parent) : QObject(parent), m_grid(COLS * ROWS, Empty) {
    m_foes.resize(4);
    m_bullets.reserve(16);
    generateMap();
    resetTanks();
    connect(&m_timer, &QTimer::timeout, this, &Backend::tick);
    m_timer.setInterval(16);
    m_timer.start();
}

QString Backend::board() const {
    QString s;
    s.reserve(COLS * ROWS);
    for (int t : m_grid) {
        switch (t) {
        case Brick: s += 'B'; break;
        case Steel: s += 'S'; break;
        case Base: s += 'X'; break;
        case BaseDead: s += 'x'; break;
        default: s += '.'; break;
        }
    }
    return s;
}

QVariantList Backend::players() const {
    QVariantList out;
    for (int i = 0; i < 2; ++i) {
        const Tank &t = m_p[i];
        out.append(QVariantMap{
            {"x", t.x}, {"y", t.y}, {"dir", t.dir}, {"alive", t.alive},
            {"lives", t.lives}, {"score", t.score},
        });
    }
    return out;
}

QVariantList Backend::enemies() const {
    QVariantList out;
    for (const auto &e : m_foes)
        if (e.alive)
            out.append(QVariantMap{{"x", e.x}, {"y", e.y}, {"dir", e.dir}});
    return out;
}

QVariantList Backend::bullets() const {
    QVariantList out;
    for (const auto &b : m_bullets)
        if (b.active)
            out.append(QVariantMap{{"x", b.x}, {"y", b.y}, {"dir", b.dir}, {"owner", b.owner}});
    return out;
}

int Backend::enemiesLeft() const {
    int dead = TOTAL_ENEMIES - m_spawned;
    return aliveEnemies() + (dead > 0 ? dead : 0);
}

void Backend::setSoundEnabled(bool v) {
    if (v == m_soundEnabled)
        return;
    m_soundEnabled = v;
    emit soundChanged();
}

void Backend::setP1Move(int dx, int dy, bool pressed) {
    if (dx < 0) m_p1Left = pressed;
    if (dx > 0) m_p1Right = pressed;
    if (dy < 0) m_p1Up = pressed;
    if (dy > 0) m_p1Down = pressed;
}

void Backend::setP2Move(int dx, int dy, bool pressed) {
    if (dx < 0) m_p2Left = pressed;
    if (dx > 0) m_p2Right = pressed;
    if (dy < 0) m_p2Up = pressed;
    if (dy > 0) m_p2Down = pressed;
}

void Backend::fireP1() {
    if (m_state != "playing" || !m_p[0].alive || m_p[0].cooldown > 0)
        return;
    m_p[0].cooldown = FIRE_GAP;
    fireBullet(m_p[0].x, m_p[0].y, m_p[0].dir, 0);
}

void Backend::fireP2() {
    if (m_state != "playing" || !m_p[1].alive || m_p[1].cooldown > 0)
        return;
    m_p[1].cooldown = FIRE_GAP;
    fireBullet(m_p[1].x, m_p[1].y, m_p[1].dir, 1);
}

void Backend::startGame() {
    generateMap();
    resetTanks();
    m_bullets.clear();
    for (auto &e : m_foes)
        e.alive = false;
    m_spawned = 0;
    m_spawnTimer = 0;
    m_p1Up = m_p1Down = m_p1Left = m_p1Right = false;
    m_p2Up = m_p2Down = m_p2Left = m_p2Right = false;
    setState("playing");
    emit frameChanged();
}

void Backend::togglePause() {
    if (m_state == "playing")
        setState("paused", "Paused — press P to resume");
    else if (m_state == "paused")
        setState("playing");
}

void Backend::setState(const QString &s, const QString &msg) {
    if (s == m_state && msg == m_message)
        return;
    m_state = s;
    m_message = msg;
    emit stateChanged();
    emit frameChanged();
}

void Backend::generateMap() {
    std::uniform_real_distribution<double> roll(0.0, 1.0);
    m_grid.assign(COLS * ROWS, Empty);
    auto idx = [](int x, int y) { return y * COLS + x; };
    for (int y = 0; y < ROWS; ++y) {
        for (int x = 0; x < COLS; ++x) {
            // Keep spawn row and base area clear before scattering.
            if (y >= ROWS - 3 && x >= 5 && x <= 9)
                continue;
            if (y == 0 && (x == 0 || x == COLS / 2 || x == COLS - 1))
                continue;
            const double r = roll(m_rng);
            if (r < 0.22)
                m_grid[idx(x, y)] = Brick;
            else if (r < 0.25)
                m_grid[idx(x, y)] = Steel;
        }
    }
    // Base with brick guard, players beside it.
    const int bx = COLS / 2, by = ROWS - 1;
    m_grid[idx(bx, by)] = Base;
    m_grid[idx(bx - 1, by)] = Brick;
    m_grid[idx(bx + 1, by)] = Brick;
    m_grid[idx(bx - 1, by - 1)] = Brick;
    m_grid[idx(bx, by - 1)] = Brick;
    m_grid[idx(bx + 1, by - 1)] = Brick;
}

void Backend::resetTanks() {
    m_p[0] = Tank{};
    m_p[1] = Tank{};
    m_p[0].x = 3.0;
    m_p[0].y = ROWS - 2.0;
    m_p[0].dir = 0;
    m_p[1].x = COLS - 4.0;
    m_p[1].y = ROWS - 2.0;
    m_p[1].dir = 0;
}

void Backend::tick() {
    if (m_state != "playing")
        return;
    constexpr double dt = 0.016;

    for (auto &t : m_p)
        if (t.cooldown > 0)
            t.cooldown -= dt;

    updateTank(m_p[0], m_p1Up, m_p1Down, m_p1Left, m_p1Right, dt, PLAYER_SPEED);
    updateTank(m_p[1], m_p2Up, m_p2Down, m_p2Left, m_p2Right, dt, PLAYER_SPEED);

    // Respawn dead players that still have lives.
    for (int i = 0; i < 2; ++i) {
        Tank &t = m_p[i];
        if (!t.alive && t.lives > 0) {
            t.respawn -= dt;
            if (t.respawn <= 0) {
                t.alive = true;
                t.x = (i == 0 ? 3.0 : COLS - 4.0);
                t.y = ROWS - 2.0;
                t.dir = 0;
            }
        }
    }

    updateEnemies(dt);
    updateBullets(dt);
    checkEnd();
    emit frameChanged();
}

void Backend::updateTank(Tank &t, bool up, bool down, bool left, bool right, double dt, double speed) {
    if (!t.alive)
        return;
    int dir = -1;
    // Last-pressed priority is approximated by fixed order: up, down, left, right.
    if (up) dir = 0;
    if (down) dir = 2;
    if (left) dir = 3;
    if (right) dir = 1;
    if (dir < 0)
        return;
    t.dir = dir;
    double nx = t.x, ny = t.y;
    if (dir == 0) ny -= speed * dt;
    if (dir == 2) ny += speed * dt;
    if (dir == 3) nx -= speed * dt;
    if (dir == 1) nx += speed * dt;
    if (!solidAt(nx, ny, TANK_SIZE)) {
        t.x = nx;
        t.y = ny;
    }
}

void Backend::updateEnemies(double dt) {
    std::uniform_real_distribution<double> roll(0.0, 1.0);
    m_spawnTimer -= dt;
    if (m_spawned < TOTAL_ENEMIES && aliveEnemies() < 3 && m_spawnTimer <= 0) {
        spawnEnemy();
        m_spawnTimer = 2.0;
    }
    for (auto &e : m_foes) {
        if (!e.alive)
            continue;
        if (e.cooldown > 0)
            e.cooldown -= dt;
        e.think -= dt;
        double nx = e.x, ny = e.y;
        if (e.dir == 0) ny -= ENEMY_SPEED * dt;
        if (e.dir == 2) ny += ENEMY_SPEED * dt;
        if (e.dir == 3) nx -= ENEMY_SPEED * dt;
        if (e.dir == 1) nx += ENEMY_SPEED * dt;
        if (!solidAt(nx, ny, TANK_SIZE)) {
            e.x = nx;
            e.y = ny;
        } else {
            e.dir = int(roll(m_rng) * 4);
            e.think = 0;
        }
        if (e.think <= 0) {
            if (roll(m_rng) < 0.35)
                e.dir = int(roll(m_rng) * 4);
            // Drift toward the base to keep pressure on.
            if (roll(m_rng) < 0.4)
                e.dir = (e.x < COLS / 2) ? 1 : 3;
            if (roll(m_rng) < 0.5)
                e.dir = 2;
            e.think = 0.8 + roll(m_rng) * 1.5;
        }
        if (e.cooldown <= 0 && roll(m_rng) < 0.02) {
            e.cooldown = 1.1;
            fireBullet(e.x, e.y, e.dir, 2);
        }
    }
}

void Backend::updateBullets(double dt) {
    for (auto &b : m_bullets) {
        if (!b.active)
            continue;
        double nx = b.x, ny = b.y;
        if (b.dir == 0) ny -= BULLET_SPEED * dt;
        if (b.dir == 2) ny += BULLET_SPEED * dt;
        if (b.dir == 3) nx -= BULLET_SPEED * dt;
        if (b.dir == 1) nx += BULLET_SPEED * dt;

        // Out of bounds.
        if (nx < 0 || ny < 0 || nx >= COLS || ny >= ROWS) {
            b.active = false;
            continue;
        }
        const int cx = int(nx), cy = int(ny);
        const int tile = tileAt(cx, cy);
        if (tile == Brick || tile == Steel || tile == Base || tile == BaseDead) {
            if (tile == Brick)
                damageTile(cx, cy);
            if (tile == Base) {
                m_grid[cy * COLS + cx] = BaseDead;
                if (m_soundEnabled)
                    emit playSound("explode");
            } else if (m_soundEnabled && tile == Brick) {
                emit playSound("hit");
            }
            b.active = false;
            continue;
        }
        // Hit players.
        bool consumed = false;
        for (int i = 0; i < 2; ++i) {
            if (m_p[i].alive && hitsTank(nx, ny, m_p[i].x, m_p[i].y)) {
                if (b.owner == 2) {
                    killPlayer(i);
                    consumed = true;
                    break;
                }
            }
        }
        if (consumed) {
            b.active = false;
            continue;
        }
        // Hit enemies.
        for (auto &e : m_foes) {
            if (e.alive && hitsTank(nx, ny, e.x, e.y)) {
                if (b.owner < 2) {
                    e.alive = false;
                    m_p[b.owner].score += 100;
                    if (m_soundEnabled)
                        emit playSound("explode");
                    b.active = false;
                    consumed = true;
                    break;
                }
            }
        }
        if (consumed)
            continue;
        // Bullet vs bullet.
        for (auto &o : m_bullets) {
            if (&o == &b || !o.active)
                continue;
            const double dx = o.x - nx, dy = o.y - ny;
            if (dx * dx + dy * dy < 0.09) {
                o.active = false;
                b.active = false;
                break;
            }
        }
        if (!b.active)
            continue;
        b.x = nx;
        b.y = ny;
    }
    m_bullets.erase(std::remove_if(m_bullets.begin(), m_bullets.end(),
                                   [](const Bullet &b) { return !b.active; }),
                    m_bullets.end());
}

void Backend::spawnEnemy() {
    const int spots[3] = {0, COLS / 2, COLS - 1};
    for (auto &e : m_foes) {
        if (e.alive)
            continue;
        e.alive = true;
        e.x = double(spots[m_spawned % 3]);
        e.y = 0.0;
        e.dir = 2;
        e.cooldown = 1.0;
        e.think = 1.0;
        ++m_spawned;
        return;
    }
}

void Backend::fireBullet(double x, double y, int dir, int owner) {
    int live = 0;
    for (const auto &b : m_bullets)
        if (b.active && b.owner == owner)
            ++live;
    if (live >= 2)
        return;
    Bullet b;
    b.x = x;
    b.y = y;
    b.dir = dir;
    b.owner = owner;
    b.active = true;
    // Start at the barrel tip so shots do not collide with the shooter.
    if (dir == 0) b.y -= 0.6;
    if (dir == 2) b.y += 0.6;
    if (dir == 3) b.x -= 0.6;
    if (dir == 1) b.x += 0.6;
    m_bullets.push_back(b);
    if (m_soundEnabled)
        emit playSound(owner == 2 ? "eshoot" : "shoot");
}

bool Backend::solidAt(double x, double y, double size) const {
    const double h = size / 2;
    const double pts[4][2] = {{x - h, y - h}, {x + h, y - h}, {x - h, y + h}, {x + h, y + h}};
    for (auto &p : pts) {
        const int cx = int(p[0]), cy = int(p[1]);
        if (cx < 0 || cy < 0 || cx >= COLS || cy >= ROWS)
            return true;
        const int t = tileAt(cx, cy);
        if (t == Brick || t == Steel || t == Base || t == BaseDead)
            return true;
    }
    return false;
}

bool Backend::hitsTank(double bx, double by, double tx, double ty) const {
    const double dx = bx - tx, dy = by - ty;
    return dx * dx + dy * dy < 0.32;
}

int Backend::tileAt(int cx, int cy) const {
    if (cx < 0 || cy < 0 || cx >= COLS || cy >= ROWS)
        return Steel;
    return m_grid[cy * COLS + cx];
}

void Backend::damageTile(int cx, int cy) {
    m_grid[cy * COLS + cx] = Empty;
}

void Backend::killPlayer(int idx) {
    Tank &t = m_p[idx];
    t.alive = false;
    t.lives -= 1;
    t.respawn = 2.0;
    if (m_soundEnabled)
        emit playSound("explode");
}

void Backend::checkEnd() {
    bool baseDead = false;
    for (int t : m_grid)
        if (t == BaseDead)
            baseDead = true;
    if (baseDead) {
        setState("gameover", "Base destroyed — press R to retry");
        return;
    }
    const bool p1Out = !m_p[0].alive && m_p[0].lives <= 0;
    const bool p2Out = !m_p[1].alive && m_p[1].lives <= 0;
    if (p1Out && p2Out) {
        setState("gameover", "Both tanks down — press R to retry");
        return;
    }
    if (m_spawned >= TOTAL_ENEMIES && aliveEnemies() == 0) {
        const int total = m_p[0].score + m_p[1].score;
        setState("victory", QString("Sector clear! Score %1 — press R for a new map").arg(total));
    }
}

int Backend::aliveEnemies() const {
    int n = 0;
    for (const auto &e : m_foes)
        if (e.alive)
            ++n;
    return n;
}

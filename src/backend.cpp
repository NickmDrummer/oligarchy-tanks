#include "backend.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double TANK_SIZE = 0.8;
constexpr double PLAYER_SPEED = 3.62;
constexpr double ENEMY_SPEED = 2.07;
constexpr double BULLET_SPEED = 8.0;
constexpr double FIRE_GAP = 0.35;
}

Backend::Backend(QObject *parent) : QObject(parent), m_grid(COLS * ROWS, Empty) {
    m_foes.resize(6);
    m_bullets.reserve(16);
    generateMap();
    resetTanks(true);
    connect(&m_timer, &QTimer::timeout, this, &Backend::tick);
    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.start();
    m_clock.start();
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
        case Forest: s += 'F'; break;
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
            {"lives", t.lives}, {"score", t.score}, {"moving", t.moving},
            {"shield", m_time < t.shieldUntil}, {"speed", m_time < t.speedUntil}, {"cannon", m_time < t.cannonUntil},
        });
    }
    return out;
}

QVariantList Backend::enemies() const {
    QVariantList out;
    for (const auto &e : m_foes)
        if (e.alive)
            out.append(QVariantMap{{"x", e.x}, {"y", e.y}, {"dir", e.dir}, {"kind", e.kind}, {"hp", e.hp}, {"moving", e.moving}});
    return out;
}

QVariantList Backend::bullets() const {
    QVariantList out;
    for (const auto &b : m_bullets)
        if (b.active)
            out.append(QVariantMap{{"x", b.x}, {"y", b.y}, {"dir", b.dir}, {"owner", b.owner}});
    return out;
}

QVariantMap Backend::playerAt(int i, int tick) const {
    Q_UNUSED(tick);
    if (i < 0 || i > 1)
        return {};
    const Tank &t = m_p[i];
    return {{"x", t.x}, {"y", t.y}, {"dir", t.dir}, {"alive", t.alive},
            {"lives", t.lives}, {"score", t.score}, {"moving", t.moving},
            {"shield", m_time < t.shieldUntil}, {"speed", m_time < t.speedUntil}, {"cannon", m_time < t.cannonUntil}};
}

QVariantMap Backend::enemyAt(int i, int tick) const {
    Q_UNUSED(tick);
    if (i < 0 || i >= int(m_foes.size()))
        return {{"x", 0.0}, {"y", 0.0}, {"dir", 2}, {"kind", 0}, {"hp", 0}, {"moving", false}, {"alive", false}};
    const Enemy &e = m_foes[i];
    return {{"x", e.x}, {"y", e.y}, {"dir", e.dir}, {"kind", e.kind},
            {"hp", e.hp}, {"moving", e.moving}, {"alive", e.alive}};
}

QVariantMap Backend::bulletAt(int i, int tick) const {
    Q_UNUSED(tick);
    if (i < 0 || i >= int(m_bullets.size()))
        return {{"x", 0.0}, {"y", 0.0}, {"dir", 0}, {"owner", 2}, {"active", false}, {"big", false}};
    const Bullet &b = m_bullets[i];
    return {{"x", b.x}, {"y", b.y}, {"dir", b.dir}, {"owner", b.owner}, {"active", b.active}, {"big", b.big}};
}

QVariantMap Backend::floaterAt(int i, int tick) const {
    Q_UNUSED(tick);
    if (i < 0 || i >= int(m_floaters.size()))
        return {{"x", 0.0}, {"y", 0.0}, {"text", QString()}, {"age", 0.0}, {"active", false}};
    const Floater &f = m_floaters[i];
    return {{"x", f.x}, {"y", f.y}, {"text", QString("+%1").arg(f.score)}, {"age", f.age}, {"active", true}};
}

QVariantMap Backend::pickupAt(int tick) const {
    Q_UNUSED(tick);
    if (!m_pickup.active)
        return {{"x", 0.0}, {"y", 0.0}, {"kind", 0}, {"age", 0.0}, {"active", false}};
    return {{"x", m_pickup.x}, {"y", m_pickup.y}, {"kind", m_pickup.kind}, {"age", m_pickup.age}, {"active", true}};
}

QVariantList Backend::floaters() const {
    QVariantList out;
    for (const auto &f : m_floaters)
        out.append(QVariantMap{{"x", f.x}, {"y", f.y}, {"text", QString("+%1").arg(f.score)}, {"age", f.age}});
    return out;
}

QVariantList Backend::pickups() const {
    QVariantList out;
    if (m_pickup.active)
        out.append(QVariantMap{{"x", m_pickup.x}, {"y", m_pickup.y}, {"kind", m_pickup.kind}, {"age", m_pickup.age}});
    return out;
}

int Backend::enemiesLeft() const {
    int dead = totalForLevel() - m_spawned;
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
    fireBullet(m_p[0].x, m_p[0].y, m_p[0].dir, 0, BULLET_SPEED);
}

void Backend::fireP2() {
    if (m_state != "playing" || !m_p[1].alive || m_p[1].cooldown > 0)
        return;
    m_p[1].cooldown = FIRE_GAP;
    fireBullet(m_p[1].x, m_p[1].y, m_p[1].dir, 1, BULLET_SPEED);
}

void Backend::startGame() {
    m_level = 1;
    m_freezeUntil = -1;
    generateMap();
    resetTanks(true);
    m_bullets.clear();
    m_floaters.clear();
    m_pickup = Pickup{};
    m_fortified.clear();
    m_fortifyUntil = -1;
    m_freezeUntil = -1;
    m_time = 0;
    for (auto &e : m_foes)
        e.alive = false;
    m_spawned = 0;
    m_spawnTimer = 0;
    m_p1Up = m_p1Down = m_p1Left = m_p1Right = false;
    m_p2Up = m_p2Down = m_p2Left = m_p2Right = false;
    setState("playing");
    emit frameChanged();
}

void Backend::nextLevel() {
    ++m_level;
    m_freezeUntil = -1;
    generateMap();
    resetTanks(false);
    m_bullets.clear();
    m_floaters.clear();
    m_pickup = Pickup{};
    m_fortified.clear();
    m_fortifyUntil = -1;
    m_spawned = 0;
    m_spawnTimer = 1.0;
    m_p1Up = m_p1Down = m_p1Left = m_p1Right = false;
    m_p2Up = m_p2Down = m_p2Left = m_p2Right = false;
    setState("playing", QString("Sector %1 — specials are faster and tougher").arg(m_level));
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
    auto clear = [&](int x, int y) {
        if (x >= 0 && y >= 0 && x < COLS && y < ROWS)
            m_grid[idx(x, y)] = Empty;
    };
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
    // Forest copses: cover that hides tanks, never solid.
    std::uniform_int_distribution<int> cell(0, COLS - 1);
    std::uniform_int_distribution<int> row(0, ROWS - 1);
    for (int blob = 0; blob < 5; ++blob) {
        const int fx = cell(m_rng), fy = row(m_rng);
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const int x = fx + dx, y = fy + dy;
                if (x < 0 || y < 0 || x >= COLS || y >= ROWS)
                    continue;
                if (roll(m_rng) < 0.5)
                    m_grid[idx(x, y)] = Forest;
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
    // Guarantee free spawn pads for both players (3x2 cells each).
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 0; ++dy) {
            clear(3 + dx, ROWS - 2 + dy);
            clear(COLS - 4 + dx, ROWS - 2 + dy);
        }
    // Guarantee free enemy entry pads at the top row.
    for (int sx : {0, COLS / 2, COLS - 1})
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = 0; dy <= 1; ++dy)
                clear(sx + dx, dy);
}

void Backend::resetTanks(bool fullStats) {
    const int lives0 = fullStats ? 3 : m_p[0].lives;
    const int lives1 = fullStats ? 3 : m_p[1].lives;
    const int score0 = fullStats ? 0 : m_p[0].score;
    const int score1 = fullStats ? 0 : m_p[1].score;
    m_p[0] = Tank{};
    m_p[1] = Tank{};
    m_p[0].lives = lives0;
    m_p[1].lives = lives1;
    m_p[0].score = score0;
    m_p[1].score = score1;
    // Cell centers, so tanks never start straddling two tiles.
    m_p[0].x = 3.5;
    m_p[0].y = ROWS - 1.5;
    m_p[0].dir = 0;
    m_p[1].x = COLS - 3.5;
    m_p[1].y = ROWS - 1.5;
    m_p[1].dir = 0;
}

void Backend::tick() {
    if (m_state != "playing")
        return;
    // Real elapsed time, so the game runs at the same speed even when the
    // machine cannot sustain 60 frames per second. Clamped to avoid jumps.
    double dt = m_clock.restart() / 1000.0;
    if (dt <= 0)
        dt = 0.016;
    if (dt > 0.05)
        dt = 0.05;
    m_time += dt;
    ++m_frameTick;

    for (auto &t : m_p)
        if (t.cooldown > 0)
            t.cooldown -= dt;

    for (int i = 0; i < 2; ++i) {
        const double boost = (m_time < m_p[i].speedUntil) ? 1.35 : 1.0;
        if (i == 0)
            updateTank(m_p[0], m_p1Up, m_p1Down, m_p1Left, m_p1Right, dt, PLAYER_SPEED * boost);
        else
            updateTank(m_p[1], m_p2Up, m_p2Down, m_p2Left, m_p2Right, dt, PLAYER_SPEED * boost);
    }

    // Respawn dead players that still have lives.
    for (int i = 0; i < 2; ++i) {
        Tank &t = m_p[i];
        if (!t.alive && t.lives > 0) {
            t.respawn -= dt;
            if (t.respawn <= 0) {
                t.alive = true;
                t.x = (i == 0 ? 3.5 : COLS - 3.5);
                t.y = ROWS - 1.5;
                t.dir = 0;
            }
        }
    }

    updateEnemies(dt);
    updateBullets(dt);
    updatePickups(dt);
    updateFloaters(dt);
    // Fortified base guard crumbles back to brick when the timer runs out.
    if (m_fortifyUntil > 0 && m_time > m_fortifyUntil) {
        for (int i : m_fortified)
            if (m_grid[i] == Steel)
                m_grid[i] = Brick;
        m_fortified.clear();
        m_fortifyUntil = -1;
    }
    checkEnd();
    emit frameChanged();
}

void Backend::updateTank(Tank &t, bool up, bool down, bool left, bool right, double dt, double speed) {
    if (!t.alive) {
        t.moving = false;
        return;
    }
    // Keep going the current way while its key is held, so diagonal
    // presses do not flip the tank back and forth.
    int dir = t.dir;
    auto held = [&](int d) {
        return (d == 0 && up) || (d == 2 && down) || (d == 3 && left) || (d == 1 && right);
    };
    if (!held(dir)) {
        dir = -1;
        if (up) dir = 0;
        if (left) dir = 3;
        if (down) dir = 2;
        if (right) dir = 1;
        if (dir < 0) {
            t.moving = false;
            return;
        }
    }
    t.dir = dir;
    // Lane assist: drift toward the nearest cell center on the cross axis,
    // so tanks slide through 1-wide gaps instead of snagging on corners.
    const double laneRate = 6.0 * dt;
    if (dir == 0 || dir == 2) {
        const double lane = std::floor(t.x) + 0.5;
        const double d = lane - t.x;
        if (std::abs(d) < 0.3)
            t.x += std::clamp(d, -laneRate, laneRate);
    } else {
        const double lane = std::floor(t.y) + 0.5;
        const double d = lane - t.y;
        if (std::abs(d) < 0.3)
            t.y += std::clamp(d, -laneRate, laneRate);
    }
    double nx = t.x, ny = t.y;
    if (dir == 0) ny -= speed * dt;
    if (dir == 2) ny += speed * dt;
    if (dir == 3) nx -= speed * dt;
    if (dir == 1) nx += speed * dt;
    if (!solidAt(nx, ny, TANK_SIZE) && !blockedByTank(nx, ny, TANK_SIZE, &t, false)) {
        t.x = nx;
        t.y = ny;
        t.moving = true;
    } else {
        t.moving = false;
    }
}

void Backend::updateEnemies(double dt) {
    std::uniform_real_distribution<double> roll(0.0, 1.0);
    const int maxConcurrent = std::min(3 + (m_level - 1), 5);
    m_spawnTimer -= dt;
    if (m_spawned < totalForLevel() && aliveEnemies() < maxConcurrent && m_spawnTimer <= 0) {
        spawnEnemy();
        m_spawnTimer = 2.0;
    }
    for (auto &e : m_foes) {
        if (!e.alive) {
            e.moving = false;
            continue;
        }
        // Freeze clock: enemy armor holds still, no aiming, no firing.
        if (m_time < m_freezeUntil) {
            e.moving = false;
            continue;
        }
        if (e.cooldown > 0)
            e.cooldown -= dt;
        e.think -= dt;
        // Only specials scale with the sector: basics stay baseline.
        const double mult = enemySpeedMult();
        const double speed = (e.kind == 0) ? ENEMY_SPEED : (e.kind == 1) ? ENEMY_SPEED * 1.35 * mult : ENEMY_SPEED * mult;
        double nx = e.x, ny = e.y;
        if (e.dir == 0) ny -= speed * dt;
        if (e.dir == 2) ny += speed * dt;
        if (e.dir == 3) nx -= speed * dt;
        if (e.dir == 1) nx += speed * dt;
        if (!solidAt(nx, ny, TANK_SIZE) && !blockedByTank(nx, ny, TANK_SIZE, &e, true)) {
            e.x = nx;
            e.y = ny;
            e.moving = true;
        } else {
            e.moving = false;
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
            e.cooldown = (e.kind == 1) ? 0.8 : 1.1;
            const double bs = (e.kind == 0) ? BULLET_SPEED : BULLET_SPEED * enemySpeedMult();
            fireBullet(e.x, e.y, e.dir, 2, bs);
        }
    }
}

void Backend::updateBullets(double dt) {
    for (auto &b : m_bullets) {
        if (!b.active)
            continue;
        double nx = b.x, ny = b.y;
        if (b.dir == 0) ny -= b.speed * dt;
        if (b.dir == 2) ny += b.speed * dt;
        if (b.dir == 3) nx -= b.speed * dt;
        if (b.dir == 1) nx += b.speed * dt;

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
        // Hit players. A shield blocks the shot with no damage.
        bool consumed = false;
        for (int i = 0; i < 2; ++i) {
            if (m_p[i].alive && hitsTank(nx, ny, m_p[i].x, m_p[i].y)) {
                if (b.owner == 2) {
                    if (m_time < m_p[i].shieldUntil) {
                        b.active = false;
                        consumed = true;
                        break;
                    }
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
        // Hit enemies. A cannon shell cracks any armor in one hit.
        for (auto &e : m_foes) {
            if (e.alive && hitsTank(nx, ny, e.x, e.y)) {
                if (b.owner < 2) {
                    e.hp -= b.big ? 99 : 1;
                    if (e.hp <= 0) {
                        e.alive = false;
                        const int pts = (e.kind == 2) ? 300 : (e.kind == 1) ? 200 : 100;
                        m_p[b.owner].score += pts;
                        m_floaters.push_back(Floater{e.x, e.y, pts, 0});
                        if (m_floaters.size() > 8)
                            m_floaters.erase(m_floaters.begin());
                        // Green runners carry the bonus: one drops where it dies.
                        if (e.kind == 1 && !m_pickup.active)
                            spawnPickupAt(e.x, e.y);
                        if (m_soundEnabled)
                            emit playSound("explode");
                    } else if (m_soundEnabled) {
                        emit playSound("hit");
                    }
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
    // First wave is always gray basics; specials join from the 5th tank on.
    // Higher sectors field more total tanks via totalForLevel().
    const int spots[3] = {0, COLS / 2, COLS - 1};
    for (auto &e : m_foes) {
        if (e.alive)
            continue;
        e.alive = true;
        e.x = double(spots[m_spawned % 3]) + 0.5;
        e.y = 0.5;
        e.dir = 2;
        e.cooldown = 1.0;
        e.think = 1.0;
        e.kind = (m_spawned < 4) ? 0 : ((m_spawned % 2 == 0) ? 1 : 2);
        e.hp = (e.kind == 2) ? 2 + (m_level >= 3 ? 1 : 0) + (m_level >= 5 ? 1 : 0) : 1;
        ++m_spawned;
        return;
    }
}

void Backend::fireBullet(double x, double y, int dir, int owner, double speed) {
    int live = 0;
    for (const auto &b : m_bullets)
        if (b.active && b.owner == owner)
            ++live;
    if (live >= 2)
        return;
    double bx = x, by = y;
    // Start at the barrel tip so shots do not collide with the shooter.
    if (dir == 0) by -= 0.6;
    if (dir == 2) by += 0.6;
    if (dir == 3) bx -= 0.6;
    if (dir == 1) bx += 0.6;
    // Never spawn outside the arena or inside a wall; pull back to center.
    if (bx < 0.15 || by < 0.15 || bx >= COLS - 0.15 || by >= ROWS - 0.15) {
        bx = x;
        by = y;
    }
    if (solidAt(bx, by, 0.25)) {
        bx = x;
        by = y;
    }
    Bullet b;
    b.x = bx;
    b.y = by;
    b.dir = dir;
    b.owner = owner;
    b.active = true;
    b.speed = speed;
    b.big = (owner < 2 && m_time < m_p[owner].cannonUntil);
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

// Tanks block each other, so allies and enemies can no longer drive
// or phase through one another.
bool Backend::blockedByTank(double x, double y, double size, const void *self, bool isEnemy) const {
    const double minDist = size * 0.9;
    auto blocks = [&](double tx, double ty, const void *other) {
        if (other == self)
            return false;
        return std::abs(tx - x) < minDist && std::abs(ty - y) < minDist;
    };
    for (int i = 0; i < 2; ++i) {
        if (m_p[i].alive && blocks(m_p[i].x, m_p[i].y, &m_p[i]))
            return true;
    }
    for (const auto &e : m_foes) {
        if (!e.alive)
            continue;
        const void *other = &e;
        if (other == self)
            continue;
        if (isEnemy) {
            // Enemies also collide with each other.
            if (std::abs(e.x - x) < minDist && std::abs(e.y - y) < minDist)
                return true;
        } else if (blocks(e.x, e.y, other)) {
            return true;
        }
    }
    (void)isEnemy;
    return false;
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
    if (m_spawned >= totalForLevel() && aliveEnemies() == 0) {
        const int total = m_p[0].score + m_p[1].score;
        setState("victory", QString("Sector %1 clear! Score %2 — press R for sector %3").arg(m_level).arg(total).arg(m_level + 1));
    }
}

int Backend::aliveEnemies() const {
    int n = 0;
    for (const auto &e : m_foes)
        if (e.alive)
            ++n;
    return n;
}

std::vector<int> Backend::baseGuard() const {
    const int bx = COLS / 2, by = ROWS - 1;
    return {by * COLS + bx - 1, by * COLS + bx + 1, (by - 1) * COLS + bx - 1,
            (by - 1) * COLS + bx, (by - 1) * COLS + bx + 1};
}

// Bonuses only drop from green runners, at the spot where they die.
void Backend::spawnPickupAt(double x, double y) {
    std::uniform_int_distribution<int> kind(0, 6);
    const int cx = int(x), cy = int(y);
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            const int px = cx + dx, py = cy + dy;
            if (px < 0 || py < 0 || px >= COLS || py >= ROWS)
                continue;
            const int tile = m_grid[py * COLS + px];
            if (tile != Empty && tile != Forest)
                continue;
            m_pickup = Pickup{double(px) + 0.5, double(py) + 0.5, kind(m_rng), 0, true};
            return;
        }
}

void Backend::applyPickup(int player) {
    Tank &t = m_p[player];
    switch (m_pickup.kind) {
    case Speed: t.speedUntil = m_time + 20; break;
    case Shield: t.shieldUntil = m_time + 10; break;
    case Fortify:
        m_fortified.clear();
        for (int i : baseGuard())
            if (m_grid[i] == Brick) {
                m_grid[i] = Steel;
                m_fortified.push_back(i);
            }
        m_fortifyUntil = m_time + 25;
        break;
    case Life: t.lives = std::min(t.lives + 1, 5); break;
    case Grenade: {
        for (auto &e : m_foes) {
            if (!e.alive)
                continue;
            e.alive = false;
            const int pts = (e.kind == 2) ? 300 : (e.kind == 1) ? 200 : 100;
            t.score += pts;
            m_floaters.push_back(Floater{e.x, e.y, pts, 0});
        }
        if (m_floaters.size() > 8)
            m_floaters.erase(m_floaters.begin(), m_floaters.begin() + (m_floaters.size() - 8));
        if (m_soundEnabled)
            emit playSound("nuke");
        break;
    }
    case Cannon: t.cannonUntil = m_time + 20; break;
    case Freeze:
        m_freezeUntil = m_time + 8;
        if (m_soundEnabled)
            emit playSound("power");
        break;
    }
    if (m_pickup.kind != Freeze && m_pickup.kind != Grenade && m_soundEnabled)
        emit playSound("power");
}

void Backend::updatePickups(double dt) {
    if (!m_pickup.active)
        return;
    m_pickup.age += dt;
    if (m_pickup.age > 15) {
        m_pickup.active = false;
        return;
    }
    for (int i = 0; i < 2; ++i) {
        if (!m_p[i].alive)
            continue;
        if (std::abs(m_p[i].x - m_pickup.x) < 0.7 && std::abs(m_p[i].y - m_pickup.y) < 0.7) {
            applyPickup(i);
            m_pickup.active = false;
            return;
        }
    }
}

void Backend::updateFloaters(double dt) {
    for (auto &f : m_floaters)
        f.age += dt;
    m_floaters.erase(std::remove_if(m_floaters.begin(), m_floaters.end(),
                                    [](const Floater &f) { return f.age > 1.2; }),
                     m_floaters.end());
}

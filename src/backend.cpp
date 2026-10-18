#include "backend.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
constexpr double TANK_SIZE = 0.8;
constexpr double PLAYER_SPEED = 3.62;
constexpr double ENEMY_SPEED = 2.07;
constexpr double BULLET_SPEED = 8.0;
constexpr double FIRE_GAP = 0.35;
// A shot never advances more than this in one frame, so it cannot jump
// clean over the quarter-cell gap of a damaged brick tile.
constexpr double BULLET_MAX_STEP = 0.2;
// A brick tile that has lost fewer than half its strips still blocks as a
// whole block. From the halfway mark on, only the masonry that is left
// blocks, so tanks roll forward to the 50% line and shots fly through the
// hole they opened.
constexpr int BRICK_BLOCKS_BELOW = 2;

int goneStrips(unsigned m) {
    int n = 0;
    for (int i = 0; i < 8; ++i)
        if ((m & (1u << i)) == 0)
            ++n;
    return n;
}
}

Backend::Backend(QObject *parent) : QObject(parent), m_grid(COLS * ROWS, Empty) {
    m_foes.resize(6);
    m_bullets.reserve(16);
    // Fixed seed for deterministic tests via TANKS_SEED.
    if (const char *seed = std::getenv("TANKS_SEED"))
        m_rng.seed(unsigned(std::atoi(seed)));
    generateMap();
    resetTanks(true);
    // The select screen is up, so no tank is on the board yet: startGame()
    // brings them in once a mode is picked.
    m_p[0].alive = m_p[1].alive = false;
    connect(&m_timer, &QTimer::timeout, this, &Backend::tick);
    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.start();
    m_clock.start();
}

QString Backend::board() const {
    if (m_boardCacheValid)
        return m_boardCache;
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
    m_boardCache = s;
    m_boardCacheValid = true;
    return s;
}

// Board mutations funnel through here: drop the cached string, bump the
// revision QML hands back to its tile queries, and notify.
void Backend::bumpBoard() {
    m_boardCacheValid = false;
    ++m_boardRev;
    emit boardChanged();
}

int Backend::tileKind(int x, int y, int rev) const {
    Q_UNUSED(rev);
    if (x < 0 || y < 0 || x >= COLS || y >= ROWS)
        return Empty;
    return m_grid[y * COLS + x];
}

bool Backend::brickStripGone(int x, int y, int strip, int rev) const {
    Q_UNUSED(rev);
    if (x < 0 || y < 0 || x >= COLS || y >= ROWS || strip < 0 || strip > 7)
        return false;
    if (m_grid[y * COLS + x] != Brick)
        return false;
    return (m_brickQ[y * COLS + x] & (1u << strip)) == 0;
}

QVariantList Backend::players() const {
    QVariantList out;
    for (int i = 0; i < 2; ++i) {
        const Tank &t = m_p[i];
        out.append(QVariantMap{
            {"x", t.x}, {"y", t.y}, {"dir", t.dir}, {"alive", t.alive},
            {"lives", t.lives}, {"score", t.score}, {"moving", t.moving},
            {"shield", m_time < t.shieldUntil}, {"speed", m_time < t.speedUntil}, {"cannon", m_time < t.cannonUntil},
            {"active", playerActive(i)}, {"joined", m_joined[i]},
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
            {"shield", m_time < t.shieldUntil}, {"speed", m_time < t.speedUntil}, {"cannon", m_time < t.cannonUntil},
            {"active", playerActive(i)}, {"joined", m_joined[i]}};
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

QVariantList Backend::explosions() const {
    QVariantList out;
    for (const auto &e : m_explosions)
        out.append(QVariantMap{{"x", e.x}, {"y", e.y}, {"age", e.age}});
    return out;
}

QVariantMap Backend::explosionAt(int i, int tick) const {
    Q_UNUSED(tick);
    if (i < 0 || i >= int(m_explosions.size()))
        return {{"x", 0.0}, {"y", 0.0}, {"age", 0.0}, {"active", false}};
    const Explosion &e = m_explosions[i];
    return {{"x", e.x}, {"y", e.y}, {"age", e.age}, {"active", true}};
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
    // QML forwards every key blindly: before the mode is picked those same keys
    // drive the select screen, which is why menuMove() is reachable from here.
    if (m_state == "select") {
        if (pressed)
            menuMove(dx != 0 ? dx : dy);
        return;
    }
    if (pressed && m_state == "playing")
        markJoined(0);
    if (dx < 0) m_p1Left = pressed;
    if (dx > 0) m_p1Right = pressed;
    if (dy < 0) m_p1Up = pressed;
    if (dy > 0) m_p1Down = pressed;
}

void Backend::setP2Move(int dx, int dy, bool pressed) {
    if (m_state == "select") {
        if (pressed)
            menuMove(dx != 0 ? dx : dy);
        return;
    }
    if (pressed && m_state == "playing")
        markJoined(1);
    if (dx < 0) m_p2Left = pressed;
    if (dx > 0) m_p2Right = pressed;
    if (dy < 0) m_p2Up = pressed;
    if (dy > 0) m_p2Down = pressed;
}

void Backend::fireP1() {
    // Space confirms the select screen and carries on past a game over, like
    // Enter below and START on a pad.
    if (m_state == "select") {
        menuConfirm();
        return;
    }
    if (m_state == "gameover") {
        toSelect();
        return;
    }
    if (m_state == "playing")
        markJoined(0);
    if (m_state != "playing" || !m_p[0].alive || m_p[0].cooldown > 0)
        return;
    m_p[0].cooldown = FIRE_GAP;
    fireBullet(m_p[0].x, m_p[0].y, m_p[0].dir, 0, BULLET_SPEED);
}

void Backend::fireP2() {
    if (m_state == "select") {
        menuConfirm();
        return;
    }
    if (m_state == "gameover") {
        toSelect();
        return;
    }
    if (m_state == "playing")
        markJoined(1);
    if (m_state != "playing" || !m_p[1].alive || m_p[1].cooldown > 0)
        return;
    m_p[1].cooldown = FIRE_GAP;
    fireBullet(m_p[1].x, m_p[1].y, m_p[1].dir, 1, BULLET_SPEED);
}

// --- Gamepads ----------------------------------------------------------------

QVariantList Backend::pads() const {
    QVariantList out;
    for (const Pad &p : m_pads) {
        QVariantMap entry;
        entry["index"] = p.index;
        entry["name"] = p.name;
        entry["player"] = p.player;
        out.append(entry);
    }
    return out;
}

const Backend::Pad *Backend::padAt(int index) const {
    for (const Pad &p : m_pads)
        if (p.index == index)
            return &p;
    return nullptr;
}

int Backend::playerForPad(int index) const {
    const Pad *p = padAt(index);
    return p ? p->player : -1;
}

void Backend::padConnected(int pad, const QString &name) {
    if (padAt(pad))
        return;
    Pad p;
    p.index = pad;
    p.name = name;
    m_pads.push_back(p);
    emit padsChanged();
}

void Backend::padDisconnected(int pad) {
    const int player = playerForPad(pad);
    for (auto it = m_pads.begin(); it != m_pads.end(); ++it) {
        if (it->index != pad)
            continue;
        m_pads.erase(it);
        break;
    }
    // The slot goes back to the keyboard and nothing stays stuck down.
    if (player >= 0)
        for (bool &held : m_padHeld[player])
            held = false;
    emit padsChanged();
}

// START on a pad: join if it has no player yet, otherwise do what P and R do.
void Backend::padStart(int pad) {
    // On the select screen START picks the highlighted mode, exactly like Space
    // and Enter on the keyboard.
    if (m_state == "select") {
        confirmSelect(pad);
        return;
    }
    // After a game over START goes back to the select screen, whatever pad it
    // came from: that is how a pad-only player starts the next run.
    if (m_state == "gameover") {
        toSelect();
        return;
    }
    // An unassigned pad joins on its first START: the only tank with one player,
    // the second tank with two. With no tank left to claim it leaves the game
    // alone.
    const bool joining = playerForPad(pad) < 0;
    if (joining && !joinPad(pad))
        return;
    // On victory START moves on. A pad that has just joined mid-game must not
    // pause it, so only a pad that already had a tank toggles the pause.
    if (m_state == "victory")
        nextLevel();
    else if (!joining)
        togglePause();
}

bool Backend::joinPad(int pad) {
    const Pad *const self = padAt(pad);
    if (!self || self->player >= 0)
        return false;
    // One pad, one human on it: with two tanks the keyboard keeps player 1, so
    // the first pad takes player 2 and a second pad takes player 1. With a
    // single tank the pad takes that one.
    static const int twoPlayers[2] = {1, 0};
    static const int onePlayer[1] = {0};
    const int *const order = m_playerCount == 1 ? onePlayer : twoPlayers;
    const int claimable = m_playerCount == 1 ? 1 : 2;
    for (int k = 0; k < claimable; ++k) {
        const int slot = order[k];
        bool taken = false;
        for (const Pad &other : m_pads)
            if (other.player == slot)
                taken = true;
        if (taken)
            continue;
        for (Pad &p : m_pads)
            if (p.index == pad)
                p.player = slot;
        // A pad claiming a tank is that player playing it: the join hint under
        // the tank gives way to the PAD label.
        m_joined[slot] = true;
        emit padsChanged();
        emit frameChanged();
        return true;
    }
    return false;
}

void Backend::padMove(int pad, int dx, int dy, bool pressed) {
    // On the select screen no tank is driven, whatever the pad's slot: the stick
    // and the d-pad move the highlight, so a pad that already played a run can
    // still pick the next mode.
    if (m_state == "select") {
        if (pressed)
            menuMove(dx != 0 ? dx : dy);
        return;
    }
    const int player = playerForPad(pad);
    if (player < 0)
        return;
    if (pressed && m_state == "playing")
        markJoined(player);
    if (dy < 0)
        m_padHeld[player][0] = pressed;
    if (dx > 0)
        m_padHeld[player][1] = pressed;
    if (dy > 0)
        m_padHeld[player][2] = pressed;
    if (dx < 0)
        m_padHeld[player][3] = pressed;
}

void Backend::padFire(int pad) {
    if (m_state == "select") {
        // Square / X is a confirm too, like START.
        confirmSelect(pad);
        return;
    }
    if (m_state == "gameover") {
        // Any pad carries on past the game over screen, joined or not.
        toSelect();
        return;
    }
    const int player = playerForPad(pad);
    if (player == 0)
        fireP1();
    else if (player == 1)
        fireP2();
}

void Backend::padSelect(int pad) {
    if (playerForPad(pad) >= 0)
        emit helpRequested();
}

void Backend::startGame() {
    m_level = 1;
    m_freezeUntil = -1;
    generateMap();
    resetTanks(true);
    m_bullets.clear();
    m_floaters.clear();
    m_explosions.clear();
    m_pickup = Pickup{};
    m_baseGuardUntil = -1;
    m_baseGuard.clear();
    m_freezeUntil = -1;
    m_time = 0;
    for (auto &e : m_foes)
        e.alive = false;
    m_spawned = 0;
    m_spawnTimer = 0;
    m_p1Up = m_p1Down = m_p1Left = m_p1Right = false;
    m_p2Up = m_p2Down = m_p2Left = m_p2Right = false;
    m_joined[0] = m_joined[1] = false;
    m_ending.clear();
    // Grace period: a freshly spawned tank is shielded so it is not killed
    // instantly on spawn or respawn.
    for (int i = 0; i < m_playerCount; ++i)
        m_p[i].shieldUntil = m_time + 3.0;
    // A pad only drives a tank that exists: starting a one-player game drops a
    // pad that held the second tank (and lets go of its directions).
    releasePads(m_playerCount);
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
    m_explosions.clear();
    m_pickup = Pickup{};
    m_baseGuardUntil = -1;
    m_baseGuard.clear();
    m_spawned = 0;
    m_spawnTimer = 1.0;
    m_p1Up = m_p1Down = m_p1Left = m_p1Right = false;
    m_p2Up = m_p2Down = m_p2Left = m_p2Right = false;
    setState("playing", QString("Sector %1 — specials are faster and tougher").arg(m_level));
}

void Backend::togglePause() {
    if (m_state == "playing") {
        // The keyboard is always live, but the hint names the input the player
        // is actually using, so a pad holder is not told to find P.
        if (m_pads.empty())
            setState("paused", "Paused — press P to resume");
        else
            setState("paused", "Paused — press START to resume");
    } else if (m_state == "paused")
        setState("playing");
}

void Backend::menuMove(int delta) {
    if (m_state != "select" || delta == 0)
        return;
    // Two options in a row: wrap, so the highlight never gets stuck at an end.
    m_menuIndex = (m_menuIndex + (delta > 0 ? 1 : -1) + 2) % 2;
    emit menuChanged();
}

void Backend::menuConfirm() {
    confirmSelect(-1);
}

void Backend::menuChoose(int mode) {
    if (m_state != "select")
        return;
    m_menuIndex = mode == 0 ? 0 : 1;
    emit menuChanged();
    confirmSelect(-1);
}

void Backend::confirmSelect(int pad) {
    if (m_state != "select")
        return;
    m_playerCount = m_menuIndex == 0 ? 1 : 2;
    emit playerCountChanged();
    // A new game hands every pad back to waiting: which tank a pad gets is
    // decided by START and the mode, never carried over from the run before.
    // (Without this a pad that played a one-player game stayed on player 1 even
    // after switching to two players.)
    releasePads();
    startGame();
    // A pad that confirmed takes the tank the rules give it: the only one with
    // a single player, the second one with two. The keyboard keeps player 1.
    if (pad >= 0)
        joinPad(pad);
}

void Backend::toSelect() {
    if (m_state == "select")
        return;
    m_ending.clear();
    clearRun();
    setState("select");
}

void Backend::releasePads(int fromPlayer) {
    bool changed = false;
    for (Pad &p : m_pads) {
        if (p.player < fromPlayer)
            continue;
        // Let go of whatever the pad was holding, or the tank would keep
        // rolling after the pad is taken away from it.
        for (bool &held : m_padHeld[p.player])
            held = false;
        p.player = -1;
        changed = true;
    }
    if (changed)
        emit padsChanged();
}

void Backend::clearRun() {
    // The select screen sits over the board, so nothing may be left on it.
    m_p[0].alive = m_p[1].alive = false;
    for (auto &e : m_foes)
        e.alive = false;
    m_bullets.clear();
    m_floaters.clear();
    m_explosions.clear();
    m_pickup = Pickup{};
    m_spawned = 0;
    m_spawnTimer = 0;
    m_freezeUntil = -1;
    m_baseGuardUntil = -1;
    m_baseGuard.clear();
}

void Backend::markJoined(int player) {
    if (player < 0 || player > 1 || m_joined[player])
        return;
    // The HUD reads this per frame, so tell QML now: the join hint under the
    // tank gives way to the KEY / PAD label without waiting for the next frame.
    m_joined[player] = true;
    emit frameChanged();
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
    // Guarantee free enemy entry pads at the top row. Three rows deep: two
    // tanks that spawn in the same lane are pushed apart by separateTanks(),
    // and a one-cell pad has nowhere to push them but into the masonry.
    for (int sx : {0, COLS / 2, COLS - 1})
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = 0; dy <= 2; ++dy)
                clear(sx + dx, dy);
    // And make sure each lane really reaches the board: a scatter can seal one
    // off, which would leave an enemy pacing a three-cell pocket at the top.
    openSpawnLanes();
    // Every surviving brick tile starts whole; pads cleared above stay empty.
    m_brickQ.assign(COLS * ROWS, 0);
    for (int i = 0; i < COLS * ROWS; ++i)
        if (m_grid[i] == Brick)
            m_brickQ[i] = 0xFF;
    bumpBoard();
}

// An enemy walks, it never digs, so a spawn lane sealed by masonry would trap
// it at the top of the board for the whole stage. The scatter can seal one
// (a brick in the lane with brick across its sides), so each lane is checked
// and, only when it really is cut off, carved down until it joins the board.
void Backend::openSpawnLanes() {
    auto reachesMiddle = [&](int sx, int sy) {
        if (m_grid[sy * COLS + sx] == Brick || m_grid[sy * COLS + sx] == Steel)
            return false;
        std::vector<char> seen(COLS * ROWS, 0);
        std::vector<int> stack;
        seen[sy * COLS + sx] = 1;
        stack.push_back(sy * COLS + sx);
        while (!stack.empty()) {
            const int at = stack.back();
            stack.pop_back();
            const int x = at % COLS, y = at / COLS;
            if (y >= 6)
                return true;
            const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
            for (int k = 0; k < 4; ++k) {
                const int nx = x + dx[k], ny = y + dy[k];
                if (nx < 0 || ny < 0 || nx >= COLS || ny >= ROWS)
                    continue;
                if (seen[ny * COLS + nx])
                    continue;
                const int t = m_grid[ny * COLS + nx];
                if (t == Brick || t == Steel || t == Base)
                    continue;
                seen[ny * COLS + nx] = 1;
                stack.push_back(ny * COLS + nx);
            }
        }
        return false;
    };
    for (const int x : {0, COLS / 2, COLS - 1}) {
        for (int y = 0; y <= 7 && !reachesMiddle(x, 0); ++y) {
            const int t = m_grid[y * COLS + x];
            if (t == Brick || t == Steel)
                m_grid[y * COLS + x] = Empty;
        }
    }
}

void Backend::resetTanks(bool fullStats) {
    const int lives0 = fullStats ? 3 : m_p[0].lives;
    const int lives1 = fullStats ? 3 : m_p[1].lives;
    const int score0 = fullStats ? 0 : m_p[0].score;
    const int score1 = fullStats ? 0 : m_p[1].score;
    m_p[0] = Tank{};
    m_p[1] = Tank{};
    m_p[0].lives = std::max(0, lives0);
    m_p[1].lives = std::max(0, lives1);
    m_p[0].score = score0;
    m_p[1].score = score1;
    // Cell centers, so tanks never start straddling two tiles.
    m_p[0].x = 3.5;
    m_p[0].y = ROWS - 1.5;
    m_p[0].dir = 0;
    m_p[1].x = COLS - 3.5;
    m_p[1].y = ROWS - 1.5;
    m_p[1].dir = 0;
    // A player that is not in this game has no tank, no lives and no score, so
    // a one-player run cannot be kept alive by a parked second tank (the bug
    // the select screen exists to fix). A player in the game who is out of
    // lives stays out for the rest of the run: coming back on the next sector
    // with zero lives handed him a phantom tank (and, on dying again, a blank
    // panel with lives at -1). Only a LIFE picked up by his partner revives him.
    for (int i = 0; i < 2; ++i) {
        if (playerActive(i) && m_p[i].lives > 0)
            continue;
        m_p[i].alive = false;
        if (!playerActive(i)) {
            m_p[i].lives = 0;
            m_p[i].score = 0;
        }
    }
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

    // Keep tanks from locking together: nudge any overlapping pair apart before
    // movement is resolved.
    separateTanks();

    for (auto &t : m_p)
        if (t.cooldown > 0)
            t.cooldown -= dt;

    for (int i = 0; i < m_playerCount; ++i) {
        const double boost = (m_time < m_p[i].speedUntil) ? 1.35 : 1.0;
        // Keyboard and pad are two sources for the same tank: the flags are
        // ORed, so letting go of a key never cancels a held stick.
        const bool up = (i == 0 ? m_p1Up : m_p2Up) || m_padHeld[i][0];
        const bool down = (i == 0 ? m_p1Down : m_p2Down) || m_padHeld[i][2];
        const bool left = (i == 0 ? m_p1Left : m_p2Left) || m_padHeld[i][3];
        const bool right = (i == 0 ? m_p1Right : m_p2Right) || m_padHeld[i][1];
        updateTank(m_p[i], up, down, left, right, dt, PLAYER_SPEED * boost);
    }

    // Respawn dead players that still have lives.
    for (int i = 0; i < m_playerCount; ++i) {
        Tank &t = m_p[i];
        if (!t.alive && t.lives > 0) {
            t.respawn -= dt;
            if (t.respawn <= 0) {
                t.alive = true;
                t.x = (i == 0 ? 3.5 : COLS - 3.5);
                t.y = ROWS - 1.5;
                t.dir = 0;
                t.shieldUntil = m_time + 3.0;
            }
        }
    }

    updateEnemies(dt);
    updateBullets(dt);
    updatePickups(dt);
    updateFloaters(dt);
    updateExplosions(dt);
    updateBaseGuard();
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
    // Try the full move first; if it is blocked, slide along the free axis so a
    // tank that grazes a wall or another tank does not freeze in place (and can
    // escape a 1/4 brick gap instead of being locked into it).
    bool moved = false;
    if (!solidAt(nx, ny, TANK_SIZE) && !blockedByTank(nx, ny, TANK_SIZE, &t, false, t.x, t.y)) {
        t.x = nx; t.y = ny; moved = true;
    } else if (dir == 0 || dir == 2) {
        // vertical intent: try sliding horizontally
        if (!solidAt(t.x, ny, TANK_SIZE) && !blockedByTank(t.x, ny, TANK_SIZE, &t, false, t.x, t.y)) {
            t.y = ny; moved = true;
        }
    } else {
        // horizontal intent: try sliding vertically
        if (!solidAt(nx, t.y, TANK_SIZE) && !blockedByTank(nx, t.y, TANK_SIZE, &t, false, t.x, t.y)) {
            t.x = nx; moved = true;
        }
    }
    // Unstuck: if the tank is sitting inside solid masonry, nudge it out along
    // the axis of least penetration so it cannot stay locked in a 1/4 strip.
    if (!moved && solidAt(t.x, t.y, TANK_SIZE)) {
        const double nudge = 0.1;
        int esc[4] = { 0, 1, 2, 3 };
        for (int k = 0; k < 4 && !moved; ++k) {
            double nx2 = t.x, ny2 = t.y;
            if (esc[k] == 0) ny2 -= nudge;
            if (esc[k] == 2) ny2 += nudge;
            if (esc[k] == 3) nx2 -= nudge;
            if (esc[k] == 1) nx2 += nudge;
            if (!solidAt(nx2, ny2, TANK_SIZE) && !blockedByTank(nx2, ny2, TANK_SIZE, &t, false, t.x, t.y)) {
                t.x = nx2; t.y = ny2; moved = true;
            }
        }
        // A nudge of 0.1 cannot free a hull that is inside a whole tile or off
        // the board, so that state is rescued outright. It should never happen;
        // it is here so a tank can never be sealed in for the rest of the run.
        if (!moved && buriedInSolid(t.x, t.y)) {
            double rx = t.x, ry = t.y;
            if (nearestFreeSpot(t.x, t.y, rx, ry)) {
                t.x = rx;
                t.y = ry;
                moved = true;
            }
        }
    }
    t.moving = moved;
}

void Backend::updateEnemies(double dt) {
    std::uniform_real_distribution<double> roll(0.0, 1.0);
    const int maxConcurrent = std::min(4 + (m_level - 1), 6);
    m_spawnTimer -= dt;
    if (m_spawned < totalForLevel() && aliveEnemies() < maxConcurrent && m_spawnTimer <= 0) {
        // A busy entry lane means another tank is coming out of that gate:
        // retry soon instead of stacking two on one cell.
        m_spawnTimer = spawnEnemy() ? 2.0 : 0.5;
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
        const double speed = (e.kind == 0) ? ENEMY_SPEED
                                : (e.kind == 1) ? ENEMY_SPEED * 1.35 * mult
                                : (e.kind == 3) ? ENEMY_SPEED * 1.15
                                : ENEMY_SPEED * mult;
        double nx = e.x, ny = e.y;
        if (e.dir == 0) ny -= speed * dt;
        if (e.dir == 2) ny += speed * dt;
        if (e.dir == 3) nx -= speed * dt;
        if (e.dir == 1) nx += speed * dt;
        bool moved = false;
        bool escaped = false;
        if (!solidAt(nx, ny, TANK_SIZE) && !blockedByTank(nx, ny, TANK_SIZE, &e, true, e.x, e.y)) {
            e.x = nx; e.y = ny; moved = true;
        } else if (e.dir == 0 || e.dir == 2) {
            if (!solidAt(e.x, ny, TANK_SIZE) && !blockedByTank(e.x, ny, TANK_SIZE, &e, true, e.x, e.y)) {
                e.y = ny; moved = true;
            }
        } else {
            if (!solidAt(nx, e.y, TANK_SIZE) && !blockedByTank(nx, e.y, TANK_SIZE, &e, true, e.x, e.y)) {
                e.x = nx; moved = true;
            }
        }
        if (!moved && solidAt(e.x, e.y, TANK_SIZE)) {
            const double nudge = 0.1;
            int esc[4] = { 0, 1, 2, 3 };
            for (int k = 0; k < 4 && !moved; ++k) {
                double nx2 = e.x, ny2 = e.y;
                if (esc[k] == 0) ny2 -= nudge;
                if (esc[k] == 2) ny2 += nudge;
                if (esc[k] == 3) nx2 -= nudge;
                if (esc[k] == 1) nx2 += nudge;
                if (!solidAt(nx2, ny2, TANK_SIZE) && !blockedByTank(nx2, ny2, TANK_SIZE, &e, true, e.x, e.y)) {
                    e.x = nx2; e.y = ny2; moved = true;
                }
            }
            // A nudge is never progress: the enemy still has to choose a way
            // out. And a hull inside a whole tile or off the board cannot be
            // nudged out at all, so it is rescued outright.
            escaped = moved;
            if (!moved && buriedInSolid(e.x, e.y)) {
                double rx = e.x, ry = e.y;
                if (nearestFreeSpot(e.x, e.y, rx, ry)) {
                    e.x = rx;
                    e.y = ry;
                    moved = true;
                    escaped = true;
                }
            }
        }
        if (moved && !escaped) {
            e.moving = true;
        } else {
            e.moving = false;
            // Take the way with the most room and commit to it for a while,
            // instead of re-rolling blind every frame: the blind roll kept
            // choosing the wall it had just hit (50% "down", 40% "toward the
            // base"), which is what made enemies jitter in the corners.
            e.dir = bestDir(e.x, e.y, &e, true, e.dir);
            e.think = 0.8 + roll(m_rng) * 1.5;
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
            const double bs = (e.kind == 0 || e.kind == 3) ? BULLET_SPEED : BULLET_SPEED * enemySpeedMult();
            fireBullet(e.x, e.y, e.dir, 2, bs);
        }
    }
}

void Backend::updateBullets(double dt) {
    for (auto &b : m_bullets) {
        if (!b.active)
            continue;
        // If a shot spawns (or lands) inside a surviving brick strip, chip it
        // instead of letting the first step carry it out of the cell. This is
        // what lets a tank pressed against a 1/4 strip break it and escape.
        {
            const int ccx = int(std::floor(b.x)), ccy = int(std::floor(b.y));
            if (ccx >= 0 && ccy >= 0 && ccx < COLS && ccy < ROWS
                && tileAt(ccx, ccy) == Brick && solidAt(b.x, b.y, 0.25)) {
                chipBrick(ccx, ccy, b.dir);
                b.active = false;
                continue;
            }
        }
        // Capped so a fast shot cannot skip over a quarter-cell gap.
        const double step = std::min(b.speed * dt, BULLET_MAX_STEP);
        double nx = b.x, ny = b.y;
        if (b.dir == 0) ny -= step;
        if (b.dir == 2) ny += step;
        if (b.dir == 3) nx -= step;
        if (b.dir == 1) nx += step;

        // Out of bounds.
        if (nx < 0 || ny < 0 || nx >= COLS || ny >= ROWS) {
            b.active = false;
            continue;
        }
        const int cx = int(std::floor(nx)), cy = int(std::floor(ny));
        const int tile = tileAt(cx, cy);
        // A damaged brick tile is a hole only when it is fully open on the
        // shot's own axis: a horizontal shot flies through only when all the
        // vertical strips are gone, a vertical shot only when all the horizontal
        // strips are gone. Checking both axes (the old test) let a shot slip
        // through a gap on the perpendicular axis and skip the surviving strip
        // it was meant to chip — which left the tank stuck against it.
        // A damaged brick tile whose strips in the shot's way are already
        // gone is a hole: the shot flies straight through it.
        const bool through = (tile == Brick) && !tileBlocksPoint(nx, ny);
        if (!through && (tile == Brick || tile == Steel || tile == Base || tile == BaseDead)) {
            if (tile == Brick) {
                // The shot shears off the strip facing the direction it came
                // from.
                chipBrick(cx, cy, b.dir);
            } else if (tile == Base) {
                m_grid[cy * COLS + cx] = BaseDead;
                bumpBoard();
                if (m_soundEnabled)
                    emit playSound("explode");
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
                    e.hp = std::max(0, e.hp - (b.big ? 99 : 1));
                    if (e.hp <= 0) {
                        e.alive = false;
                        pushExplosion(e.x, e.y);
                        const int pts = (e.kind == 2) ? 300 : (e.kind == 1) ? 200 : 100;
                        m_p[b.owner].score += pts;
                        m_floaters.push_back(Floater{e.x, e.y, pts, 0});
                        if (m_floaters.size() > 8)
                            m_floaters.erase(m_floaters.begin());
                        // Green runners carry the bonus: one drops where it dies.
                        if (e.kind == 3 && !m_pickup.active)
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

bool Backend::spawnEnemy() {
    // 16 tanks per stage, composition by wave (see AGENTS.md section 17).
    // Carriers are 2 of the basic tanks: slightly faster, flashing, drop a
    // power-up. Armored tanks take 3 hits.
    int basic, fast;
    if (m_level <= 3) { basic = 8; fast = 4; }
    else if (m_level <= 6) { basic = 6; fast = 6; }
    else { basic = 4; fast = 6; }
    // Enter through a lane no living tank is using. Stacking a tank on top of
    // another used to leave the pair shoved into the wall by separateTanks()
    // and unable to move for the rest of the stage.
    const int spots[3] = {0, COLS / 2, COLS - 1};
    int lane = -1;
    for (int k = 0; k < 3; ++k) {
        const int cand = spots[(m_spawned + k) % 3];
        if (!cellBusy(double(cand) + 0.5, 0.5)) {
            lane = cand;
            break;
        }
    }
    if (lane < 0)
        return false; // every lane is taken: retry on the next tick
    for (auto &e : m_foes) {
        if (e.alive)
            continue;
        e.alive = true;
        e.x = double(lane) + 0.5;
        e.y = 0.5;
        e.dir = 2;
        e.cooldown = 1.0;
        e.think = 1.0;
        // Carriers are 2 of the basic tanks, interleaved (not at the start).
        const int carrierPos1 = basic / 3;
        const int carrierPos2 = (2 * basic) / 3;
        int kind;
        if (m_spawned == carrierPos1 || m_spawned == carrierPos2) {
            kind = 3; // carrier
        } else if (m_spawned < basic) {
            kind = 0; // basic
        } else if (m_spawned < basic + fast) {
            kind = 1; // fast (wheeled chassis)
        } else {
            kind = 2; // armored
        }
        e.kind = kind;
        e.hp = (kind == 2) ? 3 : 1;
        ++m_spawned;
        return true;
    }
    return false; // no free slot: only maxConcurrent tanks are ever alive
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
    // If the barrel tip lands in a different, empty cell (e.g. the tank is
    // pressed against a 1/4 brick strip and the tip overshoots into the open
    // space behind it), pull back to the tank centre so the shot still hits
    // the strip it was aimed at instead of sailing past it.
    if (int(std::floor(bx)) != int(std::floor(x)) || int(std::floor(by)) != int(std::floor(y))) {
        const int tcx = int(std::floor(x)), tcy = int(std::floor(y));
        const int bcx = int(std::floor(bx)), bcy = int(std::floor(by));
        if (tileAt(bcx, bcy) == Empty && tileAt(tcx, tcy) == Brick)
            bx = x, by = y;
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
    if (owner < 2 && m_soundEnabled)
        emit playSound("shoot");
}

// Solid terrain stops a single point. Brick is special: under half
// destroyed it blocks as a whole block, so the first shot changes nothing
// for movement. From the halfway mark on only the surviving strips block,
// which lets a tank (or a shot) reach exactly the 50% line.
bool Backend::tileBlocksPoint(double x, double y) const {
    const int cx = int(std::floor(x)), cy = int(std::floor(y));
    if (cx < 0 || cy < 0 || cx >= COLS || cy >= ROWS)
        return true;
    const int t = m_grid[cy * COLS + cx];
    if (t == Steel || t == Base || t == BaseDead)
        return true;
    if (t != Brick)
        return false;
    const unsigned m = m_brickQ[cy * COLS + cx];
    if (goneStrips(m) < BRICK_BLOCKS_BELOW)
        return true;
    const int vi = std::min(3, int((x - cx) * 4));
    const int hi = std::min(3, int((y - cy) * 4));
    return (m & (1u << vi)) != 0 && (m & (1u << (4 + hi))) != 0;
}

bool Backend::solidAt(double x, double y, double size) const {
    const double h = size / 2;
    const double pts[4][2] = {{x - h, y - h}, {x + h, y - h}, {x - h, y + h}, {x + h, y + h}};
    for (auto &p : pts)
        if (tileBlocksPoint(p[0], p[1]))
            return true;
    return false;
}

bool Backend::hitsTank(double bx, double by, double tx, double ty) const {
    const double dx = bx - tx, dy = by - ty;
    return dx * dx + dy * dy < 0.32;
}

// Tanks block each other, so allies and enemies can no longer drive
// or phase through one another.
//
// This tests the *move*, not the position: two tanks that are already in
// contact may always move apart. Without that, two tanks that end up within a
// hull's width of each other are each other's wall in all four directions and
// neither can move again — which is how enemies got stuck in the corners.
bool Backend::blockedByTank(double nx, double ny, double size, const void *self, bool isEnemy,
                            double curX, double curY) const {
    const double minDist = size;
    auto blocks = [&](double ox, double oy, const void *other) {
        if (other == self)
            return false;
        if (std::abs(ox - nx) >= minDist || std::abs(oy - ny) >= minDist)
            return false; // the target position is clear of it
        if (std::abs(ox - curX) >= minDist || std::abs(oy - curY) >= minDist)
            return true; // entering contact: blocked
        // Already in contact: only a move that closes the gap is blocked.
        const double cur = std::max(std::abs(ox - curX), std::abs(oy - curY));
        const double nxt = std::max(std::abs(ox - nx), std::abs(oy - ny));
        return nxt <= cur;
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
        // Enemies collide with each other and with the players; players only
        // collide with tanks, which is the same test either way.
        if (blocks(e.x, e.y, other))
            return true;
    }
    (void)isEnemy;
    return false;
}

// True when the hull is off the board, or when the cell at the tank's centre is
// a whole tile. A tank standing on the 50% line of a damaged brick does not
// count: that is a legitimate position and the escape nudge frees it.
bool Backend::buriedInSolid(double x, double y) const {
    const double h = TANK_SIZE / 2;
    const double pts[4][2] = {{x - h, y - h}, {x + h, y - h}, {x - h, y + h}, {x + h, y + h}};
    for (auto &p : pts) {
        const int cx = int(std::floor(p[0])), cy = int(std::floor(p[1]));
        if (cx < 0 || cy < 0 || cx >= COLS || cy >= ROWS)
            return true;
    }
    const int cx = int(std::floor(x)), cy = int(std::floor(y));
    if (cx < 0 || cy < 0 || cx >= COLS || cy >= ROWS)
        return true;
    const int i = cy * COLS + cx;
    const int t = m_grid[i];
    if (t == Steel || t == Base || t == BaseDead)
        return true;
    return t == Brick && m_brickQ[i] == 0xFF;
}

bool Backend::nearestFreeSpot(double x, double y, double &outX, double &outY) const {
    const int cx0 = int(std::floor(x)), cy0 = int(std::floor(y));
    for (int r = 0; r <= 3; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r)
                    continue; // ring only, nearest first
                const int cx = cx0 + dx, cy = cy0 + dy;
                if (cx < 0 || cy < 0 || cx >= COLS || cy >= ROWS)
                    continue;
                const double px = cx + 0.5, py = cy + 0.5;
                if (!solidAt(px, py, TANK_SIZE)) {
                    outX = px;
                    outY = py;
                    return true;
                }
            }
        }
    }
    return false;
}

double Backend::freeRun(double x, double y, int dir, const void *self, bool isEnemy) const {
    const double step = 0.1;
    double run = 0;
    for (int k = 1; k <= 20; ++k) {
        double tx = x, ty = y;
        if (dir == 0) ty -= step * k;
        if (dir == 2) ty += step * k;
        if (dir == 3) tx -= step * k;
        if (dir == 1) tx += step * k;
        if (solidAt(tx, ty, TANK_SIZE) || blockedByTank(tx, ty, TANK_SIZE, self, isEnemy, x, y))
            break;
        run = step * k;
    }
    return run;
}

int Backend::bestDir(double x, double y, const void *self, bool isEnemy, int fallback) {
    int order[4] = {0, 1, 2, 3};
    for (int k = 3; k > 0; --k) {
        std::uniform_int_distribution<int> pick(0, k);
        std::swap(order[k], order[pick(m_rng)]);
    }
    int best = fallback;
    double bestRun = -1.0;
    for (int k = 0; k < 4; ++k) {
        const double run = freeRun(x, y, order[k], self, isEnemy);
        if (run > bestRun) {
            bestRun = run;
            best = order[k];
        }
    }
    return best;
}

// A spawn lane is busy when a living tank already sits on it.
bool Backend::cellBusy(double x, double y) const {
    for (int i = 0; i < m_playerCount; ++i)
        if (m_p[i].alive && std::abs(m_p[i].x - x) < TANK_SIZE && std::abs(m_p[i].y - y) < TANK_SIZE)
            return true;
    for (const auto &e : m_foes)
        if (e.alive && std::abs(e.x - x) < TANK_SIZE && std::abs(e.y - y) < TANK_SIZE)
            return true;
    return false;
}

void Backend::separateTanks() {
    const double minDist = TANK_SIZE;
    // Collect alive tanks (players + enemies) into one list of (x,y) refs.
    std::vector<std::pair<double*, double*>> tanks;
    for (int i = 0; i < m_playerCount; ++i)
        if (m_p[i].alive)
            tanks.push_back({&m_p[i].x, &m_p[i].y});
    for (auto &e : m_foes)
        if (e.alive)
            tanks.push_back({&e.x, &e.y});

    for (size_t a = 0; a < tanks.size(); ++a) {
        for (size_t b = a + 1; b < tanks.size(); ++b) {
            double &ax = *tanks[a].first;
            double &ay = *tanks[a].second;
            double &bx = *tanks[b].first;
            double &by = *tanks[b].second;
            const double dx = bx - ax;
            const double dy = by - ay;
            const double adx = std::fabs(dx);
            const double ady = std::fabs(dy);
            if (adx >= minDist || ady >= minDist)
                continue;
            // Push apart along the shallowest penetration axis, but never into
            // masonry or off the board: shoving a tank into a wall is how it
            // used to be left unable to move for the rest of the stage (its
            // own hull is wider than the escape step).
            const double penX = minDist - adx;
            const double penY = minDist - ady;
            const double half = 0.5;
            const double hx = penX * half * (dx >= 0 ? 1.0 : -1.0);
            const double hy = penY * half * (dy >= 0 ? 1.0 : -1.0);
            auto fits = [&](double x, double y) { return !solidAt(x, y, TANK_SIZE); };
            if (penX < penY) {
                if (fits(ax - hx, ay) && fits(bx + hx, by)) {
                    ax -= hx;
                    bx += hx;
                } else if (fits(ax, ay - hy) && fits(bx, by + hy)) {
                    ay -= hy;
                    by += hy;
                }
            } else {
                if (fits(ax, ay - hy) && fits(bx, by + hy)) {
                    ay -= hy;
                    by += hy;
                } else if (fits(ax - hx, ay) && fits(bx + hx, by)) {
                    ax -= hx;
                    bx += hx;
                }
            }
        }
    }
}

int Backend::tileAt(int cx, int cy) const {
    if (cx < 0 || cy < 0 || cx >= COLS || cy >= ROWS)
        return Steel;
    return m_grid[cy * COLS + cx];
}

// A shot shears off the strip on the face it hits: a horizontal shot
// takes a vertical quarter (1/4 of the width), a vertical shot takes a
// horizontal quarter (1/4 of the height). The tile only disappears once a
// whole axis is gone; until half of it is gone it still blocks as a block.
void Backend::chipBrick(int cx, int cy, int dir) {
    if (cx < 0 || cy < 0 || cx >= COLS || cy >= ROWS)
        return;
    if (m_grid[cy * COLS + cx] != Brick)
        return;
    uint8_t &m = m_brickQ[cy * COLS + cx];
    // dir: 0 up, 1 right, 2 down, 3 left. A shot travelling horizontally
    // eats a vertical strip; one travelling vertically eats a horizontal one.
    const int base = (dir == 1 || dir == 3) ? 0 : 4;
    const bool fromNearSide = (dir == 1 || dir == 2); // left or top
    if (fromNearSide) {
        for (int i = 0; i < 4; ++i)
            if (m & (1 << (base + i))) {
                m &= uint8_t(~(1 << (base + i)));
                break;
            }
    } else {
        for (int i = 3; i >= 0; --i)
            if (m & (1 << (base + i))) {
                m &= uint8_t(~(1 << (base + i)));
                break;
            }
    }
    if ((m & 0x0F) == 0 || (m & 0xF0) == 0)
        m_grid[cy * COLS + cx] = Empty;
    bumpBoard();
}

// True when the 0.8-wide tank sitting at (tx, ty) overlaps board cell
// (cx, cy).
bool Backend::cellHitsTank(int cx, int cy, double tx, double ty) {
    const double h = TANK_SIZE / 2;
    return tx + h > cx && tx - h < cx + 1 && ty + h > cy && ty - h < cy + 1;
}

// PICK rebuilds the wall, so nothing may stay standing where it lands: a
// tank caught in the gap is crushed on the spot. No score is awarded, the
// wall is not the player's doing.
void Backend::crushTanksIn(int cx, int cy) {
    for (int i = 0; i < 2; ++i)
        if (m_p[i].alive && cellHitsTank(cx, cy, m_p[i].x, m_p[i].y))
            killPlayer(i);
    for (auto &e : m_foes) {
        if (!e.alive || !cellHitsTank(cx, cy, e.x, e.y))
            continue;
        e.alive = false;
        pushExplosion(e.x, e.y);
        if (m_soundEnabled)
            emit playSound("explode");
    }
}

void Backend::pushExplosion(double x, double y) {
    m_explosions.push_back(Explosion{x, y, 0.0});
    if (m_explosions.size() > 8)
        m_explosions.erase(m_explosions.begin(), m_explosions.begin() + (m_explosions.size() - 8));
}

// PICK armours the wall around the base, like the shovel in Battle City:
// the guard tiles become concrete, and turn back into brick when it lapses.
// The wall is rebuilt whole, damage or not.
void Backend::setBaseGuard(bool steel) {
    const int bx = COLS / 2, by = ROWS - 1;
    static const int offs[5][2] = {{-1, 0}, {1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    if (steel) {
        m_baseGuard.clear();
        for (const auto &o : offs) {
            const int x = bx + o[0], y = by + o[1];
            if (x < 0 || y < 0 || x >= COLS || y >= ROWS)
                continue;
            crushTanksIn(x, y);
            m_grid[y * COLS + x] = Steel;
            m_brickQ[y * COLS + x] = 0;
            m_baseGuard.push_back(y * COLS + x);
        }
    } else {
        for (int i : m_baseGuard) {
            crushTanksIn(i % COLS, i / COLS);
            m_grid[i] = Brick;
            m_brickQ[i] = 0xFF;
        }
        m_baseGuard.clear();
    }
    bumpBoard();
}

void Backend::updateBaseGuard() {
    if (m_baseGuardUntil > 0 && m_time >= m_baseGuardUntil) {
        m_baseGuardUntil = -1;
        setBaseGuard(false);
    }
}

void Backend::killPlayer(int idx) {
    Tank &t = m_p[idx];
    t.alive = false;
    // Never below zero: a negative count is neither a spare tank nor a KILLED
    // panel, it is a blank one. Losing the last life takes the player out.
    t.lives = std::max(0, t.lives - 1);
    t.respawn = 2.0;
    if (m_soundEnabled)
        emit playSound("explode");
}

void Backend::revivePlayer(int idx) {
    Tank &t = m_p[idx];
    t.alive = true;
    t.lives = 1;
    t.x = (idx == 0 ? 3.5 : COLS - 3.5);
    t.y = ROWS - 1.5;
    t.dir = 0;
    t.respawn = 0;
    t.moving = false;
    t.shieldUntil = m_time + 3.0; // the same grace a respawn gets
}

void Backend::checkEnd() {
    bool baseDead = false;
    for (int t : m_grid)
        if (t == BaseDead)
            baseDead = true;
    // Both endings show the game over screen, which carries on to the select
    // screen with START (so a pad can always start the next run).
    int total = 0;
    for (int i = 0; i < m_playerCount; ++i)
        total += m_p[i].score;
    if (baseDead) {
        m_ending = "base";
        setState("gameover", QString("SCORE %1").arg(total));
        return;
    }
    // Game over when every tank in the game is out. With a single player that
    // means dying ends the run, instead of leaving an idle second tank alive.
    bool allOut = true;
    for (int i = 0; i < m_playerCount; ++i)
        if (m_p[i].alive || m_p[i].lives > 0)
            allOut = false;
    if (allOut) {
        m_ending = "over";
        setState("gameover", QString("SCORE %1").arg(total));
        return;
    }
    if (m_spawned >= totalForLevel() && aliveEnemies() == 0) {
        if (m_pads.empty())
            setState("victory", QString("Sector %1 clear! Score %2 — press R for sector %3").arg(m_level).arg(total).arg(m_level + 1));
        else
            setState("victory", QString("Sector %1 clear! Score %2 — press START for sector %3").arg(m_level).arg(total).arg(m_level + 1));
    }
}

int Backend::aliveEnemies() const {
    int n = 0;
    for (const auto &e : m_foes)
        if (e.alive)
            ++n;
    return n;
}

// Bonuses only drop from carriers, at the spot where they die.
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
    case Pick:
        setBaseGuard(true);
        m_baseGuardUntil = m_time + 20;
        break;
    case Life:
        // A partner who is out has first claim: the LIFE brings him back at
        // once with one tank, instead of topping up the player who still has
        // one. A one-player run has nobody to revive, so it is the old +1.
        if (m_playerCount == 2) {
            const int mate = 1 - player;
            if (!m_p[mate].alive && m_p[mate].lives <= 0) {
                revivePlayer(mate);
                break;
            }
        }
        t.lives = std::min(t.lives + 1, 5);
        break;
    case Grenade: {
        for (auto &e : m_foes) {
            if (!e.alive)
                continue;
            e.alive = false;
            pushExplosion(e.x, e.y);
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

void Backend::updateExplosions(double dt) {
    for (auto &e : m_explosions)
        e.age += dt;
    m_explosions.erase(std::remove_if(m_explosions.begin(), m_explosions.end(),
                                     [](const Explosion &e) { return e.age > 0.5; }),
                       m_explosions.end());
}

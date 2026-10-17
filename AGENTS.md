# AGENTS.md — Tanks

> **This is the agent entry point for this repository.** Any model or agent
> picking up this project must read this file first, then `README.md` for the
> user-facing docs.
>
> Purpose: give an unambiguous picture of **what is solid, what is half-done and
> what is debris** — so we stop losing working code when the model changes.
>
> Keep it current: update the state sections (`Current tree state`, `Feature
> inventory`, `Next move`) whenever the state of the build changes.
>
> Snapshot: the tree is **clean**. `HEAD` is the commit *"Shear bricks in quarter
> strips, make PICK rebuild the base wall, fix the frame rate"*, on top of the
> checkpoint `ec59cd9` (brick damage, legible sprites, seamless forest, Loader
> gating). That commit carries the 50% brick collision rule, PICK rebuilding the
> whole base wall, the contour-free gap and the frame-rate fix (**130 ms -> 14 ms**
> a frame); tests and the user's own play session both pass. See section 6.

---

## 0. Ground rules (read before touching anything)

1. **English only** in code, comments, docs, commit messages and UI strings
   (global rule). Chat replies to the user are in Spanish.
2. **Nothing is "done" until it is verified.** Verified means `bin/build`
   succeeds, `bin/test` passes, and the behaviour was seen on screen (or is
   asserted by a test). Do not claim a fix works from reading the diff.
3. **No debug scaffolding in the tree, ever.** A temporary rig must be removed
   before hand-off. If a rig is needed, hide it behind an env var or use a
   throwaway branch — never a `// TMP RIG` block in the middle of a file.
4. **Commits are atomic, and only with the user's approval.** Agreed policy:
   commit **only** when the thing works, **only** after the user agrees, and
   **only** after the user has tested it. Never commit on your own initiative.
5. **One checkpoint commit before starting the next feature.** This is the whole
   lesson learnt here: large uncommitted diffs are what destroyed previous work.

---

## 1. What the project is

A retro **Battle City–style, 2-player local co-op tank game vs AI**, shipped as an
Omarchy desktop app: **Qt 6 / C++ backend + QML frontend, single binary**, built
with `qmake6` + `make`.

- 15x15 random map per game; brick / steel / forest / base tiles.
- 2 player tanks (P1 yellow, P2 red) and tiered enemies (gray basic, green fast,
  gray steel-armoured).
- 8 enemies per sector, more in later sectors; only **specials** scale (speed,
  HP, bullet speed) — basic enemies stay constant.
- 7 bonuses, dropping only from green enemies.
- Defend the base, which renders the official Omarchy pixel wordmark `omarchy`;
  it becomes rubble when destroyed.

---

## 2. Repo map

```
~/Work/tanks
├── AGENTS.md            # this file: agent entry point + state of the build
├── bin/build            # qmake6 + make, with stale-build self-heal
├── bin/test             # test build + run, stale-build self-heal + TANKS_SEED=7
├── bin/install          # installs the Arch package (PKGBUILD)
├── pkgbuild/            # PKGBUILD, .desktop, .svg
├── README.md            # user-facing docs (controls, build)
├── tanks.pro
├── src/
│   ├── main.cpp         # app setup, QML_DISABLE_DISK_CACHE, buildId from binary mtime
│   ├── backend.h        # all game state + interface (Q_PROPERTY / Q_INVOKABLE)
│   ├── backend.cpp      # game loop, collisions, bonuses, sectors, sound
│   ├── Main.qml         # window, HUD, board, sprites, overlays, help, footer
│   ├── TankSprite.qml   # 16x16 tank with animated treads
│   ├── PickupIcon.qml   # all 7 bonus sprites (per-pixel colour grids)
│   ├── PixelButton.qml
│   ├── theme.cpp/.h     # accent + palette read from the Omarchy theme
│   ├── resources.qrc
│   └── sounds/*.wav     # engine, shoot, eshoot, hit, explode, power, nuke
└── tests/
    ├── tanks_tests.cpp  # 18 QtTest cases (20 slots)
    └── tanks_tests.pro
```

---

## 3. Architecture

**`Backend` owns 100% of the game state. QML only renders and forwards keys.**

- A 16 ms `QTimer` runs `Backend::tick()`; every frame emits `frameChanged()`.
- QML never stores game state. It reads `Q_PROPERTY` values and pulls per-slot
  data via `playerAt(i, tick)` / `enemyAt(i, tick)` / `bulletAt(i, tick)` /
  `floaterAt(i, tick)` / `pickupAt(tick)`. Delegates are **created once with a
  fixed count** and only update properties, so the scene graph never rebuilds
  mid-game. (`tick` is a dummy argument that forces re-evaluation.)
- Input is abstracted **per player** on purpose: `setP1Move(dx, dy, pressed)`,
  `setP2Move(...)`, `fireP1()`, `fireP2()`. A future gamepad layer drives the
  same path. **Gamepad is not implemented.**
- Time is real: `QElapsedTimer`, with `dt` clamped to `0.05` max so a hitch
  cannot teleport tanks.

---

## 4. Constants and invariants

Gameplay constants (`src/backend.cpp`, anonymous namespace):

| Constant | Value |
|---|---|
| `TANK_SIZE` | `0.8` |
| `PLAYER_SPEED` | `3.62` (x1.35 while SPEED is active) |
| `ENEMY_SPEED` | `2.07` |
| `BULLET_SPEED` | `8.0` |
| `FIRE_GAP` | `0.35` |

World constants (`src/backend.h`): `COLS = ROWS = 15`, `TOTAL_ENEMIES = 8`,
per-sector total `8 + 2*(level-1)`, max concurrent `min(3+(level-1), 5)`,
`enemySpeedMult() = 1.0 + 0.1*(level-1)`.

### 4.1 Brick slices — the important invariant

- `Tile` enum (`src/backend.h`):
  `Empty=0, Brick=1, Steel=2, Base=3, BaseDead=4, Forest=5`.
- `std::vector<uint8_t> m_brickQ` holds a **strip mask per brick tile**.
  Bits **0-3 are the four vertical strips `V0..V3`, left to right**; bits
  **4-7 are the four horizontal strips `H0..H3`, top to bottom**. `0xFF` is a
  whole tile, `0` means nothing survives on that axis.
- `dir` is `0 up, 1 right, 2 down, 3 left`. A shot shears **one 1/4 strip on the
  face it arrives from** — i.e. perpendicular to its own travel:
  - `dir 1` (fired from the left) takes the lowest surviving `V` bit.
  - `dir 3` (fired from the right) takes the highest surviving `V` bit.
  - `dir 2` (fired from above) takes the lowest surviving `H` bit.
  - `dir 0` (fired from below) takes the highest surviving `H` bit.
- So a horizontal shot removes **1/4 of the tile's width, full height**, and a
  vertical shot **1/4 of its height, full width**. That is the Battle City look.
- The tile only becomes `Empty` once a whole axis is used up
  (`(m & 0x0F) == 0 || (m & 0xF0) == 0`).
- **Collision — the 50% law (user spec, do not soften it):** a tile that has
  lost **fewer than two strips still blocks as a whole block**, however it looks.
  From the second strip on, **only the masonry that is left blocks**, so a tank
  (or a shot) pushes past the tile edge and comes to rest exactly **on the 50%
  line** — at three strips gone, on the 25% line. Both axes count together:
  `gone = 8 - popcount(m)`, floor at `BRICK_BLOCKS_BELOW = 2` (`backend.cpp`).
- Implemented in `tileBlocksPoint(x, y)` (`backend.cpp`): for a live brick tile it
  takes the quarter the point falls in (`vi = (x-cx)*4`, `hi = (y-cy)*4`) and
  blocks only when **both** that vertical strip and that horizontal strip
  survive. `solidAt()` is now just four corner samples of that test. A pure-axis
  case gives exactly a surviving half/quarter; a mixed case (shots from two
  faces) gives the natural L/quadrant complement.
- A shot **flies through the strips it has already opened** and stops on the
  surviving masonry, chipping the strip it hits there (`updateBullets()`, the
  `through` flag). So the fifth shot from a fixed spot still reaches the tile
  behind — but only because four shots have cleared the tile by then.
  `BULLET_MAX_STEP = 0.2` caps a shot's per-frame travel so it cannot skip over
  a quarter-cell gap during a frame spike.
- QML never reads the board as a string any more. The two queries it uses are
  `Q_INVOKABLE int tileKind(x, y, rev)` and
  `Q_INVOKABLE bool brickStripGone(x, y, strip, rev)` (`backend.h`); `rev` is
  `boardRev`, an int bumped by `bumpBoard()` inside `boardChanged`. Passing it
  back is what makes the binding re-run **only when the map changes** — see
  section 7 for why that is not an optimisation but a fix. `board()` survives
  for tests and probes, cached behind `m_boardCacheValid`.
- **Every** board mutation must go through `bumpBoard()`: `generateMap()`,
  `chipBrick()`, `setBaseGuard()`, the base turning into `BaseDead` in
  `updateBullets()`, and the test helper `TanksTestAccess::putTile`. It drops the
  cached string, bumps `boardRev` and emits `boardChanged`.
- QML draws strip `i` as a hole rectangle (`win.brickGone` in `Main.qml`):
  0-3 vertical left to right, 4-7 horizontal top to bottom. The rectangle covers
  the **whole cell slice, edges included** (integer edges: `Math.round(slot*cell/4)`)
  and is painted in the board colour `#050507`: the gap is *nothing*. Do not
  inset it — a 1px inset leaves the mortar alive as a **contour line** around the
  tile, which is exactly the bug the user reported. Do not fill it with a lighter
  colour either.
- **Locked behaviour — do not "fix" without updating the tests:**
  `shotsChipSlices`, `oneChipStillBlocksTheWholeTile`,
  `halfGoneOpensTheTileUpToTheFiftyPercentLine`,
  `tankRollsIntoTheGapUpToTheFiftyPercentLine`, `fourShotsFromOneFaceClearTile`,
  `shotChipsOneSlice`, `laneOpensOnceTheTileIsGone`.

### 4.2 Bonus drop rule

Only **green (kind 1)** enemies drop a bonus, where they die (`spawnPickupAt`).
Exactly **one pickup at a time**; it expires after 15 s.
`PickupKind`: `Speed=0, Shield=1, Pick=2, Life=3, Grenade=4, Cannon=5, Freeze=6`.

| # | Name | Effect | Duration |
|---|---|---|---|
| 0 | SPEED | +35% movement | 20 s |
| 1 | SHIELD | blocks shots | 10 s |
| 2 | PICK | base wall becomes concrete | 20 s |
| 3 | 1UP | +1 life (cap 5) | instant |
| 4 | GRENADE | kills all enemies, awards their score, nuke sound | instant |
| 5 | CANNON | one-shot armour (big shell) | 20 s |
| 6 | FREEZE | enemies stop | 8 s |

**PICK is the Battle City shovel, not a steel breaker.** It **rebuilds the whole
wall around the base — all five guard tiles, whatever state they were left in**
(whole, chipped, or shot away) — as `Steel`, and turns them back into whole
`Brick` when it lapses. The old "only armour tiles that are still brick"
exception is gone: the user asked for a full rebuild, so the wall always comes
back.

**Nothing may be left standing inside the wall.** Before a guard tile is
converted, `crushTanksIn(cx, cy)` kills every tank overlapping that cell:
an enemy dies with **no score for anyone** (the wall is not the player's doing),
a player loses a life through the normal `killPlayer()` path and respawns. The
same crush runs on the revert, so nobody is ever sealed inside concrete.
`cellHitsTank()` is the overlap test (the 0.8 hull against the cell).
**Nothing in the game destroys `Steel`.** The old "bullets smash steel while PICK
is active" behaviour, the per-player `Tank::pickUntil` and the `Bullet::pick`
flag are **gone** — do not reintroduce them. Implementation:
`setBaseGuard(bool)`, `crushTanksIn()`, `m_baseGuardUntil`, `m_baseGuard`,
`updateBaseGuard()`, called from `tick()`.

### 4.3 Determinism

- `TANKS_SEED=<n>` seeds `m_rng`. `bin/test` runs with `TANKS_SEED=7` so map
  generation is reproducible.
- Env-based seeding is a **real feature** (tests depend on it) — keep it.

---

## 5. Feature inventory

### Verified working
- Full game loop; 15x15 random map with brick / steel / base / forest.
- 2 player tanks + tiered enemies, bullets, collisions, lives, score.
- Pause (`P`), restart (`R`), sound toggle (`M`), help (`?`), quit (`Q`).
- Help auto-pauses the game and resumes only if the help caused the pause.
- Victory / game-over overlays; sector progression with scaling specials.
- Pixel Omarchy wordmark base, plus rubble when destroyed.
- 7 bonuses, green-only drops, one at a time.
- Floating kill-score popups in pixel type.
- 8-bit sound set; the engine loop is driven by a `Timer`.
- `?` help lists the 7 bonus icons; the menu legend shows them at 40x40.
- `buildId` derived from the running binary's mtime, shown in the footer.
- Stale-build self-heal in `bin/build` and `bin/test`.
- Board and bonus-icon rendering gated with `Loader`: **69 167 -> 7 204** items
  (measured, see section 7).
- User-confirmed on screen: the forest leaf sprite reads well.
- **Frame rate fixed** (section 7.1): **130 ms -> 14 ms** a frame (~7 -> ~71 fps)
  at the same window size, with *more* items than the checkpoint. Cause was not
  the item count but QML reading the board **as a string** on every frame. The
  user confirmed the game feels fast.
- **Brick slice damage + the 50% collision law** (section 4.1). Verified twice:
  `bin/test` cases, and a pixel probe of the real rendered board (section 8.2) —
  a shot from the left leaves a 15px gap out of 59, full height, board colour
  edge to edge with no rim; two shots leave a 30px gap; a shot from below leaves
  a full-width gap 15px tall; and a tank driving into the gap stops with its hull
  on the 50% line.
- **PICK rebuilds and defends the base wall** (section 4.2). Verified the same
  way: a shot-away guard tile comes back as `Steel` along with the rest of the
  wall, a tank parked in the gap is crushed (player loses a life, enemy dies with
  no score), and `updateBaseGuard()` puts whole brick back.
- All tests pass (`bin/test`): 20 slots = `initTestCase` + 18 cases +
  `cleanupTestCase`.
- **User-confirmed on screen, by playing** (this was the gate for the commit):
  - The sheared strips disappear as *nothing*: no contour line, no lighter fill.
  - As the strips go, the tank pushes further into the block. The user prefers
    this over a hard stop at the first chip ("por mí está bien, me parece más
    lógico"). Note the code still blocks the whole tile while **fewer than two**
    strips are gone (`BRICK_BLOCKS_BELOW`); from the second strip on it advances
    per strip. **That is the accepted behaviour — do not "fix" it.**
  - PICK rebuilds the wall, and a player parked in a shot-away guard cell is
    crushed when it lands.
- **Lives and respawn are normal**: one life per death, and the tank comes back
  after the 2 s delay. A "picking up CANNON ate my lives and I did not respawn"
  report was **withdrawn** by the user after a second session counting lives.
  Do not go chasing it.

### Implemented but not verified on screen by the user
- Menu legend size bump (30 -> 40 px icons, `side: 28 -> 36`).

### Broken / unresolved
- Nothing blocking.

### Known missing
- Gamepad support (path prepared, not implemented).

---

## 6. Current tree state

The tree is **clean**. `HEAD` is the commit *"Shear bricks in quarter strips,
make PICK rebuild the base wall, fix the frame rate"*, sitting on top of the
previous checkpoint `ec59cd9`. Nothing pushed.

That commit contains exactly these files (line counts omitted on purpose — this
file changes itself):

```
 AGENTS.md              this file: the 50% law, PICK rebuild, the frame fix
 src/Main.qml           int tile queries, contour-free gap rectangles
 src/backend.cpp        the 50% collision law, PICK rebuild + crush, bumpBoard
 src/backend.h          boardRev/tileKind/brickStripGone, boardQ removed
 tests/tanks_tests.cpp  TanksTestAccess rework + 18 cases (20 slots)
```

`build/` is git-ignored.

### 6.1 Already reverted — debug scaffolding, do not bring it back

- `src/backend.cpp`: the two `// TMP RIG` blocks (auto `startGame()`, auto-fire
  timer, 500 ms log to `/tmp/rig_state.txt`; and the rigged empty map with
  `m_p[0].pickUntil = 9999`) plus `#include <QDebug>/<QDateTime>/<QFile>/<QTextStream>`.
- `src/backend.h`: `qint64 m_rigStart`.
- `src/Main.qml`: `visibility: Window.FullScreen`.
- A temporary item counter in `src/main.cpp` (env `TANKS_COUNT_ITEMS`) used to
  measure section 7 — removed after measuring.

The rig above also referenced `m_p[0].pickUntil`, an API that no longer exists
(section 4.2). `#include <cstdlib>` and the `TANKS_SEED` block in the constructor
**stay**: they are real features the tests depend on.

### 6.2 What the checkpoint commit (`ec59cd9`) contains

- `bin/build`: stale-build guard (wipe + rebuild once from clean if any input is
  newer than `build/tanks`).
- `bin/test`: same guard for the test binary, plus `TANKS_SEED=7`.
- `src/backend.h` / `src/backend.cpp`: `TANKS_SEED` seeding, `boardQ()`,
  `friend struct TanksTestAccess;`, the `m_brickQ` member.
- `src/main.cpp`: `buildId` from `QFileInfo(applicationFilePath()).lastModified()`.
- `src/PickupIcon.qml`: the legible sprites **and** `Loader` gating per icon kind.
- `src/Main.qml`: `Loader` gating per tile kind, the forest canopy, legend icon
  sizes.
- `README.md`: pointer to this file. `AGENTS.md`: this file.

### 6.3 What the brick/PICK/frame-rate commit contains, by file

- `src/backend.h`: the strip-mask comment for `m_brickQ`; `chipBrick()`,
  `setBaseGuard()`, `updateBaseGuard()`, `crushTanksIn()`, `cellHitsTank()`,
  `bumpBoard()`, `tileBlocksPoint()` declarations; `boardRev()`, `tileKind()`,
  `brickStripGone()`; `m_baseGuardUntil`, `m_baseGuard`, `m_boardRev`,
  `m_boardCache`, `m_boardCacheValid`; `boardQ()`, `invalidateBoardQ()`,
  `m_boardQCache`, `m_boardQCacheValid` **removed**; `Tank::pickUntil`,
  `Bullet::pick` and `damageTile()` **removed**.
- `src/backend.cpp`: `board()` cached behind `m_boardCacheValid`; `bumpBoard()`,
  `tileKind()`, `brickStripGone()`, `tileBlocksPoint()`, `crushTanksIn()`,
  `cellHitsTank()` added; `goneStrips()`, `BRICK_BLOCKS_BELOW`,
  `BULLET_MAX_STEP` in the anonymous namespace; `solidAt()` reduced to four
  corner samples of `tileBlocksPoint()`; `updateBullets()` caps the step and lets
  a shot fly through the strips it already opened; the base turning into
  `BaseDead` now calls `bumpBoard()`; `setBaseGuard()` rebuilds all five guard
  tiles and crushes before converting; `players()` / `playerAt()` / `bulletAt()`
  drop the `pick` key; `startGame()` and `nextLevel()` clear the guard;
  `generateMap()` writes `0xFF` and bumps; `fireBullet()` no longer sets
  `b.pick`; `applyPickup()` `Pick` case calls `setBaseGuard(true)` + arms the
  timer; `tick()` calls `updateBaseGuard()`.
- `src/Main.qml`: `readonly property int tileEmpty/tileBrick/tileSteel/tileBase/
  tileDead/tileForest`; `win.brickGone(cx, cy, strip)` calls the invokable with
  `backend.boardRev`; the tile delegate's `t` string became
  `property int kind: backend.tileKind(cx, cy, backend.boardRev)`; the forest
  `tileAt()` returns `tileKind`; `brickPart` keeps the exact original brickwork
  (four courses, offset joints, top highlight) plus eight hole rectangles that
  cover the whole cell slice in the board colour (this is the contour fix); the
  player panels drop the `PICK` label; the manual gains
  `PICK: BASE WALL BECOMES CONCRETE FOR 20 S`.
- `tests/tanks_tests.cpp`: `TanksTestAccess` reworked (`strips()`, `chip(x, y,
  dir)`, `solid()`, `putPlayer()`, `putEnemy()`, `takePickup()`,
  `baseGuardActive()`, `lapseBaseGuard()`); cases are
  `tileQueriesReportTheBoard`, `shotsChipSlices`,
  `oneChipStillBlocksTheWholeTile`,
  `halfGoneOpensTheTileUpToTheFiftyPercentLine`,
  `tankRollsIntoTheGapUpToTheFiftyPercentLine`, `fourShotsFromOneFaceClearTile`,
  `shotChipsOneSlice`, `laneOpensOnceTheTileIsGone`, `shotsNeverBreakSteel`,
  `pickRebuildsTheWholeBaseWall`, `pickCrushesTanksInTheWall`.


---

## 7. FIXED — QML item bloat was stalling the GUI thread

**Symptom (measured with the old debug rig).** Game time advanced at ~5% of wall
time (`t=0.05 wall=2536` ... `t=1.35 wall=25620`), `dt` sat pinned at the `0.05`
clamp, and the early game looked frozen. That made earlier "visual proof"
screenshots misleading: steel looked intact and only one brick had been chipped,
because the game had barely ticked.

**Cause.** `visible: false` saves nothing — a hidden item still exists. Every one
of the 225 board tiles instantiated the brick grid, the steel grid, the
135-rectangle base wordmark and a 64-rectangle forest canopy, and every
`PickupIcon` instantiated all seven bonus sprites, whatever it actually was.

**Fix.** Wrap each type-specific block in `Loader { active: ... }` with the
content in a `Component`, so only the kind a tile or an icon really is gets
built. Applied to the brick / steel / base tile parts, the forest canopy and all
seven `PickupIcon` sprites.

**Measured.** Same instrumentation, `QT_QPA_PLATFORM=offscreen`, 1.5 s after
`startGame()`, counting `QQuickItem` in the window's visual tree:

| Build | QQuickItems |
|---|---|
| Everything instantiated (`active: true`, the old behaviour) | 69 167 |
| After the fix | 7 204 |

About 9.6x fewer. The remaining top contributors are legitimate content:
`TankSprite`'s 16x16 pixel grid (256 `Rectangle`s per tank), ~1 000 `Loader`
items (225 tiles x 4 + 225 forest + 15 icons) and the ~800 forest leaves.

**Still to confirm on screen:** the item count is measured, the frame rate is
not — though the user has since reported play as smooth. Note that restoring the
full brickwork sprite in section 6.3 costs roughly 45 items per brick tile again
(~8 small `Rectangle`s per course plus 8 hole rectangles), which is legitimate
content, not bloat.

**How to re-measure** if it ever regresses: temporarily walk
`QQuickWindow::contentItem()`'s `childItems()` after `engine.load()`, count
`QQuickItem` instances, then delete the snippet. An env-gated counter such as
`TANKS_COUNT_ITEMS` works. Never leave the counter in the tree. Even better: use
the harness in 7.1, which counts items *and* measures real frames without
touching the tree at all.

### 7.1 Measured again — the real cause was reading a QString property every frame

The item count was a red herring for the *current* slowness. Restoring the full
brickwork sprite in section 6.3 added ~2 700 items and the user reported the game
crawling ("el rendimiento está lento"). Measured on the real display with the
harness below:

| Build | QQuickItems | frame period | ~fps |
|---|---|---|---|
| checkpoint `ec59cd9` | 7.7 k | **40 ms** | 25 |
| with the new brickwork, before the fix | 10.3 k | **130 ms** | 7.7 |
| after the fix (this tree) | ~10 k | **14 ms** | 71 |

The experiments that isolated it (`/tmp/opencode/exp`, a throwaway copy of the
tree, `sed`-patched and rebuilt):

| Variant | period |
|---|---|
| as-is | 130 ms |
| `brickStripGone()` returns a constant — no string read | **27 ms** |
| `brickStripGone()` re-runs every frame but reads no string | **31 ms** |

So it was **not** the item count, the draw calls, the GPU (a Radeon, measured) or
the per-frame *re-evaluation* of the bindings: it was **reading the `boardQ`
QString property from QML**. Roughly 1 250 reads of an 1 800-char property per
frame cost ~100 ms, i.e. ~80 µs per read — by far the most expensive thing QML
can be asked to do here. `backend.board` (the 225-char string in every tile's `t`
binding) had the same shape and was the reason even the checkpoint build sat at
40 ms.

**Rule from now on: never drive a per-frame QML binding from a QString
property.** Expose numbers and invokables keyed on `boardRev`, and read the
string form only in tests and probes.

Fix, in three parts:
1. `board` / `boardQ` no longer notify `frameChanged`; board state has its own
   `boardChanged` (bumped only by `bumpBoard()`).
2. The tile `t` string became `property int kind: backend.tileKind(cx, cy,
   backend.boardRev)`; the forest rim and the hole rectangles do the same.
3. `boardQ` was deleted outright; the strip query is
   `brickStripGone(x, y, strip, rev)` returning a bool.

**How to re-measure** (throwaway, keep it out of the repo). `/tmp/opencode/perf`
holds a harness that links the project's own objects, loads the real
`qrc:/Main.qml` on the real platform, calls `startGame()` after 0.5 s and
reports, over 4 s of play:

- `items=`: `QQuickItem` count in `contentItem()`, recursively.
- `period`: the interval between `QQuickWindow::frameSwapped` signals (the honest
  user-visible frame time).
- `work`: time from `beforeSynchronizing` to `afterRendering`, i.e. the frame's
  own CPU work. A near-zero `work` with a huge `period` means the GUI thread is
  busy *outside* the render, which is exactly the signature of the string-read
  bug above.

Build and run exactly like the visual probe (section 8.2) but with
`/tmp/opencode/perf/main.cpp`; `/tmp/opencode/build_and_measure.sh <tree> <out>
<runs>` does the flag scraping and the runs in one go. Note the window size is
chosen by the compositor and varies between runs (942x1028 or 1898x1028 have both
been seen) — quote it with the numbers.

---

## 8. Verification workflow

```sh
cd ~/Work/tanks

bin/build                                  # qmake6 + make, self-heals stale builds
bin/test                                   # 20 test slots, TANKS_SEED=7, offscreen
QT_QPA_PLATFORM=offscreen ./build/tanks    # QML load smoke test; must print no warnings
./build/tanks                              # real run
```

Important:
- `QML_DISABLE_DISK_CACHE=1` is set in `src/main.cpp`. **Do not remove it** — it
  stops stale QML being served from `~/.cache/tanks/qmlcache`, which caused at
  least one "the fix isn't showing up" session.
- The footer shows `BUILD <buildId>`. The id comes from the binary mtime, so it
  always names the exact binary on screen. **Any screenshot must show it.**

### 8.1 Taking one verified screenshot

1. Launch the game and reach the target state (strips sheared, PICK on).
2. Get the window geometry: `hyprctl clients -j` -> use the entry's `at` (x, y)
   plus the **monitor offset**, obtained by matching the monitor **by `id`, not by
   name**.
3. `grim -g "X,Y WxH" out.png`.
4. Zoom with nearest-neighbour to inspect pixels:
   `ffmpeg -vf "crop=...,scale=...:flags=neighbor"`.
5. Check: brick strips sheared on the correct face, the base guard turned to
   concrete, the build id visible, the HUD not frozen.

The drawback: the menu's START is a `MouseArea` with no keyboard activation, and
this environment has no pointer-injection tool (`wtype` sends keys only, and
`ydotool` / `wlrctl` are not installed). You cannot click START from a script.

### 8.2 Offscreen visual probe — verify rendering without touching the tree

This is how the two fixes in section 6.3 were proven to render correctly, and it
is the way to check QML output when the screen is not usable. Everything lives in
`/tmp`, so nothing is rigged inside the repo:

1. Write a small `main.cpp` in `/tmp/opencode/vis` that includes `backend.h` and
   `theme.h`, **redefines `struct TanksTestAccess`** (the `friend` declaration in
   `backend.h` is what grants it access), creates `Theme` + `Backend`, sets the
   `theme` / `backend` / `buildId` context properties, loads `qrc:/Main.qml` and
   grabs with `QQuickWindow::grabWindow()`.
2. Build it against the project's own object files, reusing the generated
   Makefile's flags (zsh needs `${=VAR}` to word-split them):
   ```sh
   cd ~/Work/tanks/build
   CXXFLAGS=$(sed -n 's/^CXXFLAGS *= *//p' Makefile | sed 's/\$(DEFINES)//')
   DEFINES=$(sed -n 's/^DEFINES *= *//p' Makefile)
   INCPATH=$(sed -n 's/^INCPATH *= *//p' Makefile)
   LIBS=$(sed -n 's/^LIBS *= *//p' Makefile | sed 's/\$(SUBLIBS)//')
   g++ ${=CXXFLAGS} ${=DEFINES} ${=INCPATH} -I../src -w -c /tmp/opencode/vis/main.cpp -o /tmp/opencode/vis/vis.o
   g++ -Wl,-O1 -o /tmp/opencode/vis/vis /tmp/opencode/vis/vis.o \
       backend.o theme.o qrc_resources.o moc_backend.o moc_theme.o ${=LIBS}
   QT_QPA_PLATFORM=offscreen /tmp/opencode/vis/vis
   ```
   `qrc_resources.o` is what makes `qrc:/Main.qml` resolvable, so the harness
   renders the **real** QML, not a copy.
3. Find the board item by walking `contentItem()->childItems()` for the biggest
   square item (it is `15 * cell`), then `mapToItem()` **each item you care
   about** to get its window position.
4. **Never guess a pixel origin.** Two traps bit this harness: the board's 4px
   border means the tile grid does *not* start at the board's `x`, and the
   compositor can resize the window between the grab and the read. Probing from
   a guessed origin produced a picture that looked like a 43px brick block with an
   11px gap — pure measurement error; the rect on screen really is 15px wide.
   Instead: locate the item (the hole `Rectangle`, the tile `Item`) and probe
   `item->mapToItem(window->contentItem(), QPointF(dx, dy))`. Same for the
   bitmaps: render each pixel through `mapToItem` and print a 1 char per pixel
   map of the tile, which is how the strips were checked (a full-height 15px band
   of `.` for a shot from the left).
5. Classify each sample: a sample is a hole if it is the board colour
   (`~#050507`). The mortar base is `#2e1a10`, so only genuine holes classify as
   holes — that is what proves there is no 1px contour left around the gap.
6. `qInfo` output may not reach the terminal in a release build — write the probe
   report to a file instead.

Two traps this harness already hit: a nested `QTimer` lambda that captured an
outer lambda's locals **by reference** produced a delayed `SIGSEGV` /
`std::bad_alloc` (put the state on the heap and capture the pointer), and
`save()` inside a lambda must not depend on lifetimes either.


---

## 9. Environment gotchas (these cost a lot of time)

- **`pkill -f build/tanks` kills the invoking shell.** Use `pkill -x tanks`.
- **`hyprctl` here is a Lua shim**: `hyprctl dispatch focuswindow` does **not**
  exist. Available: `hl.dsp.focus({direction=...})`, `hl.dsp.window.cycle_next()`,
  etc. Keyboard focus could not be forced from the shell.
- **Focus stealing** by Steam / the browser is common, and the game window lands
  on different monitors between runs. Verify geometry immediately before capture.
- `SoundEffect.playing` is **read-only** in Qt 6. A binding on it silently kills
  the whole QML load (no clear error). The engine loop is now driven by a `Timer`.
- `Model size of -98` and `Unable to assign [undefined] to QString` at
  `Main.qml:539` were both real and are fixed; do not reintroduce the patterns
  that caused them.

### 9.1 QML pitfalls (hard-won, both are traps)

1. **String-grid sprite reads inside nested delegates must go through an `id`.**
   Reading the model role / scope name directly inside a nested `Repeater`
   resolves to `undefined`. That is why `PickupIcon.qml` reads `root.bolt[r][c]`
   and `Main.qml` reads `tileItem.cy` / `forestTile.canopy(...)`.
2. **Never bind to read-only properties** (see `SoundEffect.playing` above).

---

## 10. Sprite reference (`src/PickupIcon.qml`)

Grids are string arrays of characters, each mapped to a colour and sized with
`Math.max(1, Math.floor(root.side / N))` so pixels stay integral.

| Kind | Grid | Columns | Colours |
|---|---|---|---|
| 0 SPEED | `bolt` 9x12 | 9 | `K` #1a1a22 outline, `Y` #ffdf3d body, `O` #f5a623 shade, `W` #ffffff highlight |
| 1 SHIELD | `shield` 7x8 | 7 | `X` #8d8d94 body, `W` #d8dbe2 highlight |
| 2 PICK | `pick` 9x9 | 9 | `P` #c9ced6 steel, `p` #8b9099 shade, `H` #8a5a32 wood handle |
| 3 LIFE | tank icon | — | whitish-gray tank |
| 4 GRENADE | `grenade` 7x7 | — | dark green frag body with `X` highlights |
| 5 CANNON | `cannon` | — | bronze shell |
| 6 FREEZE | `snow` 7x7 | — | pale snowflake |

The bolt is deliberately a **sharp zigzag with a hard middle step** (a smooth
curve read as a banana). The pick is an **arched head with downward tips plus a
wood handle** (a straight bar read as a knife). Legibility was the whole point.

The icon is still a pickaxe, but its **effect** is the Battle City shovel
(section 4.2): it armours the base wall. The name "PICK" is the user's and stays.

---

## 11. Next move, in order

1. ~~Revert the scaffolding.~~ **Done** (section 6.1).
2. ~~Rebuild clean and prove the baseline.~~ **Done.**
3. ~~Fix the QML item bloat.~~ **Done and measured** (section 7).
4. ~~Fix PICK and the brick damage model.~~ **Done.**
5. ~~Make the brick rule 50%-based and rebuild the base with PICK.~~ **Done**:
   20/20 test slots pass and the pixel probe shows a gap with no contour.
6. ~~Fix the frame rate.~~ **Done and measured**: 130 ms -> 14 ms a frame; the
   cause was per-frame QString property reads, not the item count (section 7.1).
7. ~~The user tests on screen — the gate for the commit.~~ **Done**: the user
   played and confirmed the contour-free gaps, the per-strip advance (which they
   prefer, section 5), PICK rebuilding the wall and crushing a parked tank, and
   the frame rate. The "CANNON ate my lives" report was withdrawn after a second
   session (section 5).
8. ~~Commit.~~ **Done** — one commit, message in section 12.
9. Optional: a verified screenshot for the record (section 8.1).

**No open work item right now.** Nice-to-haves: gamepad support (section 5,
`Known missing`) and seeing the menu legend size bump on screen.

---

## 12. The commit that was made

The checkpoint `ec59cd9` was already in. On top of it, one commit:
**"Shear bricks in quarter strips, make PICK rebuild the base wall, fix the frame
rate"** — the four changes described in section 6.3, which touch the same QML
tile code and were verified together (tests + the user playing it).

The message, for the record:

```
Shear bricks in quarter strips, make PICK rebuild the base wall, fix the frame rate

- A shot takes a 1/4 strip off the face it arrives from (horizontal shots take
  width, vertical shots take height) and flies on through strips it has already
  opened, stopping on the masonry that is left.
- Under half destroyed a tile still blocks as a whole block; from then on only
  the surviving strips block, so a tank noses further in with every strip it
  loses. Same from every face.
- The gap is painted as nothing (board colour) over the whole cell slice, edges
  included: no mortar rim, no lighter fill, no contour line.
- PICK rebuilds all five guard tiles as Steel whatever state they were in, and
  crushes any tank caught where the wall lands (enemy: no score; player: one
  life). Same crush on the revert, so nobody is sealed in. Steel stays
  indestructible.
- Board state has its own boardChanged signal and its own revision counter;
  QML's tile queries are int-valued invokables keyed on boardRev. Reading a
  QString property from QML every frame cost ~100 ms of the 130 ms frame, so
  boardQ is gone and board() is cached. Frame time 130 ms -> 14 ms.
- Tests cover the slice model, the 50% law, the passage rule and the PICK
  rebuild + crush (20 slots).
```

For the next commits, the same rules apply: **never commit scaffolding** — no
`// TMP RIG` blocks, no `TANKS_COUNT_ITEMS` counter, no `Window.FullScreen` — and
only after the thing works and the user has seen it.

---

## 13. Lessons learnt (why work was lost before)

- **The blocker was never the model change — it was an uncommitted tree mixing
  three things a newcomer cannot tell apart:** real fixes, debug rigs, and
  convenience changes. Without git history, anyone must re-read the diff and
  *guess the intent*, which risks reverting a real fix or committing a rig.
- **"Works in my head" is not "verified".** `bin/test` is executable memory: the
  cheapest protection against losing decisions when the model changes.
- **Never leave a rig in the tree.** It contaminated a real measurement once and
  led to a misleading diagnosis. A harness that has to touch the app belongs
  outside the repo (section 8.2).
- **Commit small, commit often, commit early.** A dirty tree becomes a liability
  the moment the operator or the model changes.
- **When a spec is ambiguous, ask before building it.** The previous model read
  "divide the damage into 4" as a request to redraw the sprite; the user meant the
  damage only. One short question ("which part of the tile, and how does it
  look?") avoided a second wasted iteration.
- **Backend tests are not enough for a visual fix.** The strip maths can be
  perfectly correct while the tile renders wrong. Probing the rendered pixels
  (section 8.2) is what actually closed this one.
- **A crash in your own harness is not a crash in the app.** The delayed
  `bad_alloc` looked alarming for a minute; the backtrace pointed straight at a
  dangling capture in the throwaway harness.
- **A performance cure can only be found by measuring, and the answer is often
  not where you are looking.** The obvious suspect (10 000 QQuickItems) was
  innocent; one `sed`-patch that made a single function return a constant took
  the frame from 130 ms to 27 ms and named the real culprit in one step. Cheap
  experiments on a throwaway copy of the tree beat reasoning about the diff.
- **The GUI thread, not the GPU, was the bottleneck.** `work`
  (`beforeSynchronizing` -> `afterRendering`) sat at 0.01 ms while the frame took
  130 ms: that signature means the cost is in QML binding evaluation, not
  rendering. Learn to read it.
- **Never guess screen coordinates in a visual probe.** Adding the border inset
  by hand gave a picture of an 11px gap where the real one was 15px, and nearly
  sent the work chasing a phantom rendering bug. `mapToItem` from the item you
  are actually testing removes the guesswork; when a measurement looks wrong,
  suspect the probe first.


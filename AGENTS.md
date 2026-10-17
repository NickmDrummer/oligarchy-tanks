# AGENTS.md — Tanks

> **This is the agent entry point for this repository.** Any model or agent
> picking up this project must read this file first, then `README.md` for the
> user-facing docs.
>
> Purpose: give an unambiguous picture of **what is solid, what is half-done and
> what is debris** — so we stop losing working code when the model changes.
>
> Keep it current: update the state sections (`Dirty working tree`, `Feature
> inventory`, `Next move`) whenever the state of the build changes.
>
> Snapshot: `HEAD = 7da113c`. The debug scaffolding has been reverted and the
> QML item-bloat fix is in; the tree is still **uncommitted** (see section 6).

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
    ├── tanks_tests.cpp  # 13 QtTest cases
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

### 4.1 Brick quarters — the important invariant

- `Tile` enum (`src/backend.h`):
  `Empty=0, Brick=1, Steel=2, Base=3, BaseDead=4, Forest=5`.
- `std::vector<uint8_t> m_brickQ` holds a **2x2 quarter mask per brick tile**.
  Bit order: **0 = top-left, 1 = top-right, 2 = bottom-left, 3 = bottom-right.**
- Which quarter a point falls in:
  `bit = (fx < 0.5 ? 0 : 1) + (fy < 0.5 ? 0 : 2)`, where `fx`/`fy` are the
  fractional parts inside the tile.
- A brick tile is whole (`0xF`) when generated. Each hit clears exactly one bit.
  When the mask reaches `0`, the tile becomes `Empty`.
- `boardQ()` exposes the mask to QML as **30x30 = 900 chars**, 2x2 per tile.
  Index: `(y*2 + qy) * COLS*2 + (x*2 + qx)` -> `'B'` if that quarter survives.
- **Locked behaviour — do not "fix" without updating the tests:** a bullet passes
  through a quarter that is already destroyed, and `solidAt()` only blocks on
  surviving quarters. This is what lets a tank fire from a fixed spot, chew a
  hole, and reach the steel behind it.

### 4.2 Bonus drop rule

Only **green (kind 1)** enemies drop a bonus, where they die (`spawnPickupAt`).
Exactly **one pickup at a time**; it expires after 15 s.
`PickupKind`: `Speed=0, Shield=1, Pick=2, Life=3, Grenade=4, Cannon=5, Freeze=6`.

| # | Name | Effect | Duration |
|---|---|---|---|
| 0 | SPEED | +35% movement | 20 s |
| 1 | SHIELD | blocks shots | 10 s |
| 2 | PICK | bullets smash steel | 20 s |
| 3 | 1UP | +1 life (cap 5) | instant |
| 4 | GRENADE | kills all enemies, awards their score, nuke sound | instant |
| 5 | CANNON | one-shot armour (big shell) | 20 s |
| 6 | FREEZE | enemies stop | 8 s |

### 4.3 Determinism

- `TANKS_SEED=<n>` seeds `m_rng`. `bin/test` runs with `TANKS_SEED=7` so the
  quarter-chipping test is reproducible.
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
- `?` help lists the 7 bonus icons; the menu legend shows them (now 40x40, `side: 36`).
- Brick quarter destruction (backend + `boardQ()` + 4-quarter QML rendering).
- Bullet pass-through of destroyed quarters.
- `buildId` derived from the running binary's mtime, shown in the footer.
- Stale-build self-heal in `bin/build` and `bin/test`.
- All tests pass (`bin/test`): 14 slots = `initTestCase` + 12 cases +
  `cleanupTestCase`.
- Board and bonus-icon rendering gated with `Loader`: **69 167 -> 7 204** items
  (measured, see section 7).

### Implemented but NOT yet verified on screen
- New / rewritten sprites: PICK (arched head + wood handle), SHIELD (flat top,
  pointed bottom, highlight), SPEED (sharp zigzag bolt).
- Rewritten forest canopy (global-coordinate dithered leaf field with real
  transparent holes and a ragged rim).
- Menu legend size bump (30 -> 40 px icons, `side: 28 -> 36`).
- Full-speed play after the item-count fix (the count is measured, the frame
  rate is not).

### Broken / unresolved
- Nothing blocking. See section 7 for the on-screen confirmation still pending.

### Known missing
- Gamepad support (path prepared, not implemented).

---

## 6. Current tree state

`HEAD = 7da113c`. Everything below is **uncommitted**.

```
 M README.md             +5      pointer to AGENTS.md
 M bin/build             +11     stale-build self-heal
 M bin/test              +14     stale-build self-heal + TANKS_SEED=7
 M src/Main.qml          +340    Loader gating, quarter rendering, forest, legend sizes
 M src/PickupIcon.qml    +243    Loader gating + new sprites
 M src/backend.cpp       +77     quarters, pass-through, TANKS_SEED
 M src/backend.h         +10     boardQ, damageBrickAt, m_brickQ, test friend
 M src/main.cpp          +9      buildId from the binary mtime
 M tests/tanks_tests.cpp +118    TanksTestAccess + 5 cases
?? AGENTS.md                     this file
```

### 6.1 Already reverted — debug scaffolding, do not bring it back

- `src/backend.cpp`: the two `// TMP RIG` blocks (auto `startGame()`, auto-fire
  timer, 500 ms log to `/tmp/rig_state.txt`; and the rigged empty map with
  `m_p[0].pickUntil = 9999`) plus `#include <QDebug>/<QDateTime>/<QFile>/<QTextStream>`.
- `src/backend.h`: `qint64 m_rigStart`.
- `src/Main.qml`: `visibility: Window.FullScreen`.
- A temporary item counter in `src/main.cpp` (env `TANKS_COUNT_ITEMS`) used to
  measure section 7 — removed after measuring.

`#include <cstdlib>` and the `TANKS_SEED` block in the constructor **stay**: they
are real features the tests depend on.

### 6.2 The real work, by file

- `bin/build`: stale-build guard (wipe + rebuild once from clean if any input is
  newer than `build/tanks`).
- `bin/test`: same guard for the test binary, plus `TANKS_SEED=7`.
- `src/backend.h`: `Q_PROPERTY(QString boardQ ...)`, the `boardQ()` decl,
  `friend struct TanksTestAccess;`, the `damageBrickAt()` decl, the `m_brickQ`
  member.
- `src/backend.cpp`: `TANKS_SEED` seeding; `boardQ()`; `generateMap()` setting
  `m_brickQ = 0xF` on every Brick tile; the quarter-aware `updateBullets()`
  branch; the quarter-aware `solidAt()`; `damageBrickAt()`.
- `src/main.cpp`: `buildId` from `QFileInfo(applicationFilePath()).lastModified()`.
- `tests/tanks_tests.cpp`: `TanksTestAccess` plus the new cases
  (`shotsChipQuarters`, `shotChipsOneQuarter`, `shotsPassThroughBrokenQuarter`,
  `pickBonusBreaksSteel`, `pickReachesSteelBehindBrick`) and the `boardQ().size()`
  assertion in the first test.
- `src/PickupIcon.qml`: new sprites **and** `Loader` gating per icon kind.
- `src/Main.qml`: brick quarter rendering, `Loader` gating per tile kind, forest
  canopy, legend icon sizes.
- `AGENTS.md` / `README.md`: this handover and a pointer to it.

---

## 7. FIXED — QML item bloat was stalling the GUI thread

**Symptom (measured with the old debug rig).** Game time advanced at ~5% of wall
time (`t=0.05 wall=2536` ... `t=1.35 wall=25620`), `dt` sat pinned at the `0.05`
clamp, and the early game looked frozen. That made earlier "visual proof"
screenshots misleading: steel looked intact and only one quarter was chipped,
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
not. Play a game and check it runs smoothly.

**How to re-measure** if it ever regresses: temporarily walk
`QQuickWindow::contentItem()`'s `childItems()` after `engine.load()`, count
`QQuickItem` instances, then delete the snippet. An env-gated counter such as
`TANKS_COUNT_ITEMS` works. Never leave the counter in the tree.

---

## 8. Verification workflow

```sh
cd ~/Work/tanks

bin/build                                  # qmake6 + make, self-heals stale builds
bin/test                                   # 14 test slots, TANKS_SEED=7, offscreen
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

1. Launch the game and reach the target state (quarters chipped, PICK on).
2. Get the window geometry: `hyprctl clients -j` -> use the entry's `at` (x, y)
   plus the **monitor offset**, obtained by matching the monitor **by `id`, not by
   name**.
3. `grim -g "X,Y WxH" out.png`.
4. Zoom with nearest-neighbour to inspect pixels:
   `ffmpeg -vf "crop=...,scale=...:flags=neighbor"`.
5. Check: quarter bricks visibly destroyed, steel broken by PICK, build id
   visible, HUD not frozen.

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

---

## 11. Next move, in order

1. ~~Revert the scaffolding.~~ **Done** (section 6.1).
2. ~~Rebuild clean and prove the baseline.~~ **Done**: clean build with no
   warnings, 14/14 test slots pass, offscreen load is silent.
3. ~~Fix the QML item bloat.~~ **Done and measured** (section 7).
4. **Verify on screen** (the user does this): smooth full-speed play, bricks
   chipping quarter by quarter, PICK breaking steel, forest canopy legible and
   hiding tanks, and the seven bonus icons readable in the menu legend.
5. **One verified capture**: build id visible, gameplay not frozen, quarter
   damage and PICK-on-steel visible.
6. **Commit in atomic pieces** (section 12) — only after the user agrees and has
   tested.

---

## 12. Proposed commit plan (atomic, ordered)

Main.qml and PickupIcon.qml are heavily entangled, so the split below is by
concern and by file. If the user prefers a finer history, `git add -p` can split
either file further.

1. `Damage bricks per quarter and let shots pass through broken quarters`
   -> `src/backend.h`, `src/backend.cpp`, `tests/tanks_tests.cpp`, and the brick
   quarter block in `src/Main.qml`.
   *Coupling note:* the tests use `boardQ()` and `TanksTestAccess`, so backend and
   tests must land together.
2. `Legible bonus sprites, seamless forest, and Loader gating`
   -> `src/PickupIcon.qml` and the tile / forest / legend size changes in
   `src/Main.qml`. This is the change that took the item count from 69 167 to
   7 204 (section 7).
3. `Self-heal stale builds and seed the test map`
   -> `bin/build`, `bin/test`.
4. `Stamp the build id from the binary's own mtime`
   -> `src/main.cpp`.
5. `Add AGENTS.md handover and point README at it`
   -> `AGENTS.md`, `README.md`.

Never commit scaffolding: no `// TMP RIG` blocks, no `TANKS_COUNT_ITEMS` counter,
no `Window.FullScreen`.

---

## 13. Lessons learnt (why work was lost before)

- **The blocker was never the model change — it was an uncommitted tree mixing
  three things a newcomer cannot tell apart:** real fixes, debug rigs, and
  convenience changes. Without git history, anyone must re-read the diff and
  *guess the intent*, which risks reverting a real fix or committing a rig.
- **"Works in my head" is not "verified".** `bin/test` is executable memory: the
  cheapest protection against losing decisions when the model changes.
- **Never leave a rig in the tree.** It contaminated a real measurement this time
  and led to a misleading diagnosis.
- **Commit small, commit often, commit early.** A dirty tree becomes a liability
  the moment the operator or the model changes.

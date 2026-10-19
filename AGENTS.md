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
> Snapshot: `HEAD` is the **power-ups, round 1** (section 19) on top of the
> **section 18 round** — `0399f60` *Fix enemies getting stuck and a player who is
> out on the next sector* and `b41a9f6` *Recolour the tanks and redraw the
> wheeled chassis with a grey contour* — which in turn sits on the **enemy
> overhaul, phase C** (section 17) — *"Overhaul the enemies: 16 a stage,
> carriers, and the death/freeze/shield visuals"* — the UI overhaul, phase 1
> (section 16), the player-count select screen (commit **C**, section 12.2), the
> two gamepad commits (section 12.1 **A** and **B**) and the brick/PICK/frame-rate
> commit. **Nothing pushed.** It builds and `bin/test` passes **56/56**.
> **Section 19 is not committed and not played yet**: it is implemented,
> `bin/test` passes and the `/tmp/opencode/pick` probe checks it, and the gate is
> the user's play session.
> **Open:** one unreproducible `QSocketNotifier: Invalid socket 59 and type
> 'Read', disabling...` during a play session, together with a game that had to
> be interrupted with `^C` right when the base went down. It is not caused by
> anything the pad reader does (see section 15.5), so it is left to be
> reproduced with more information rather than fixed blind.
>
> The commit below it carries the 50% brick collision rule, PICK rebuilding the
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

A retro **Battle City–style tank game vs AI for one or two local players**,
shipped as an
Omarchy desktop app: **Qt 6 / C++ backend + QML frontend, single binary**, built
with `qmake6` + `make`.

- 15x15 random map per game; brick / steel / forest / base tiles.
- 2 player tanks (P1 yellow, P2 green) and four enemy kinds: gray basic and
  pastel-metallic-yellow wheeled fast, armored (pastel purple ->
  cyan -> gray as it takes hits, with treads) and the red-flashing carrier.
- **16 enemies per sector, always**; what changes with the sector is the mix
  (more fast and more armored). See section 17.
- 7 bonuses, dropping only from carriers (the red-flashing basics).
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
├── pkgbuild/            # PKGBUILD, .desktop, .svg, tanks.install (input group note)
├── README.md            # user-facing docs (controls, build, gamepad)
├── tanks.pro
├── src/
│   ├── main.cpp         # app setup, QML_DISABLE_DISK_CACHE, buildId from binary mtime
│   ├── backend.h        # all game state + interface (Q_PROPERTY / Q_INVOKABLE)
│   ├── backend.cpp      # game loop, collisions, bonuses, sectors, sound
│   ├── padmap.h/.cpp    # PadState + PadMapper: what a pad control means (Qt-free)
│   ├── gamepad.h/.cpp   # evdev reader + hotplug, hands PadState to the mapper
│   ├── Main.qml         # window, HUD, board, sprites, overlays, help, footer
│   ├── TankSprite.qml   # 16x16 tank with animated treads
│   ├── ExplosionSprite.qml  # 16x16 death explosion (flash -> smoke -> fire)
│   ├── FrostSprite.qml      # 16x16 celeste frost patch for frozen enemies
│   ├── ShieldSprite.qml     # two frames of a jagged lightning ring (shield)
│   ├── PickupIcon.qml   # all 7 bonus sprites (per-pixel colour grids)
│   ├── PixelButton.qml
│   ├── KeyCap.qml       # one key, drawn as a cap (pixel arrows, no glyphs)
│   ├── PadIcon.qml      # stick / d-pad / trigger / face button, drawn
│   ├── theme.cpp/.h     # accent + palette read from the Omarchy theme
│   ├── resources.qrc
│   ├── fonts/           # Silkscreen Regular + Bold + OFL.txt (bundled)
│   └── sounds/*.wav     # engine, shoot, eshoot, hit, explode, power, nuke
└── tests/
    ├── tanks_tests.cpp  # 35 QtTest cases (37 slots)
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
  `setP2Move(...)`, `fireP1()`, `fireP2()`. Keyboard and gamepad are **two
  sources for the same tank**: `tick()` ORs the keyboard flags with
  `m_padHeld[player][dir]`, so letting go of a key never cancels a held stick,
  and vice versa. Fire is edge-triggered in the mapper and reuses `fireP1()` /
  `fireP2()`. See section 14.
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

World constants (`src/backend.h`): `COLS = ROWS = 15`, `TOTAL_ENEMIES = 16`,
`totalForLevel() = 16` (constant — section 17), max concurrent
`min(4+(level-1), 6)`, `enemySpeedMult() = 1.0 + 0.1*(level-1)`.

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

Only **carriers (kind 3)** drop a bonus, where they die (`spawnPickupAt`). A
carrier is one of the basic tanks: slightly faster, flashing red<->gray, and two
per stage (section 17). Green runners no longer drop anything.
Exactly **one pickup at a time**; it expires after 15 s.
`PickupKind`: `Speed=0, Shield=1, Pick=2, Life=3, Nuke=4, Cannon=5, Freeze=6,
LevelUp=7, BlastMissile=8` (`Grenade` was renamed to `Nuke` in section 19; the
sound is still `nuke.wav`).

| # | Name | Effect | Duration |
|---|---|---|---|
| 0 | SPEED | +35% movement | 20 s |
| 1 | SHIELD | blocks shots | 10 s |
| 2 | PICK | base wall becomes concrete | 15 s |
| 3 | LIFE | +1 life (cap 5), or revives an eliminated partner | instant |
| 4 | NUKE | kills all enemies, awards their score, nuke sound, board-wide blast drawn by QML | instant |
| 5 | CANNON | one-shot armour (big shell) | 15 s |
| 6 | FREEZE | enemies stop, and blink through their last 2 s | 8 s |
| 7 | LEVELUP | raises offensive tier (bullet speed, double shot, armour, air support) | instant |
| 8 | BLAST | blast missile for 5 s: slower shot, area damage, passes through blocks | 5 s |

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

**Steel is no longer indestructible, but only CANNON shells chip it.** A CANNON
bullet (big shell) shears steel by the same 4-strip + 50% law as brick; a
normal shot is stopped by it. The base wall (the five guard tiles granted by
PICK) is concrete: it never chips and never passes, no matter what hits it.
The old "bullets smash steel while PICK is active" behaviour, the per-player
`Tank::pickUntil` and the `Bullet::pick` flag are **gone** — do not
reintroduce them. Implementation: `m_brickQ` now carries a strip mask for steel
tiles too (0xFF whole, 0 when fully chipped), `tileBlocksPoint()` applies the
strip law to steel unless the cell is in `m_baseGuard`, `chipBrick()` chips
steel unless it is a guard tile, and `updateBullets()` only chips steel on a
`big` bullet. `setBaseGuard(bool)`, `crushTanksIn()`, `m_baseGuardUntil`,
`m_baseGuard`, `updateBaseGuard()`, called from `tick()`.

### 4.3 Determinism

- `TANKS_SEED=<n>` seeds `m_rng`. `bin/test` runs with `TANKS_SEED=7` so map
  generation is reproducible.
- Env-based seeding is a **real feature** (tests depend on it) — keep it.

---

## 5. Feature inventory

### Verified working
- Full game loop; 15x15 random map with brick / steel / base / forest.
- A mode select screen (1 / 2 players) at boot and after a game over.
- One or two player tanks + four kinds of enemies, bullets, collisions, lives,
  score.
- 16 enemies a stage, with the mix by sector (section 17).
- The death explosion, the frost under frozen enemies and the shield aura:
  hand-drawn pixel sprites, user-confirmed in play (section 17).
- Pause (`P`), restart (`R`), sound toggle (`M`), help (`?`), quit (`Q`).
- Help auto-pauses the game and resumes only if the help caused the pause.
- Victory overlay with sector progression and scaling specials; a game over (or a
  destroyed base) returns to the select screen, which shows the score of the run.
- Pixel Omarchy wordmark base, plus rubble when destroyed.
- 7 bonuses, carrier-only drops, one at a time.
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
- All tests pass (`bin/test`): **56 slots** = `initTestCase` + 54 cases +
  `cleanupTestCase` (20 before the gamepad work: 3 mapper cases and 8 pad cases
  were added, plus 12 for the select screen, the player count and the game over
  screen — sections 14 and 15 — then 8 for section 18 and 4 for section 19).
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
- The UI overhaul, phase 1 (commit **D**, section 16): the Silkscreen pixel font,
  no Material style, and the select screen rebuilt out of key caps, pad pictures
  and a tank marker. `bin/test` 43/43 and the QML probe 37/37, and the user
  approved the screenshots — but **nobody has played it yet**.
- **The round of section 18**: the stuck enemies, the eliminated player, the tank
  sprites and the palette. Committed after the user approved the wheeled chassis
  and asked for the fast tank's pastel yellow; the yellow itself was never seen in
  play.
- **The power-ups, round 1** (section 19): the sprites, the two durations and the
  two effects. Not committed, not played.
- Nothing else. The player-count select screen and the game over screen
  (section 15) were played by the user (1 and 2 players, one and two pads,
  dying alone, `START` to carry on, switching modes with the pad, `R`, pause,
  unplug/replug) and every step behaved; the old "menu legend 40 px" item went
  away with the legend itself (section 16).

### Broken / unresolved
- One unreproducible `QSocketNotifier: Invalid socket 59 and type 'Read',
  disabling...` with a game that had to be interrupted with `^C` (section
  15.5). Not blocking, and not reproduced by two stress runs; left for more
  information rather than fixed blind.

### Known missing
- Nothing blocking the game as it stands.

---

## 6. Current tree state

`HEAD` is **`b41a9f6`** *Recolour the tanks and redraw the wheeled chassis with a
grey contour* on top of **`0399f60`** *Fix enemies getting stuck and a player who
is out on the next sector* — the round of section 18 — which sits on the **enemy
overhaul, phase C** (section 17), the **UI overhaul, phase 1** (commit **D**,
section 16), *"Add a player-count select screen and a game over screen"* (commit
**C**, section 12.2), *"Read gamepads from /dev/input and let one join a player"*,
*"Add a gamepad mapping layer and per-source movement flags"* (section 12.1 **A**
and **B**) and the brick/PICK/frame-rate commit (`f0d5eb4`). Nothing pushed.
`bin/test` passes **56/56**.

**In the working tree, uncommitted: the power-ups, round 1** (section 19) —
`src/backend.h/.cpp` (the `Nuke` rename, the 15 s PICK and CANNON, `nuke()`,
`updateNuke()`, `freezeEnding()`), `src/PickupIcon.qml` (all seven sprites plus
the wrapper-Item centring fix), `src/Main.qml` (the NUKE blast, the freeze blink,
the labels and the manual rows), `tests/tanks_tests.cpp` (4 slots) and this file.
The gate is the user's play session.

### 6.0 The round of section 18, by file

```
0399f60  Fix enemies getting stuck and a player who is out on the next sector
   src/backend.cpp         blockedByTank() as a movement test, spawnEnemy() -> bool
                           + cellBusy(), the 3x3 entry pads, openSpawnLanes(),
                           buriedInSolid()/nearestFreeSpot(), bestDir() + the 0.8-2.3 s
                           commitment, separateTanks() terrain check, resetTanks() ->
                           clamp at 0 and leave an out player out, killPlayer(),
                           revivePlayer()
   src/backend.h           the same declarations, freezeEnding() (section 19 adds
                           nuke()/updateNuke())
   tests/tanks_tests.cpp   8 cases (stuck enemies, the eliminated player)

b41a9f6  Recolour the tanks and redraw the wheeled chassis with a grey contour
   src/TankSprite.qml      the outline property (#6b7280), variant 1 = the user's
                           wheeled chassis, variant 2 = the thicker armoured hull,
                           the interleaved tread frames (cells on every row)
   src/Main.qml            P2 green, the fast tank pastel yellow, enemyVariant,
                           carrierFlash on the new grey, the queue on win.foeGray*,
                           win.treadFrame at 90 ms
   README.md               the 16 enemies a sector line
   AGENTS.md               section 18
```

Phase C contains exactly these files:

```
   src/backend.h          TOTAL_ENEMIES = 16, totalForLevel() constant,
                          kind 3 carriers, fixed hp 3 armor, Explosion +
                          m_explosions, pushExplosion/updateExplosions,
                          explosions()/explosionAt(), the 3 s spawn grace
   src/backend.cpp        composition by level, carriers interleaved at 1/3 and
                          2/3 of the basic pool, carrier-only drops, max
                          concurrent 4-6, explosions on every enemy death,
                          explosion lifetime 0.5 s
   src/ExplosionSprite.qml  new: 16x16 pixel flash -> smoke -> fire (0.5 s)
   src/FrostSprite.qml      new: 16x16 celeste + white frost patch
   src/ShieldSprite.qml     new: two frames of a jagged, 4-fold symmetric
                            lightning ring, alternating
   src/Main.qml           armored hp colours, carrier flashing, frost patches
                          (enemies only), the explosion delegate, the shield
                          loader
   src/resources.qrc      the three new QML components
   tests/tanks_tests.cpp  the stage composition (16 tanks, kinds by level,
                          carriers interleaved) and 16/16 in the level test
   AGENTS.md              section 17
```

The tree is **clean** again: phase C is one commit, so the next feature starts
from a checkpoint, which is the rule this project learnt the hard way
(section 13).

Commit **D** contains exactly these files:

```
Set the UI in a pixel font, drop Material and rebuild the select screen
   src/fonts/             Silkscreen Regular + Bold + OFL.txt, bundled
   src/resources.qrc      KeyCap.qml, PadIcon.qml and the two font files
   src/main.cpp           Basic style, Silkscreen registered on the application
   src/KeyCap.qml         one key, drawn as a cap (pixel arrows)
   src/PadIcon.qml        stick / d-pad / trigger / face button, drawn
   src/Main.qml           no font families, select screen rebuilt, key caps
   src/PixelButton.qml    font family dropped
   pkgbuild/PKGBUILD      the font's licence is installed with the project's
   README.md              the select screen's keys, the join hint, the credits
   AGENTS.md              section 16
```

The throwaway probes (`/tmp/opencode/qmlprobe`, `/tmp/opencode/shots`) stay
outside the repo.

`build/` is git-ignored, and so are `pkgbuild/pkg/`, `pkgbuild/src/` and
`pkgbuild/*.pkg.tar.zst` (checked: `git status` stays clean after `makepkg`).

The two gamepad commits contain exactly these files (line counts omitted on
purpose — this file changes itself):

```
 A 2d31ac5  Add a gamepad mapping layer and per-source movement flags
   src/padmap.h/.cpp      PadState, PadSink, PadMapper (Qt-free, unit tested)
   src/backend.h/.cpp     pad registry, pad* invokables, per-source m_padHeld
   tests/tanks_tests.cpp  3 mapper cases + 8 pad cases
   tests/tanks_tests.pro  padmap.cpp

 B 3a756d0  Read gamepads from /dev/input and let one join a player
   src/gamepad.h/.cpp     evdev reader, hotplug, sysfs filter, TANKS_PAD_DEBUG
   src/main.cpp           create and start Gamepad after Backend
   tanks.pro              padmap.cpp, gamepad.cpp
   src/Main.qml           pad labels, join prompt, manual lines, helpRequested
   README.md / AGENTS.md  docs
   pkgbuild/              install=tanks.install + the input group note
```

Both were verified **in isolation** before landing (a throwaway `git worktree` at
commit A: `bin/build` clean and 31/31 slots), so each commit builds on its own.

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

### 7.2 Measured: the gamepad hotplug scan froze the event loop

The gamepad reader rescans `/dev/input` once a second so pads can be plugged in
and out while playing (section 14). The first version called `open()` +
`EVIOCGBIT` + `close()` on **every** node it had not identified yet, on every
scan. Measured with the harness in section 14 (`worst event-loop stall`, a
`waitMs()` that reports how much longer than asked it took):

| Build | scan cost | worst stall |
|---|---|---|
| probing every node, every second | **~620 ms** | **633 ms** |
| with the sysfs filter + rejected-node cache | **0-2 ms** | **1-2 ms** |

A 620 ms freeze every second is a broken game, and it would have shown up as
"the pad works but the game stutters". Two causes, both measured from outside
the repo with `/tmp/opencode/pad/probe.c`:

- **`close()` on an evdev node costs ~20 ms here** (`open` and the ioctls are
  free: 0 ms). 36 nodes x ~18 ms = 644 ms, all of it in `close()`.
- The nodes are re-probed on **every** scan, once a second.

Fix, in two parts:

1. **`m_rejected`** (`gamepad.h`): a path that has been looked at and is not a
   gamepad is not looked at again, so the steady-state scan is just a
   `QDir::entryList` (0 ms). A path that disappears is dropped from the set, so
   a node reused by a new device is examined again.
2. **`looksLikeGamepad()`** (`gamepad.cpp`): the capability masks are in sysfs
   too (`/sys/class/input/eventN/device/capabilities/{abs,key}`, words printed
   most significant first in hex), so the "is this a stick with gamepad
   buttons?" test is answered by reading two small files and only real
   candidates are ever opened. If sysfs cannot answer, the function returns true
   and the node is probed the old way — so the filter can only ever make it
   faster, never blind.

That took the first scan from **601 ms to 2 ms**. The user's real DualSense and
both `uinput` pads still pass the filter (verified: the harness reports them).

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

**Every part is an `Item` wrapping the sprite.** The `Loader` that shows one icon
(see section 7) resizes the **root** item of the component it loads, so a `Grid`
as the component's root gets pinned to (0,0) at the loader's size: every icon
used to sit in the **corner** of its box instead of the middle of it (measured:
ink 4 px from the left and 19 from the right in a 45 px box). One level down the
sprite keeps the geometry it declares and the box centres it (10/10/10/10).
The grid's own origin is `Math.round((parent.width - width) / 2)`: a half-pixel
origin antialiases the whole sprite (AGENTS.md section 17.2).

| Kind | Grid | Columns | Colours |
|---|---|---|---|
| 0 SPEED | `bolt` 9x12 | 9 | `K` #d9a11b amber rim, `Y` #ffdf3d body, `O` #f5a623 tail shade, `W` #fff3bd edge |
| 1 SHIELD | `shield` 7x8 | 7 | `X` #8d8d94 body, `W` #d8dbe2 highlight |
| 2 PICK | `pick` 9x9 | 9 | `P` #c9ced6 steel, `p` #8b9099 shade, `H` #8a5a32 wood handle |
| 3 LIFE | tank icon | — | whitish-gray tank at `side * 0.78` |
| 4 NUKE | `nuke` 13x15 | 13 | `G` #6d7d4a body, `D` #464f2c shade, `L` #9cb073 highlight, `Y` #e8c53c disc, `K` #1b1c20 trefoil |
| 5 CANNON | `cannon` 9x9 | 9 | `B` #c8862e brass, `D` #8a5a20 case, `W` #f6e8cc shine |
| 6 FREEZE | `snow` 9x9 | 9 | `X` #9fd8ff arms, `W` #ffffff core |

The bolt is deliberately a **sharp zigzag with a hard middle step** (a smooth
curve read as a banana). The pick is an **arched head with downward tips plus a
wood handle** (a straight bar read as a knife). Legibility was the whole point.

**No icon carries a near-black outline.** The bolt's rim and the CANNON's old
outline (#1a1a22, #14141a) were what made those two illegible, and the grey
contour rule of section 18.3 applies to sprites as much as to tanks. The nuke is
a mushroom cloud, not a device: it reads at 9x9 where a bomb did not.

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
10. ~~Gamepad support.~~ **Implemented, verified and committed** (sections 14 and
    12.1): 31/31 test slots, the `/tmp/opencode/pad` harness driving the real
    reader with two virtual pads *and* the user's real DualSense, `bin/build`, the
    offscreen smoke run, the user's own play session ("funciona perfecto") and a
    measured **620 ms -> 2 ms** fix for the hotplug scan (section 7.2).
11. ~~The user plays with the DualSense on screen.~~ **Done**: that was the gate,
    and it passed.
12. ~~The two commits of section 12.1.~~ **Done**: `2d31ac5` (A) and `3a756d0`
    (B).
13. **The player-count select screen** (section 15). Implemented: 37/37 slots, the
    QML probe and the uinput harness both drive the new flow.
14. **The user plays it** (1 player and 2, one pad and two, dying alone, `START`
    to retry from the game over). **This was the gate for commit C.**
15. Commit **C** (section 12.2). **Done**, and played.
16. **The UI overhaul, phase 1** (section 16): the bundled pixel font, no more
    Material style, and the select screen rebuilt from key caps, pad pictures
    and a tank marker. **Committed (section 16)**: 43/43 slots, 37/37 QML probe checks,
    the user approved the result from the screenshots.
17. **The user plays commit D.** Still owed: it was approved from screenshots,
    not from a play session. Any surprise there is a small follow-up commit.
18. **Phase C, the enemy overhaul** (section 17). **Committed.** The user played
    the death explosion, the frost and the shield across several rounds and
    asked for the commit; the fixed **16 tanks a stage** was the one piece not
    played (it was implemented at commit time — section 17).
19. **Phase 2 of the overhaul**: the manual (`?`) first, then the in-game
    interface, both from the user's notes, one at a time — and each one with the
    same gate: user plays, then commit.
20. ~~**The round of section 18**~~ **Committed** as `0399f60` (the two bugs,
    § 18.1-18.2) and `b41a9f6` (the sprites and the palette, § 18.3), after the
    user approved the wheeled chassis and asked for the fast tank's pastel
    yellow.
21. **The power-ups, round 1** (section 19): the sprites, the two durations and
    the two new effects. Implemented and verified with `bin/test` (56 slots) and
    the `/tmp/opencode/pick` probe. **Waiting for the user to play, then
    commit.** Landed with the same batch: CANNON breaking steel by strip
    (section 19.4, now done). Still to come from the same list: the LEVELUP
    five tiers and the BLAST MISSILE. The `GAME` section of the manual is
    now implemented (C8).

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

### 12.1 The two gamepad commits — **made**

Both landed after the user played with the pad, in this order, each verified on
its own (B on top of A, and A alone in a throwaway worktree):

**A — `2d31ac5`, *Add a gamepad mapping layer and per-source movement flags***

```
Add a gamepad mapping layer and per-source movement flags

- src/padmap.*: PadState (one pad's controls), PadSink (what the mapper
  reports) and PadMapper (deadzone, trigger threshold, edge-triggered fire and
  start). Qt-free and device-free, so it is unit tested directly.
- Backend keeps pad state: a registry of connected pads (pads()/padsChanged),
  the pad* invokables the reader drives, and helpRequested for SELECT.
- Movement flags are kept per source and ORed in tick(), so letting go of a key
  no longer cancels a held stick and vice versa. The keyboard stays live for
  both players at all times.
- A pad joins with START: the first pad takes player 2 (one pad, one human on
  it) and a second pad takes player 1.
- Tests: 3 mapper cases and 8 backend cases on top of the brick/PICK ones.
```

**B — `3a756d0`, *Read gamepads from /dev/input and let one join a player***

```
Read gamepads from /dev/input and let one join a player

- src/gamepad.*: reads /dev/input/event* through evdev (linux/input.h, no new
  dependency), keeps a PadMapper per pad and rescans once a second so pads can
  be plugged in and out while playing. Every axis is scaled through its own
  EVIOCGABS range, so a DualSense (0..255) and an Xbox pad (-32768..32767)
  both work, and the d-pad is read from the hat or from BTN_DPAD_* buttons.
- The naive hotplug scan froze the event loop for ~620 ms a second (closing an
  evdev node costs ~20 ms here and every node was reopened on every scan). It
  now reads the capability masks from sysfs to skip what is not a gamepad and
  remembers the paths it rejected: 601 ms -> 2 ms on the first scan and a
  1-2 ms worst stall.
- UI: the player panels name the device that drives each tank (KEY / PAD n), a
  blinking PRESS START TO JOIN shows while a pad is waiting, SELECT opens the
  manual, and the manual lists the pad controls.
- Docs: README pad section, the input group requirement and TANKS_PAD_DEBUG,
  which prints what the reader sees; the package prints the group note.
```

### 12.2 Commit C — **made**

One commit for the select screen (section 15), after the user played it. Files:
`README.md`, `src/Main.qml`, `src/backend.cpp`, `src/backend.h`,
`tests/tanks_tests.cpp`, and this file. It is one change — the game's opening
screen and the single-player model — verified together, so it stays one commit.

```
Add a player-count select screen and a game over screen

- The game opens on a select screen: 1 PLAYER (one tank, and dying ends the
  run) or 2 PLAYERS (both tanks, as before). Left/Right or A/D move the
  highlight, the pad's stick and d-pad do the same, and Space/Enter/START/
  Square or a click confirms it; R confirms the highlighted mode too.
- One player means one tank: resetTanks() clears any slot at or above the
  player count and tick()/checkEnd() only see the tanks in the game, so the
  parked second tank no longer keeps a lost run alive.
- Every run ends on its own screen: GAME OVER, or BASE DESTROYED when the base
  was lost, with the score of the run and START / Space / Enter to carry on to
  the select screen. R mid-game also goes back to the select screen (on
  victory it advances the sector, as before) and leaves nothing on the board
  behind the overlay.
- A pad drives the select screen whatever its slot, assigned or not, so a pad
  that just played a run can still pick the next mode. Confirming a mode hands
  every pad back to waiting: which tank a pad gets is decided by the mode and
  by who presses START, never carried over from the run before.
- Each panel shows its tank and, until that player touches a control, a
  blinking PRESS SPACE / PRESS ENTER (OR START with a pad) hint under it.
- Tests: 43 slots (the select screen, both modes, both endings and what
  carries on from them, an assigned pad moving the highlight, pads released
  when a mode is confirmed).
```

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
- **Check that your harness actually rebuilt.** Twice in a row the `/tmp` gamepad
  harness read an *old* report and a stale binary, because the compile had failed
  and the runner swallowed it: two "fixes" were declared working that had never
  run. The runner now deletes the report first and exits non-zero on a failed
  build. **A stale report is worse than no report.**
- **Measure the thing you are about to blame.** The stall that looked like "the
  pad delays input" was the game's own hotplug scan spending 620 ms closing
  fds; one instrumented `waitMs()` and one standalone `probe.c` named it in
  minutes (section 7.2).
- **A harness must respect the game's own rules.** Fire checks that pressed the
  trigger into the 0.35 s cooldown, or counted *enemy* bullets, or fired into a
  brick one cell away, "failed" a working feature three times. Wait for the
  game's own state (cooldown, bullets, cleared ground) before asserting.

---

## 14. The gamepad layer

**Goal:** both players on the keyboard by default; a DualSense or Xbox pad joins
with `START` and drives a player; nothing about the keyboard changes.

Two layers, deliberately:

- **`src/padmap.h` / `padmap.cpp`** — `PadState` (one pad's snapshot: `lx/ly`,
  `lt/rt`, the four d-pad bools, `west`, `start`, `select`), `PadSink` (what the
  mapper reports: `padMove/padFire/padStart/padSelect`) and `PadMapper`
  (deadzone `0.35`, trigger threshold `0.4`, **edge-triggered** fire/start/select,
  `release()` to let go of everything). **Qt-free and free of any device**, so it
  is unit tested directly (3 cases, `PadCalls` fake sink in the test file).
- **`src/gamepad.h` / `gamepad.cpp`** — a `QObject` + `PadSink` that reads
  `/dev/input/event*` straight through evdev (`linux/input.h`, no new
  dependency), keeps a `PadMapper` per process, and forwards to `Backend`.
  Nothing but `PadState` crosses between the layers; the reader knows nothing
  about tanks.

### 14.1 The rules (user spec — do not "improve" without asking)

- **The keyboard is always live for both players.** A pad never takes a tank
  away. `tick()` ORs the keyboard flags with `m_padHeld[player][dir]`, so a key
  release cannot cancel a held stick and vice versa.
- **The first pad to press `START` joins**, and which tank it gets depends on the
  mode picked on the select screen (section 15): with **one** tank a pad takes
  *that* tank; with **two** tanks player 1 keeps the keyboard, so the first pad
  takes player 2 and a second pad takes player 1 (`joinPad()` claims slots in
  the order `{1, 0}`). A third pad finds nothing free and leaves the game alone.
- **A pad does nothing until it presses `START`** — that is the whole point of
  the hint under each tank. On the select screen `START` (and `Square`/`X`)
  confirms the highlighted mode and the confirming pad takes its tank; once the
  pad has a player, `START` toggles pause; on victory it calls `nextLevel()`.
  **Joining mid-game must not pause** (locked by
  `secondPadJoinsPlayerOne`).
- **Movement:** left stick or d-pad. **Fire:** `L2`/`R2` *or* the face button in
  the west position — `Square` on a PlayStation pad, `X` on an Xbox one
  (`BTN_WEST`), so it is the same physical button on both brands. `L1`/`R1` are
  deliberately not fire.
- **`SHARE`/`SELECT`** emits `helpRequested`, which QML turns into the same
  manual as `?` (including the auto-pause).
- **Unplugging** returns the slot to the keyboard (`padDisconnected` clears
  `m_padHeld` and `PadMapper::release()` reports the release), and the join hint
  under that tank comes back. The pad index is the smallest free one, so a
  **reconnect may come back with a different index and must press `START`
  again** — documented behaviour, not a bug.

### 14.2 What the reader has to swallow (measured, not assumed)

- The kernel already presents both brands with the same standard codes, so one
  table covers them: left stick `ABS_X/ABS_Y`, triggers `ABS_Z/ABS_RZ`, hat
  `ABS_HAT0X/ABS_HAT0Y`, `BTN_WEST/START/SELECT`, and some pads report the d-pad
  as `BTN_DPAD_*` buttons instead of a hat — both paths are handled.
- **Ranges differ per driver**, so every axis is scaled through its own
  `EVIOCGABS` range: the user's real DualSense reports **0..255** for sticks
  *and* triggers, a 360 pad reports **-32768..32767** for sticks and 0..255 for
  triggers. Sticks are scaled bipolar around the midpoint, triggers unipolar
  0..1. Do not hardcode either.
- **Trigger detection is a range test, not a code test**: `triggersAreZ` is true
  when `ABS_Z` is `lo >= 0 && hi <= 1023`. If a pad puts a *stick* on `ABS_Z`,
  those axes are simply not read as triggers.
- **Jitter filter:** analog axes are ignored unless the raw value moved by
  `>= 5` counts. It applies to `ABS_X/Y/Z/RZ` only — **never to the hat**, whose
  whole range is -1/0/1 and would be swallowed by a 5-count deadband (that bug
  was written and caught in review).

### 14.3 Hotplug, and why it is cheap now

`start()` scans once and then every second. `m_rejected` remembers the paths
that are not gamepads and `looksLikeGamepad()` answers the "stick with gamepad
buttons" test from sysfs capability masks without opening anything. Both exist
because the naive version froze the event loop for 620 ms a second — the
measurement, the cause (`close()` costs ~20 ms here) and the fix are in section
7.2. `TANKS_PAD_DEBUG=1` prints what the reader sees: every node it rejects, each
pad it finds with its axis ranges, every axis and button it reads, each scan's
cost, and each pad that goes away. That is the tool to ask a user with an
untested pad to run.

### 14.4 Tests and the harness

- `bin/test` (43 slots) covers the mapper (deadzone, diagonals, release, edge
  fire/start/select) and the backend (a pad joins on `START`, the select screen,
  the player count, the game over screen and what carries on from it, joining
  mid-game does not pause, first pad takes P2 then a second takes P1, a third
  pad is ignored, `START` pauses once the pad has a player, a pad moves only its
  own tank, keyboard + pad do not cancel each other, fire comes from the right
  tank, disconnect returns the slot and leaves no direction held).
- **The end-to-end proof is `/tmp/opencode/pad`** (throwaway, section 8.2 style;
  it was **wiped when `/tmp` was cleaned** — this description is the recipe to
  rebuild it, and it only has to be rebuilt when the reader itself changes):
  `main.cpp` creates one **Xbox-shaped** virtual pad (16-bit sticks, hat d-pad)
  and one **DualSense-shaped** pad (8-bit sticks, `BTN_DPAD_*`) through
  `/dev/uinput`, then drives the **real `Gamepad` + `Backend`** and asserts 37
  checks: detection with the right axis ranges, the select screen (an unassigned
  pad moves the highlight, `START` confirms the mode and joins P2), stick /
  deadzone / hat / button movement on the right tank only, fire from `BTN_WEST`
  and from a trigger past half pull (and *not* at 100/255), one shot per press
  while held, `START` pause/resume, `SELECT` → manual, keyboard + pad
  coexistence, leaving a run and confirming another mode with the pad itself,
  and unplug/replug. `run.sh` builds it against the project's own objects and
  leaves `report.txt` and `debug.txt`; it prints the report even when a check
  fails (a `set -e` script used to swallow it). `probe.c` is the timing probe
  from section 7.2.
- Harness traps, all hit and fixed: pads are found **by name** (the machine has
  a real DualSense on it); enemies are silenced and the board cleared before a
  movement or fire measurement (`dt` is clamped to 0.05 s, so a stall measures
  as one short step — sample movement every 50 ms instead of sleeping once);
  shots are counted from the game's own `playSound("shoot")` rather than from
  live bullets; and "press into a cooldown" waits for the game's cooldown first.

### 14.5 Left to do

Nothing — the pad is committed (`2d31ac5`, `3a756d0`) and the user confirmed it
plays well. What is left on the pad front is part of the select screen
(section 15): any pad, assigned or not, drives the mode options, `START`
confirms the highlighted mode, and confirming hands every pad back to waiting.

---

## 15. The player-count select screen

**Why it exists.** The game used to open straight into a two-tank game with no
choice, and in a game played by one person the second tank just sat at its spawn
point. Because "game over" tested *both* tanks, the run never ended: the parked
tank kept it alive. The user asked for the console-style opening screen and for
one player to mean one tank.

### 15.1 The states

`m_state` is `select` | `playing` | `paused` | `victory` | `gameover`.
**`menu` is gone**: `select` is the opening screen *and* where a run goes back to
when it ends, which is what lets a pad always start the next run with `START`.

The ending gets its own screen so it cannot be missed: `gameover` draws the title
(`GAME OVER`, or `BASE DESTROYED` when the base was lost) and the score of the
run, and `START` / `Space` / `Enter` / a click carries on to `select`. The
*reason* the run ended rides in `m_ending` (`"over"` | `"base"`, read by QML as
`ending`) and the score in `m_message` (`"SCORE 4120"`).

`m_playerCount` (1 or 2) is what the whole game reads. `m_menuIndex` (0 = `1
PLAYER`, 1 = `2 PLAYERS`) is the highlight, and QML gets both through small
properties (`playerCount`, `menuIndex`, each with its own `*Changed` signal).

### 15.2 The rules (user spec)

- **1 PLAYER**: one tank only. The keyboard drives it (`Space`/`WASD`); a pad
  takes it over when it presses `START`. Dying **ends the run**, because there is
  no second tank.
- **2 PLAYERS**: keyboard P1, first pad P2, second pad P1 — unchanged.
- The choice is made with `Left`/`Right` (`A`/`D`), the pad's stick or d-pad,
  and confirmed with `Space`/`Enter`, `START`, `Square`/`X`, or a click.
- **The keyboard is always live for both players**, in every mode.

### 15.3 How it is implemented

- **Input needs no QML routing.** `setP1Move` / `setP2Move` / `fireP1` /
  `fireP2` already exist and QML forwards keys blindly; when the state is
  `select` they call `menuMove(dx)` / `menuConfirm()` instead (`dx` or, if the
  press was vertical, `dy`, so `W`/`S` and `Up`/`Down` work as well as the
  horizontal pair). `padMove` / `padFire` / `padStart` do the same, and on the
  select screen a pad goes to the menu **whatever its slot, assigned or not** —
  the check comes before the `playerForPad()` lookup. That was a real bug: once
  a pad owned a tank it could no longer pick the next mode, so after a
  one-pad run only the keyboard could move the highlight. So the select screen
  is keyboard- and pad-driven without a single new key binding in QML, and the
  architecture rule ("QML only renders and forwards keys") still holds.
- **`gameover` is one keypress deep.** `checkEnd()` sets `m_ending` and then
  `setState("gameover", "SCORE n")`; `fireP1()` / `fireP2()` / `padStart()` /
  `padFire()` see that state (for a pad *before* the `playerForPad()` lookup, so
  a pad that never joined carries on too) and call `toSelect()`. `toSelect()` is
  also `R` mid-game: `clearRun()` (tanks not alive, enemies dead, bullets /
  floaters / pickup / spawn / freeze / base-guard cleared) leaves an empty board
  behind the overlay and the state goes back to `select` with no message.
- **A tank that does not exist is not alive.** `resetTanks()` forces
  `alive = false`, `lives = 0`, `score = 0` for any slot at or above
  `m_playerCount`; `playerActive(i)` is `i < m_playerCount`. `tick()` moves and
  respawns only active players, and `checkEnd()` calls it game over when every
  **active** player is out. That is the bug fix, in one line of the loop
  condition.
- **A new game releases every pad** (`confirmSelect()` calls `releasePads()`
  before `startGame()`, which clears `m_padHeld` as well so no direction stays
  stuck). Which tank a pad gets is therefore decided by the mode and by who
  presses `START`, never carried over from the run before — without this, a pad
  that played a one-player game stayed glued to player 1 after switching to two
  players. `releasePads(fromPlayer = 0)` doubles as the safety net
  `startGame()` needs (drop a pad whose player is out of range, e.g. going from
  two players back to one).
- **Per-player "joined" flag.** `m_joined[player]` is set by the first input
  from either source (`setP1Move`/`fireP1` while playing, `padMove`/`padFire`)
  *and* by a pad claiming the tank in `joinPad()`. It is exposed in
  `players()` / `playerAt()` as `joined`, next to the existing `active`. QML
  draws the blinking hint under that tank while it is false and swaps to the
  `KEY` / `PAD n` label once it is true; `markJoined()` emits `frameChanged()`
  so the swap is immediate rather than one frame late.
- **At boot nothing is on the board**: the constructor calls `resetTanks(true)`
  and then sets both tanks not-alive, so the select screen has no tank sprites
  peeking out from behind the overlay.
- **`R` on the select screen confirms the highlighted mode**; mid-game it goes
  back to the select screen (`toSelect()`, see above); on victory it advances
  the sector, as before.
- **UI**: the overlay's title is `TANKS` / `GAME OVER` / `BASE DESTROYED` /
  `PAUSED` / `VICTORY`; the two options are a `Row` of `Repeater` delegates with
  a `TankSprite` marker on the highlighted one (section 16 replaced the blinking
  `>` with it) and a `MouseArea` for the mouse; a
  `gameover` block adds the score line (bigger than the other messages) and a
  blinking row of `START` / `SPACE` / `ENTER` key caps, and the button under it
  reads
  `>> CONTINUE <<` (`>> RESUME (P) <<` / `>> NEXT SECTOR (R) <<` elsewhere);
  the panels each gained a small `TankSprite` and the hint line, the P2 panel is
  `visible: backend.playerCount === 2`, and the footer's old global
  `PRESS START TO JOIN` is gone (it lives under each tank now). The legend block
  (keys, bonus icons) is gone entirely (section 16): the game over screen stays
  down to the title, the score and the one way out, and the select screen shows
  only the modes and their keys.

### 15.4 Tests and probes

- `bin/test` (43 slots): `selectScreenPicksTheMode`, `onePlayerModeLeavesOneTank`,
  `twoPlayerModeKeepsBothTanks`, `dyingAsASinglePlayerEndsTheRun`,
  `baseDestroyedEndsTheRun`, `gameOverLeadsToAnEmptySelectScreen`,
  `leavingARunGoesBackToTheSelectScreen`, `anAssignedPadDrivesTheSelectScreen`,
  `confirmingAModeReleasesThePadsOfTheLastRun`, `aPadConfirmingTheModeTakesItsTank`,
  `unassignedPadPicksTheMode`, `padTakesTheOnlyTankInASinglePlayerGame`,
  `singlePlayerModeReleasesTheSecondPad`, plus `startsOnTheSelectScreen`
  (renamed from `startsPausedInMenu`). `TanksTestAccess` gained `outPlayer()`
  and `endCheck()` so a run can be lost on purpose.
- **`/tmp/opencode/padvis`** (QML probe, 34 checks and 6 screenshots — **wiped with
  `/tmp`**, see section 16 for its replacement, `/tmp/opencode/qmlprobe`): the two
  options and the chevron, choosing 1 player hides the P2 panel and shows
  `PRESS SPACE`, a pad joining takes the tank and the hint gives way to `PAD 1`,
  dying alone shows `GAME OVER` + `SCORE 0` + `PRESS START, SPACE OR ENTER`
  (shot `6-game-over.png`) with no mode options on it, `Space` then carries on
  to the select screen, an **assigned** pad still moves the highlight there
  (the reported bug), and in 2 players the hints read `PRESS SPACE` /
  `PRESS ENTER` (`... OR START` with a pad plugged in).
- **`/tmp/opencode/pad`** (uinput, 37 checks): an unassigned pad moves the
  highlight with the hat and confirms the mode with `START`, which is the check
  that proves the whole new flow works through the real evdev reader; later on
  the **assigned** pad leaves the run, moves the highlight and confirms
  `2 PLAYERS` again with `START` — claiming P2 while the other pad goes back to
  waiting. That last step is the user's bug reproduced and fixed end to end.

### 15.5 Open: the `QSocketNotifier: Invalid socket` warning

During the first play session of this commit the user saw, once,

```
QSocketNotifier: Invalid socket 59 and type 'Read', disabling...
```

at the same time as the base going down, and had to interrupt the game with
`^C`. It was **not** reproduced: `coredumpctl` has no core dump for `tanks` (so
the process did not die on its own), and two purpose-built stress runs came back
clean:

- `/tmp/opencode/padstress` (30 plug/unplug cycles through the real reader,
  each device destroyed **while directions and a button are held**, so events
  are in flight): 30/30 clean, 0 warnings, every pad released.
- The whole game (QML + audio + reader) running while 12 hotplug cycles went on
  underneath: still alive at the end, 0 warnings, 8 devices opened and 8 closed.

Where the message comes from: `qeventdispatcher_glib.cpp` prints it when `poll()`
reports `POLLNVAL` for a descriptor still registered in a `QSocketNotifier` —
that is, **an fd closed while its notifier is still enabled**. The only code in
the game that closes descriptors is the pad reader, and `closeDevice()` deletes
the notifier (which unregisters it) *before* `::close(fd)`; `openDevice()`'s
error paths close fds that never had a notifier, and the destructor runs outside
the event loop. Qt's own plugins (Wayland, multimedia) hold notifiers too, so
the candidates are outside the game unless a new clue appears.

To pick this up again: run it as `./build/tanks 2>&1 | tee /tmp/tanks.log`, and
note whether the window froze (process alive, no response, sound stopped or
looping) or the process exited by itself, and whether the warning came exactly
with the `GAME OVER` screen. That decides whether it is the pad reader, Qt's
event dispatcher, or the compositor dropping the game's Wayland connection.



---

## 16. The UI overhaul (phase 1) — committed as D

**Why.** The UI looked like a text dump: a Monospace font that fought the pixel
art, Material widgets the game never used, and a select screen that explained
itself in sentences ("CHOOSE WITH LEFT/RIGHT - CONFIRM WITH SPACE, ENTER OR
START", the legend of tile values, the bonus icon row). The user asked for a
pixel font, no Material, and the information moved into **pictures**: key caps
instead of key names, a tank instead of a `>`, pad controls instead of a
sentence. Detail-by-detail iteration comes after this phase.

**What the commit carries.**

- **Font — Silkscreen** (Regular + Bold, SIL OFL), vendored in `src/fonts/` and
  compiled into the binary through `resources.qrc`. `main.cpp` registers both
  faces with `QFontDatabase::addApplicationFont` and sets the family on
  `QGuiApplication`, so **every `Text` inherits it and none names a family**
  (the 44 `font.family: "Monospace"` lines are gone). A bundled font is a
  licensing obligation: `pkgbuild/PKGBUILD` installs `src/fonts/OFL.txt` next to
  the project's own `LICENSE`, and the README credits it.
  - Silkscreen has **no arrow glyphs** (U+2190-2193 absent), which is why
    `KeyCap.qml` draws its arrows from a 5x5 pixel grid; it *does* have U+2014,
    so the three em dashes in `backend.cpp`'s messages stay as they are.
- **No Material**: `QQuickStyle::setStyle("Basic")`. `QtQuick.Controls` is only
  used for `ApplicationWindow`, so nothing else changes.
- **`src/KeyCap.qml`**: one key. Dark face, 2px border, a darker strip along the
  bottom for depth, square corners, optional pixel arrow, and `face` drives both
  the border and the legend — which is what makes a whole row fade.
- **`src/PadIcon.qml`**: one pad control (`stick` | `dpad` | `trigger` | `face`),
  drawn with the same flat language. The stick/face buttons are circles, so they
  are the only anti-aliased shapes in the UI; every row of them sets `face`, so
  they fade with the rest.
- **The select screen** (`Main.qml`): each mode is a `Column` — its name with a
  mini `TankSprite` (player 1's tank, treads running on `win.treadFrame`) where
  the blinking `>` used to be, and under it **its own** row of key caps
  (`win.modeKeys`). The highlighted mode's row is white, the other fades. The
  marker is anchored to the *name's* left edge (not the block's), so the name and
  its keys share one axis.
  - **The pad row** (stick / d-pad → `MOVE`, `L2` `R2` `X` → `FIRE`) is centred
    under both modes and only drawn when a pad is connected: a pad can drive
    either tank, so it belongs to neither option.
  - **Deleted**: the `CHOOSE WITH LEFT/RIGHT...` line, the two key-name lines,
    the pad sentence, the tile-value legend, the bonus icon row and the green
    line. All of it is in the manual (`?`), which is where it belongs.
  - The panel hints (`PRESS SPACE` / `PRESS ENTER OR START`) and the game over
    hint (`PRESS START, SPACE OR ENTER`) are now rows of key caps too;
    `win.joinKeys(player)` keeps the old rule (START is only offered to P2, and
    to P1 only in a one-player game, because the first pad takes P2).
- **The select screen breathes**: 22px between its lines and 18px between a
  mode's name and its keys, applied **only while `state === "select"`** — the
  paused / game over / victory overlays keep their tighter spacing. The panel
  box is sized from the column, so it grows on its own.
- **The gotcha that cost a round trip:** inside a `Repeater`'s delegate,
  `index` is the *key's* index, not the option's, so colouring the row needs the
  option index captured one level up (`property int option: index` on the keys
  `Row`). Comparing `backend.menuIndex === index` lit the same *position* in both
  rows. Caught by probing pixels, not by reading the QML.

**Verification so far.**

- `bin/build` clean, `bin/test` **43/43** (the tests are backend-only and are
  untouched by this phase).
- The offscreen smoke run prints **no QML warnings**.
- `/tmp/opencode/qmlprobe` (throwaway, rebuilt after `/tmp` was wiped): loads the
  real `qrc:/Main.qml`, walks the states and prints **37 checks, 37 passed** —
  the font family on every `Text`, the two modes, every deleted line proven
  absent, the key caps per row and their centring against their own name, the
  highlight on one row only, the single tank marker, the pad row's five icons and
  its absence with no pad, the 1P/2P join hints per panel, and the game over /
  paused / manual overlays with their buttons.
- `/tmp/opencode/shots` saves a PNG per state for eyeballing (1040x800 offscreen).
- **Play session: still owed.** The user approved the result from the
  screenshots and asked for the commit; nobody has played commit D yet. If
  anything misbehaves in play, it is a small follow-up commit, not a revert.

**Decisions worth keeping.**

- Only the *select screen* stopped explaining itself in words. The manual stays a
  text reference on purpose, so the pad sentence still exists — in the manual.
- The pad row is drawn from `backend.pads.length`, so it appears and disappears
  with hotplug; it is `visible: false` (built but hidden) when no pad is there,
  which is the same trade the rest of the UI makes.
- The em dash stays because Silkscreen carries it; do not "clean up" the
  `backend.cpp` messages.
- Phase 2 comes next, from the user's own notes: **the manual first, then the
  in-game interface**. Only the select screen was decluttered in this phase, on
  purpose, so the two are still text-heavy and picture-light respectively.

---

## 17. The enemy overhaul (phase C) — committed

**Why.** Enemy waves were one shape for ever: 8 tanks a stage, more each
sector, the first four gray and the rest alternating green and armour, and the
only power-up source was a green runner. The user asked for a fixed roster with
a real mix, for the green runner's drop to move to a distinct **carrier**, and
for the death explosion, the freeze and the shield to actually look like what
they are. The agreed plan is `/home/…/.opencode/plan/enemy-overhaul.md`; this
section is the record of what shipped.

### 17.1 The rules (user spec — do not "improve" without asking)

- **16 tanks a stage, always.** `TOTAL_ENEMIES = 16` and `totalForLevel()` is
  that constant: later sectors do **not** add tanks. What changes is the mix.
- **Mix by level** (in `spawnEnemy()`): levels 1-3 = 8 basic + 4 fast + 4
  armored; 4-6 = 6 fast + 6 basic + 4 armored; 7+ = 4 basic + 6 fast + 6
  armored. Armored is the remainder, so the three always sum to 16.
- **Four kinds.** `0` basic gray, treads (hp 1, `ENEMY_SPEED`); `1` fast, a
  wheeled chassis (hp 1, `ENEMY_SPEED * 1.35 * mult`), recoloured from the basic
  tank's grey to a pastel metallic yellow in section 18.3; `2`
  armored, plated hull and a thicker cannon (**fixed hp 3**, `ENEMY_SPEED *
  mult`); `3` carrier, treads (**hp 1, `ENEMY_SPEED * 1.15`**).
- **Armoured colour is by hits left, not by sector:** 3 = pastel purple, 2 =
  cyan, 1 = gray. No more armour pips.
- **Carriers are 2 of the basic pool**, not extra tanks, and they are
  **interleaved** at roughly 1/3 and 2/3 of that pool (`basic/3` and
  `2*basic/3`), never both at the start. They flash **red <-> gray** and are the
  **only** power-up droppers.
- **Max concurrent** scales gradually: `min(4 + (level-1), 6)`.
- **Spawn/respawn grace:** every player is shielded for **3 s** on spawn and on
  respawn (`shieldUntil`), so nothing is killed the instant it appears.
- Explosions live **0.5 s** (the plan said 0.8; the user settled on 0.5).

### 17.2 The visuals (three hand-drawn pixel sprites)

All three follow the 16x16 (shield: 20x20) string-grid language of
`TankSprite` / `PickupIcon`, each a `Grid` of `Math.max(1, floor(min(w,h)/N))`
blocks at a **whole-pixel** origin (a fractional origin antialiases the grid
into a smudge). The first frost and shield attempts used plain `Rectangle`s and
read as coloured circles; that is the mistake the pixel grids fix.

- **`ExplosionSprite.qml`** — the user's favourite. Three phases driven by
  `progress` (`1 - progress/0.30` flash; smoke ramps 0.12->0.24 and fades
  0.75->1.0; fire ramps 0.45->0.63 and fades 0.82->1.0), each phase a separate
  16x16 grid behind its own `Loader`, so the silhouette is a bumpy cloud and
  **the flash -> smoke transition is visible**. The delegate scales it
  `cell * (1.3 + 1.0 * boom)` with `boom = min(1, age/0.5)`.
- **`FrostSprite.qml`** — celeste + white ice, drawn **under enemies only**,
  `z: -1`, never on players. A single big patch under the tank; the smaller
  patch to the lower-right was removed at the user's request ("bocha de hielo").
  The whole block sits in a `Loader { active: backend.frozen }`, so nothing is
  built while time runs.
  **The full-board blue overlay is gone and must stay gone.**
- **`ShieldSprite.qml`** — two hand-drawn 20x20 frames of a jagged ring of
  lightning around the hull, **alternating** every ~96 ms, **4-fold symmetric**
  (mirrored on both axes) and of even thickness. Not a circle, not a square
  frame, no scatter of sparks: the user rejected both a round ring and a square
  one, and rejected a "cloud of noise" version. The tiers tried, in order: round
  aura -> square frame of blocks -> sparse scattered bolts -> the two symmetric
  frames that shipped. Keep them symmetric and uniform.
- The shield power-up icon is gray, and the aura reflects that (white/gray).

### 17.3 Verification

- `bin/build` clean, `bin/test` **44/44** (43 before; the level test was
  rewritten to 16/16 and a composition test was added), offscreen smoke with no
  QML warnings.
- `stageFieldsSixteenTanksWithInterleavedCarriers` locks the new rule: 16 tanks
  a stage, 6+2/4/4 kinds at level 1 and 2+2/6/6 at level 7, carriers at indices
  2 and 5 (never 0), armor hp 3 and basic hp 1.
- `/tmp/opencode/shots` (throwaway) dumps each sprite as ASCII luminance and
  saves a PNG per state; that is how the silhouettes were checked with no screen
  available. The user then played the explosion, the frost and the shield.
- **Not played: the fixed 16 a stage.** The tree still had `8 + 2*(level-1)`
  while the new composition assumed 16, so levels 1-2 would have had no green
  and no armour at all. It was corrected to 16 at commit time, on the user's
  explicit choice; the mix is otherwise as above.

### 17.4 Decisions worth keeping

- The three sprite grids are the pattern for any future effect: string arrays,
  integral block size, a whole-pixel origin, and a `Loader` per variant so the
  invisible ones cost nothing.
- The composition lives entirely in `spawnEnemy()` as the remainder of `basic`
  and `fast`; the total is the constant, so the three kinds can never disagree
  with it.
- The explosion/floater/frost all hang off `frameChanged`, like everything else;
  `Explosion` is a plain `{x, y, age}` on the heap-owning `m_explosions` vector,
  capped at 8.

---

## 18. Stuck enemies, the eliminated player and the tank sprites

**Why.** Three things the user reported after playing phase C: enemies that
stand still at the top-left corner, the top-right corner and the top centre with
nothing touching them; a two-player game where P1 runs out of lives, P2 finishes
the sector, both tanks come back and P1's panel shows neither spare tanks nor
`KILLED`; and a list of look changes — P2 has to be green, the fast tank cannot
be, the treads have to read as treads, and the armoured tank has to look more
like one. The rule work is sections 18.1-18.3.

### 18.1 Enemies getting stuck (measured, not reasoned)

The first guess (a corner-specific rule in the AI) was wrong. A throwaway probe
(`/tmp/opencode/stuck`, 200 seeded maps x 60 s with the players removed) showed
the real causes, and the probe was kept as the gate: **0 stuck spans over 1000
maps** after the fix, against 4% of maps before it.

- **Two tanks on one cell.** `spawnEnemy()` picked the entry lane from the spawn
  counter without checking whether another tank was standing there. Two enemies
  on the same spot were then pushed apart by `separateTanks()` with **no terrain
  check**, so one landed inside a whole tile or off the board (y = 0.22). A hull
  of 0.8 can never step out of that with the 0.1 nudge, so the tank froze for the
  rest of the sector. That is most of the reports, and it is exactly "the
  corners", because the corner lanes are the ones that were busy.
- **`blockedByTank()` was a position test.** Two tanks within a hull of each
  other were each other's wall in all four directions and could never back away.
  It is now a movement test: a target inside the other hull is allowed when the
  move does not close the gap (`blockedByTank(x, y, size, self, isEnemy, curX,
  curY)`).
- **AI thrash.** A blocked frame re-rolled the direction immediately, and the
  rules (50% down, 40% drift to the base) kept choosing the wall just hit, so the
  tank jittered on the spot. A blocked enemy now takes `bestDir()` (most free
  run, random tie-break) and **commits to it for 0.8-2.3 s**; the escape nudge no
  longer counts as progress either.
- **Sealed entry pockets.** The scatter could brick a lane shut (2 of 14 spans in
  one sample, `reachable=9`), leaving an enemy pacing a three-cell pocket.
  `openSpawnLanes()` flood-fills each of the three lanes and, **only when it does
  not reach row 6**, carves it straight down. The clear entry pad also grew from
  3x2 to 3x3, so two tanks that do come out of the same gate have somewhere to be
  pushed.
- **Last-resort rescue.** `buriedInSolid()` + `nearestFreeSpot()` snap a tank
  that somehow ends up inside solid ground (or off the board) to the nearest free
  cell, so a stuck tank recovers instead of standing there.
- `spawnEnemy()` now returns `bool`: a busy lane means "wait 0.5 s and try
  again" instead of stacking.

Tests: `twoTanksInContactCanAlwaysBackAway`, `aTankBuriedInMasonryIsFreed`,
`enemySpawnsNeverStackOnOneCell`, `theEntryLanesAreNeverSealed` (50 maps),
`enemiesLeaveTheTopOfTheBoardOnEveryMap` (12 seeded maps, 8 s each, the enemy
must move more than 1.5 cells from its spawn).

### 18.2 A player who is out of lives

**Cause.** `resetTanks(false)` (what `nextLevel()` calls) preserves the life
count but does `m_p[i] = Tank{}`, whose default is `alive = true`. A player who
was out with 0 lives therefore came back as a **phantom**: a tank on the board
with no spares. Dying again took him to `-1`, and the panel draws an icon per
`index < lives` and `KILLED` only at `lives === 0`, so at `-1` it showed neither
— the blank placeholder the user saw.

**Fix, and the rule behind it (user spec).** A player with no lives **stays out
until his partner picks up a LIFE**; the LIFE revives the eliminated player
instead of topping up the one who took it.

- `resetTanks(false)` leaves a player with `lives <= 0` **not alive**, and clamps
  `lives` at 0.
- `killPlayer()` clamps at 0 too: losing the last life takes the player out, it
  never counts below zero.
- The panels show `KILLED` for `lives <= 0` (defensive; the clamp is what really
  fixes it).
- `applyPickup(Life)` on a two-player game revives an eliminated partner via
  `revivePlayer()` — alive, `lives = 1`, at his own spawn point, with the usual
  3 s grace — and only tops up (`min(lives + 1, 5)`) when both players are in.
  A one-player run has nobody to revive, so it is the old +1.

Tests: `anEliminatedPlayerStaysOutInsteadOfReturning` (P1 out, sector advances,
still out, and dying again cannot go below 0), `aLifeFromThePartnerRevivesThe
EliminatedPlayer`, `aLifeTopsUpThePickerWhileBothAreIn`.

### 18.3 The tank sprites and the palette

- **P2 is green** (`#5da24a` / `#2e5b26` / `#bfe3b4`, the old fast-tank green)
  and P1 stays yellow. Every panel, pad label and board sprite follows
  `win.p2body`, so nothing was recoloured by hand.
- **The fast tank can no longer be green**, so it is told apart from the basic
  one by **chassis and colour**: `TankSprite.variant` is `0` treads (players,
  basic, carrier), `1` **wheels** and `2` the **armoured** hull (double plates, a
  4-wide cannon, one row longer — longer only, never wider, so it still fits the
  same lanes). The basic tank is grey (`#a8adb8` / `#5d626c` / `#e4e8ee`,
  lighter than the old `#8d8d8d`) and the fast one is a **pastel metallic
  yellow** (`win.foeYellow*`, `#d8c878` / `#8c7a3c` / `#f6f2d8` — the user's
  ask, after the wheeled hull was signed off; it is paler and flatter than P1's
  saturated gold `#c8a028`, and the chassis tells them apart anyway).
- **The wheels are three a side**, each a **one cell column three rows tall that
  swells into a three cell bar on its middle row** (the hub is the light cell of
  that bar), with an empty row between one wheel and the next. The **gaps
  matter**: a version that ran wheels down the middle rows with the tread-style
  per-row animation was read as a broken tread, so the gaps stay and the wheels
  do not animate. An earlier, chunkier version (three cell blocks at columns
  0-2/13-15) was replaced by the user's own drawing.
- **The wheeled hull is the user's drawing, kept cell for cell.** It was the
  third iteration and the only rule is: **that grid is the sprite**. A flared
  cannon (light tip, two dark rows, a 5-wide dark row, a light collar), a light
  front plate, a dark hull outline with the body colour inside, a **hatch window
  with a raised centre** (rows 8-12: two body cells, a gap, a centre cell, a gap,
  two body cells) and a dark bottom (a full-width row and a 3-cell block below
  it). Two things about it: it is **centred on column 8**, so column 0 is margin
  and column 15 carries the right wheel - do not "centre" it, that would move
  every feature half a cell; and a first attempt of ours compressed a 19-column
  sketch into the 16 the grid allows, which is **not** what shipped. The `D`
  character means the hull's own dark shade, the same as `S`/`G`/`C`; `W` (the
  old wheel body cell) is gone. `D` used to be **unmapped**, so the old hatch
  rendered as transparent holes - i.e. as the black board showing through - and
  it stays mapped for that reason.
- **The armoured colours on hit are unchanged** (purple -> cyan -> grey); its
  last stage is now the same grey as the basic tank, told apart by the shape.
- **The treads are always cells.** The old grid lit the whole tread band on one
  frame only, which read as a flicker and as "the cells only show while moving".
  A tread row is now a cell on **every** row: frame 0 lights the even rows and
  frame 1 the odd ones, so the two frames interleave and the band always reads as
  a track. The clock is the same 2-state `win.treadFrame`, now at 90 ms (was
  140). Only the treads animate; the wheels are static by design.
- **The tank's contour is neutral grey** — `TankSprite.outline`, `#6b7280` — and
  not a darkened hull colour. The tread trace was `Qt.darker(dark, 1.3)`, which
  on the near-black board (`#050507`) reads as black: `#23461d` for P2's green,
  `#371712` for the carrier. That trace is most of the silhouette, so the tank
  came out as a coloured body floating on black wings and the body's outline
  could not be told from the background (user report). The trace column and the
  dark half of the animated tread cells now use that one grey, which reads on
  black for every hull (yellow, green, grey, purple, cyan, red) and is lighter
  than any hull's own dark shade, so the body still separates from it. The
  hull's own dark tone still draws the body's shading (`S`, `G`, `C`) and the
  cannon; only the trace changed. The wheeled chassis has no trace and is
  untouched.
- The side panel's enemy queue was a hardcoded grey; it now uses the same palette
  properties as the board (`win.foeGray*`).

**Verified with a throwaway probe** (`/tmp/opencode/tank`, outside the repo): one
case per variant/colour rendered in isolation at 6 px a cell and printed as a
letter grid (`B` body, `D` dark, `L` light, `E` outline, `.` empty), one PNG
per case for the user, **and** a second phase that loads the real `qrc:/Main.qml`,
puts one tank of each kind on the board and prints the properties of every board
`TankSprite` — which is what proves the delegate wiring (kind -> variant and
palette) and not just the sprite file. That second phase is what caught the
queue's hardcoded grey.

---

## 19. The power-ups, round 1

**Why.** The user's list, after playing section 18: several bonus sprites were
hard to read (SPEED looked like a bolt with a black shell, CANNON's outline ate
its shape, FREEZE barely read as a snowflake), two effects ran longer than they
wanted, and two effects had no visual of their own. The LEVELUP and BLAST
MISSILE bonuses from the same list are separate rounds (19.4).

### 19.1 Sprites (section 10 has the grids)

- **SPEED**: all yellow. `K` is an amber rim (#d9a11b) instead of #1a1a22, and
  the edge highlight is a pale yellow (#fff3bd) instead of white.
- **CANNON**: redrawn as a brass shell with **no outline at all**, and the grid
  is now a real 9x9: it *declared* 7 columns while its rows were 8-9 characters,
  so the sprite wrapped and the cell size came from a different divisor (`/9`).
  Both bugs are gone; it is `columns: 9` and `Math.floor(side / 10)`.
- **FREEZE**: redrawn 9x9 in two tones, eight arms (vertical, horizontal and both
  diagonals) with their branch ticks around a white diamond core, so it reads as
  a crystal instead of an asterisk.
- **NUKE** (was GRENADE): a cartoon bomb, 13x15, fat military-green zeppelin
  body with a green tail and fins (nose down) and a yellow oval disc on the
  body carrying a black trefoil. The tail is green with a dark-green outline,
  the body a dark-green outline with a light highlight, and the nose tapers
  to a point. Two extra body rows make it taller so it no longer reads as
  flattened. The CANNON points left now, the way the user asked.
- **LIFE** (was 1UP): the tank at `side * 0.78`, so it reads as an icon and not
  as a board sprite.
- **The centring bug (this one was real).** `PickupIcon` shows one icon through a
  `Loader`, and a `Loader` **resizes the root item of the component it loads**.
  With a `Grid` as the component's root its `anchors.centerIn` was overridden and
  the sprite was pinned to the loader's (0,0): every icon sat in the **corner** of
  its box. Measured on the real scene, PICK's ink was 4 px from the box's left
  and 19 px from its right in a 45 px box. Every part is now an `Item` wrapping
  the sprite, which keeps the geometry it declares: 10/10/10/10. **A Grid must
  never be a Loader's component root.** The same override is why "make the LIFE
  sprite smaller" did nothing until the wrapper landed.
- The grids are centred with `Math.round((parent.width - width) / 2)` on purpose:
  a half-pixel origin antialiases the whole sprite. The probe counts the blended
  pixels ("soft pixels") for exactly that.

### 19.2 Durations

PICK **20 -> 15 s** and CANNON **20 -> 15 s** (user spec). SPEED, SHIELD, LIFE
and FREEZE are unchanged. Both are asserted against the clock the effects read
(`guardLeft()`, `cannonLeft()`), not against the constant.

### 19.3 The two new effects

- **NUKE blast.** The bonus starts a circle where it was **picked up** (not at the
  board's centre, not on the player): `m_nukeX/m_nukeY` + `m_nukeAge`, exposed as
  `nuke` (`{x, y, progress, active}`), advanced by `updateNuke(dt)` on a
  **0.6 s** clock (`NUKE_FLASH_TIME`) and cleared by `clearRun()`, `startGame()`
  and `nextLevel()`. QML draws one semi-transparent yellow circle whose radius
  grows from the pickup spot to a whole board diagonal (so it reaches every
  corner from anywhere) and fades over its last 30%. Verified on screen by pixel:
  at 0.25 only the middle is tinted, at 0.5 all four corners are, at 1.0 the
  board is clean again.
- **FREEZE blink.** `freezeEnding()` is true while less than 2 s of the freeze is
  left; the enemy delegate's `opacity` then alternates 1 / 0.25 on a ~320 ms
  clock (`frameTick / 20`). The clock is the frame counter, not `treadFrame`, so
  the blink does not read as the treads running.

### 19.4 Still to do from the same list

- **CANNON breaks `Steel` by strip** (never the base wall). **Done** (C5):
  steel tiles now carry a strip mask in `m_brickQ`, `tileBlocksPoint()`
  applies the 4-strip + 50% law to steel unless the cell is a base-guard tile,
  `chipBrick()` chips steel unless it is a guard tile, and `updateBullets()`
  only chips steel on a `big` (CANNON) bullet. The base wall stays concrete.
  Section 4.2 has been rewritten accordingly.
- **The CANNON's "one-shot armour" is not implemented either**: `cannonUntil`
  only marks bullets `big`. The plan is `tierArmor` + `armorLayers() = max(
  tierArmor, m_time < cannonUntil ? 1 : 0)` in the LEVELUP round, an enemy hit
  spending the tier layer first and then clearing `cannonUntil`.
- **LEVELUP** (implemented, C6): raises offensive tier. Tier 1 = bullets +30%
  speed; tier 2 = two bullets fired at once; tier 3 = 3 armour layers; tier 4 =
  air support (3 missiles near the player, one every 1.5 s, tanks only, never
  blocks, recharges after 10 armored kills). Tiers survive a sector change and
  reset on death. CANNON one-shot armour is max(tierArmor, cannon ? 1 : 0).
  Level indicator shown in the player panel when tier > 0.
  **Air-support sound** (wired): file at `src/sounds/air-support.wav`
  (2 s — first 1 s missile falling, second 1 s explosion). The sound plays
  immediately when the missile is launched; the explosion, the score and the
  radio circle appear 1 s later. Plays `air-support.wav` on launch.
- **BLAST MISSILE** (implemented, C7): pickup that replaces the normal shot for
  5 bullets (not 5 seconds). The missile is 25% slower than a normal bullet,
  deals area damage (2 HP to enemies within 1.5 cells), affects only enemies
  and never blocks. Plays `blast_missile-shoot.wav` on fire (volume 0.805) and
  `blast_missile-hit.wav` on impact (volume 0.96). The impact shows a small
  orange radio circle (0.4 s) plus an explosion (without sound). The bullet is
  rendered as a longer dark-gray missile with a tip and a fire trail, rotated
  to the fire direction. Air-support sound (air-support.wav, 2 s) is now wired.

### 19.5 Verification

- `bin/build` clean, `bin/test` **56/56** (+4 slots: the blast starts where the
  bonus was picked up, the blast grows and goes away, the freeze only blinks
  while it is ending, PICK and CANNON last 15 s), offscreen smoke with no QML
  warnings.
- `/tmp/opencode/pick` (throwaway): phase 1 renders every icon in isolation at
  the legend size and the board size, prints the bounding box, the per-cell
  letter grid and the count of blended pixels, and saves a PNG per case; phase 2
  loads the real `qrc:/Main.qml` and checks the blast by pixel, the icon inside
  its real pickup box, and the enemy delegates' opacity while frozen, while the
  freeze ends and after it. That phase is what caught the corner-centring bug and
  the LIFE sprite's dead `width` binding.
- Not yet played by the user: everything in this section.


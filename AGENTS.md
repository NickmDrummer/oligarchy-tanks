# AGENTS.md — Oligarchy Tanks

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
~/Work/oligarchy-tanks
├── AGENTS.md            # this file: agent entry point + state of the build
├── bin/build            # qmake6 + make, with stale-build self-heal
├── bin/test             # test build + run, stale-build self-heal + OLIGARCHY_TANKS_SEED=7
├── bin/install          # installs the Arch package (PKGBUILD)
├── pkgbuild/            # PKGBUILD, .desktop, .svg, oligarchy-tanks.install (input group note)
├── README.md            # user-facing docs (controls, build, gamepad)
├── oligarchy-oligarchy-tanks.pro
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
    ├── oligarchy-oligarchy-tanks_tests.cpp  # 35 QtTest cases (37 slots)
    └── oligarchy-oligarchy-tanks_tests.pro
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

- `OLIGARCHY_TANKS_SEED=<n>` seeds `m_rng`. `bin/test` runs with `OLIGARCHY_TANKS_SEED=7` so map
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
the labels and the manual rows), `tests/oligarchy-oligarchy-tanks_tests.cpp` (4 slots) and this file.
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
   tests/oligarchy-oligarchy-tanks_tests.cpp   8 cases (stuck enemies, the eliminated player)

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
   tests/oligarchy-oligarchy-tanks_tests.cpp  the stage composition (16 tanks, kinds by level,
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
   tests/oligarchy-oligarchy-tanks_tests.cpp  3 mapper cases + 8 pad cases
   tests/oligarchy-oligarchy-tanks_tests.pro  padmap.cpp

 B 3a756d0  Read gamepads from /dev/input and let one join a player
   src/gamepad.h/.cpp     evdev reader, hotplug, sysfs filter, OLIGARCHY_TANKS_PAD_DEBUG
   src/main.cpp           create and start Gamepad after Backend
   oligarchy-tanks.pro              padmap.cpp, gamepad.cpp
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
(section 4.2). `#include <cstdlib>` and the `OLIGARCHY_TANKS_SEED` block in the constructor
**stay**: they are real features the tests depend on.

### 6.2 What the checkpoint commit (`ec59cd9`) contains

- `bin/build`: stale-build guard (wipe + rebuild once from clean if any input is
  newer than `build/oligarchy-tanks`).
- `bin/test`: same guard for the test binary, plus `OLIGARCHY_TANKS_SEED=7`.
- `src/backend.h` / `src/backend.cpp`: `OLIGARCHY_TANKS_SEED` seeding, `boardQ()`,
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
- `tests/oligarchy-oligarchy-tanks_tests.cpp`: `TanksTestAccess` reworked (`strips()`, `chip(x, y,
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
cd ~/Work/oligarchy-tanks

bin/build                                  # qmake6 + make, self-heals stale builds
bin/test                                   # 20 test slots, OLIGARCHY_TANKS_SEED=7, offscreen
QT_QPA_PLATFORM=offscreen ./build/oligarchy-tanks    # QML load smoke test; must print no warnings
./build/oligarchy-tanks                              # real run
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
   cd ~/Work/oligarchy-tanks/build
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

- **`pkill -f build/oligarchy-tanks` kills the invoking shell.** Use `pkill -x oligarchy-tanks`.
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
- Docs: README pad section, the input group requirement and OLIGARCHY_TANKS_PAD_DEBUG,
  which prints what the reader sees; the package prints the group note.
```

### 12.2 Commit C — **made**

One commit for the select screen (section 15), after the user played it. Files:
`README.md`, `src/Main.qml`, `src/backend.cpp`, `src/backend.h`,
`tests/oligarchy-oligarchy-tanks_tests.cpp`, and this file. It is one change — the game's opening
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
- **`SHARE`/`SELECT`** emits `menuRequested` (round 21.10): QML applies the
  same state rules as the `R` key — back to the select screen mid-game (and
  out of a pause), next sector on victory — and does nothing on the select
  screen so a pad never confirms a mode by accident. The `?` key on the
  keyboard still opens the manual.
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
7.2. `OLIGARCHY_TANKS_PAD_DEBUG=1` prints what the reader sees: every node it rejects, each
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

To pick this up again: run it as `./build/oligarchy-tanks 2>&1 | tee /tmp/tanks.log`, and
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

- `bin/build` clean, `bin/test` **56/56 at the time of this batch's commits**
  (it is 58/58 after the section-20 fix round; +4 slots: the blast starts where
  the bonus was picked up, the blast grows and goes away, the freeze only
  blinks while it is ending, PICK and CANNON last 15 s), offscreen smoke with
  no QML warnings.
- `/tmp/opencode/pick` (throwaway): phase 1 renders every icon in isolation at
  the legend size and the board size, prints the bounding box, the per-cell
  letter grid and the count of blended pixels, and saves a PNG per case; phase 2
  loads the real `qrc:/Main.qml` and checks the blast by pixel, the icon inside
  its real pickup box, and the enemy delegates' opacity while frozen, while the
  freeze ends and after it. That phase is what caught the corner-centring bug and
  the LIFE sprite's dead `width` binding.
- Played by the user in full. That session is what surfaced bugs 1-4; they
  are all fixed and committed — see section 20.


## 20. The post-play-test fix round (bugs 1-4 from the user's session)

### 20.1 Bug 3: the tier-2 shoot sound doubled
`fireP1()/fireP2()` fire the LEVELUP tier-2 trailing second bullet and both
played the shoot sound. `fireBullet()` gained `bool playSnd = true`; the
trailing bullet passes `false` (commit `4ae2119`).

### 20.2 Bug 4: the manual had no scrolling
The manual section is wrapped in a `Flickable` with a capped height; the BUILD
and CLOSE buttons stay outside it, always visible (commit `e00542a`).

### 20.3 Bug 1: the armour visuals (the parenthesis saga, then shields)
The user's first spec was white parenthesis-shaped lines in front of the tank.
Eight attempts failed the review (Repeater grid lines, Shape/PathArc arcs,
bar+clipped-pill, pixel half-hexagon brackets). The final design they picked is
**three shield outlines drawn on the tank sprite's own boundary** (commit
`6d01478`), all inscribed in the tank's bounding box so each one reaches the
whole perimeter, same ~2 px pixel line, drawn by `armourCanvas` (a `Canvas`
next to `TankSprite` in the player delegate):

- armour 1: **square**, the outline exactly on the sprite edge
- armour 2: **diamond**, vertices on the side midpoints
- armour 3: **circle**, inscribed in the same box
- on a hit the **circle is lost first**, then the diamond, the square is last
  (armorHP 3 -> 2 -> 1)

Notes from the debugging round worth keeping:
- The canvas is centred on the item (`x: -(width - parent.width) / 2`, width
  1.2x the item) and every point is snapped to a ~`parent.width/20` pixel grid;
  measured with a temp red marker at the canvas centre, it sits within ~1 px of
  the tank's centre. The rotation `[-90,0,90,180][p.dir]` (local +x = forward)
  is enough and the shapes are rotation-symmetric enough at 90-degree steps.
- The off-centre look the user reported was **not** the overlay: it was the
  **spawn grace crackling ring** (`ShieldSprite`, 3 s of `shieldUntil` per
  spawn) drawn over it, whose white streaks stick out further on one side per
  frame. The loader is now `active: p.shield && p.armorHP <= 0`, so the ring
  shows only when there is no armour overlay.
- Debugging trick: `wp findChildren` misses some dynamically re-parented QML
  delegates; walking `contentItem()->childItems()` recursively does find them.
  Also, offscreen `grabWindow()` screenshots caught the shield ring because
  every capture longer than ~3 s walked out of the grace window - shots at
  1.2 s of game time are inside it.
- The `armor`/`armorRows` variant stays in `TankSprite.qml` (section 18.3's
  grids untouched); the player overlay lives only in `Main.qml`.

### 20.4 Bug 2 fixed: air support recharges after 10 armoured kills again
`updateAirStrikes()` credited the score and the armoured kill of every
explosion to hard-coded `m_p[0]`, so P2's tier-4 air support never counted his
own kills and never recharged. `AirStrike` now carries an `owner` field (the
player that launched it) and `updateAirStrikes()` credits score, armoured
kills and the pickup drop to `m_p[owner]`. Two tests cover it: the strike
credits its owner (`airStrikeCreditsItsOwner`) and the tier-4 tank refills 3
missiles once `armoredKills` reaches 10 (`airSupportRechargesAtTenArmouredKills`).
`bin/test` is 58/58 after this round.

### 20.5 tier 2 x BLAST MISSILE
A tier-2 player with BLAST shots used to fire two missiles per press (the
trailing second bullet was a second BLAST) and played the shoot sound twice.
The trailing bullet is now gated on `blastShots <= 0`: BLAST fully replaces the
shot and never stacks with tiers (commit `74e486b`).

### 20.6 Verification state
`bin/build` clean, `bin/test` **60/60**, offscreen smoke with no output.
Played and approved by the user: the whole section-19 batch, the shield
visuals, the tier-2/BLAST fix, the fullscreen/F11 window, the air-support
owner/recharge fix and the pressed-against-block shot fix are committed.

### 20.7 Window: fullscreen by default, F11 toggles
The window was hard-coded 1040x800 and had no native fullscreen, so Hyprland
opened it at ~50% of the monitor and the compositor's fractional scaling made
the pixel-art blurry until the user maxed it. It now opens
`visibility: Window.FullScreen` (always 1:1) and an F11 `Shortcut` toggles
fullscreen <-> windowed; windowed keeps the 1040x800 design size so the pixel
layout reads the same in both states. This is also why chat screenshots of the
game must be taken in fullscreen to be pixel-representative.

### 20.8 Fixed: no shot spawned while the tank was pressed against a block
The `solidAt()` pull-back spawned the bullet at the tank centre whenever the
barrel tip landed inside solid terrain, so a tank flush against a block fired
a shot that was hidden under the hull, walked ~3 frames and died on the wall —
it read as "the shot never came out". Enemies did not trigger it because
`solidAt()` only sees tiles, so the tip stayed clear and the shot was born at
the muzzle. `fireBullet()` now walks the barrel back from the tip (0.6) in
0.05 steps to the last clear point, and only falls back to the centre when
nothing on the barrel is clear (the tank sits inside blocked terrain; the
first-update chip in `updateBullets()` still handles a blocked-centre spawn).
The same walk replaced the old out-of-bounds-to-centre rule, so firing along
the arena edge also shows the shot leaving the muzzle. Tests:
`shotLeavesTheMuzzleAgainstABlock` (hull-edge spawn + the wall chips) and
`shotFiresAtTheArenaEdge`; `bin/test` is 60/60.

---

## 21. The Oligarchy Tanks rename, and the road to publication

The game is renamed **Oligarchy Tanks** (user decision, session of Oct 2026).
The repo folder is `~/Work/oligarchy-tanks`; the repo, the binary, the Arch
package, the `.desktop` file, the icon and the Wayland app id are all
**`oligarchy-tanks`** (the Omarchy app-convention name). Ran in one round,
commit "Rename to Oligarchy Tanks": the `.pro` files (`oligarchy-tanks.pro`,
`tests/oligarchy-tanks_tests.pro`), `bin/build` / `bin/test` paths, `main.cpp`
(application name, desktop file, icon), the window title and the visible
strings (`OLIGARCHY TANKS`, select 30 px vs 46 px so the long name fits, the
manual header 26 -> 20 px, the bar title auto-fits at a 380 px item),
`pkgbuild/oligarchy-tanks.{desktop,install,svg}`, the README/LICENSE titles and
the env vars, which moved to **`OLIGARCHY_TANKS_SEED`, `OLIGARCHY_TANKS_TIER`,
`OLIGARCHY_TANKS_PAD_DEBUG`, `OLIGARCHY_TANKS_RELEASE`**.

Naming rule for the code and the docs: the word **`tank`/`Tank` stays as the
noun everywhere** (`Backend::Tank`, `TankSprite.qml`, `hitsTank`,
`TanksTestAccess`, prose like "16 tanks per stage"), except
`tests/oligarchy-tanks_tests.cpp`, whose file name follows the test target.

### 21.1 Compatibility boundary (verified, not promised)

- Portable as-is: C++/QML over Qt6 (`core gui qml quick quickcontrols2
  multimedia`), fonts and sounds embedded in the `.qrc`, offscreen tests,
  fullscreen/F11, the theme reads a file with a `#FFD60A` fallback.
- Linux-only by design: the gamepad reader (`<linux/input.h>`, `/dev/input`,
  evdev/sysfs). On Windows/macOS the game still builds as a Qt app once these
  two things happen: **drop `dbus` from both `.pro` files** (no QtDBus code
  exists; Qt for Windows ships no QtDBus) and exclude `gamepad.cpp` behind a
  Linux guard. Beyond that, keyboard-only play covers both players on one
  keyboard, so 1P/2P stays playable everywhere.
- Not viable: WebAssembly (Qt multimedia + pad input).
- Bigger cross-platform support (real Windows/macOS builds, pads via SDL) is
  explicitly out of scope unless the user asks for it later.

### 21.2 Publication route (user decision)

The **plugin marketplace is plugins only** (Quickshell/QML kinds: bar-widget,
bar, panel, overlay, menu, service; `omarchy plugin add` clones QML and never
installs binaries), so an app cannot be listed there. The chosen route:

1. **GitHub public repo `NickmDrummer/oligarchy-tanks`** + tag `v0.1.0`.
2. **AUR package `oligarchy-tanks`** (source recipe, not this repo's local
   `pkgbuild/PKGBUILD`, which builds from `$startdir/..` and only works for
   `bin/install`). Needs the user's AUR account + SSH key.
3. **Omarchy Store listing** (omarchystore.com; requires install through a
   package manager, install/removal docs in the README, and preview captures
   taken by the user in fullscreen).
4. **PR to `omacom/omarchy-pkgs`** (`pkgbuilds/oligarchy-tanks/` + `.omarchy/
   package.json`) with the source pinned by tag and a GitHub watch, so it gets
   `omarchy-pkg-add oligarchy-tanks`. It links Qt with a `build()`, so it stays
   off the fast ring and reaches `rc`/`stable` by promotion.

### 21.3 Next round: power-up adjustments (user list, not started)

- Air support (tier 4): **5 missiles instead of 3**, and the effect **+25%
  volume**.
- A visible counter of armoured kills toward the recharge — bar or counter
  with an icon, e.g. `AIR-SUPPORT: xx%` (design to settle with the user).
- BLAST missile: **area radius 1.5 -> 3** cells.
- Pickups: the user confirmed (Oct 2026) that killing a carrier must drop the
  bonus at a **random free spot** on the board, never at (or near) the
  carrier's death spot.
- The tier-3 armour visuals, settled: **reuse the shield power-up's crackling
  ring** (ShieldSprite lightning) in a **special colour** while `armorHP > 0`;
  it disappears once the three hits are spent. This replaces the
  square/diamond/circle canvas outlines shipped in `6d01478`.

### 21.4 Aesthetics backlog (user ideas, pending, low priority)

- **The board is not centred on the screen** — the playfield sits off-centre
  in the window. Parked as pending (user asked to leave it noted).
- A **CRT effect** over the map (scanlines/curvature/vignette style).
- **Arcade stickers** (bezel art) around the display perimeter, covering the
  board's frame like a coin-op cabinet.
- **The manual needs corrections** (the user's list, pending).

### 21.6 Portability round (small, done)

- `dbus` removed from both `.pro` files: no QtDBus code exists anywhere, and
  Qt for Windows ships no QtDBus — it was blocking a Windows build for nothing.
- `gamepad.cpp` is no longer compiled outside Linux (`linux { }` block in the
  `.pro`), `gamepad.h` travels with it, and `main.cpp` guards the `Gamepad`
  use with `Q_OS_LINUX`; a header note says the file is Linux-only.
- README gained an "Other platforms" section (Windows/macOS builds with
  qmake6 + windeployqt/macdeployqt, keyboard 2P, no pads, no WASM), and its
  build block now names `./build/oligarchy-tanks`.
- `bin/test` 60/60, offscreen smoke clean.

### 21.5 Fixed round: the base draws the Omarchy mark

The base tile now renders the **square Omarchy logo** instead of the 3x5
"omarchy" wordmark: the official mark (`/usr/share/omarchy/icon.txt`) collapsed
2x2 into a 27x13 block grid, hard coded in `Main.qml` so the game needs nothing
from the host. Reviewed over three user captures:

- white glyph out: the glyph is transparent and shows the board, so the mark
  reads as the green square alone,
- the plate is hard-coded **`#9ece6a`**, not `theme.accent` — the user wants
  the Omarchy green regardless of the desktop theme,
- the mark is 2:1 by design, so uniform blocks left the tile half-empty and
  read squashed; the vertical block is stretched (`cell/17` vs `cell/29`) to
  fill the tile,
- the tile's accent border is gone (user request; it read as UI chrome).
The dead variant keeps its muted reds (`#7a3535` plate on `#3d1010`). Approved
by the user over the final capture.

### 21.7 Aesthetics round: the centred display block and the CRT tube

**Phase A — display centring (approved on screen).** The board and the side
panel form **one centred cabinet screen**: `cell` = the largest value >= 30 the
real space of the block allows (window area minus panel `160`, the `14` gap and
an `8` border allowance, measured on both axes), the panel sits at
`board.x + board.width + displayGap` with the board's own height, and the board
shifts left by `(panelW + displayGap) / 2`, so the leftover space splits evenly
around the block.

**Phase B — the CRT tube (approved on screen).** The picture is drawn through a
layer and reshaped by a shader:

- `playfield` (the board's content item, margins 4) is **visible** and draws
  entirely through `layer { enabled: true; smooth: false; effect: ShaderEffect }`.
  The layer texture is captured fresh every frame, which is exactly what the old
  `ShaderEffectSource`-over-a-hidden-item arrangement silently did not do: that
  texture froze at the first frame (tiles in, tanks out). Keep the layer path.
- The shaders live in `src/shaders/crt.vert` / `crt.frag` and are **precompiled
  to `.qsb` with the qsb tool** — Qt 6 no longer accepts inline GLSL strings in
  ShaderEffect (it treats them as file names and renders nothing, silently).
  They are compiled into the binary through `resources.qrc`; regenerating:
  `qsb --qt6 -o shaders/crt.vert.qsb shaders/crt.vert` (same for `.frag`). The
  GLSL is the Vulkan-style form (`layout()` declarations, `texture()`).
- The effect: barrel curvature (`off = abs(uv.yx) / vec2(4.5, 4.0)`, squared),
  everything outside the curved mask painted the board colour, chroma split
  growing toward the edges (`0.004 * dot(v,v) * 4.0`), screen-space scanlines
  (one dark device line every 3 px, strength 0.18) and a vignette (0.5 at the
  corners, clear in the middle).
- `layer.smooth: false` keeps the texture NEAREST, so the pixel art is not
  blurred before the curving.
- The `tube` group is down to the glass Canvas. The state overlay (z 20) stays
  a direct board child, above the tube, crisp.
- Software rendering (offscreen/package smoke) draws the straight picture: the
  effect needs the OpenGL scene graph. No warnings either way.
- **Stderr cleanup found along the way:** `bulletAt()`'s empty-slot return (no
  bullets alive) was missing the `blast` key, so every bullet delegate's
  `visible: b.blast` bound `[undefined]` to bool roughly 9 000 times a session.
  It now returns `blast: false`. Never return dicts missing the keys QML reads.

**Verification:** `bin/build` clean, `bin/test` **60/60**, offscreen smoke
silent, a GL run probed end to end (start game, join P1, grab) showing tanks,
enemies, the curve, the scanlines and the fringing, and the user played a full
session ("quedó increíble"), with one follow-up: the straight glass frame line
(board border + glass Canvas stroke) does not follow the curve — the curved-rim
round is next.

**The curved-rim follow-up (approved on screen).** The user disliked the one
straight thing left in the tube: the rectangular glass frame. The board's
border (`#3a3a44`) and the glass `Canvas` stroke are **gone**; the shader draws
the rim itself off the mask-distance `d` — a ~9 device px darkening band
(`rim = 1 - smoothstep(0, 0.020, d)`, strength 0.5) and a ~3 device px blend of
the mask edge into the board colour (`edge = smoothstep(0, 0.006, d)`), so both
the rim and the phosphor edge follow the curvature exactly. Anything else
rectangular around the tube must not come back.

**Phase C — the cabinet bezel and the wall mural (approved on screen).** The
display is now a monitor set into a full arcade cabinet:

- **A bezel** (`win.bezW` 20 px, reserved from the `cell` formula) frames the
  whole display block: matte face `#171720`, a 2 px brass lip `#c8862e` around
  the opening, a screw in every corner, and a bottom rail sticker reading
  `DEFEND THE OLIGARCHY BASE`. Nothing here covers playfield pixels.
- **A wall mural** (`wallmap`, a window-level Item behind the whole layout)
  fills the window from the top edge **down to just above the footer** — the
  footer stays clean — with a game map of brick and steel tiles at **175% of
  the screen's own tile size** (`win.cell * 1.75`), same inks as the game's
  tiles, with street gaps so it reads as a map, not a wall. One Canvas painted
  per resize only; zero cost during play. Hidden under a 1100 px-wide window.
- **The game name lives on the mural**: `OLIGARCHY TANKS` at 62 px on a black
  plate, centred in the HUD row between the two panels. The old in-HUD-bar
  title is gone. The two title `Text`s need `horizontalAlignment:
  Text.AlignHCenter` — with fit sizing and default left alignment the ink left
  the plate (caught by pixel screenshot).
- **The player panels** sit on the mural behind a **10 px black frame**
  (`anchors.margins: -10`), so they cut against the bricks.
- **Six decorative tanks** (`muralTanks`, inside `boardArea`) stage one of
  every kind (P1, P2, basic, wheeled fast, armored purple, carrier red) on the
  perimeter roads around the display, sized like the mural tiles, treads
  running on `win.treadFrame`.
- The display grew back to **cell 57** (+14% linear) by dropping the earlier
  marquee band reserve and slimming the bezel 22 -> 20.

**Verification:** `bin/build` clean, `bin/test` **60/60** (backend untouched),
offscreen smoke silent, GL probe render checked end to end, and the user
approved the full-screen result (with one fix: the title alignment above).

### 21.8 The publication polish round: green accent, victory message, marquee font

Three items from the user's pending list, one commit after they seen and
approved the result (`16c748d` is the mural round, committed first):

- **The accent is the Omarchy green, fixed.** `Theme` no longer reads the
  desktop theme's `colors.toml` — the user's blue accent must never leak into
  the UI. `theme.h/.cpp` are now a minimal class: `m_accent = "#9ece6a"` hard
  coded (the same green the base tile's plate already uses, section 21.5) and
  `accentForeground()` unchanged (black reads on it). The file-watching
  machinery is gone with the reading.
- **The victory message** is `Sector N Clear! Score: X` — the trailing
  "— press R for sector N…" is gone (the `>> NEXT SECTOR <<" button under it
  already says the way on).
- **The marquee is set in the Omarchy wordmark font**
  (`github.com/markcuda/Omarchy-Font`, MIT by Mark Cuda, vendored as
  `src/fonts/OmarchyFont.ttf` + `OmarchyFont-LICENSE.txt`, compiled into the
  binary through `resources.qrc` and registered in `main.cpp` on demand).
  **Only** the cabinet marquee names `font.family: "Omarchy Font"` — Silkscreen
  stays the global font, and no other `Text` names a family. The marquee
  dropped bold + letterSpacing (the font is block-built enough on its own) and
  sits at 56 px (62 px Silkscreen-size overflowed the plate).
- Housekeeping: the PKGBUILD installs the font's MIT licence, the README
  credits it, the theme probe in `/tmp/opencode/crt` registers the font so the
  probe renders the real thing.

The two theme test slots (`themeFallsBack`, `themeReadsAccent`) became one:
`accentIsTheOmarchyGreen` asserts `#9ece6a` + `black`. `bin/test` is **59/59**,
`bin/build` clean, offscreen smoke silent, GL probe run.

### 21.9 The power-up round (section 21.3): air support, blast radius, drops and the armour ring

The five §21.3 asks, to be committed as one block once the user has seen and
approved the result on their screen:

- **Air support fires 5 missiles per salvo** (was 3): every place that
  refills the salvo — the tier-4 LEVELUP pickup, the ten-armoured-kill
  recharge and the `OLIGARCHY_TANKS_TIER` debug grant — now sets
  `airMissiles = 5`.
- **The air-support effect plays 25% louder**: `sndAirSupport.volume` went
  0.8 -> 1.0 (the SoundEffect ceiling is 1.0, so 0.8 * 1.25 lands exactly
  there).
- **BLAST radius 1.5 -> 3 cells.** New `BLAST_RADIUS = 3.0` constant in
  `backend.cpp` drives the area-damage test in the blast branch of
  `updateBullets`; the QML impact circle grew to a matching 6-cell diameter
  (`Main.qml` `blastFlash`).
- **Bonuses land at a random free spot anywhere on the board**
  (`Backend::spawnPickupAt`): candidates are every Empty/Forest cell with no
  living tank on it, one is drawn at random; the old neighbourhood scan
  survived only as a last-resort fallback for a packed board.
- **The tier-armour visual is the ShieldSprite crackling ring, not the
  square/diamond/circle canvas** (replaces the commit `6d01478` canvas).
  `ShieldSprite.qml` gained `tint`/`tintDark` properties (the power-up grace
  ring keeps the white/gray default); the player delegate loads the ring
  while `armorHP > 0` in gold `#ffd60a`/`#a5820e` — chosen to read apart from
  the white grace ring, which still hides while armour is up.
- **The armoured-kill counter is visible** in both player panels when
  tier >= 4. It is a state line: **`AIR-SUPPORT ON`** (accent green) while a
  salvo is in flight, **`AIR-SUPPORT n%`** (celeste) while the armoured-kill
  bank is under ten. `playerAt()` and the per-frame `players()` list carry
  `armoredKills`/`airMissiles` — the panels read `players()`, which is where
  the first probe round tripped.
- **The air-support cycle is bank-driven (user design, settled after two
  bus reports)**. The armoured-kill bank IS the charge: ten banked kills
  release a salvo of five strikes, the bank starts over, and the cycle
  repeats while tier 4 stays alive. Kills bank during a live salvo (a dead
  salvo with ten banked fires the next one on the spot); a new salvo never
  stacks on a live one; the bank crosses stage changes untouched
  (`nextLevel()` -> `resetTanks(false)` keeps tier/armor/kills, the salvo
  itself resets with the `Tank{}` wipe); death drops the bank with the tank
  (`killPlayer` zeroes `armoredKills`). The old `airRecharging` flag and
  the LEVELUP pickup's instant salvo grant are gone — the charge alone
  drives the salvo. The `OLIGARCHY_TANKS_TIER=4` debug grant still hands
  out a full salvo for testing.
- **BLAST missiles home in** on the nearest enemy ahead of the player's
  area — in the facing half-plane, within +/-4 cells sideways — when one
  exists at fire time (`Bullet.target`); they curve toward it every step
  and still fly over every block, snapping onto the hull when a step would
  overshoot, so they cannot orbit the target. Nobody ahead -> the old
  straight lane. Missiles keep flying straight if the target dies mid-flight.

Tests: the air-support tests tell the whole cycle:
`airSupportFiresAtTheTenthArmouredKill` (nine banked hold the hand; the
tenth releases 5 and zeroes the bank; in-flight banking waits; exhausted
salvo with ten banked respends instantly),
`airSupportRechargesAtTenArmouredKills` (P2 side, same rule),
`airSupportCycleSurvivesTheStageChange` (a 40% bank crosses the sector and
fires when six more kills land — nothing granted on stage entry),
`airSupportBankDiesWithThePlayer`, `theBlastAreaReachesThreeCells`
(impact target + one at ~2.3 cells die, one at ~4.0 survives),
`theBlastHomesOnAnEnemyAhead` (missile curves 4 cells off the lane and kills
the target), `theBlastStaysStraightWithNobodyAhead` and
`pickupDropsOnAFreeSpot` (the drop lands clear of every tank, inside the
board). `bin/test` **66/66**, `bin/build` clean, offscreen smoke silent, the
`OLIGARCHY_TANKS_TIER=4` probe render checked (panel state line and salvo,
gold ring around the hull).

### 21.11 The player panels overflowed; the tier mark moved to the tank's left (delivered)

- Reported after 21.10: with every information row stacked in a panel
  (name tank, SCORE, effects line, the tier/air-support state line, the
  join hint), the text ran past the panel's bottom edge — the box was
  sized to the old row count (240x112) while the content grew.
- **Measured, not guessed** (`/tmp/opencode/crt/panelprobe`, throwaway):
  the worst realistic stack — tier 4 + AIR-SUPPORT + effects + unjoined
  join keys — is **125 px of content in a 112 px box** (13 px of spill).
  Also found sideways: the widest effects line, `SHIELD SPEED CANNON
  MISSILE LV 4`, painted **293.7 px** inside a 240 px box. The
  `KILLED`-line stack is smaller (~108) and can never coexist with tier 4
  (the tier resets on death), so 125 px is the real ceiling.
- **The fix is a budget, not content-sizing**: sizing the box from the
  column would resize the panel whenever a state line appears and
  **rescale the whole board mid-game** (`win.cell` reads the HUD row's
  height — the side panel "stays in place so the board never jumps" rule
  from section 21.7). The panel and `hudRow` went 112 -> **144 px**, which
  fits the measured worst case with ~9.5 px of margin and never changes
  during play.
- **The effects line is confined**: 216 px wide, centered,
  `fontSizeMode: Text.HorizontalFit` — with everything stacked the font
  shrinks (painted 200.5 px <= 216 after the LV move below), with little
  text it renders at its 14 px size as before.
- **The tier mark left the effects line** (user request in the same round,
  "el indicador de LV a la izquierda del tanque"): a small `LV n` gold
  label now rides **to the left of the name tank** in both panels — it is
  rank, not a temporary effect. With it out of the effects line, the
  widest line is `SHIELD SPEED CANNON MISSILE` at **206.5 px painted —
  fits without even shrinking**. Name row with `LV 4` rides at
  189.3/192.9 px <= 216.
- Side effect, accepted: the taller HUD row costs the board ~2 px of
  `cell` in fullscreen (~55 -> ~53); the panels' readability wins.
  The offscreen probe's 800 px window crams the marquee over the panels —
  that overlap pre-dates this round (horizontally independent of heights)
  and at the user's 1920x1080 the plate sits flush between the panels by
  design. User approved the result on screen ("quedo excelente").
- `bin/test` 70/70 (backend untouched), offscreen smoke silent.

### 21.12 New player sprite (variant 3), pending the armoured one

- `TankSprite.qml` gained `playerRows`, the **user's own 16x16 drawing** for
  the player tanks, exposed as `variant: 3` (narrow light front plate, a 2x2
  dark badge over the body and a tapered dark rear skirt). One shape for P1
  and P2 — the palettes tell them apart. It is applied **cell for cell, not
  recentred** (the user's grid rule).
- `variant: 0` (treads) is now **basic and carrier only**, so `treadRows`
  keeps the old drawing untouched for them. Wheels (1) and armour (2) are
  unchanged.
- `variant: 3` wired to every player-facing use: the board `playerAt`
  delegates, the panels' name tank and the five life icons, the select
  screen's tank marker and the mural's P1/P2 decoration.
- Sentry: the armoured tank's redesign is still owed (user will send the
  grid); until then `armorRows` stays. — **Delivered and applied in the same
  round**: `armorRows` is now the user's own grid (forked cannon, bare-hull
  middle wider than the tread band, a double-plate core and narrow feet),
  applied cell for cell. The visual middle actually protrudes past the tread
  band where the sides are transparent (`....`) — drawn that way; the
  collision hull is unaffected. Refined twice by the user (filled fork,
  then the short-cannon shoulders + speckled plate v3), all applied as sent.
- **The armoured palette is no longer purple/cyan**: the user wanted a
  colour that "denotes hardness" and picked the dark moss green from a
  six-palette probe. Hit progression is now **moss green `#4e5d38`/
  `#303a20`/`#7d9060` (3 HP) -> dark brown `#6e4f33`/`#46311f`/`#a07a52`
  (2 HP) -> the basic tank's gray (1 HP)** — the degradation itself tells
  the story. Wired in the enemy delegate's three colour properties, the
  mural's armoured decoration and the manual line
  (`ARMORED (3 HP, MOSS>BROWN>GRAY)`). Cyan is gone from the game's hulls.
- `bin/test` 68/68 (backend untouched), offscreen smoke silent, probe at
  `/tmp/opencode/crt/spriteprobe` renders variants 0-3 side by side
  (`/tmp/opencode/crt/sprites3.png`) plus the mural/panel renders in the
  tier-4 probe. Verified: both player palettes, rotations (dir 1/3 on the
  mural), the 12 px life icons and the 30 px panel tank.

### 21.13 The LEVELUP sound played twice, and it deserved its own arpeggio (delivered)

- Reported after 21.12: the user heard the LEVELUP pickup about twice as
  loud as every other power-up. Diagnosis: `applyPickup` emitted
  `playSound("power")` inside the `LevelUp` case AND again after the switch
  (the post-switch emit only skipped Freeze and Nuke), so the sample fired
  twice on the same frame — two overlapping plays read as double volume.
  **BLAST/MISSILE carried the same double emission**, unnoticed.
- Fix: each pickup now sounds exactly once. The in-case emits are gone from
  `LevelUp` and `BlastMissile`; the post-switch emit plays `"levelup"` for
  LEVELUP and `"power"` for everything else (Freeze and Nuke keep sounding
  inside their cases).
- The user asked for a dedicated sound: a cheerful ascending arpeggio to
  mark the tier up ("the most important power-up of all"). New
  `sounds/levelup.wav`, a chiptune square-wave climb (C5 E5 G5 C6 held into
  E6, 0.7 s, per-note decay) generated at the game's sample format
  (22050 Hz, mono, 16-bit), wired as `sndLevelUp` in `Main.qml` at volume
  0.6 like the other pickups, plus its `resources.qrc` entry. The user
  loved it but asked for +50%: the wav was regenerated at 0.45/0.51 peak
  gain (playback volume stays 0.6).
- Tests: `levelUpSoundsOnceWithItsOwnArpeggio` (QSignalSpy: exactly one
  `playSound("levelup")`) and `blastMissileSoundsOnlyOnce` (exactly one
  `playSound("power")`). `bin/test` now totals 70/70.

### 21.14 The fifth guided missile was silent-only, and air support wanted +15% (delivered)

- **Diagnosis corrected along the way**: the user said "guided missile",
  which the manual uses for the **BLAST MISSILE pickup** (5 shots), not
  for air support. First reading here wrongly blamed the air-support
  salvo.
- **The real bug is only on the FIFTH blast shot, with tier >= 2**:
  `fireBullet()` spends the shot count during the main fire, so the
  section 20.5 gate (`blastShots <= 0`) read 0 afterwards, judged "no
  BLAST in play" and launched the tier-2 trailing bullet right behind the
  missile; the two collided (bullet vs bullet) and the missile died at
  the muzzle — nothing but the shoot sound. Shots 1-4 were never affected
  (the gate saw 4, 3, 2, 1 > 0).
- Fix: both `fireP1()`/`fireP2()` capture `blastReplacing` (the count
  still > 0) **before** firing and the tier-2 gate now also requires
  `!blastReplacing`. The 20.5 rule (BLAST replaces the shot, never
  stacks) is unchanged for every shot.
- Tests: `theFifthBlastShotSpawnsItsMissile` (five taps through
  `fireP1()`, exactly one blast missile per press) and
  `tier2TwinShotsSurviveWithoutBlast` (no BLAST -> two bullets, trailing
  0.4 cells behind — the 20.5 gate itself is regression-locked now).
  Found while writing the test: the per-frame `bullets()` list lacked the
  `blast`/`big` keys that only `bulletAt()` carried — same
  per-frame-plus-invokable rule as `players()`; both added.
  `bin/test` **72/72**.
- **Air support +15% more volume** (user spec): the playback volume was
  already at the SoundEffect ceiling (1.0), so the sample itself was
  scaled — `sounds/air-support.wav` regenerated through an in-place
  amplitude x1.15 with a hard clamp (peak 22260 -> 25599 of 32767), same
  22050 Hz mono 16-bit format, duration unchanged, playback volume
  untouched at 1.0. **Verified by the user, both parts**: the fifth shot
  falls ("el bug del guided missile: CORREGIDO") and the boosted air
  support reads on their ears ("si mejoró con el 15%"). Committed as
  `bc731a4` (naming was to stay as is: `GUIDED MISSILE` only in the
  manual description, the visible label keeps `MISSILE`).

### 21.15 Open again: the `QSocketNotifier: Invalid socket` crash (twice, tier-4 sessions)

The section 15.5 warning came back **twice in one play session** while the
user tested with `OLIGARCHY_TANKS_TIER=4`: `Invalid socket 72 and type
'Read', disabling...` and, on the next run, `Invalid socket 62...`. The
user calls both a **crash** (the game died or froze — wait for the
detail, see below).

New clues vs section 15.5:
- The binary is now `oligarchy-tanks`; `coredumpctl` shows **no core dump
  for it** after the sessions (the only core in the log is an old
  `/tmp/opencode/crt/probe`), so whatever happened was **not a SIGSEGV**
  — the process either exited through a clean-ish path or froze with the
  message being the last thing it printed.
- Both sessions were **tier 4** — whose tier-4 salvo plays a 2.0 s
  `air-support.wav` every 1.5 s, so plays overlap (the sample is longer
  than the launch interval). The prime suspect set is unchanged from
  15.5 (a fd closed under a live notifier: the pad reader, Qt
  multimedia's audio stream, or Wayland), but the **correlation with
  tier-4 audio churn is the first pattern** the issue ever showed.

Reproduction plan for the next round:
1. The user runs `./build/oligarchy-tanks 2>&1 | tee /tmp/tanks.log` at
   the next tier-4 session, and notes at the crash: did the window
   **freeze** (process alive, no response, sound stopped or looping) or
   **exit by itself**; what was on screen; whether a pad was plugged or
   unplugged around it; whether the salvo was firing.
2. Same-time stress from here: tier-4 grab-loop probes hammering the
   salvo while watching stderr for the message; then the same with the
   sound toggled off (`M` -> `soundEnabled=false`) to bisect the audio
   path. Not started yet — the session above is logged first.

**Round of Oct 20 — the user's description pins the symptom.**

- The window **froze completely with the idle engine loop still
  sounding**, and ~3 s later Hyprland showed its wait-or-kill dialog.
  That is a **wedged GUI thread**, not a dead process: the audio lives
  in Qt's multimedia threads and keeps rendering what was already
  playing, the GUI thread stops ticking and the compositor times the
  client out. Combined with the poll-level `QSocketNotifier` warning, the
  shape is: some fd died (audio stream or Wayland), and the next touch
  of the dead object from the GUI thread blocked it for ever.
- Journal around the session shows **no USB events during the runs** —
  the 00:21:46 `USB disconnect` was the EDIFIER M60 speaker, before all
  of it. Not a hotplug event, then.
- The pad reader's fd lifecycle was re-reviewed: `closeDevice()` deletes
  the notifier **before** `::close(fd)`, both loops are bounded. The
  closing code in the game is not the suspect.
- The only tier-4-specific resource pattern stays the audio churn: a
  2.0 s `air-support.wav` played every 1.5 s (overlapping plays of the
  same `SoundEffect`), on top of the two infinite engine loops.
- **The fuse that turns an fd warning into a full GUI wedge: the 100 ms
  engine timer touches `SoundEffect` state on the GUI thread every 100
  ms** (`sndEngine.playing` read + `.volume` re-assignment, Main.qml
  lines 160-183). If the audio layer wedges, the GUI blocks within one
  tick of it — exactly matching "froze while the idle loop kept playing".
- Bug found while mapping this: **that timer ignores
  `backend.soundEnabled`**, so `M` never silences the engine loops (and
  the churn keeps running after a mute — this also weakens the naive
  "bisect with M" plan; the timer needs its own mute branch for the
  bisection to mean anything).

Capture recipe at the next freeze (leave the dialog on "Wait"):
`ptrace_scope=1` blocks attaching from a sibling shell, so sudo it:

```sh
PID=$(pgrep -x oligarchy-tanks)
sudo eu-stack -p $PID > /tmp/tanks-hang.txt
sudo gdb -p $PID -batch -ex "thread apply all bt" >> /tmp/tanks-hang.txt 2>&1
ls -l /proc/$PID/fd > /tmp/tanks-fds.txt
for t in /proc/$PID/task/*; do echo "$t $(cat $t/comm) $(cat $t/wchan)"; done > /tmp/tanks-threads.txt
```

The auto-capture harness (this round, throwaway in `/tmp/opencode/freeze`)
wraps the project wrapper: a probe process that loads the real QML with
audio, drives tier-4 salvos endlessly (bank topped so a salvo always
restarts) plus a real 100 ms heartbeat written by a `QTimer` on the GUI
thread; a watchdog shell process — parent of the probe, so it may ptrace
without sudo — detects a stalled heartbeat (>1.5 s) and runs `eu-stack`,
gdb and the fd listing on its own. A hang caught this way needs no user
reflexes. `watchdog_real.sh` is the same idea for a session of play: it
launches the real binary (`OLIGARCHY_TANKS_TIER=4`) as its own child and
reacts to a 3 s stall of the main thread's CPU clock, capturing and then
killing the game so the compositor dialog never shows.

**Results of the first capture rounds (Oct 20, after the commit):**

- **Offscreen stress, audio on, the tier-4 + BLAST mix driven for ever**
  (salvo refilled as soon as the bank drains, blast shots through the
  game's own fire path, engine loops live, 100 ms heartbeat watched):
  two clean 150 s rounds before the queue was stopped to make room for a
  real session. No hang.
- **A real ~17 min tier-4 session by the user under `watchdog_real.sh`**
  (real fullscreen, real audio, real pad present): played to completion,
  the user exited normally at 01:50. The watchdog sat ready the whole
  time, sampled the main thread every 300 ms and never saw a stall;
  the game's stderr holds **zero lines** — not one `QSocketNotifier`
  warning, not even a mild one. No hang, no symptom at all.
- Interpretation so far: the issue is rarer than 2-of-2 — yesterday's
  twin freezes were a burst, not an everyday state. The audio-fd suspect
  stands; so does "stop chasing, hunt with data". The watchdog is the
  standing tool: run `/tmp/opencode/freeze/watchdog_real.sh` before any
  play session and, on a hang, it captures everything by itself.
- Still open for the two failed sessions: how long they ran before
  freezing, one or two players, keyboard or pad driving, and whether
  BLAST was firing right before (the fifth-missile collision bug was
  live in yesterday's build and gone in this one — if a freeze came just
  after a fifth blast shot, the bullet churn is suspect number two
  behind the audio).
- **The user's answer (Oct 20):** the game was opened with the tier-4
  grant and **froze mid-air-support**, playing alone, **with the pad
  driving**, and with **Zen (browser) open on another monitor playing
  Twitter**. So the freeze environment was: a busy machine (a browser
  scrolling media on the second screen), audio salvo overlapping in
  flight, pad read by the game. All three stand: system load around the
  audio fd, and the audio churn remains suspect one. Today's clean ~17
  min session was single-player pad-present but the browser/context load
  was not reported — treat "browser doing video on the second monitor"
  as a possible ingredient, not proven.

### 21.16 The v0.2.0 publication round (Oct 20, shipped)

The game round of 21.9-21.14 lands tagged and public:

- The mute fix rode a last commit before tagging: **`f990382`** — the
  100 ms engine timer now stops both loops while muted (the user
  verified M silences the putter; bin/test 72/72 and the offscreen smoke
  were already green). The study record of 21.15 travelled in the same
  commit.
- **Tag `v0.2.0`** is on `f990382`; `master` pushed (everything from the
  CRT round on went public at once: `16c748d` .. `f990382`).
- **GitHub release published**:
  https://github.com/NickmDrummer/oligarchy-tanks/releases/tag/v0.2.0 —
  highlights cover the cabinet/CRT/mural round, the user's own sprites
  with the moss/brown/gray armour degradation, the power-up tuning (bank
  air support, 3-cell homing blast, drops anywhere free), the sound round
  (levelup arpeggio, single emissions, louder air support), the panel
  budget, and the two fixes (fifth blast shot, M turning the engine
  loops off).
- **`omarchy-pkgs` recipe PR #1348 raised**:
  https://github.com/omacom/omarchy-pkgs/pull/1348 (branch
  `add-oligarchy-tanks` on the fork). Files:
  `pkgbuilds/oligarchy-tanks/{PKGBUILD,oligarchy-tanks.install,.omarchy/package.json}`.
  - Source-pinned tag tarball (`v0.2.0`, sha256 recorded); build is
    plain `qmake6 oligarchy-tanks.pro && make`.
  - **Verified before raising**: the package built clean from the v0.2.0
    tarball itself, and note what differs from the in-repo dev build —
    the plain tarball build produces the binary at the project root
    (`./oligarchy-tanks`), not `build/oligarchy-tanks` (that path is a
    `bin/build` convenience); re-verified that `upstream-watch.py check`
    prints `already current` and `tests/pinned-sources.sh` passes.
  - Installs: the binary, LICENSE + the three font licences, the desktop
    file and the icon as `oligarchy-tanks.svg` (the in-repo PKGBUILD
    has now been aligned to `0.2.0` and `oligarchy-tanks.svg` as well),
    and the pad/input-group note as `install-note.txt`.
- **Left standing for this round**: PR #1348's CI build (x86_64 and
  aarch64) is watched as part of the store round.
- **The Store is down, and not by us**: `omarchystore.com` now serves a
  "We took the store offline. We'll be back, better and safer" page —
  the store's own maintainers took it down over concerns about how the
  installer and the update hook handle privileges, and they are
  rebuilding installs so the user always sees and approves them, plus
  re-reviewing every listing. `/submit/` answers **404** (the site is
  a holding page now, not a live store). So the Store listing is
  **not actionable today** and does not need the user's captures yet;
  when it reopens, the listing needs the install command, the
  install/removal docs in the README and the fullscreen previews.
- **AUR registration is still closed** (2026-10-20: `/accounts/register`
  answers **503**, the AUR root is up), and the name `oligarchy-tanks`
  is still free (AUR RPC: 0 hits). The AUR recipe is therefore
  **written and verified, waiting only to be pushed**: it lives in
  `aur/` (PKGBUILD + `oligarchy-tanks.install` + `.gitignore` +
  `.SRCINFO`), a source recipe pinned to the `v0.2.0` tag tarball
  (same sha256 as the omarchy-pkgs one), building with
  `qmake6 oligarchy-tanks.pro && make`. **Verified**: `makepkg` built
  `oligarchy-tanks-0.2.0-1-x86_64.pkg.tar.zst` clean from that recipe
  and `makepkg --printsrcinfo` produced a valid `.SRCINFO`. Pushing
  it needs the user's AUR account, which cannot be created while
  registration is closed.
- The freeze hunt stays open as 21.15 — the watchdog lives at
  `/tmp/opencode/freeze/watchdog_real.sh` and the user reported a clean
  ~17 min session; the round scope was timeboxed (user: "terminemos con
  la ronda de publicación si es rápido").
- **PR #1348's merge pipeline, read from the repo's own workflows** (as
  of the same afternoon): the PR sits `OPEN`/`MERGEABLE`/`BLOCKED` with
  no failure — the required `result` check stays pending until an
  Omarchy maintainer labels it **`build-approved`**; first-time
  contributors are not in `.github/VOUCHED.td`, so `auto-merge-pr` ran
  but decided not to enable itself (its script rule: trusted = bot /
  collaborator / vouched, else labelled). The `approve` check showing
  SKIPPED is normal: that workflow only releases the held `build-pr`
  runs once the label is on. With the label, `build-pr` builds x86_64
  (self-hosted) plus aarch64 (GitHub ARM runners); once `result`,
  `self-tests` and `build-isolation` are green, auto-merge merges and
  `publish.yml` publishes the one artifact into every channel the
  package ships to — a brand-new package first appears on `edge`, and
  the ordinary `edge → rc → stable` promotion train carries it to
  stable users later. Install path for end users: `omarchy-pkg-add
  oligarchy-tanks` (the pkgs.omarchy.org repo is preconfigured on
  Omarchy). Realistic review risks: a request to drop `aarch64` if the
  ARM `qt6-*` mass is incomplete on ALARM, or dependency trims;
  corrections go on the same `add-oligarchy-tanks` branch and the PR
  updates itself. Local clone of the recipe fork lives at
  `/tmp/omarchy-pkgs` (branch `add-oligarchy-tanks`).
- **The recipe was tightened after the PR went up** (commit `d659e3da`
  on the branch, so the PR picked it up): the source now uses the
  repo's explicit tarball form
  (`"$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz"`)
  and builds from `$srcdir`, `hicolor-icon-theme` was added so the
  desktop icon resolves, and `.omarchy/package.json` gained
  **`"auto_merge": true`** — a new tag in the game's own GitHub repo
  now updates the recipe by itself, with no further PR. Verified
  before pushing: the recipe builds a clean
  `oligarchy-tanks-0.2.0-1-x86_64.pkg.tar.zst` with the right file
  list (binary, desktop file, icon, four licences), `pinned-sources.sh`
  passes and `upstream-watch.py` still prints `already current`.

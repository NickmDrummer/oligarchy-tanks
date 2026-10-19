# Oligarchy Tanks

Retro local tank battle vs AI, in the style of classic Battle City on Sega.

Defend the star base alone or with a friend. 16 enemies per sector, random map
every game.

## Start

The game opens on the arcade select screen: **1 PLAYER** or **2 PLAYERS**.

- `Left` / `Right` (or `A` / `D`, or the pad's stick and d-pad) move the
  highlight.
- `Space` / `Enter`, `START` on a pad, or a click confirms it.

Each mode shows its own keys under its name, drawn as key caps: player 1 on
`W A S D` and `Space`, player 2 on the arrows and `Enter`. With a pad plugged
in, a row of pad pictures appears below them (`L2` / `R2` / `X` to fire).

In a **one-player game there is a single tank**: when it runs out of lives the
run is over. Either way the run ends on a **`GAME OVER`** screen with the score
(`BASE DESTROYED` if the base was lost), and `START`, `Space` or `Enter` carries
on to the select screen with the last mode still highlighted.

`R` during a game also returns to the select screen; on `victory` it goes
straight to the next sector.

## Controls

| Key | Action |
|-----|--------|
| P1 `W A S D` | Move player 1 |
| `Space` | Fire player 1 |
| P2 Arrow keys | Move player 2 |
| `Enter` | Fire player 2 |
| `P` | Pause / resume |
| `R` | Back to the select screen (next sector on `victory`) |
| `M` | Sound on / off |
| `?` | Help overlay |
| `Q` | Quit |

## Gamepad

A DualSense or Xbox pad works out of the box: it is read straight from
`/dev/input`, with no extra software. **The keyboard always stays live for both
players**, so a pad never takes a tank away from anyone.

| Pad | Action |
|-----|--------|
| `START` | Confirm the mode on the select screen, join a tank, pause and resume |
| Left stick / d-pad | Move (and pick the mode on the select screen) |
| `L2` / `R2`, or `Square` (PlayStation) / `X` (Xbox) | Fire (and confirm the mode) |
| `SHARE` / `SELECT` | Open the manual, like `?` |

Each tank shows the device driving it (`KEY` or `PAD n`) and, underneath,
a blinking hint with the keys that drive it (`Space`; `Enter` for player 2;
`START` too while a pad is around), drawn as key caps. The hint is only an
invitation: it goes away as soon as that player uses a control, and it never
blocks anything.

On the select screen any pad drives the choice, even one that just played a
run: stick or d-pad moves the highlight, `START` (or `Square` / `X`) confirms it.
Starting a new game hands every pad back to waiting, so which tank a pad gets is
always decided by the mode and by who presses `START`, never carried over from
the run before.

Joining is by `START`:

- **One player** — the keyboard drives the tank (or `Space`); a pad takes it
  over as soon as it presses `START`.
- **Two players** — player 1 keeps the keyboard, the **first** pad to press
  `START` takes player 2 and a second pad takes player 1, so one pad means one
  human on it.

Unplugging a pad hands its tank back to the keyboard and the hint comes back;
plug it in again and press `START` to rejoin (it may come back with a different
pad number).

Reading `/dev/input/event*` requires being in the `input` group. Most desktop
installs already are; if pads are not detected, check with `id -nG`. Without the
group, or with no pad plugged in, the game simply runs on the keyboard as
before. To diagnose a pad that does not work, run `OLIGARCHY_TANKS_PAD_DEBUG=1 ./build/oligarchy-tanks`
and watch the `[pad]` lines.

## Build

```
bin/build
./build/tanks
bin/test
bin/install
```

## Development

Architecture notes, the state of the build and the handover for the next
contributor live in [`AGENTS.md`](AGENTS.md).

## Credits

The UI is set in [Silkscreen](https://github.com/googlefonts/silkscreen) by
Jason Kottke, bundled under the SIL Open Font License (see
`src/fonts/OFL.txt`). Descriptive text in the manual uses
[Departure Mono](https://departuremono.com/) by Helena Zhang, also SIL OFL
(see `src/fonts/OFL-DepartureMono.txt`).

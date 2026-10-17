# Tanks

Retro 2-player local tank battle vs AI, in the style of classic Battle City on Sega.

Defend the star base with a friend. 8 enemies per sector, random map every game.

## Controls

| Key | Action |
|-----|--------|
| P1 `W A S D` | Move player 1 |
| `Space` | Fire player 1 |
| P2 Arrow keys | Move player 2 |
| `Enter` | Fire player 2 |
| `P` | Pause / resume |
| `R` | Restart (new random map) |
| `M` | Sound on / off |
| `?` | Help overlay |
| `Q` | Quit |

## Gamepad

A DualSense or Xbox pad works out of the box: it is read straight from
`/dev/input`, with no extra software. **The keyboard always stays live for both
players**, so a pad never takes a tank away from anyone.

| Pad | Action |
|-----|--------|
| `START` | Join (the prompt `PRESS START TO JOIN` shows while a pad is waiting), pause and resume |
| Left stick / d-pad | Move |
| `L2` / `R2`, or `Square` (PlayStation) / `X` (Xbox) | Fire |
| `SHARE` / `SELECT` | Open the manual, like `?` |

The first pad to press `START` takes **player 2** — player 1 keeps the keyboard.
A second pad takes **player 1**, so each player gets their own pad. Unplugging a
pad hands its tank back to the keyboard and the prompt comes back; plug it in
again and press `START` to rejoin (it may come back with a different pad number).

Reading `/dev/input/event*` requires being in the `input` group. Most desktop
installs already are; if pads are not detected, check with `id -nG`. Without the
group, or with no pad plugged in, the game simply runs on the keyboard as
before. To diagnose a pad that does not work, run `TANKS_PAD_DEBUG=1 ./build/tanks`
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

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

Gamepad (DualSense / Xbox) is planned: input is already abstracted per player in `Backend` (`setP1Move` / `setP2Move` / `fireP1` / `fireP2`), so a stick + button layer can drive the same path later.

## Build

```
bin/build
./build/tanks
bin/test
bin/install
```

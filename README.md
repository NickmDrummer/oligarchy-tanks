# Oligarchy Tanks

Retro local tank battle vs AI, in the style of classic Battle City on Sega.

Defend the star base alone or with a friend. 16 enemies per sector, random map
every game. Every sector opens with a short overture and plays to its own
loop — `M` silences all of it.

<p align="center">
  <img src="docs/screenshots/gameplay1.png" alt="A run in the first stage" width="100%">
</p>

<p align="center">
  <img src="docs/screenshots/select.png" alt="The arcade select screen" width="49%">
  <img src="docs/screenshots/gameplay2.png" alt="A two-player run in the second stage" width="49%">
</p>

## Install

On Arch and Omarchy:

```sh
omarchy pkg add oligarchy-tanks
```

Or from the AUR, once the package is published:

```sh
yay -S oligarchy-tanks
```

Building from source needs Qt 6 (`qt6-base`, `qt6-declarative`,
`qt6-multimedia`):

```sh
bin/build
./build/oligarchy-tanks
```

## Start

The game opens on the arcade select screen: **1 PLAYER** or **2 PLAYERS**.

- `Left` / `Right` (or `A` / `D`, or the pad's stick and d-pad) move the
  highlight.
- `Space` / `Enter`, `START` on a pad, or a click confirms it.

Each mode shows its own keys under its name, drawn as key caps: player 1 on
`W A S D` and `Space`, player 2 on the arrows and `Enter`. With a pad plugged
in, a row of pad pictures appears below them (`L2` / `R2` / `X` to fire).

In a **one-player game there is a single tank**: when it runs out of lives the
run is over. A defeat ends on a **`GAME OVER`** screen with the run's total, and
`START`, `Space` or `Enter` carries on to the select screen with the last mode
still highlighted.

Clear a sector and the screen reads **`SECTOR N CLEAR`**, then counts the score
up from zero with its own blips. Only when the count is spent does
**`NEXT SECTOR`** appear, on `START` or `R`: the game waits for the tally, so a
stray press cannot skip it. A victory with nothing to count shows the total at
once.

`R` during a game also returns to the select screen.

## Controls

| Key | Action |
|-----|--------|
| P1 `W A S D` | Move player 1 |
| `Space` | Fire player 1 |
| P2 Arrow keys | Move player 2 |
| `Enter` | Fire player 2 |
| `P` | Pause / resume |
| `R` | Back to the select screen (next sector on `victory`, after the count) |
| `M` | Sound on / off |
| `V` | Pad vibration on / off |
| `?` | Help overlay |
| `Q` | Quit |

## Gamepad

A DualSense or Xbox pad works out of the box: on Linux the game reads it
straight from `/dev/input`, and on macOS and Windows through **SDL2** (part of
the build steps under *Other platforms*), with no extra software beyond that.
**The keyboard always stays live for both players**, so a pad never takes a
tank away from anyone.

| Pad | Action |
|-----|--------|
| `START` | Confirm the mode on the select screen, join a tank, pause and resume |
| Left stick / d-pad | Move (and pick the mode on the select screen) |
| `L2` / `R2`, or `Square` (PlayStation) / `X` (Xbox) | Fire (and confirm the mode) |
| `SHARE` / `SELECT` | Menu, like `R` (back from a run, next sector on victory) |

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

On Linux the pad node needs the user in the `input` group, whichever reader is
in use. Most desktop installs already are; if pads are not detected, check with
`id -nG`. On macOS the system may ask for **Input Monitoring** permission the
first time. Without the group, without the permission, or with no pad plugged
in, the game simply runs on the keyboard as before. To diagnose a pad that does
not work, run `OLIGARCHY_TANKS_PAD_DEBUG=1 ./build/oligarchy-tanks` and watch
the `[pad]` lines.

## Rumble

DualSense pads vibrate with the game's own sounds (shoots, hits,
explosions, power-ups) over hidraw, on USB — **on Linux only**: the rumble is a
raw hidraw packet, so macOS and Windows pads play without vibration. Bluetooth
pads and Xbox pads are out of scope for now. Sound and vibration are separate
switches: `M` toggles the sound only, `V` toggles the pad vibration
only — the footer keeps an icon for each. To disable rumble entirely,
run `OLIGARCHY_TANKS_RUMBLE=0 ./build/oligarchy-tanks`.

## Build

```
bin/build
./build/oligarchy-tanks
bin/test
bin/install
```

## Other platforms

The game is a plain Qt 6 app: everything it needs (fonts, sounds, the engine)
ships inside the binary, and the desktop theme has a hard-coded fallback, so it
runs wherever Qt 6 runs:

### macOS

```sh
brew install qt pkg-config sdl2
# Homebrew keeps Qt keg-only, so qmake is not on the PATH by default.
export PATH="$(brew --prefix qt)/bin:$PATH"
qmake oligarchy-tanks.pro
make -j"$(sysctl -n hw.ncpu)"
macdeployqt oligarchy-tanks.app -libpath="$(brew --prefix)/lib"
```

The Homebrew `qt` formula ships everything the game needs: Qt Quick, Quick
Controls 2 and Qt Multimedia. `sdl2` is the gamepad reader (see below), and
`pkg-config` is how qmake finds it — without it the build still works, on the
keyboard.

The app lands in `build/`; run it with `open build/oligarchy-tanks.app` or copy
it to `/Applications`. `macdeployqt` bundles Qt and, with `-libpath`, the SDL2
library. macOS may ask for **Input Monitoring** permission the first time a pad
is read; grant it or the pad stays silent.

If the SDL2 library is not picked up, bundle it by hand:

```sh
cp "$(brew --prefix sdl2)/lib/libSDL2-2.0.0.dylib" \
   build/oligarchy-tanks.app/Contents/Frameworks/
```

### Windows

**Qt Online Installer** (MSVC or MinGW, 64-bit): install *Qt 6.x → Desktop*,
which brings Qt Quick, Quick Controls 2 and Qt Multimedia. The installer does
not ship SDL2 or pkg-config, so get the SDL2 development package from
[libsdl.org](https://github.com/libsdl-org/SDL/releases) (the *devel* archive
for your compiler) and point qmake at it from the **Qt 6.x.x (MinGW)** or
**x64 Native Tools** prompt:

```bat
qmake oligarchy-tanks.pro CONFIG+=sdlpad ^
      INCLUDEPATH+=C:/SDL2/include LIBS+=-LC:/SDL2/lib -lSDL2
mingw32-make -j%NUMBER_OF_PROCESSORS%      rem nmake with MSVC
windeployqt build\oligarchy-tanks.exe
copy C:\SDL2\lib\SDL2.dll build\
```

`windeployqt` does not know about SDL2: the DLL has to sit next to the
executable (or in the PATH) for the pad to work.

**MSYS2** is the lighter route, from its UCRT64 shell:

```sh
pacman -S mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-declarative \
          mingw-w64-ucrt-x86_64-qt6-multimedia mingw-w64-ucrt-x86_64-SDL2 \
          mingw-w64-ucrt-x86_64-tools
qmake6 oligarchy-tanks.pro
make -j"$(nproc)"
windeployqt build/oligarchy-tanks.exe
cp /ucrt64/bin/SDL2.dll build/
```

`windeployqt` is not optional: it copies the Qt libraries, the QML modules and
the platform plugin next to the executable, which is what lets it run outside
the build shell.

### Platform limits

- **Gamepads** use SDL2 on macOS and Windows, and on Linux too when SDL2 is
  installed (the evdev reader is the fallback there). The pad joins a player by
  pressing `START`; `SELECT` opens the menu, exactly as on Linux.
- **Rumble is Linux only**: it is a DualSense hidraw packet, so on macOS and
  Windows the pad plays without vibration. `M` and `V` still work; `V` simply
  has nothing to switch there.
- There is no WebAssembly build (Qt multimedia and pad input do not fit it).

## Development

Architecture notes, the state of the build and the handover for the next
contributor live in [`AGENTS.md`](AGENTS.md).

## Credits

The UI is set in [Silkscreen](https://github.com/googlefonts/silkscreen) by
Jason Kottke, bundled under the SIL Open Font License (see
`src/fonts/OFL.txt`). Descriptive text in the manual uses
[Departure Mono](https://departuremono.com/) by Helena Zhang, also SIL OFL
(see `src/fonts/OFL-DepartureMono.txt`). The game's marquee is set in
[Omarchy Font](https://github.com/markcuda/Omarchy-Font) by Mark Cuda, a
pixel recreation of the official Omarchy wordmark, bundled under the MIT
license (see `src/fonts/OmarchyFont-LICENSE.txt`).

The 8-bit sound set is original project material — no third-party samples.
Some sounds were synthesised specifically for the game, and the rest was
produced by hand in FL Studio Mobile ("the explosions are ours too").

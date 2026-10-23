# Testing the gamepad build (v0.2.4-rc1)

This guide is for someone with a Mac or a Windows PC and a controller who has
never used a terminal. Follow it one step at a time: copy a block, paste it,
press Enter, wait, and read only the last line or two that come out.

You need: a Mac (macOS 12 or newer) or a Windows 10/11 PC, a DualSense or Xbox
pad with a **USB cable** (Bluetooth is not supported yet), and about 20 minutes.

If something fails, jump to [What to send back](#what-to-send-back). Nothing you
do here touches your system beyond installing the tools listed.

## macOS

### 1. Open Terminal

Press `Cmd + Space`, type `Terminal`, press Enter. A window with a prompt
appears: that is where every block below goes.

### 2. Install the compiler tools

```sh
xcode-select --install
```

A dialog appears: click **Install** and wait (5–10 minutes). If it says the
tools are already installed, you are done with this step.

### 3. Install Homebrew

First check whether it is there:

```sh
brew --version
```

If that prints a version, skip to step 4. If it says `command not found`, run:

```sh
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

At the end it prints **two lines starting with `echo`**: copy and paste them
(they put Homebrew on your PATH), then close and reopen Terminal.

### 4. Install Qt, pkg-config and SDL2

```sh
brew install qt pkg-config sdl2
```

This downloads a lot (10–20 minutes). When it finishes you are ready to build.

### 5. Download and build the game

```sh
cd ~
git clone https://github.com/NickmDrummer/oligarchy-tanks.git
cd oligarchy-tanks
git checkout v0.2.4-rc1
./bin/build
```

The last line should say `Built …/build/oligarchy-tanks`. If it stops with an
error, copy the last 15 lines and send them.

### 6. Run it

```sh
./build/oligarchy-tanks
```

Plug the pad in with the USB cable.

**If the pad does nothing:** open **System Settings → Privacy & Security →
Input Monitoring**, switch **Terminal** on, quit the game (`Cmd + Q`) and run it
again. macOS asks for this permission the first time a program reads a pad.

## Windows

### 1. Install MSYS2

Go to <https://www.msys2.org>, download the installer and click **Next** until
it finishes. It installs a terminal, not a virus scanner: nothing else changes.

### 2. Open the right terminal

From the Start menu open **MSYS2 UCRT64** (the blue icon — not "MSYS2 MSYS").
Every block below goes there.

### 3. Update the package list

```sh
pacman -Syu
```

Answer `Y` when it asks. If it says to close the window, close it, reopen
**MSYS2 UCRT64** and run `pacman -Su`.

### 4. Install Qt 6, SDL2 and the tools

```sh
pacman -S git mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-declarative \
          mingw-w64-ucrt-x86_64-qt6-multimedia mingw-w64-ucrt-x86_64-SDL2 \
          mingw-w64-ucrt-x86_64-tools
```

Answer `Y` to the install question.

### 5. Download and build the game

```sh
cd ~
git clone https://github.com/NickmDrummer/oligarchy-tanks.git
cd oligarchy-tanks
git checkout v0.2.4-rc1
./bin/build
```

The last line should say `Built …/build/oligarchy-tanks.exe`.

### 6. Run it

```sh
./build/oligarchy-tanks.exe
```

Plug the pad in with the USB cable. If Windows asks about the firewall, allow
it; the game never uses the network, but the prompt is normal.

## The checklist

Play for a few minutes and tick what works. Anything not ticked is what we need
to hear about.

- [ ] The pad appears on the select screen (a pad row appears under the keys).
- [ ] `START` on the pad joins a tank (in 1 player) or player 2 (in 2 players).
- [ ] The left stick and the d-pad move the tank.
- [ ] `Square` (PlayStation) / `X` (Xbox) fires — and so do `L2` / `R2`.
- [ ] `SHARE` / `SELECT` opens the menu, like `R` does on the keyboard.
- [ ] The keyboard still moves and fires at the same time (it must keep working).
- [ ] Unplug the pad mid-game: the tank goes back to the keyboard, nothing stays
      stuck. Plug it back in and press `START` to rejoin.
- [ ] With two pads, two players: each pad joins with `START`.
- [ ] Quit with the window's close button (or `Cmd + Q` on macOS): the game
      closes, and the Terminal prompt comes back.
- [ ] No freeze, no crash, no sound stuck after closing.

## What to send back

1. Your OS and version (macOS "About This Mac", or Windows "winver").
2. The pad model (DualSense, DualShock 4, Xbox Series…).
3. Which checklist items failed and what happened instead.
4. The build output, if it did not compile.
5. If the pad is not detected at all, run this, press some buttons, quit, and
   send the `pad.log` file it writes:

```sh
OLIGARCHY_TANKS_PAD_DEBUG=1 ./build/oligarchy-tanks 2> pad.log
```

Thank you — this is the only way the non-Linux pad path gets tested.

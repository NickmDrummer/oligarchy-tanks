# Screenshots

The images the README shows. All of them are full-screen captures of the real
game (1920x1080), taken on the DP-1 monitor in fullscreen, with the
`BUILD <id>` footer visible so each shot names the exact binary.

| File | What it shows |
|------|----------------|
| `select.png` | The arcade select screen: 1 PLAYER / 2 PLAYERS, the key caps, the cabinet marquee, the wall mural and the two player panels. |
| `gameplay1.png` | A run in the **first stage**: the board in the CRT tube, the brickwork, the enemy queue and the player panel with its lives. |
| `gameplay2.png` | The **second stage** with two players (P1 and P2 scoring) and the **LV 1** mark next to player one's tank. |

The manual overlay (`?`) and the victory screen were not captured; the three
above are the ones the README uses.

## How to take them

Omarchy binds the screenshot to `SUPER + END`, which saves into
`~/Pictures/Screenshots/`:

```sh
# 1. the game: it opens fullscreen, so the capture is the whole monitor
./build/oligarchy-tanks

# 2. press SUPER + END (or capture the region by hand with grim)
grim -g "0,0 1920x1080" docs/screenshots/gameplay1.png
```

`grim` captures the region of the screen, not the window, so the game must be
fullscreen and the shot must be taken while it is in front.

## Notes

- The footer carries the build id, so every screenshot names the binary it
  came from. Keep it visible.
- The shot must show the game in fullscreen: the windowed layout is 1040x800
  and the compositor scales it, which blurs the pixel art.

# Screenshots

The images the README shows. All of them are full-screen captures of the real
game (1920x1080), taken on the DP-1 monitor in fullscreen, with the
`BUILD <id>` footer visible so each shot names the exact binary.

| File | What it shows |
|------|----------------|
| `select.png` | The arcade select screen: 1 PLAYER / 2 PLAYERS, the key caps, the cabinet marquee and the wall mural. |
| `gameplay.png` | A run in progress: the board in the CRT tube, the player panel with the tier and the air-support line, the brickwork and enemies. |
| `manual.png` | The manual overlay (`?`), scrolled to the power-up rows. |
| `victory.png` | The victory screen, or the game-over screen with the score. |

## How to take them

The game opens fullscreen, so the capture is the whole monitor:

```sh
# 1. the game: pick 1 PLAYER with Space, then play for a few seconds
./build/oligarchy-tanks

# 2. capture the focused monitor
hyprctl clients -j | python3 -c "..."   # to confirm the geometry
grim -g "0,0 1920x1080" docs/screenshots/gameplay.png
```

`grim` captures the region of the screen, not the window, so the game must be
fullscreen and the shot must be taken while it is in front.

## Notes

- The footer carries the build id, so every screenshot names the binary it
  came from. Keep it visible.
- The shot must show the game in fullscreen: the windowed layout is 1040x800
  and the compositor scales it, which blurs the pixel art.

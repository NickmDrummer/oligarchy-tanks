Oligarchy Tanks
===============

HOW TO PLAY

1. Double-click "oligarchy-tanks.app".

2. The first time, macOS will refuse to open it, because this build is not
   signed by an identified developer (that needs a paid Apple account the
   project does not have). On macOS 15 and newer the old right-click -> Open
   shortcut is gone; do this instead:
   - Double-click the app and let macOS complain.
   - Open System Settings -> Privacy & Security.
   - Scroll down to the message about "oligarchy-tanks" and click
     "Open Anyway", then confirm with your password or Touch ID.
   From then on, a normal double-click works.

3. If macOS says the app is damaged, open Terminal and run this once:
     xattr -cr /Applications/oligarchy-tanks.app
   (change the path if the app lives somewhere else), then open it again.

CONTROLS

   Player 1   W A S D to move, Space to fire
   Player 2   arrow keys to move, Enter to fire
   P pause    R back to the menu    M mute    Q quit

A DualSense or Xbox pad works over USB: press START to join, left stick or
d-pad to move, Square / X (or L2 / R2) to fire, SHARE / SELECT for the menu.
The first time, macOS may ask for "Input Monitoring" permission; allow it for
the app (or for Terminal, if you launch it from there) or the pad stays quiet.

This folder is self-contained: Qt and SDL2 live inside the app. Leave the
files together.

#!/bin/bash
# Reproduce the freeze the way it actually happens: the game's audio dies
# and never comes back.
#
# The first attempt unbound the USB audio dongle, but the dongle was
# re-bound right after and pipewire rebuilt the PCM, so the game
# recovered. The real failure (Oct 21 01:30:55) left the stream dead:
#
#   usb 1-2: USB disconnect, device number 3
#   spa.alsa: front:4p: snd_pcm_status error: No such device
#   spa.alsa: front:4p: close failed: No such device
#
# and nothing brought the device back, so the game's SoundEffect kept
# pointing at a dead fd and the GUI thread blocked on it for ever.
#
# So: kill the game's own pipewire stream. The SoundEffect is left
# holding a dead fd, the sink stays up, and nothing recovers it -- the
# state the game was in when it froze.
#
# Usage: tools/repro-freeze.sh [seconds-until-the-stream-dies]
set -u

REPO="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT="${OLIGARCHY_REPRO_OUT:-/tmp/opencode/freeze}"
DELAY="${1:-20}"

mkdir -p "$OUT"

# The game, with the watchdog around it.
"$REPO/tools/watchdog.sh" "$((DELAY / 60 + 3))" 4 &
WATCHDOG_PID=$!

# Wait for the game to be up.
sleep "$DELAY"

GAME_PID=$(pgrep -x oligarchy-tanks | head -1)
echo "game pid: ${GAME_PID:-none}"
if [[ -z "${GAME_PID:-}" ]]; then
  echo "the game is not running; nothing to kill" >&2
  kill "$WATCHDOG_PID" 2>/dev/null
  exit 1
fi

# The game must be IN a run: the engine loops only sound while a tank is
# alive, and the tier-4 salvo only fires in a run. The select screen is
# silent, so a dead device there would prove nothing.
echo "starting a run (Space on the select screen)"
wtype -k space 2>/dev/null || wtype " " 2>/dev/null
sleep 3
# A run also needs the tank to move, or the gallop stays silent.
for key in w a s d; do
  wtype -k "$key" 2>/dev/null
  sleep 0.3
done

# The game's own stream: mono at 22050 Hz is the game's sample format, so
# it is the one to kill. The others belong to the browser and the shell.
STREAM=""
for line in $(pactl list short sink-inputs 2>/dev/null | awk '$3=="1" && $4 ~ /22050/ {print $1}'); do
  if pactl list sink-inputs 2>/dev/null | grep -A12 "Sink Input #$line" | grep -q "application.name = \"oligarchy-tanks\""; then
    STREAM="$line"
    break
  fi
done
if [[ -z "$STREAM" ]]; then
  # Fall back on the format alone: the game is the only mono 22050 client.
  STREAM=$(pactl list short sink-inputs 2>/dev/null | awk '$3=="1" && $4 ~ /22050/ {print $1}' | head -1)
fi
echo "the game's stream: ${STREAM:-none}"

if [[ -n "$STREAM" ]]; then
  echo "killing it: the SoundEffect is left with a dead fd and nothing recovers it"
  pactl kill-sink-input "$STREAM" 2>/dev/null || echo "(pactl could not kill it)"
else
  echo "no stream found; the game may not be in a run" >&2
fi

# Give the game time to touch the dead stream.
sleep 25

# The watchdog is still watching; let it notice the stall and capture.
wait "$WATCHDOG_PID" 2>/dev/null

echo
echo "=== the watchdog log ==="
cat "$OUT/watchdog.log" 2>/dev/null
if [[ -s "$OUT/real-stack.txt" ]]; then
  echo "=== CAPTURED: $OUT/real-stack.txt ==="
  head -50 "$OUT/real-stack.txt"
else
  echo "=== no stall was captured ==="
fi

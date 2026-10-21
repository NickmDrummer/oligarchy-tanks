#!/bin/bash
# Freeze watchdog for a real Oligarchy Tanks play session.
#
# The GUI thread of the game has wedged: the window froze while the idle
# engine loop kept sounding, and a few seconds later the compositor
# offered to kill the client. The cause is known (Aug 21): the USB audio
# device dies, pipewire loses its PCM, and the SoundEffect the game keeps
# touching from its 100 ms timer blocks the GUI thread for ever.
#
# What this does:
#   - runs the game UNDER gdb, so gdb is the direct parent and may ptrace
#     the inferior even with kernel.yama.ptrace_scope=1 (a process may
#     always inspect its own children; a separate gdb -p would be refused);
#   - watches the main thread's CPU clock from the shell: 3 s without a
#     single tick is a hang (the GUI thread is what freezes, while the
#     audio threads keep playing);
#   - on a hang it interrupts the inferior so gdb prints every thread's
#     backtrace, captures the fd list and the per-thread wait channels,
#     and then kills the game so the compositor's dialog never appears.
#
# The game's own stdout and stderr land next to the captures, which is
# how a QSocketNotifier warning would be caught.
#
# Usage: tools/watchdog.sh [minutes] [tier]
#   minutes  how long to watch (default 60)
#   tier     the tier to grant with OLIGARCHY_TANKS_TIER (default 4, the
#            tier whose overlapping air-support sample the freezes had)
#
# Artifacts (in $OUT):
#   real-stack.txt   gdb's backtrace of every thread (the game's own stderr
#                    is in there too, since gdb captured both streams)
#   real-fds.txt     the open file descriptors
#   real-threads.txt every thread and its wait channel
#   gdb-out.txt      the raw gdb output
#   watchdog.log     what the watchdog saw

set -u

REPO="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
# The binary to watch. It defaults to the game, but any program works,
# which is how the probe (the real QML in a forced tier-4 run) is tested.
BIN="${OLIGARCHY_WATCHDOG_BIN:-$REPO/build/oligarchy-tanks}"
OUT="${OLIGARCHY_WATCHDOG_OUT:-/tmp/opencode/freeze}"
STALL_SECONDS=3
SAMPLE_MS=300

MINUTES="${1:-60}"
TIER="${2:-4}"
DURATION=$((MINUTES * 60))

if [[ ! -x "$BIN" ]]; then
  echo "no binary at $BIN -- run bin/build first" >&2
  exit 1
fi

mkdir -p "$OUT"
rm -f "$OUT/real-stack.txt" "$OUT/real-fds.txt" "$OUT/real-threads.txt" \
      "$OUT/game-stdout.log" "$OUT/game-stderr.log" "$OUT/watchdog.log" \
      "$OUT/gdb-out.txt"

# The tier grant: the tier-4 sessions are the ones that froze.
export OLIGARCHY_TANKS_TIER="$TIER"

# gdb runs the game, so it is the direct parent and may ptrace it. The
# inferior's stdout and stderr are the game's own.
gdb -batch \
    -ex "set debuginfod enabled off" \
    -ex "set confirm off" \
    -ex "handle SIGINT stop print nopass" \
    -ex run \
    -ex "thread apply all bt" \
    -ex "info threads" \
    -ex "kill" \
    --args "$BIN" >"$OUT/gdb-out.txt" 2>&1 &
GDB_PID=$!

# Wait for the game to be up. gdb runs it as its inferior, so the process
# is the one gdb forked; it is found by its name.
for _ in $(seq 1 40); do
  GAME_PID=$(pgrep -x "$(basename "$BIN")" | head -1)
  [[ -n "${GAME_PID:-}" ]] && break
  sleep 0.25
done
if [[ -z "${GAME_PID:-}" ]]; then
  echo "the game did not start" >&2
  cat "$OUT/gdb-out.txt" >&2
  kill "$GDB_PID" 2>/dev/null
  exit 1
fi

# The tier-4 grant is what the failing sessions used.
echo "game pid $GAME_PID, tier $TIER, watching for $DURATION s" | tee "$OUT/watchdog.log"

# Main-thread CPU ticks (utime + stime), from the thread's own stat file.
main_ticks() {
  local stat
  stat=$(cat "/proc/$1/task/$1/stat" 2>/dev/null) || return 1
  # The comm field may contain spaces, so cut after the closing parenthesis.
  local rest="${stat#*) }"
  # rest: state ppid ... -> utime is field 14 and stime field 15 of the
  # original numbering, i.e. fields 12 and 13 of the remainder.
  echo "$rest" | awk '{print $12 + $13}'
}

LAST=0
STALLED=0
START=$(date +%s)

while kill -0 "$GAME_PID" 2>/dev/null; do
  NOW=$(date +%s)
  if (( NOW - START > DURATION )); then
    echo "watch window over, letting the game run" | tee -a "$OUT/watchdog.log"
    break
  fi

  TICKS=$(main_ticks "$GAME_PID" 2>/dev/null) || break
  if [[ -z "$TICKS" ]]; then
    sleep "$(awk "BEGIN{print $SAMPLE_MS/1000}")"
    continue
  fi

  if (( TICKS == LAST )); then
    STALLED=$((STALLED + 1))
    NEEDED=$((STALL_SECONDS * 1000 / SAMPLE_MS))
    if (( STALLED >= NEEDED )); then
      echo "HANG: main thread idle for ${STALL_SECONDS}s at $(date -Is)" | tee -a "$OUT/watchdog.log"
      # The fd list and the wait channels first: they are read from /proc and
      # the process must still be alive for them.
      ls -l "/proc/$GAME_PID/fd" >"$OUT/real-fds.txt" 2>&1
      for t in /proc/$GAME_PID/task/*; do
        echo "$t $(cat "$t/comm" 2>/dev/null) $(cat "$t/wchan" 2>/dev/null)"
      done >"$OUT/real-threads.txt" 2>&1
      # Interrupt the inferior: gdb stops it and prints the backtraces.
      kill -INT "$GAME_PID" 2>/dev/null
      sleep 3
      {
        echo "=== captured $(date -Is) ==="
        echo
        cat "$OUT/gdb-out.txt"
      } >"$OUT/real-stack.txt"
      echo "captured into $OUT/real-stack.txt; killing the game" | tee -a "$OUT/watchdog.log"
      kill -9 "$GAME_PID" 2>/dev/null
      break
    fi
  else
    STALLED=0
  fi

  LAST=$TICKS
  sleep "$(awk "BEGIN{print $SAMPLE_MS/1000}")"
done

if kill -0 "$GAME_PID" 2>/dev/null; then
  echo "game still running (pid $GAME_PID) -- leaving it alone" | tee -a "$OUT/watchdog.log"
else
  wait "$GDB_PID" 2>/dev/null
  echo "game exited on its own" | tee -a "$OUT/watchdog.log"
fi

if [[ -s "$OUT/gdb-out.txt" ]]; then
  echo "--- the game's own output ---"
  cat "$OUT/gdb-out.txt"
fi

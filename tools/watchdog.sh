#!/bin/bash
# Freeze watchdog for a real Oligarchy Tanks play session.
#
# The GUI thread of the game has wedged twice (Oct 19-20, 2026): the window
# froze while the idle engine loop kept sounding, and a few seconds later
# the compositor offered to kill the client. Three more times on a second
# machine, so it is not one machine's audio stack. The symptom is a blocked
# GUI thread, not a crash: no core dump, no SIGSEGV.
#
# What this does:
#   - launches the real binary as its OWN child, so the script may ptrace it
#     without sudo even with kernel.yama.ptrace_scope=1 (a process may always
#     inspect its own descendants);
#   - samples the main thread's CPU clock every 300 ms and calls 3 s without a
#     single tick a hang (the GUI thread is what freezes; the audio threads
#     keep playing, which is exactly the reported symptom);
#   - on a hang it captures eu-stack + gdb (every thread's backtrace) + the fd
#     list + the per-thread wait channel, then kills the game so the
#     compositor's wait-or-kill dialog never appears.
#
# The game's own stdout and stderr land next to the captures, which is how a
# QSocketNotifier warning would be caught.
#
# Usage: tools/watchdog.sh [minutes] [tier]
#   minutes  how long to watch (default 60)
#   tier     the tier to grant with OLIGARCHY_TANKS_TIER (default 4, the
#            tier whose overlapping air-support sample both freezes had)
#
# Artifacts (in $OUT):
#   real-stack.txt   eu-stack + gdb backtraces
#   real-fds.txt     the open file descriptors
#   real-threads.txt every thread and its wait channel
#   game-stderr.log  the game's own stderr
#   watchdog.log     what the watchdog saw

set -u

REPO="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
BIN="$REPO/build/oligarchy-tanks"
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
      "$OUT/game-stdout.log" "$OUT/game-stderr.log" "$OUT/watchdog.log"

# The tier-4 grant is what the failing sessions used.
OLIGARCHY_TANKS_TIER="$TIER" "$BIN" >"$OUT/game-stdout.log" 2>"$OUT/game-stderr.log" &
GAME_PID=$!
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
      {
        echo "=== captured $(date -Is) ==="
        echo
        echo "--- eu-stack ---"
        eu-stack -p "$GAME_PID" 2>&1
        echo
        echo "--- gdb: thread apply all bt ---"
        gdb -p "$GAME_PID" -batch -ex "thread apply all bt" 2>&1
      } >"$OUT/real-stack.txt"
      ls -l "/proc/$GAME_PID/fd" >"$OUT/real-fds.txt" 2>&1
      for t in /proc/$GAME_PID/task/*; do
        echo "$t $(cat "$t/comm" 2>/dev/null) $(cat "$t/wchan" 2>/dev/null)"
      done >"$OUT/real-threads.txt" 2>&1
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
  wait "$GAME_PID" 2>/dev/null
  echo "game exited on its own" | tee -a "$OUT/watchdog.log"
fi

if [[ -s "$OUT/game-stderr.log" ]]; then
  echo "--- stderr ---"
  cat "$OUT/game-stderr.log"
fi

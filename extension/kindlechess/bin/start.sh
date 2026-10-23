#!/bin/sh
HERE="$(dirname "$0")"
EXTDIR="$(cd "$HERE/.." && pwd)"
cd "$EXTDIR" || exit 1

export KINDLECHESS_HOME="$EXTDIR"
export KINDLECHESS_ENGINE="$EXTDIR/bin/stockfish"
export LD_LIBRARY_PATH="$EXTDIR/lib:$LD_LIBRARY_PATH"

# Prevent screen saver while playing chess
lipc-set-prop -i com.lab126.powerd preventScreenSaver 1 2>/dev/null || true

# Ensure binaries have executable bit
[ -f "$KINDLECHESS_ENGINE" ] && chmod +x "$KINDLECHESS_ENGINE"
[ -f "$EXTDIR/bin/kindlechess" ] && chmod +x "$EXTDIR/bin/kindlechess"

# Rotate log file if it exceeds 256KB
LOGFILE="$EXTDIR/data/kindlechess.log"
mkdir -p "$EXTDIR/data"
if [ -f "$LOGFILE" ]; then
    LOGSIZE=$(wc -c < "$LOGFILE" 2>/dev/null || echo 0)
    if [ "$LOGSIZE" -gt 262144 ]; then
        tail -n 500 "$LOGFILE" > "$LOGFILE.tmp" 2>/dev/null && mv "$LOGFILE.tmp" "$LOGFILE"
    fi
fi

# Clear e-ink screen before launching to remove KUAL ghosting
which eips >/dev/null 2>&1 && eips -c 2>/dev/null || true

# Launch with higher priority for responsive touch controls
if which nice >/dev/null 2>&1; then
    nice -n -2 "$EXTDIR/bin/kindlechess" >> "$LOGFILE" 2>&1
else
    "$EXTDIR/bin/kindlechess" >> "$LOGFILE" 2>&1
fi

# Clean up: clear screen and restore screensaver when exiting
which eips >/dev/null 2>&1 && eips -c 2>/dev/null || true
lipc-set-prop -i com.lab126.powerd preventScreenSaver 0 2>/dev/null || true

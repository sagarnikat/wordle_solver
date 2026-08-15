#!/usr/bin/env bash
# Run the solver against every possible answer and log the stats.
# Usage: scripts/run_all_words.sh [--plain]
set -euo pipefail

cd "$(dirname "$0")/.."

BIN=build/wordle-benchmark
if [[ ! -x "$BIN" ]]; then
    echo "Building benchmark (first run)..." >&2
    cmake -S . -B build >/dev/null
    cmake --build build --target wordle-benchmark -j >/dev/null
fi

LOG_DIR=logs
mkdir -p "$LOG_DIR"
LOG="$LOG_DIR/benchmark-$(date +%Y%m%d-%H%M%S).log"

echo "== wordle-benchmark $* | $(date) ==" | tee -a "$LOG"
"$BIN" "$@" | tee -a "$LOG"
echo "Logged to $LOG"
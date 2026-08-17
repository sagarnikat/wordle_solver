#!/usr/bin/env bash
# Run the solver against every possible answer and log the stats.
# Usage: scripts/run_all_words.sh [--plain]
set -euo pipefail

cd "$(dirname "$0")/.."

BIN=wordle-benchmark
if [[ ! -x "$BIN" ]]; then
    echo "Building benchmark (first run)..." >&2
    g++ -std=c++17 -O2 -Iinclude benchmarks/benchmark_solver.cpp \
        src/feedback.cpp src/word_list.cpp src/entropy.cpp src/solver.cpp \
        -o "$BIN"
fi

LOG_DIR=logs
mkdir -p "$LOG_DIR"
LOG="$LOG_DIR/benchmark-$(date +%Y%m%d-%H%M%S).log"

echo "== wordle-benchmark $* | $(date) ==" | tee -a "$LOG"
"$BIN" "$@" | tee -a "$LOG"
echo "Logged to $LOG"
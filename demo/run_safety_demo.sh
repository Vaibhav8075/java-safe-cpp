#!/usr/bin/env bash
# Runs every program in demo/safety and shows how each unsafe action ends.
# Usage: demo/run_safety_demo.sh [path-to-jscpp]   (default: build/jscpp)
cd "$(dirname "$0")/.." || exit 1
J="${1:-build/jscpp}"
for f in demo/safety/*.cppsafe; do
    echo "=== $f"
    cat "$f"
    echo "--- jscpp run $f"
    "$J" run "$f"
    echo "(exit status $?)"
    echo
done

#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STOP_TIMEOUT_SECONDS="${NEBULA_STOP_TIMEOUT_SECONDS:-10}"

pids=()
pidfiles=()

for pidfile in "$ROOT"/run/*.pid; do
    [ -e "$pidfile" ] || continue
    pid="$(cat "$pidfile")"
    pidfiles+=("$pidfile")
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
        pids+=("$pid")
        kill "$pid" 2>/dev/null || true
    fi
done

if [[ "${#pids[@]}" -gt 0 ]]; then
    deadline=$((SECONDS + STOP_TIMEOUT_SECONDS))
    while (( SECONDS < deadline )); do
        alive=0
        for pid in "${pids[@]}"; do
            if kill -0 "$pid" 2>/dev/null; then
                alive=1
                break
            fi
        done
        if [[ "$alive" -eq 0 ]]; then
            break
        fi
        sleep 0.2
    done

    for pid in "${pids[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill -KILL "$pid" 2>/dev/null || true
        fi
    done
fi

for pidfile in "${pidfiles[@]}"; do
    rm -f "$pidfile"
done

#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
for pidfile in "$ROOT"/run/*.pid; do [ -e "$pidfile" ] || continue; kill "$(cat "$pidfile")" 2>/dev/null || true; rm -f "$pidfile"; done

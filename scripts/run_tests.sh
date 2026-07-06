#!/usr/bin/env bash
set -euo pipefail
MODE="${1:---unit-only}"
if [[ "$MODE" == "--unit-only" ]]; then
  ctest --test-dir build -L unit --output-on-failure
elif [[ "$MODE" == "--integration" ]]; then
  ctest --test-dir build -L integration --output-on-failure
else
  ctest --test-dir build --output-on-failure
fi

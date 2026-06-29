#!/usr/bin/env bash
set -euo pipefail
if ! command -v clang-format >/dev/null; then echo "clang-format not found"; exit 0; fi
find common gateway user_service relation_service message_service push_service examples tests benchmark -name '*.h' -o -name '*.cpp' | xargs clang-format -i

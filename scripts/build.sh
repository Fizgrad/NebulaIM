#!/usr/bin/env bash
set -euo pipefail
mkdir -p build
cd build
cmake ..
cmake --build . -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"

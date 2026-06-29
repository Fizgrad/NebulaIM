#!/usr/bin/env bash
set -euo pipefail
SCENE="${1:-login}"
mkdir -p benchmark/results
case "$SCENE" in
  tcp) ./build/benchmark/bench_tcp_connections | tee benchmark/results/tcp.txt ;;
  login) ./build/benchmark/bench_gateway_login | tee benchmark/results/login.txt ;;
  single) ./build/benchmark/bench_single_message | tee benchmark/results/single.txt ;;
  group) ./build/benchmark/bench_group_message | tee benchmark/results/group.txt ;;
  e2e) ./build/benchmark/bench_push_e2e | tee benchmark/results/e2e.txt ;;
  *) echo "unknown benchmark: $SCENE"; exit 1 ;;
esac

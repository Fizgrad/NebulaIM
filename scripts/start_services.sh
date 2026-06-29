#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/logs" "$ROOT/run"
start_one(){ name=$1; bin=$2; port=$3; nohup "$ROOT/build/$bin" --config "$ROOT/config/nebula.conf" > "$ROOT/logs/$name.log" 2>&1 & echo $! > "$ROOT/run/$name.pid"; echo "$name started pid=$(cat "$ROOT/run/$name.pid") port=$port"; }
start_one user_service user_service/nebula_user_service 50051
start_one relation_service relation_service/nebula_relation_service 50053
start_one message_service message_service/nebula_message_service 50052
start_one push_service push_service/nebula_push_service 50054
start_one gateway gateway/nebula_gateway "9000/50055"

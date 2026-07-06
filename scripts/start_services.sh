#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG="${NEBULA_CONFIG:-$ROOT/config/nebula.conf}"
mkdir -p "$ROOT/logs" "$ROOT/run"

if [[ "${NEBULA_ENV:-}" == "prod" ]]; then
    if [[ "$CONFIG" == "$ROOT/config/nebula.conf" ]]; then
        echo "refusing production start with development config: $CONFIG" >&2
        exit 1
    fi
    "$ROOT/scripts/validate_prod_config.sh" "$CONFIG"
fi

wait_one(){
    local service=$1
    if [[ "${NEBULA_SKIP_READY_WAIT:-false}" != "true" ]]; then
        "$ROOT/scripts/wait_ready.sh" "$CONFIG" "$service"
    fi
}

start_one(){
    local name=$1
    local bin=$2
    local port=$3
    local readiness=$4
    wait_one "$readiness"
    setsid nohup "$ROOT/build/$bin" --config "$CONFIG" > "$ROOT/logs/$name.log" 2>&1 < /dev/null &
    pid=$!
    echo "$pid" > "$ROOT/run/$name.pid"
    sleep 0.2
    if ! kill -0 "$pid" 2>/dev/null; then
        echo "$name failed to start; see $ROOT/logs/$name.log" >&2
        exit 1
    fi
    echo "$name started pid=$pid port=$port config=$CONFIG"
}

start_one user_service user_service/nebula_user_service 50051 user
start_one relation_service relation_service/nebula_relation_service 50053 relation
start_one conversation_service conversation_service/nebula_conversation_service 50056 conversation
start_one message_service message_service/nebula_message_service 50052 message
start_one gateway gateway/nebula_gateway "9000/50055" gateway
start_one push_service push_service/nebula_push_service 50054 push
start_one admin_service admin_service/nebula_admin_service 50057 admin

#!/usr/bin/env bash
set -euo pipefail

CONFIG="${1:-/etc/nebulaim/nebula.conf}"
SERVICE="${2:-all}"
TIMEOUT_SECONDS="${NEBULA_WAIT_TIMEOUT_SECONDS:-60}"

get_cfg() {
    local key="$1"
    local default="${2:-}"
    if [[ -f "${CONFIG}" ]]; then
        local value
        value="$(awk -F= -v key="${key}" '$0 !~ /^[[:space:]]*#/ && $1 == key {print substr($0, index($0, "=") + 1)}' "${CONFIG}" | tail -n 1)"
        if [[ -n "${value}" ]]; then
            echo "${value}"
            return
        fi
    fi
    echo "${default}"
}

can_tcp() {
    timeout 2 bash -c ":</dev/tcp/$1/$2" >/dev/null 2>&1
}

wait_tcp() {
    local name="$1"
    local host="$2"
    local port="$3"
    local deadline=$((SECONDS + TIMEOUT_SECONDS))
    until can_tcp "${host}" "${port}"; do
        if (( SECONDS >= deadline )); then
            echo "[wait-ready][FAIL] ${name} ${host}:${port}" >&2
            return 1
        fi
        sleep 1
    done
    echo "[wait-ready][OK] ${name} ${host}:${port}"
}

wait_mysql() {
    wait_tcp mysql "$(get_cfg mysql.host 127.0.0.1)" "$(get_cfg mysql.port 3306)"
}

wait_redis() {
    wait_tcp redis "$(get_cfg redis.host 127.0.0.1)" "$(get_cfg redis.port 6379)"
}

wait_kafka() {
    local broker host port
    broker="$(get_cfg kafka.brokers 127.0.0.1:9092)"
    host="${broker%%:*}"
    port="${broker##*:}"
    wait_tcp kafka "${host}" "${port}"
}

wait_user() {
    wait_tcp user-service "$(get_cfg user_service.host 127.0.0.1)" "$(get_cfg user_service.port 50051)"
}

wait_message() {
    wait_tcp message-service "$(get_cfg message_service.host 127.0.0.1)" "$(get_cfg message_service.port 50052)"
}

wait_relation() {
    wait_tcp relation-service "$(get_cfg relation_service.host 127.0.0.1)" "$(get_cfg relation_service.port 50053)"
}

wait_conversation() {
    wait_tcp conversation-service "$(get_cfg conversation_service.host 127.0.0.1)" "$(get_cfg conversation_service.port 50056)"
}

wait_device() {
    wait_tcp device-service "$(get_cfg device_service.host 127.0.0.1)" "$(get_cfg device_service.port 50058)"
}

wait_gateway_rpc() {
    wait_tcp gateway-rpc "$(get_cfg gateway.rpc.host 127.0.0.1)" "$(get_cfg gateway.rpc.port 50055)"
}

case "${SERVICE}" in
    user|conversation|admin)
        wait_mysql
        wait_redis
        ;;
    device)
        wait_mysql
        wait_redis
        wait_gateway_rpc
        ;;
    relation)
        wait_mysql
        wait_redis
        wait_user
        ;;
    message)
        wait_mysql
        wait_redis
        wait_kafka
        wait_user
        wait_conversation
        ;;
    gateway)
        wait_redis
        wait_user
        wait_message
        wait_relation
        ;;
    push)
        wait_mysql
        wait_redis
        wait_kafka
        wait_gateway_rpc
        ;;
    all)
        wait_mysql
        wait_redis
        wait_kafka
        wait_user
        wait_relation
        wait_conversation
        wait_message
        wait_gateway_rpc
        wait_device
        ;;
    *)
        echo "usage: $0 <config> {user|relation|conversation|device|message|gateway|push|admin|all}" >&2
        exit 2
        ;;
esac

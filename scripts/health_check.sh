#!/usr/bin/env bash
set -euo pipefail

CONFIG="${1:-/etc/nebulaim/nebula.conf}"

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

check_tcp() {
    local name="$1"
    local host="$2"
    local port="$3"
    if timeout 2 bash -c ":</dev/tcp/${host}/${port}" 2>/dev/null; then
        echo "[health][OK] ${name} ${host}:${port}"
    else
        echo "[health][FAIL] ${name} ${host}:${port}" >&2
        return 1
    fi
}

failures=0

check_tcp mysql "$(get_cfg mysql.host 127.0.0.1)" "$(get_cfg mysql.port 3306)" || failures=$((failures + 1))
check_tcp redis "$(get_cfg redis.host 127.0.0.1)" "$(get_cfg redis.port 6379)" || failures=$((failures + 1))
check_tcp kafka 127.0.0.1 9092 || failures=$((failures + 1))
check_tcp gateway "$(get_cfg gateway.tcp.host 127.0.0.1)" "$(get_cfg gateway.tcp.port 9000)" || failures=$((failures + 1))
check_tcp gateway-rpc "$(get_cfg gateway.rpc.host 127.0.0.1)" "$(get_cfg gateway.rpc.port 50055)" || failures=$((failures + 1))
check_tcp user-service "$(get_cfg user_service.host 127.0.0.1)" "$(get_cfg user_service.port 50051)" || failures=$((failures + 1))
check_tcp message-service "$(get_cfg message_service.host 127.0.0.1)" "$(get_cfg message_service.port 50052)" || failures=$((failures + 1))
check_tcp relation-service "$(get_cfg relation_service.host 127.0.0.1)" "$(get_cfg relation_service.port 50053)" || failures=$((failures + 1))
check_tcp push-service "$(get_cfg push_service.host 127.0.0.1)" "$(get_cfg push_service.port 50054)" || failures=$((failures + 1))
check_tcp conversation-service "$(get_cfg conversation_service.host 127.0.0.1)" "$(get_cfg conversation_service.port 50056)" || failures=$((failures + 1))
check_tcp admin-service "$(get_cfg admin_service.host 127.0.0.1)" "$(get_cfg admin_service.port 50057)" || failures=$((failures + 1))

if command -v systemctl >/dev/null 2>&1; then
    for unit in nebula-user nebula-relation nebula-conversation nebula-message nebula-push nebula-admin nebula-gateway; do
        if systemctl is-active --quiet "${unit}.service"; then
            echo "[health][OK] ${unit}.service active"
        else
            echo "[health][WARN] ${unit}.service is not active"
        fi
    done
fi

if (( failures > 0 )); then
    echo "[health] failed: ${failures} endpoint(s) unavailable" >&2
    exit 1
fi

echo "[health] ok"

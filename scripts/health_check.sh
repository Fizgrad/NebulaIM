#!/usr/bin/env bash
set -euo pipefail

CONFIG="${1:-/etc/nebulaim/nebula.conf}"
OUTBOX_PENDING_WARN="${OUTBOX_PENDING_WARN:-1000}"
OUTBOX_DEAD_FAIL="${OUTBOX_DEAD_FAIL:-1}"

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

mysql_cmd() {
    local host port user password database
    host="$(get_cfg mysql.host 127.0.0.1)"
    port="$(get_cfg mysql.port 3306)"
    user="$(get_cfg mysql.user nebula)"
    password="$(get_cfg mysql.password nebula)"
    database="$(get_cfg mysql.database nebula_im)"
    if command -v mysql >/dev/null 2>&1; then
        mysql -h "${host}" -P "${port}" -u "${user}" "-p${password}" "${database}" "$@"
    elif command -v docker >/dev/null 2>&1 && docker inspect nebula-mysql >/dev/null 2>&1; then
        docker exec -i nebula-mysql mysql -h 127.0.0.1 -u "${user}" "-p${password}" "${database}" "$@"
    else
        return 127
    fi
}

redis_ping() {
    local host port password
    host="$(get_cfg redis.host 127.0.0.1)"
    port="$(get_cfg redis.port 6379)"
    password="$(get_cfg redis.password '')"
    if command -v redis-cli >/dev/null 2>&1; then
        if [[ -n "${password}" ]]; then
            redis-cli -h "${host}" -p "${port}" -a "${password}" ping 2>/dev/null
        else
            redis-cli -h "${host}" -p "${port}" ping 2>/dev/null
        fi
    elif command -v docker >/dev/null 2>&1 && docker inspect nebula-redis >/dev/null 2>&1; then
        if [[ -n "${password}" ]]; then
            docker exec nebula-redis redis-cli -a "${password}" ping 2>/dev/null
        else
            docker exec nebula-redis redis-cli ping 2>/dev/null
        fi
    else
        return 127
    fi
}

kafka_topics() {
    local broker
    broker="$(get_cfg kafka.brokers 127.0.0.1:9092)"
    if command -v kafka-topics.sh >/dev/null 2>&1; then
        kafka-topics.sh --bootstrap-server "${broker}" --list
    elif command -v docker >/dev/null 2>&1 && docker inspect nebula-kafka >/dev/null 2>&1; then
        docker exec nebula-kafka /opt/kafka/bin/kafka-topics.sh --bootstrap-server "${broker}" --list
    else
        return 127
    fi
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

check_tls() {
    local name="$1"
    local host="$2"
    local port="$3"
    if command -v openssl >/dev/null 2>&1 && timeout 3 openssl s_client -connect "${host}:${port}" -servername "${host}" </dev/null >/dev/null 2>&1; then
        echo "[health][OK] ${name} tls ${host}:${port}"
    elif command -v openssl >/dev/null 2>&1; then
        echo "[health][FAIL] ${name} tls ${host}:${port}" >&2
        return 1
    else
        echo "[health][WARN] openssl missing, falling back to tcp check for ${name}"
        check_tcp "${name}" "${host}" "${port}"
    fi
}

check_mysql() {
    if mysql_cmd --batch --skip-column-names -e "SELECT 1" >/dev/null 2>&1; then
        echo "[health][OK] mysql query"
    else
        echo "[health][FAIL] mysql query" >&2
        return 1
    fi
}

check_redis() {
    if [[ "$(redis_ping 2>/dev/null || true)" == "PONG" ]]; then
        echo "[health][OK] redis ping"
    else
        echo "[health][FAIL] redis ping" >&2
        return 1
    fi
}

check_kafka() {
    if kafka_topics >/dev/null 2>&1; then
        echo "[health][OK] kafka metadata"
    else
        echo "[health][FAIL] kafka metadata" >&2
        return 1
    fi
}

check_outbox() {
    local result pending dead
    result="$(mysql_cmd --batch --skip-column-names -e "SELECT COALESCE(SUM(status IN (0,2,4)),0), COALESCE(SUM(status=3),0) FROM outbox_events" 2>/dev/null || true)"
    if [[ -z "${result}" ]]; then
        echo "[health][WARN] outbox stats unavailable"
        return 0
    fi
    pending="$(awk '{print $1}' <<<"${result}")"
    dead="$(awk '{print $2}' <<<"${result}")"
    if [[ "${dead}" =~ ^[0-9]+$ && "${dead}" -ge "${OUTBOX_DEAD_FAIL}" ]]; then
        echo "[health][FAIL] outbox dead=${dead}" >&2
        return 1
    fi
    if [[ "${pending}" =~ ^[0-9]+$ && "${pending}" -ge "${OUTBOX_PENDING_WARN}" ]]; then
        echo "[health][WARN] outbox pending=${pending}"
    else
        echo "[health][OK] outbox pending=${pending:-0} dead=${dead:-0}"
    fi
}

failures=0

check_mysql || failures=$((failures + 1))
check_redis || failures=$((failures + 1))
check_kafka || failures=$((failures + 1))
check_outbox || failures=$((failures + 1))

if [[ "$(get_cfg gateway.tls.enabled false)" =~ ^(true|1|yes|on)$ ]]; then
    check_tls gateway "$(get_cfg gateway.tcp.host 127.0.0.1)" "$(get_cfg gateway.tcp.port 9000)" || failures=$((failures + 1))
else
    check_tcp gateway "$(get_cfg gateway.tcp.host 127.0.0.1)" "$(get_cfg gateway.tcp.port 9000)" || failures=$((failures + 1))
fi
check_tcp gateway-rpc "$(get_cfg gateway.rpc.host 127.0.0.1)" "$(get_cfg gateway.rpc.port 50055)" || failures=$((failures + 1))
check_tcp user-service "$(get_cfg user_service.host 127.0.0.1)" "$(get_cfg user_service.port 50051)" || failures=$((failures + 1))
check_tcp message-service "$(get_cfg message_service.host 127.0.0.1)" "$(get_cfg message_service.port 50052)" || failures=$((failures + 1))
check_tcp relation-service "$(get_cfg relation_service.host 127.0.0.1)" "$(get_cfg relation_service.port 50053)" || failures=$((failures + 1))
check_tcp push-service "$(get_cfg push_service.host 127.0.0.1)" "$(get_cfg push_service.port 50054)" || failures=$((failures + 1))
check_tcp conversation-service "$(get_cfg conversation_service.host 127.0.0.1)" "$(get_cfg conversation_service.port 50056)" || failures=$((failures + 1))
check_tcp device-service "$(get_cfg device_service.host 127.0.0.1)" "$(get_cfg device_service.port 50058)" || failures=$((failures + 1))
check_tcp admin-service "$(get_cfg admin_service.host 127.0.0.1)" "$(get_cfg admin_service.port 50057)" || failures=$((failures + 1))

if command -v systemctl >/dev/null 2>&1; then
    for unit in nebula-user nebula-relation nebula-conversation nebula-message nebula-gateway nebula-device nebula-push nebula-admin; do
        if systemctl is-active --quiet "${unit}.service"; then
            echo "[health][OK] ${unit}.service active"
        else
            echo "[health][WARN] ${unit}.service is not active"
        fi
    done
fi

if (( failures > 0 )); then
    echo "[health] failed: ${failures} check(s) unavailable" >&2
    exit 1
fi

echo "[health] ok"

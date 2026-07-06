#!/usr/bin/env bash
set -euo pipefail

CONFIG="${1:-/etc/nebulaim/nebula.conf}"

if [[ ! -f "${CONFIG}" ]]; then
    echo "[config] missing file: ${CONFIG}" >&2
    exit 2
fi

errors=0
warnings=0

get_cfg() {
    local key="$1"
    awk -F= -v key="${key}" '
        $0 !~ /^[[:space:]]*#/ && $1 == key {
            value=substr($0, index($0, "=") + 1)
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", value)
            print value
        }
    ' "${CONFIG}" | tail -n 1
}

fail() {
    echo "[config][FAIL] $*" >&2
    errors=$((errors + 1))
}

warn() {
    echo "[config][WARN] $*" >&2
    warnings=$((warnings + 1))
}

require_key() {
    local key="$1"
    local value
    value="$(get_cfg "${key}")"
    [[ -n "${value}" ]] || fail "required key ${key} is empty or missing"
}

require_key admin_service.admin_tokens
require_key mysql.password
require_key redis.password
require_key kafka.brokers

tokens="$(get_cfg admin_service.admin_tokens)"
if [[ "${tokens}" == *CHANGE_ME* ]]; then
    fail "admin_service.admin_tokens still contains CHANGE_ME"
fi
if [[ ! "${tokens}" =~ (^|[;\ ])[^:]+:sha256:[0-9a-fA-F]{64}: ]]; then
    fail "admin_service.admin_tokens must use name:sha256:<64-hex>:scope format"
fi

for dev_hash in \
    7d16ede5b02f503797228451a0a82e8c78ff0edabe098730d36004964aab3550 \
    0bc0145e191c3c55ae1f96d0f13e4c1d04e06d1ceec51b7155452813f1497d6e \
    c761dbd5c62dc88b900525d86c41489a79fd9a663ae21786a192cc13696064e9 \
    64bd36a04f7f5f7fe6fdb06c79baa6a1e47f9ae7d9ba867b0483d778af0f3881; do
    if [[ "${tokens}" == *"${dev_hash}"* ]]; then
        fail "admin_service.admin_tokens contains a bundled development token hash"
    fi
done

mysql_password="$(get_cfg mysql.password)"
redis_password="$(get_cfg redis.password)"
[[ "${mysql_password}" != "nebula" && "${mysql_password}" != CHANGE_ME* ]] || fail "mysql.password must be changed"
[[ -n "${redis_password}" && "${redis_password}" != CHANGE_ME* ]] || fail "redis.password must be set"

grpc_tls="$(get_cfg grpc.tls.enabled)"
loopback_required_keys=(
    user_service.host
    message_service.host
    relation_service.host
    conversation_service.host
    push_service.host
    admin_service.host
    gateway.rpc.host
)

for key in "${loopback_required_keys[@]}"; do
    value="$(get_cfg "${key}")"
    if [[ "${grpc_tls}" != "true" && "${value}" != "127.0.0.1" && "${value}" != "localhost" ]]; then
        fail "${key}=${value} exposes internal RPC without grpc.tls.enabled=true"
    fi
done

gateway_host="$(get_cfg gateway.tcp.host)"
gateway_tls="$(get_cfg gateway.tls.enabled)"
if [[ "${gateway_host}" != "127.0.0.1" && "${gateway_host}" != "localhost" && "${gateway_tls}" != "true" && "${ALLOW_PUBLIC_GATEWAY_PLAINTEXT:-false}" != "true" ]]; then
    fail "gateway.tcp.host=${gateway_host}; bind Gateway to loopback, terminate TLS at Nginx, or enable gateway.tls.enabled=true"
fi

auth_min_length="$(get_cfg auth.password_min_length)"
if [[ "${auth_min_length}" =~ ^[0-9]+$ && "${auth_min_length}" -lt 8 ]]; then
    warn "auth.password_min_length is below 8"
fi

for path_key in grpc.tls.ca_cert_path grpc.tls.server_cert_path grpc.tls.server_key_path; do
    value="$(get_cfg "${path_key}")"
    if [[ "${grpc_tls}" == "true" && ! -r "${value}" ]]; then
        fail "${path_key}=${value} is not readable"
    fi
done

for path_key in gateway.tls.cert_path gateway.tls.key_path; do
    value="$(get_cfg "${path_key}")"
    if [[ "${gateway_tls}" == "true" && ! -r "${value}" ]]; then
        fail "${path_key}=${value} is not readable"
    fi
done

gateway_ca="$(get_cfg gateway.tls.ca_cert_path)"
if [[ "${gateway_tls}" == "true" && "$(get_cfg gateway.tls.require_client_auth)" == "true" && ! -r "${gateway_ca}" ]]; then
    fail "gateway.tls.ca_cert_path=${gateway_ca} is required for client certificate authentication"
fi

if [[ "$(get_cfg trace.enabled)" == "true" && -z "$(get_cfg trace.otlp_endpoint)" ]]; then
    fail "trace.enabled=true requires trace.otlp_endpoint"
fi

if [[ "$(get_cfg internal_rpc.auth.enabled)" == "true" ]]; then
    internal_token="$(get_cfg internal_rpc.auth.token)"
    internal_token_sha="$(get_cfg internal_rpc.auth.token_sha256)"
    [[ -n "${internal_token}" && "${internal_token}" != CHANGE_ME* ]] || fail "internal_rpc.auth.token must be set when internal RPC auth is enabled"
    [[ "${internal_token_sha}" =~ ^[0-9a-fA-F]{64}$ ]] || fail "internal_rpc.auth.token_sha256 must be a 64-hex SHA-256 digest"
    if [[ "${internal_token_sha}" == CHANGE_ME* ]]; then
        fail "internal_rpc.auth.token_sha256 still contains CHANGE_ME"
    fi
fi

if (( errors > 0 )); then
    echo "[config] validation failed: ${errors} error(s), ${warnings} warning(s)" >&2
    exit 1
fi

echo "[config] validation ok: ${warnings} warning(s)"

#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="/opt/nebulaim"
CONFIG_DIR="/etc/nebulaim"
SERVICE_USER="nebula"
START_SERVICES="${START_SERVICES:-false}"

if [[ "${EUID}" -ne 0 ]]; then
    echo "[install] run as root" >&2
    exit 1
fi

required_bins=(
    user_service/nebula_user_service
    relation_service/nebula_relation_service
    conversation_service/nebula_conversation_service
    message_service/nebula_message_service
    device_service/nebula_device_service
    push_service/nebula_push_service
    admin_service/nebula_admin_service
    gateway/nebula_gateway
)

for bin in "${required_bins[@]}"; do
    [[ -x "${ROOT}/build/${bin}" ]] || {
        echo "[install] missing build/${bin}; run cmake --build build -j first" >&2
        exit 1
    }
done

if ! getent group "${SERVICE_USER}" >/dev/null 2>&1; then
    groupadd --system "${SERVICE_USER}"
fi

if ! id "${SERVICE_USER}" >/dev/null 2>&1; then
    useradd --system --gid "${SERVICE_USER}" --home-dir "${PREFIX}" --shell /usr/sbin/nologin "${SERVICE_USER}"
fi

install -d -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${PREFIX}" "${PREFIX}/bin" /var/log/nebulaim /var/lib/nebulaim /run/nebulaim
install -d "${CONFIG_DIR}"

for bin in "${required_bins[@]}"; do
    install -m 0755 "${ROOT}/build/${bin}" "${PREFIX}/bin/"
done

rm -rf "${PREFIX}/docs" "${PREFIX}/deploy" "${PREFIX}/scripts"
cp -a "${ROOT}/docs" "${PREFIX}/"
cp -a "${ROOT}/deploy" "${PREFIX}/"
cp -a "${ROOT}/scripts" "${PREFIX}/"
chown -R "${SERVICE_USER}:${SERVICE_USER}" "${PREFIX}/docs" "${PREFIX}/deploy" "${PREFIX}/scripts"

if [[ ! -f "${CONFIG_DIR}/nebula.conf" ]]; then
    install -m 0640 -o root -g "${SERVICE_USER}" "${ROOT}/config/nebula.prod.conf.example" "${CONFIG_DIR}/nebula.conf"
    echo "[install] wrote ${CONFIG_DIR}/nebula.conf from production example; edit secrets before starting"
else
    echo "[install] keeping existing ${CONFIG_DIR}/nebula.conf"
fi

install -m 0644 "${ROOT}"/deploy/systemd/*.service /etc/systemd/system/
install -m 0644 "${ROOT}"/deploy/systemd/nebulaim.target /etc/systemd/system/
install -m 0644 "${ROOT}/deploy/logrotate/nebulaim" /etc/logrotate.d/nebulaim

systemctl daemon-reload
systemctl enable nebulaim.target

if [[ "${START_SERVICES}" == "true" ]]; then
    "${ROOT}/scripts/validate_prod_config.sh" "${CONFIG_DIR}/nebula.conf"
    systemctl start nebulaim.target
fi

echo "[install] done"

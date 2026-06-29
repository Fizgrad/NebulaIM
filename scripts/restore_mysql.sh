#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: NEBULA_RESTORE_CONFIRM=yes $0 <backup.sql.gz>" >&2
    exit 2
fi

if [[ "${NEBULA_RESTORE_CONFIRM:-no}" != "yes" ]]; then
    echo "[restore] refusing destructive restore; set NEBULA_RESTORE_CONFIRM=yes" >&2
    exit 1
fi

backup="$1"
if [[ ! -r "${backup}" ]]; then
    echo "[restore] backup is not readable: ${backup}" >&2
    exit 1
fi

MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
MYSQL_PORT="${MYSQL_PORT:-3306}"
MYSQL_USER="${MYSQL_USER:-nebula}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-nebula}"
MYSQL_DATABASE="${MYSQL_DATABASE:-nebula_im}"

if command -v mysql >/dev/null 2>&1; then
    restore_cmd=(mysql -h "${MYSQL_HOST}" -P "${MYSQL_PORT}" -u "${MYSQL_USER}" "-p${MYSQL_PASSWORD}" "${MYSQL_DATABASE}")
elif command -v docker >/dev/null 2>&1 && docker inspect nebula-mysql >/dev/null 2>&1; then
    restore_cmd=(docker exec -i nebula-mysql mysql -h 127.0.0.1 -u "${MYSQL_USER}" "-p${MYSQL_PASSWORD}" "${MYSQL_DATABASE}")
else
    echo "[restore] mysql client not found and nebula-mysql container is unavailable" >&2
    exit 1
fi

gzip -dc "${backup}" | "${restore_cmd[@]}"
echo "[restore] restored ${backup} into ${MYSQL_DATABASE}"

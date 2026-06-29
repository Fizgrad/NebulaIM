#!/usr/bin/env bash
set -euo pipefail

BACKUP_DIR="${BACKUP_DIR:-/var/backups/nebulaim/mysql}"
RETENTION_DAYS="${RETENTION_DAYS:-7}"
MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
MYSQL_PORT="${MYSQL_PORT:-3306}"
MYSQL_USER="${MYSQL_USER:-nebula}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-nebula}"
MYSQL_DATABASE="${MYSQL_DATABASE:-nebula_im}"

mkdir -p "${BACKUP_DIR}"
chmod 700 "${BACKUP_DIR}"

timestamp="$(date +%Y%m%d_%H%M%S)"
output="${BACKUP_DIR}/${MYSQL_DATABASE}_${timestamp}.sql.gz"

if command -v mysqldump >/dev/null 2>&1; then
    dump_cmd=(mysqldump -h "${MYSQL_HOST}" -P "${MYSQL_PORT}" -u "${MYSQL_USER}" "-p${MYSQL_PASSWORD}")
elif command -v docker >/dev/null 2>&1 && docker inspect nebula-mysql >/dev/null 2>&1; then
    dump_cmd=(docker exec nebula-mysql mysqldump -h 127.0.0.1 -u "${MYSQL_USER}" "-p${MYSQL_PASSWORD}")
else
    echo "[backup] mysqldump not found and nebula-mysql container is unavailable" >&2
    exit 1
fi

"${dump_cmd[@]}" \
    --single-transaction \
    --no-tablespaces \
    --routines \
    --triggers \
    --events \
    "${MYSQL_DATABASE}" | gzip -c > "${output}"

chmod 600 "${output}"

find "${BACKUP_DIR}" -type f -name "${MYSQL_DATABASE}_*.sql.gz" -mtime "+${RETENTION_DAYS}" -delete

echo "[backup] wrote ${output}"

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MIGRATION_DIR="${ROOT_DIR}/deploy/mysql/migration"

MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
MYSQL_PORT="${MYSQL_PORT:-3306}"
MYSQL_USER="${MYSQL_USER:-nebula}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-nebula}"
MYSQL_DATABASE="${MYSQL_DATABASE:-nebula_im}"

if command -v mysql >/dev/null 2>&1; then
    mysql_cmd=(mysql -h "${MYSQL_HOST}" -P "${MYSQL_PORT}" -u "${MYSQL_USER}" "-p${MYSQL_PASSWORD}" "${MYSQL_DATABASE}")
elif command -v docker >/dev/null 2>&1 && docker inspect nebula-mysql >/dev/null 2>&1; then
    mysql_cmd=(docker exec -i nebula-mysql mysql -h 127.0.0.1 -u "${MYSQL_USER}" "-p${MYSQL_PASSWORD}" "${MYSQL_DATABASE}")
else
    echo "[migrate] mysql client not found and nebula-mysql container is unavailable" >&2
    exit 1
fi

echo "[migrate] ensuring schema_migrations exists"
"${mysql_cmd[@]}" <<'SQL'
CREATE TABLE IF NOT EXISTS schema_migrations (
    version VARCHAR(64) PRIMARY KEY,
    applied_at BIGINT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
SQL

for file in "${MIGRATION_DIR}"/V*.sql; do
    version="$(basename "${file}" .sql)"
    applied="$("${mysql_cmd[@]}" --batch --skip-column-names -e "SELECT COUNT(*) FROM schema_migrations WHERE version='${version}'")"
    if [[ "${applied}" == "1" ]]; then
        echo "[migrate] skip ${version}"
        continue
    fi
    echo "[migrate] apply ${version}"
    "${mysql_cmd[@]}" < "${file}"
    now_ms="$(date +%s%3N)"
    "${mysql_cmd[@]}" -e "INSERT INTO schema_migrations(version, applied_at) VALUES('${version}', ${now_ms})"
done

echo "[migrate] done"

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MIGRATION_DIR="${ROOT_DIR}/deploy/mysql/migration"
CONFIG="${1:-${NEBULA_CONFIG:-}}"

get_cfg() {
    local key="$1"
    local default="${2:-}"
    if [[ -n "${CONFIG}" && -f "${CONFIG}" ]]; then
        local value
        value="$(awk -F= -v key="${key}" '$0 !~ /^[[:space:]]*#/ && $1 == key {print substr($0, index($0, "=") + 1)}' "${CONFIG}" | tail -n 1)"
        if [[ -n "${value}" ]]; then
            echo "${value}"
            return
        fi
    fi
    echo "${default}"
}

MYSQL_HOST="${MYSQL_HOST:-$(get_cfg mysql.host 127.0.0.1)}"
MYSQL_PORT="${MYSQL_PORT:-$(get_cfg mysql.port 3306)}"
MYSQL_USER="${MYSQL_USER:-$(get_cfg mysql.user nebula)}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-$(get_cfg mysql.password nebula)}"
MYSQL_DATABASE="${MYSQL_DATABASE:-$(get_cfg mysql.database nebula_im)}"
MIGRATION_LOCK_NAME="${MIGRATION_LOCK_NAME:-nebulaim_schema_migrations}"
MIGRATION_LOCK_TIMEOUT="${MIGRATION_LOCK_TIMEOUT:-30}"
NEBULA_MIGRATE_BACKUP="${NEBULA_MIGRATE_BACKUP:-false}"

if command -v mysql >/dev/null 2>&1; then
    mysql_cmd=(mysql -h "${MYSQL_HOST}" -P "${MYSQL_PORT}" -u "${MYSQL_USER}" "-p${MYSQL_PASSWORD}" "${MYSQL_DATABASE}")
elif command -v docker >/dev/null 2>&1 && docker inspect nebula-mysql >/dev/null 2>&1; then
    mysql_cmd=(docker exec -i nebula-mysql mysql -h 127.0.0.1 -u "${MYSQL_USER}" "-p${MYSQL_PASSWORD}" "${MYSQL_DATABASE}")
else
    echo "[migrate] mysql client not found and nebula-mysql container is unavailable" >&2
    exit 1
fi

release_lock() {
    "${mysql_cmd[@]}" --batch --skip-column-names -e "SELECT RELEASE_LOCK('${MIGRATION_LOCK_NAME}')" >/dev/null 2>&1 || true
}

if [[ "${NEBULA_MIGRATE_BACKUP}" == "true" || "${NEBULA_ENV:-}" == "production" ]]; then
    echo "[migrate] creating pre-migration backup"
    MYSQL_HOST="${MYSQL_HOST}" MYSQL_PORT="${MYSQL_PORT}" MYSQL_USER="${MYSQL_USER}" MYSQL_PASSWORD="${MYSQL_PASSWORD}" MYSQL_DATABASE="${MYSQL_DATABASE}" \
        "${ROOT_DIR}/scripts/backup_mysql.sh"
fi

echo "[migrate] acquiring lock ${MIGRATION_LOCK_NAME}"
lock_acquired="$("${mysql_cmd[@]}" --batch --skip-column-names -e "SELECT GET_LOCK('${MIGRATION_LOCK_NAME}', ${MIGRATION_LOCK_TIMEOUT})")"
if [[ "${lock_acquired}" != "1" ]]; then
    echo "[migrate] failed to acquire migration lock ${MIGRATION_LOCK_NAME}" >&2
    exit 1
fi
trap release_lock EXIT

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
	    if ! "${mysql_cmd[@]}" < "${file}"; then
	        echo "[migrate] failed at ${version}; restore from the pre-migration backup with scripts/restore_mysql.sh if rollback is required" >&2
	        exit 1
	    fi
	    now_ms="$(date +%s%3N)"
	    "${mysql_cmd[@]}" -e "INSERT INTO schema_migrations(version, applied_at) VALUES('${version}', ${now_ms})"
done

echo "[migrate] done"

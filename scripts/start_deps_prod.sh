#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

: "${NEBULA_MYSQL_ROOT_PASSWORD:?set NEBULA_MYSQL_ROOT_PASSWORD}"
: "${NEBULA_MYSQL_PASSWORD:?set NEBULA_MYSQL_PASSWORD}"
: "${NEBULA_REDIS_PASSWORD:?set NEBULA_REDIS_PASSWORD}"
: "${NEBULA_GRAFANA_PASSWORD:?set NEBULA_GRAFANA_PASSWORD}"

cd "${ROOT}/deploy"
docker compose -f docker-compose.yml -f docker-compose.prod.yml up -d mysql redis kafka prometheus grafana

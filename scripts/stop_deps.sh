#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../deploy"
docker compose stop mysql redis kafka prometheus grafana

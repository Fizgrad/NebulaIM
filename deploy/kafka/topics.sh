#!/usr/bin/env bash
set -euo pipefail

BROKER="${KAFKA_BROKER:-localhost:9092}"
TOPICS=(
  nebula.message.single
  nebula.message.group
  nebula.message.offline
  nebula.message.retry
  nebula.message.dlq
)

for topic in "${TOPICS[@]}"; do
  kafka-topics.sh --bootstrap-server "${BROKER}" --create --if-not-exists --topic "${topic}" --partitions 3 --replication-factor 1
done

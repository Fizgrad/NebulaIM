#!/usr/bin/env bash
set -euo pipefail

BROKER="${KAFKA_BROKER:-127.0.0.1:9092}"
TOPICS=(
  nebula.message.single
  nebula.message.group
  nebula.message.offline
  nebula.message.retry
  nebula.message.dlq
)

if command -v kafka-topics.sh >/dev/null 2>&1; then
  kafka_topics_cmd=(kafka-topics.sh)
elif command -v docker >/dev/null 2>&1 && docker inspect nebula-kafka >/dev/null 2>&1; then
  kafka_topics_cmd=(docker exec nebula-kafka /opt/kafka/bin/kafka-topics.sh)
else
  echo "kafka-topics.sh not found and nebula-kafka container is unavailable" >&2
  exit 1
fi

for topic in "${TOPICS[@]}"; do
  "${kafka_topics_cmd[@]}" --bootstrap-server "${BROKER}" --create --if-not-exists --topic "${topic}" --partitions 3 --replication-factor 1
done

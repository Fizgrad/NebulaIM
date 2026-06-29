#!/usr/bin/env bash
set -euo pipefail
MODE="${1:---unit-only}"
if [[ "$MODE" == "--unit-only" ]]; then
  ctest --test-dir build --output-on-failure
else
  ctest --test-dir build --output-on-failure
  for t in test_mysql_pool test_user_dao test_message_dao test_redis_client test_kafka_producer test_kafka_consumer; do ./build/tests/$t; done
fi

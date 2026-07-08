# NebulaIM Validation Guide

Pick the smallest validation set that proves the change.

## Always Useful

```bash
./scripts/build.sh
```

Use this after C++ source, proto, CMake or generated-code inputs change.

## Unit Tests

```bash
./scripts/run_tests.sh --unit-only
ctest --test-dir build -L unit --output-on-failure
```

Use for `common/`, protocol, helpers and isolated service logic.

## Targeted Builds And Tests

```bash
cmake --build build --target test_message_service_impl -j
./build/tests/test_message_service_impl
```

Use target names matching the touched module. If a test requires Docker dependencies or a non-default config, run it with the same config as the active environment, for example:

```bash
NEBULA_CONFIG=build/nebula.prod.test.conf ./build/tests/test_message_service_impl
```

## Integration Tests

```bash
./scripts/start_deps.sh
./scripts/migrate_db.sh
./scripts/init_topics.sh
ctest --test-dir build -L integration --output-on-failure
```

Use when MySQL, Redis, Kafka or gRPC boundaries are part of the behavior.

## Full Backend E2E

```bash
NEBULA_RUN_BACKEND_E2E=1 ./build/tests/test_backend_final_e2e
```

Use when Gateway, services, persistence and push behavior must be validated together.

## Deployment And Ops

```bash
./scripts/validate_prod_config.sh /etc/nebulaim/nebula.conf
./scripts/health_check.sh /etc/nebulaim/nebula.conf
```

Use after changing config keys, systemd units, deploy scripts, nginx examples or operational docs.

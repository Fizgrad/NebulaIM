# AGENTS.md

## Purpose

These are repository rules for AI agents working on NebulaIM. Keep changes aligned with the current C++17 distributed IM backend; do not revive old skeleton-era assumptions.

## Current System

- NebulaIM is a C++17/Linux backend with a custom epoll/Reactor Gateway, TCP/WebSocket PacketCodec, Protobuf/gRPC services, MySQL, Redis, Kafka, Prometheus/Grafana, tracing and systemd deployment assets.
- Main services: `gateway`, `user_service`, `relation_service`, `message_service`, `conversation_service`, `push_service`, `admin_service`.
- Shared code lives under `common/`; service definitions live under `proto/`; tests live under `tests/`; operational assets live under `deploy/`; documentation lives under `docs/`.
- Browser clients use the separate Web Bridge project. This backend still owns the binary Packet + Protobuf protocol and gRPC services.

## Change Rules

- Read the relevant service code, proto files and docs before editing. Prefer existing patterns over new abstractions.
- Keep generated output, logs and runtime state out of source: `build/`, `logs/`, `run/`, CMake artifacts and local secrets must not be committed.
- Do not commit raw tokens, passwords, private keys or production host-specific secrets. Examples must use placeholders such as `<secret>`.
- Keep docs current with behavior changes. Update README and the service-specific doc when a user-visible API, protocol field, deployment variable or operational flow changes.
- Preserve backward-compatible protobuf fields. Adding fields is safer than reusing or renumbering existing fields.
- Prefer focused tests that cover the changed service or shared component.

## C++ Rules

- Use C++17, RAII, smart pointers, move semantics and explicit ownership.
- Class names use PascalCase; private members use trailing underscores.
- Keep business logic out of service `main.cpp` files.
- Avoid magic numbers in business logic; use config, protocol constants or named limits.
- Keep comments sparse and useful. Explain non-obvious concurrency, protocol, storage or failure-handling behavior.

## Storage And Middleware Rules

- MySQL schema changes belong under `deploy/mysql/migration/` and must preserve existing query paths.
- Redis keys must have explicit TTL and failure behavior where applicable.
- Kafka publishing should keep per-conversation ordering where possible and remain idempotent for redelivery.
- Outbox behavior is part of message durability. Do not bypass it for MessageService sends or recalls unless the documented mode explicitly disables it.

## Validation

- Build: `./scripts/build.sh`
- Unit tests: `./scripts/run_tests.sh --unit-only`
- Targeted CMake test: `cmake --build build --target <target> -j`
- Integration tests require MySQL, Redis and Kafka from Docker: `ctest --test-dir build -L integration --output-on-failure`
- Backend E2E is opt-in: `NEBULA_RUN_BACKEND_E2E=1 ./build/tests/test_backend_final_e2e`
- Do not claim validation passed unless the command was run in this workspace.

## Learning Artifacts

- `docs/agent_rules_skills_mcp.md` explains rules, skills and MCP with this repo as the example.
- `skills/nebulaim-backend/` is a repository-local example skill for NebulaIM backend work.
- `tools/mcp/nebulaim-mcp-server.mjs` is a minimal stdio MCP server for learning JSON-RPC tool exposure.

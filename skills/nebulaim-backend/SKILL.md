---
name: nebulaim-backend
description: Workflows for developing, debugging, documenting and validating the NebulaIM C++ backend. Use when working on Gateway Packet/WebSocket protocol code, gRPC services, MessageService/RelationService/UserService/ConversationService/PushService/AdminService behavior, MySQL/Redis/Kafka integration, production config, tests or backend docs in this repository.
---

# NebulaIM Backend

## Quick Start

1. Read `AGENTS.md` first for repository rules.
2. Read the relevant doc before editing code:
   - Architecture or cross-service flow: `docs/architecture.md`
   - Packet/WebSocket protocol: `docs/protocol.md`, `docs/websocket_gateway.md`
   - Message behavior: `docs/message_service.md`, `docs/outbox_pattern.md`
   - Storage: `docs/storage.md`
   - Deployment/security: `docs/deployment.md`, `docs/security_design.md`, `docs/single_node_production.md`
3. Inspect the owning service and shared `common/` code before changing behavior.
4. Update docs when behavior, config, schema, protocol or deployment steps change.
5. Run the narrowest meaningful validation command, then broaden when shared code or cross-service contracts are touched.

## Change Workflow

- Classify the change as protocol, service logic, storage, middleware, deployment, docs or tests.
- Check proto compatibility before editing `proto/*.proto`; never reuse field numbers.
- Keep service entrypoints thin. Put reusable behavior in service implementation or `common/`.
- For MessageService sends and recalls, preserve outbox consistency unless explicitly working on the documented non-outbox mode.
- For Gateway changes, verify native TCP and WebSocket paths still use the same Packet body semantics.
- For user-visible or operational changes, update README plus the matching `docs/*.md` file.

## Validation Workflow

Use `references/validation.md` for command selection.

Default sequence:

```bash
./scripts/build.sh
./scripts/run_tests.sh --unit-only
```

For integration paths, start Docker dependencies and run the targeted test or CTest label. Do not report tests as passing without running them in this workspace.

## Reference Files

- Read `references/service-map.md` when locating owners for a change.
- Read `references/validation.md` when choosing build/test commands.

# Tracing

NebulaIM has lightweight OpenTelemetry-compatible tracing without adding the full OpenTelemetry C++ SDK dependency.

## Components

- `TraceId`: generates request trace IDs and span IDs.
- `TraceContext`: stores thread-local trace ID/span ID and supports scoped propagation.
- `TraceSpan`: RAII span wrapper for Gateway, MessageService, and PushService hot paths.
- `TraceManager`: bounded async span queue and background exporter.
- `OtlpTraceExporter`: sends OTLP/HTTP JSON batches to a collector such as Jaeger all-in-one.

Trace IDs are propagated through gRPC metadata key `x-nebula-trace-id` and Kafka payload fields. They are not used as Prometheus labels.

## Config

```text
trace.enabled=true
trace.service_name=nebulaim
trace.otlp_endpoint=http://127.0.0.1:4318/v1/traces
trace.export_timeout_ms=2000
trace.batch_size=64
trace.flush_interval_ms=1000
trace.max_queue_size=4096
```

Docker Compose starts Jaeger with OTLP enabled:

```bash
./scripts/start_deps.sh
```

Jaeger UI:

```text
http://127.0.0.1:16686
```

## Current Scope

Implemented spans cover Gateway request entry, MessageService send paths, and PushService Kafka consumption. This is enough to connect client request IDs, message persistence, outbox/Kafka delivery, and push decisions during single-node operations.

The current exporter is intentionally small: HTTP only, no TLS exporter transport, no baggage, no sampling policy beyond enable/disable, and no automatic instrumentation.

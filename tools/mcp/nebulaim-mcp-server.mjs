#!/usr/bin/env node
import fs from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { pathToFileURL } from "node:url";

const rootDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");

const docs = {
  architecture: "docs/architecture.md",
  protocol: "docs/protocol.md",
  websocket: "docs/websocket_gateway.md",
  message_service: "docs/message_service.md",
  relation_service: "docs/relation_service.md",
  user_service: "docs/user_service.md",
  push_service: "docs/push_service.md",
  storage: "docs/storage.md",
  deployment: "docs/deployment.md",
  security: "docs/security_design.md"
};

const tools = [
  {
    name: "nebulaim_repo_overview",
    description: "Return a concise overview of the NebulaIM backend repository.",
    inputSchema: {
      type: "object",
      properties: {},
      additionalProperties: false
    }
  },
  {
    name: "nebulaim_doc_read",
    description: "Read a whitelisted NebulaIM documentation topic from docs/.",
    inputSchema: {
      type: "object",
      properties: {
        topic: {
          type: "string",
          enum: Object.keys(docs)
        },
        maxBytes: {
          type: "integer",
          minimum: 1000,
          maximum: 20000,
          default: 6000
        }
      },
      required: ["topic"],
      additionalProperties: false
    }
  },
  {
    name: "nebulaim_validation_plan",
    description: "Suggest build and test commands for a NebulaIM backend change type.",
    inputSchema: {
      type: "object",
      properties: {
        changeType: {
          type: "string",
          enum: ["protocol", "service", "storage", "deployment", "docs", "full"]
        }
      },
      required: ["changeType"],
      additionalProperties: false
    }
  }
];

let inputBuffer = Buffer.alloc(0);
let keepAlive;
let stdinEnded = false;
let pumping = false;

export function startStdioServer() {
  keepAlive = setInterval(() => {}, 1 << 30);
  process.stdin.on("data", (chunk) => {
    inputBuffer = Buffer.concat([inputBuffer, chunk]);
    void pumpMessages();
  });

  process.stdin.on("end", () => {
    stdinEnded = true;
    finishIfIdle();
  });
  process.stdin.resume();
}

async function pumpMessages() {
  if (pumping) return;
  pumping = true;
  try {
  while (true) {
    const headerEnd = inputBuffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) return;

    const header = inputBuffer.subarray(0, headerEnd).toString("utf8");
    const match = /content-length:\s*(\d+)/i.exec(header);
    if (!match) {
      inputBuffer = Buffer.alloc(0);
      return;
    }

    const length = Number(match[1]);
    const bodyStart = headerEnd + 4;
    const bodyEnd = bodyStart + length;
    if (inputBuffer.length < bodyEnd) return;

    const rawBody = inputBuffer.subarray(bodyStart, bodyEnd).toString("utf8");
    inputBuffer = inputBuffer.subarray(bodyEnd);

    let message;
    try {
      message = JSON.parse(rawBody);
      const response = await handleJsonRpcMessage(message);
      if (response) writeMessage(response);
    } catch (error) {
      writeMessage({
        jsonrpc: "2.0",
        id: message?.id ?? null,
        error: {
          code: -32700,
          message: error instanceof Error ? error.message : "Invalid JSON-RPC message"
        }
      });
    }
  }
  } finally {
    pumping = false;
    finishIfIdle();
  }
}

function finishIfIdle() {
  if (keepAlive && stdinEnded && !pumping && inputBuffer.length === 0) {
    clearInterval(keepAlive);
    keepAlive = undefined;
  }
}

export async function handleJsonRpcMessage(message) {
  if (!("id" in message)) return null;

  try {
    if (message.method === "initialize") {
      return resultMessage(message.id, {
        protocolVersion: "2024-11-05",
        capabilities: {
          tools: {}
        },
        serverInfo: {
          name: "nebulaim-mcp",
          version: "0.1.0"
        }
      });
    }

    if (message.method === "ping") {
      return resultMessage(message.id, {});
    }

    if (message.method === "tools/list") {
      return resultMessage(message.id, { tools });
    }

    if (message.method === "tools/call") {
      const result = await callTool(message.params?.name, message.params?.arguments ?? {});
      return resultMessage(message.id, result);
    }

    return errorMessage(message.id, -32601, `Unknown method: ${message.method}`);
  } catch (error) {
    return errorMessage(message.id, -32000, error instanceof Error ? error.message : "Tool failed");
  }
}

async function callTool(name, args) {
  if (name === "nebulaim_repo_overview") {
    return textResult([
      "NebulaIM is a C++17 distributed IM backend.",
      "Core paths: gateway/, user_service/, relation_service/, message_service/, conversation_service/, push_service/, admin_service/, common/, proto/, tests/, deploy/, docs/.",
      "Runtime dependencies: MySQL, Redis, Kafka, Prometheus/Grafana and optional Jaeger tracing.",
      "Primary validation: ./scripts/build.sh and targeted tests under build/tests/."
    ].join("\n"));
  }

  if (name === "nebulaim_doc_read") {
    const topic = String(args.topic ?? "");
    const relativePath = docs[topic];
    if (!relativePath) throw new Error(`Unknown doc topic: ${topic}`);

    const maxBytes = clampInteger(args.maxBytes, 6000, 1000, 20000);
    const absolutePath = path.join(rootDir, relativePath);
    const content = await fs.readFile(absolutePath, "utf8");
    const truncated = content.length > maxBytes;
    return textResult(`${relativePath}\n\n${content.slice(0, maxBytes)}${truncated ? "\n\n[truncated]" : ""}`);
  }

  if (name === "nebulaim_validation_plan") {
    return textResult(validationPlan(String(args.changeType ?? "")));
  }

  throw new Error(`Unknown tool: ${name}`);
}

function validationPlan(changeType) {
  const plans = {
    protocol: [
      "./scripts/build.sh",
      "ctest --test-dir build -R 'packet|protobuf|websocket|gateway' --output-on-failure"
    ],
    service: [
      "./scripts/build.sh",
      "cmake --build build --target test_message_service_impl -j",
      "Run the service-specific test target for the touched service."
    ],
    storage: [
      "./scripts/build.sh",
      "./scripts/start_deps.sh",
      "./scripts/migrate_db.sh",
      "ctest --test-dir build -L integration --output-on-failure"
    ],
    deployment: [
      "./scripts/validate_prod_config.sh /etc/nebulaim/nebula.conf",
      "./scripts/health_check.sh /etc/nebulaim/nebula.conf"
    ],
    docs: [
      "Check README.md and the matching docs/*.md file for current behavior.",
      "No build is required for docs-only changes unless examples changed."
    ],
    full: [
      "./scripts/build.sh",
      "./scripts/run_tests.sh --unit-only",
      "ctest --test-dir build -L integration --output-on-failure",
      "NEBULA_RUN_BACKEND_E2E=1 ./build/tests/test_backend_final_e2e"
    ]
  };
  if (!plans[changeType]) throw new Error(`Unknown changeType: ${changeType}`);
  return plans[changeType].join("\n");
}

function clampInteger(value, fallback, min, max) {
  const parsed = Number(value);
  if (!Number.isInteger(parsed)) return fallback;
  return Math.min(max, Math.max(min, parsed));
}

function textResult(text) {
  return {
    content: [
      {
        type: "text",
        text
      }
    ]
  };
}

function resultMessage(id, result) {
  return {
    jsonrpc: "2.0",
    id,
    result
  };
}

function errorMessage(id, code, message) {
  return {
    jsonrpc: "2.0",
    id,
    error: {
      code,
      message
    }
  };
}

function writeMessage(message) {
  const body = Buffer.from(JSON.stringify(message), "utf8");
  process.stdout.write(`Content-Length: ${body.length}\r\n\r\n`);
  process.stdout.write(body);
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  startStdioServer();
}

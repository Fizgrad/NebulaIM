#!/usr/bin/env node
import { handleJsonRpcMessage } from "./nebulaim-mcp-server.mjs";

const requests = [
  {
    jsonrpc: "2.0",
    id: 1,
    method: "initialize",
    params: {
      protocolVersion: "2024-11-05",
      capabilities: {},
      clientInfo: {
        name: "nebulaim-mcp-smoke-test",
        version: "0.1.0"
      }
    }
  },
  {
    jsonrpc: "2.0",
    id: 2,
    method: "tools/list",
    params: {}
  },
  {
    jsonrpc: "2.0",
    id: 3,
    method: "tools/call",
    params: {
      name: "nebulaim_validation_plan",
      arguments: {
        changeType: "service"
      }
    }
  }
];

const responses = [];
for (const request of requests) {
  const response = await handleJsonRpcMessage(request);
  if (response) responses.push(response);
}

const framed = Buffer.concat(responses.map(frame));
const parsed = parseFrames(framed);
const initialize = parsed.find((message) => message.id === 1);
const list = parsed.find((message) => message.id === 2);
const call = parsed.find((message) => message.id === 3);

if (initialize?.result?.serverInfo?.name !== "nebulaim-mcp") {
  throw new Error(`initialize response is missing serverInfo: ${JSON.stringify(parsed)}`);
}
if (!Array.isArray(list?.result?.tools) || list.result.tools.length !== 3) {
  throw new Error("Expected three tools from tools/list.");
}
if (!call?.result?.content?.[0]?.text?.includes("./scripts/build.sh")) {
  throw new Error("Expected validation plan text.");
}

console.log("MCP smoke test passed.");

function frame(message) {
  const body = Buffer.from(JSON.stringify(message), "utf8");
  return Buffer.concat([
    Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, "utf8"),
    body
  ]);
}

function parseFrames(buffer) {
  const messages = [];
  let remaining = buffer;
  while (remaining.length > 0) {
    const headerEnd = remaining.indexOf("\r\n\r\n");
    if (headerEnd < 0) break;

    const header = remaining.subarray(0, headerEnd).toString("utf8");
    const match = /content-length:\s*(\d+)/i.exec(header);
    if (!match) throw new Error("Missing Content-Length header.");

    const length = Number(match[1]);
    const bodyStart = headerEnd + 4;
    const bodyEnd = bodyStart + length;
    if (remaining.length < bodyEnd) break;

    messages.push(JSON.parse(remaining.subarray(bodyStart, bodyEnd).toString("utf8")));
    remaining = remaining.subarray(bodyEnd);
  }
  return messages;
}

# Agent Rules, Skills And MCP

This document explains the learning artifacts in this repository:

- `AGENTS.md`: project rules for AI agents.
- `skills/nebulaim-backend/`: a repository-local example skill.
- `tools/mcp/nebulaim-mcp-server.mjs`: a minimal stdio MCP server.

## 1. Rules

Rules are persistent instructions for agents working in a repository. They are not executable code. They tell the agent how to behave when reading, editing, testing and documenting the project.

In this repository, `AGENTS.md` defines:

- Current NebulaIM architecture and top-level ownership.
- C++ style rules.
- Storage, Redis, Kafka and outbox constraints.
- Validation commands.
- Safety rules for secrets, build output and runtime files.

Rules are best for stable project constraints. They should not describe obsolete plans or old compatibility modes.

## 2. Skills

A skill is a folder with a required `SKILL.md` file. The frontmatter tells an agent when the skill applies. The body tells the agent what workflow to follow after the skill is loaded.

This repository includes:

```text
skills/nebulaim-backend/
  SKILL.md
  agents/openai.yaml
  references/service-map.md
  references/validation.md
```

The skill is intentionally repository-local for learning. To make it auto-discoverable by Codex, copy it into your Codex skills directory:

```bash
mkdir -p ~/.codex/skills
cp -a skills/nebulaim-backend ~/.codex/skills/
```

Then ask for it explicitly:

```text
Use $nebulaim-backend to plan a MessageService change.
```

### Skill Loading Model

Skills use progressive disclosure:

1. Skill metadata is visible first: `name` and `description`.
2. `SKILL.md` is loaded only when the skill triggers.
3. `references/` files are loaded only when the task needs them.

This keeps the agent context small while still giving it project-specific procedures.

## 3. MCP

MCP means Model Context Protocol. It lets an agent host connect to external tools through a standard JSON-RPC protocol. The agent does not directly import your code. Instead:

```text
Agent Host
  starts MCP server
  sends initialize
  asks tools/list
  calls tools/call
MCP Server
  returns structured tool results
```

The example server in this repo uses stdio transport. Messages are JSON-RPC 2.0 payloads framed with `Content-Length` headers:

```text
Content-Length: 123

{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}
```

The blank line between headers and JSON is required. The body length is counted in bytes.

## 4. Example MCP Server

Run the smoke test:

```bash
node tools/mcp/smoke-test.mjs
```

The server exposes three safe tools:

- `nebulaim_repo_overview`: returns a short repo overview.
- `nebulaim_doc_read`: reads a whitelisted doc topic.
- `nebulaim_validation_plan`: returns validation commands for a change type.

It intentionally does not execute arbitrary shell commands. That keeps the example safe and makes the protocol easier to understand.

## 5. Example MCP Client Config

Copy `tools/mcp/nebulaim-mcp-config.example.json` into the MCP config format used by your agent host and replace `/path/to/NebulaIM` with the absolute path to this repository.

The important fields are:

- `command`: the executable that starts the MCP server.
- `args`: arguments passed to the executable.
- The server communicates with the host over stdin/stdout.

## 6. How The Three Pieces Fit Together

- Rules answer: "How should an agent behave in this repo?"
- Skills answer: "What workflow should an agent follow for a recurring task?"
- MCP answers: "What tools can the agent call through a stable protocol?"

For NebulaIM:

- `AGENTS.md` keeps edits safe and consistent.
- `$nebulaim-backend` gives an agent a backend workflow.
- `nebulaim-mcp-server.mjs` teaches how an agent can discover and call repo-aware tools.

## 中文说明

`AGENTS.md` 是规则，适合写稳定约束，例如代码风格、测试命令、不能提交密钥。

`skills/nebulaim-backend/` 是 skill 示例。它把 NebulaIM 后端开发流程封装成可复用的工作流。真正使用时可以复制到 `~/.codex/skills/`，然后用 `$nebulaim-backend` 显式触发。

`tools/mcp/nebulaim-mcp-server.mjs` 是一个最小 MCP server。它通过 stdio 收发带 `Content-Length` 的 JSON-RPC 消息，暴露 `tools/list` 和 `tools/call`。这就是 MCP 的核心：agent host 启动 server，发现工具，再调用工具获取结构化结果。

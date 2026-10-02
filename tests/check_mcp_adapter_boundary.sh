#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
adapter="$root/tools/mcp"

[[ -f "$adapter/Cargo.toml" && -f "$adapter/src/main.rs" ]] || {
  echo "ERROR: the standalone MCP adapter is incomplete." >&2
  exit 1
}

if rg -n '(^|[/"])(core|gui|app|models|mvr|viewer2d|viewer3d|local_ipc)([/"]|::)' "$adapter/src"; then
  echo "ERROR: MCP adapter source must not depend directly on Perastage implementation modules." >&2
  exit 1
fi

if rg -n 'streamable|http|axum|reqwest|std::net|TcpStream|UdpSocket|resources|prompts' "$adapter/src" "$adapter/Cargo.toml"; then
  echo "ERROR: the initial MCP adapter must remain tools-only and stdio-only." >&2
  exit 1
fi

if ! rg -q 'Command::new\(&self\.executable\)\.args\(arguments\)' "$adapter/src/backend.rs" ||
   rg -n '(sh -c|cmd /[cC]|powershell|Command::new\("(sh|bash|cmd|powershell))' "$adapter/src"; then
  echo "ERROR: MCP requests must execute perastage-cli directly without a shell." >&2
  exit 1
fi

echo "OK: MCP remains a standalone stdio adapter over typed perastage-cli argv mappings."

#!/usr/bin/env bash
set -euo pipefail

if rg "ConfigManager::Get\(" gui -n; then
  echo "Found forbidden direct ConfigManager::Get() usages in gui/." >&2
  exit 1
fi

echo "OK: no direct ConfigManager::Get() usages in gui/."

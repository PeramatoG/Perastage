#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
config_manager="$repo_root/core/configmanager.cpp"

if ! rg -n "sourceKind = MvrImportSourceKind::ProjectRestore" "$config_manager" >/dev/null; then
  echo "ConfigManager::LoadProject must use the ProjectRestore MVR import source kind." >&2
  exit 1
fi

if ! rg -n "options\.applyDictionary = false" "$config_manager" >/dev/null; then
  echo "ConfigManager::LoadProject must disable dictionary remapping for embedded scene.mvr restores." >&2
  exit 1
fi

if rg -n "ImportAndRegister\([^\n]*scenePath, false, true" "$config_manager" >/dev/null; then
  echo "ConfigManager::LoadProject must not restore scene.mvr with applyDictionary=true." >&2
  exit 1
fi

if ! rg -n "originalMvrGdtfSpec" "$repo_root/models/fixture.h" "$repo_root/mvr/mvrimporter.cpp" "$repo_root/mvr/mvrexporter.cpp" >/dev/null; then
  echo "Fixture original MVR GDTF preservation metadata is missing." >&2
  exit 1
fi

echo "OK: project restore MVR import preserves embedded GDTF references without dictionary remapping."

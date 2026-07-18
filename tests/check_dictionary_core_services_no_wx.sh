#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

files=(
  core/truss_asset_ingestion.h
  core/truss_asset_ingestion.cpp
  core/dictionary_reset_service.h
  core/dictionary_reset_service.cpp
)

if rg -n "wx[A-Za-z_]*|<wx/" "${files[@]}"; then
  echo "Dictionary core services must not depend on wxWidgets UI APIs." >&2
  exit 1
fi

echo "OK: dictionary core transaction services remain GUI-independent."

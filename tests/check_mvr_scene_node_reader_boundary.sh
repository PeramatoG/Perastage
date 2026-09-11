#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

if rg -n '#include .*gui/|#include "(consolepanel|logindialog|credentialstore|gdtfnet)' \
    mvr/mvr_scene_node_reader.h; then
  echo "The MVR scene-node reader must not depend on GUI or download code." >&2
  exit 1
fi

if rg -n 'nodeName == "(Fixture|Truss|Support|SceneObject|GroupObject)"' \
    mvr/mvrimporter.cpp; then
  echo "MvrImporter must delegate logical scene-node dispatch to the reader." >&2
  exit 1
fi

echo "MVR scene-node reader boundary check passed."

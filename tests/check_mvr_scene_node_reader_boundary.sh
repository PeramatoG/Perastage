#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

if rg -n '#include .*gui/|#include "(consolepanel|logindialog|credentialstore|gdtfnet)' \
    mvr/mvr_scene_node_reader.h mvr/mvr_scene_node_reader.cpp \
    mvr/mvr_scene_node_reader_detail.h mvr/mvr_scene_node_reader_support.cpp; then
  echo "The MVR scene-node reader must not depend on GUI or download code." >&2
  exit 1
fi

if rg -n 'inline void ReadMvrSceneNodes|auto &[A-Za-z].*auto &' \
    mvr/mvr_scene_node_reader.h; then
  echo "The scene-node reader interface must remain a compiled, concrete API." >&2
  exit 1
fi

if awk 'NR > 100 && /#include "mvr_scene_node_reader.h"/ { found = 1 } END { exit found ? 0 : 1 }' \
    mvr/mvrimporter.cpp; then
  echo "The scene-node reader header must be included from the normal include block." >&2
  exit 1
fi

if rg -n 'nodeName == "(Fixture|Truss|Support|SceneObject|GroupObject)"' \
    mvr/mvrimporter.cpp; then
  echo "MvrImporter must delegate logical scene-node dispatch to the reader." >&2
  exit 1
fi

echo "MVR scene-node reader boundary check passed."

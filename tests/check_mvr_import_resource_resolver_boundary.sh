#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

resolver_files=(mvr/mvr_import_resource_resolver.h mvr/mvr_import_resource_resolver.cpp)
if rg -n '#include .*gui/|#include "(credentialstore|gdtfnet|logindialog|consolepanel)|wx(Dialog|Window|MessageBox)' "${resolver_files[@]}"; then
  echo "The MVR import resource resolver must remain GUI and download independent." >&2
  exit 1
fi

if rg -n 'resolvedGdtfPathCache|gdtfModesCache|gdtfModeChannelCountCache|gdtfFixtureMetadataCache|trussDefinitionCache|dictionaryEntryByTypeCache' mvr/mvrimporter.cpp; then
  echo "MvrImporter must not own resource-resolution caches." >&2
  exit 1
fi

if ! rg -q 'MvrImportResourceResolver &resources' mvr/mvr_scene_node_reader.h; then
  echo "The scene reader must consume the cohesive resource resolver boundary." >&2
  exit 1
fi

echo "MVR import resource resolver boundary check passed."

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

for callback in remapArchivePath normalizeGdtfSpec resolveGdtfPath \
  fixtureMetadata resolveGdtfMode dictionaryEntry resolveScenePath; do
  if ! rg -q "$callback" mvr/mvr_scene_node_reader.h; then
    echo "The scene reader must retain the $callback read-service callback." >&2
    exit 1
  fi
done
if ! rg -q 'resources\.ResolveGdtfPath' mvr/mvrimporter.cpp; then
  echo "The application importer must adapt the cohesive resource resolver into the shared reader." >&2
  exit 1
fi

echo "MVR import resource resolver boundary check passed."

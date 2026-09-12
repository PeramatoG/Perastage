#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

resolver_files=(mvr/mvr_import_reference_resolver.h mvr/mvr_import_reference_resolver.cpp)
if rg -n '#include .*gui/|#include "(configmanager|consolepanel|logindialog)|wx(Dialog|Window|MessageBox)' "${resolver_files[@]}"; then
  echo "The MVR import reference resolver must remain GUI and application-state independent." >&2
  exit 1
fi

if rg -n 'unknown_motor_fixture_uuid|unknown_(hoist|truss)_info_uuid|unknown_project_fixture_metadata_uuid' mvr/mvrimporter.cpp; then
  echo "MvrImporter must delegate post-parse reference diagnostics to the resolver." >&2
  exit 1
fi

if rg -n 'uuidRemap|fixtureUuidRemap' mvr/mvr_scene_node_reader.h mvr/mvr_scene_node_reader.cpp; then
  echo "The MVR scene-node reader must not own or mutate fixture UUID remap storage." >&2
  exit 1
fi

if ! rg -q 'services\.recordFixtureUuid\(rawFixtureUuid, fixture\.uuid\)' mvr/mvr_scene_node_reader.cpp; then
  echo "The MVR scene-node reader must record fixture aliases through its service boundary." >&2
  exit 1
fi

echo "MVR import reference resolver boundary check passed."

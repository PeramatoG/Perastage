#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if rg -n 'ConfigManager|wx/|wx[A-Z]|gui/|mvrimporter|mvrexporter|filesystem' \
    core/fixture_patch_service.h core/fixture_network_service.h models/fixture_dmx_addresses.h models/fixture_network_assignment.h; then
  echo "Fixture DMX patch policy must remain GUI-independent and free of I/O." >&2
  exit 1
fi
rg -q 'SetFixturePrimaryPatch' core/autopatcher.cpp
rg -q 'SetFixturePrimaryPatch' gui/fixturetable/fixture_table_edit_service.cpp
rg -q 'ResolveFixturePatch' mvr/mvr_export_preparation.cpp
rg -q 'ReadFixtureAddresses' mvr/mvr_scene_node_reader.cpp
rg -q 'CheckFixtureAddressProjection' mvr/mvrexporter.cpp
rg -q 'ScanFixtureAddresses' mvr/mvr_standard_preservation.cpp
if rg -n 'ConfigManager|wx/|wx[A-Z]|gui/' core/fixture_network_reference.{h,cpp}; then
  echo "Fixture Network reference evidence must remain GUI-independent." >&2
  exit 1
fi
rg -q 'CheckReferences' mvr/mvr_standard_preservation.cpp
rg -q 'inspectNetworks' core/gdtf/editor/project_fixture_gdtf_apply_adapter.cpp
echo "Fixture patch preservation boundary check passed."

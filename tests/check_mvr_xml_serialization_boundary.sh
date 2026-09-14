#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
serializer_files=(
  "$repo_root/mvr/mvr_xml_document_writer.h"
  "$repo_root/mvr/mvr_xml_document_writer.cpp"
  "$repo_root/mvr/mvr_xml_extension_writer.h"
  "$repo_root/mvr/mvr_xml_extension_writer.cpp"
  "$repo_root/mvr/mvr_xml_scene_object_writer.h"
  "$repo_root/mvr/mvr_xml_scene_object_writer.cpp"
)
exporter="$repo_root/mvr/mvrexporter.cpp"

for forbidden in 'ConfigManager::Get' 'wxZip' 'wxFileOutputStream' 'PutNextEntry' \
  'runtime_storage' 'CreateExportWorkspace' 'GdtfDictionary' 'GdtfLoader' \
  'GdtfCanonicalizer' 'GdtfMutation' 'filesystem' 'copy_file' '../gui/' 'gui/'; do
  if rg -n "$forbidden" "${serializer_files[@]}" >/dev/null; then
    echo "MVR XML serializers must not own application, resource, or package behavior: $forbidden" >&2
    exit 1
  fi
done

rg -q 'mvr_xml_serialization::CreateDocument' "$exporter"
rg -q 'mvr_xml_serialization::AppendPreparedPositions' "$exporter"
rg -q 'mvr_xml_serialization::AppendFixture' "$exporter"
rg -q 'mvr_xml_serialization::AppendSymdef' "$exporter"
rg -q 'mvr_xml_extension::AppendFixtureTypes' "$exporter"
rg -q 'mvr_xml_extension::AppendProjectFixtures' "$exporter"
rg -q 'mvr_xml_extension::AppendPrimitiveGeometryMap' "$exporter"

for node in GeneralSceneDescription AUXData Position Symdef Layers Layer Fixture Truss Support SceneObject GroupObject ChildList FixtureTypeInfoMap ProjectFixtureMetadataMap PrimitiveGeometryMap TrussInfo HoistInfo; do
  if rg -n "NewElement\(\"${node}\"" "$exporter" >/dev/null; then
    echo "MvrExporter must delegate $node XML construction to a focused writer." >&2
    exit 1
  fi
done
rg -q 'class MvrExporter' "$repo_root/mvr/mvrexporter.h"

echo "MVR XML serialization boundary check passed."

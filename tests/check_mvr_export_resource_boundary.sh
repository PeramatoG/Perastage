#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
resource_files=("$root/mvr/mvr_export_resource_collection.h" "$root/mvr/mvr_export_resource_collection.cpp" "$root/mvr/mvr_export_gdtf_resources.cpp")
for forbidden in 'ConfigManager::Get' 'wxFileOutputStream' 'wxZipOutputStream' 'wxZipEntry' 'PutNextEntry' 'mvr_xml_' 'gui/' 'MainWindow' 'Dialog' 'ExportToBuffer'; do
  if rg -n --fixed-strings "$forbidden" "${resource_files[@]}"; then
    echo "MVR resource boundary violation: $forbidden" >&2
    exit 1
  fi
done
xml_files=("$root/mvr/mvr_xml_document_writer.cpp" "$root/mvr/mvr_xml_scene_object_writer.cpp" "$root/mvr/mvr_xml_extension_writer.cpp")
for forbidden in 'GdtfDictionary' 'GdtfCanonicalizer' 'WritePrimitiveModelForToken' 'TemporaryWorkspace' 'directory_iterator'; do
  if rg -n --fixed-strings "$forbidden" "${xml_files[@]}"; then
    echo "MVR XML resource-discovery boundary violation: $forbidden" >&2
    exit 1
  fi
done
rg -q --fixed-strings '#include "mvr_export_resource_collection.h"' "$root/mvr/mvrexporter.cpp"
rg -q --fixed-strings 'resourceCollection.Finalize' "$root/mvr/mvrexporter.cpp"
rg -q --fixed-strings 'resourceCollection.PrepareGdtfResources' "$root/mvr/mvrexporter.cpp"
for forbidden in 'GdtfCanonicalizer::CanonicalizeArchive' 'CreatePatchedGdtf' 'Collect3dsTextureReferences' 'CollectGltfExternalReferences' 'WritePrimitiveModelForToken' 'mvr-export-canonical' 'canonicalWorkspace'; do
  if rg -n --fixed-strings "$forbidden" "$root/mvr/mvrexporter.cpp"; then
    echo "MvrExporter still owns resource preparation: $forbidden" >&2
    exit 1
  fi
done
echo "MVR export resource boundary check passed."

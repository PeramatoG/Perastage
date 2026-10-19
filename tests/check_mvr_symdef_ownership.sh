#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
if rg -n 'symdefFiles|symdefMatrices|symdefTypes|symdefGeometries' . --glob '*.{cpp,h}'; then
  echo "Canonical Symdefs must have one definition table, not parallel partial registries." >&2
  exit 1
fi
files=(mvr/mvr_matrix_precision.cpp mvr/mvr_symdef_reader.cpp mvr/mvr_symdef_preservation.cpp mvr/mvr_model_dependencies.cpp)
if rg -n 'ConfigManager|wx[A-Z]|gui/|symbols/|symbol_svg|ProjectFixtureSymbol|copy_file|ofstream' "${files[@]}"; then
  echo "MVR definition preservation must not own GUI, GDTF SVG, PSTG symbols, or resource mutation." >&2
  exit 1
fi
if rg -n '\.Parse\(|LoadFile\(' mvr/mvr_symdef_reader.cpp; then
  echo "Symdef import must use the existing MVR document rather than parse another scene." >&2
  exit 1
fi
rg -q 'unordered_map<std::string, SymdefDefinition> symdefs' models/mvrscene.h
rg -q 'CheckSerializedSymdefs' mvr/mvrexporter.cpp
rg -q 'InspectModelDependencies' mvr/mvr_export_resource_collection.cpp
rg -q 'InspectModelDependencies' mvr/mvr_symdef_preservation.cpp
printf '%s\n' 'MVR Symdef ownership check passed.'

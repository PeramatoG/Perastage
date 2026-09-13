#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

parser_components=(
  mvr/mvr_import_package.h
  mvr/mvr_import_package.cpp
  mvr/mvr_scene_node_reader.h
  mvr/mvr_scene_node_reader.cpp
  mvr/mvr_scene_node_reader_support.cpp
  mvr/mvr_import_resource_resolver.h
  mvr/mvr_import_resource_resolver.cpp
  mvr/mvr_import_reference_resolver.h
  mvr/mvr_import_reference_resolver.cpp
)

if rg -n 'mvr_import_project_application|MvrImportProjectApplication|ConfigManager::Get\(\)\.Reset\(\)|RemapFixtureLabelOverrideKeys' "${parser_components[@]}"; then
  echo "Reusable MVR parsing components must not depend on project application." >&2
  exit 1
fi

if rg -n 'ConfigManager::Get\(\)\.Reset\(\)|ConfigManager::Get\(\)\.GetScene\(\)\s*=' mvr/mvrimporter.cpp; then
  echo "MvrImporter parsing and result orchestration must delegate project replacement." >&2
  exit 1
fi

application_files=(
  mvr/mvr_import_project_application.h
  mvr/mvr_import_project_application.cpp
)
if ! rg -q 'config_\.Reset\(\)' "${application_files[@]}" ||
   ! rg -q 'config_\.GetScene\(\)\s*=' "${application_files[@]}"; then
  echo "The project-application boundary must own reset and scene installation." >&2
  exit 1
fi

if rg -n 'RemapFixtureLabelOverrideKeys' mvr/mvrimporter.cpp; then
  echo "Fixture-label migration must not be duplicated in registration wrappers." >&2
  exit 1
fi
if rg -n 'LoadFixtureLabelOverrides|SaveFixtureLabelOverrides' "${application_files[@]}"; then
  echo "Project application must not preserve old fixture-label overrides around reset." >&2
  exit 1
fi
if [[ "$(rg -l 'RemapFixtureLabelOverrideKeys' mvr | wc -l | tr -d ' ')" != "1" ]]; then
  echo "MVR fixture-label migration must have one application-side owner." >&2
  exit 1
fi

echo "MVR import project-application boundary check passed."

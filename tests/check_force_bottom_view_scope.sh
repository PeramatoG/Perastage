#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

mapfile -t files < <(rg -l "view2d_top_fixtures_inverted" viewer3d | sort)

expected=("viewer3d/render/opaque_fixture_pass.cpp")

if [[ "${#files[@]}" -ne "${#expected[@]}" ]]; then
  echo "Expected ${#expected[@]} viewer3d file(s) to use view2d_top_fixtures_inverted, found ${#files[@]}." >&2
  printf 'Found:\n%s\n' "${files[*]:-<none>}" >&2
  exit 1
fi

for i in "${!expected[@]}"; do
  if [[ "${files[$i]}" != "${expected[$i]}" ]]; then
    echo "Unexpected view2d_top_fixtures_inverted usage in viewer3d: ${files[$i]}" >&2
    exit 1
  fi
done

echo "OK: force-bottom configuration is scoped to fixture rendering in viewer3d."

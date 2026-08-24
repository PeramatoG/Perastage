#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
key_header="$root/viewer3d/resources/resource_reference_cache_key.h"
files=(
  "$root/viewer3d/resources/resource_sync_system.cpp"
  "$root/viewer3d/culling/bounds_cache_system.cpp"
  "$root/viewer3d/culling/visibilitysystem.cpp"
  "$root/viewer3d/render/opaque_pass_utils.cpp"
)

rg -q 'BuildResourceReferenceCacheKey' "$key_header"
for file in "${files[@]}"; do
  rg -q 'BuildResourceReferenceCacheKey' "$file"
done
if rg -n 'static std::string ResolveCacheKey' \
  "$root/viewer3d/resources/resource_sync_system.cpp" \
  "$root/viewer3d/culling/bounds_cache_system.cpp" \
  "$root/viewer3d/culling/visibilitysystem.cpp"; then
  echo "Resource reference map owners must use the shared cache-key builder." >&2
  exit 1
fi

echo "Resource reference cache maps share one canonical key contract."

#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if rg -n '#include.*(wx/|gui/|configmanager|gdtfnet)' \
    "$root/core/gdtf_resolution.h" "$root/core/gdtf_resolution.cpp" \
    "$root/core/gdtf_resolution_resources.h" "$root/core/gdtf_resolution_resources.cpp" \
    "$root/mvr/gdtf_candidate_ranking.cpp" "$root/mvr/gdtf_catalog_matcher.h" \
    "$root/mvr/gdtf_catalog_matcher.cpp" "$root/mvr/gdtf_patch_evidence.h" \
    "$root/mvr/gdtf_patch_evidence.cpp"; then
  echo "GDTF ranking, decisions and resource validation must remain GUI-independent." >&2
  exit 1
fi
if rg -n 'DownloadRevision|GdtfSearchDialog|GdtfLoginDialog|GDTF download queue' \
    "$root/mvr/mvrimporter.cpp"; then
  echo "The MVR importer must delegate interactive GDTF resolution." >&2
  exit 1
fi
if ! rg -q 'RunMvrGdtfResolution' "$root/mvr/mvrimporter.cpp"; then
  echo "Missing import resolution integration." >&2
  exit 1
fi
echo "GDTF resolution boundary check passed."

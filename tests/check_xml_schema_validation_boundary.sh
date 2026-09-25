#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cmake="$root/core/CMakeLists.txt"
header="$root/core/inspection/xml_schema_validation.h"
source="$root/core/inspection/xml_schema_validation.cpp"
fail() { echo "XML schema validation boundary check failed: $1" >&2; exit 1; }
grep -Fq 'add_library(perastage_inspection_validation STATIC' "$cmake" || fail "dedicated validation target is missing"
links="$(sed -n '/target_link_libraries(perastage_inspection_validation/,/^)/p' "$cmake")"
printf '%s\n' "$links" | grep -Fq 'LibXml2::LibXml2' || fail "validation target must own libxml2"
if rg -n 'LibXml2::LibXml2|<libxml/' "$root"/{app,gui,models,mvr,viewer2d,viewer3d,viewer_common} "$root/core" --glob '!xml_schema_validation.cpp' --glob '!CMakeLists.txt'; then
  fail "libxml2 escaped its dedicated implementation"
fi
if rg -n 'xml(Doc|Schema|Parser|Node|Error)|libxml' "$header"; then fail "public validation API exposes libxml2"; fi
if rg -n 'wx[A-Z]|wxWidgets' "$header" "$source"; then fail "validation depends on GUI types"; fi
grep -Fq 'XML_PARSE_NONET' "$source" || fail "XML parsing must prohibit network access"
if rg -n 'XML_PARSE_NOENT' "$source"; then fail "entity expansion must remain disabled"; fi
echo "XML schema validation boundary check passed"

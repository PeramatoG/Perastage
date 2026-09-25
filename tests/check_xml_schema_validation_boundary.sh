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
if rg -n 'filesystem|SchemaDescriptor.*path' "$header"; then fail "validation descriptors must use embedded schema bytes"; fi
if rg -n 'wx[A-Z]|wxWidgets' "$header" "$source"; then fail "validation depends on GUI types"; fi
grep -Fq 'XML_PARSE_NONET' "$source" || fail "XML parsing must prohibit network access"
grep -Fq 'XML_PARSE_NO_XXE' "$source" || fail "XML parsing must disable external entities"
grep -Fq 'XML_PARSE_NO_SYS_CATALOG' "$source" || fail "XML parsing must disable system catalogs"
grep -Fq 'xmlCtxtSetErrorHandler' "$source" || fail "XML parsing must use a context-local error handler"
grep -Fq 'xmlCtxtSetResourceLoader' "$source" || fail "XML parsing must install a resource-denial loader"
grep -Fq 'xmlSchemaSetResourceLoader' "$source" || fail "schema parsing must install a resource-denial loader"
grep -Fq '#if LIBXML_VERSION >= 21200' "$source" || fail "const structured-error callbacks must use the libxml2 2.12 transition"
grep -Fq '#if LIBXML_VERSION >= 21500' "$source" || fail "2.15-only resource APIs must retain an independent guard"
grep -Fq '#if LIBXML_VERSION < 21500' "$source" || fail "older libxml2 must retain a secure schema fallback"
grep -Fq 'HasExternalSchemaDependency' "$source" || fail "older libxml2 must reject external schema dependencies structurally"
if rg -n '(reinterpret_cast|static_cast|const_cast|\([^)]*xmlStructuredErrorFunc[^)]*\)).*CollectStructuredError' "$source"; then fail "structured-error callbacks must not use compatibility casts"; fi
if rg -n 'find\("<xs:(import|include)' "$source"; then fail "schema security must not rely on substring matching"; fi
if rg -n 'XML_PARSE_NOENT' "$source"; then fail "entity expansion must remain disabled"; fi
if rg -n 'PERASTAGE_STANDARD_SCHEMA_DIR|ReadSchema' "$source" "$cmake"; then fail "validation must not require source-tree schemas at runtime"; fi
echo "XML schema validation boundary check passed"

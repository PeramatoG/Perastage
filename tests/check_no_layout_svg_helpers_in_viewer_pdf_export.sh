#!/usr/bin/env bash
set -euo pipefail

file="viewer2d/pdf/layout_pdf_exporter.cpp"

if [[ ! -f "$file" ]]; then
  echo "ERROR: $file not found" >&2
  exit 1
fi

viewer_scope=$(awk '
  /Viewer2DExportResult ExportViewer2DToPdf\(/ {in_fn=1}
  /Viewer2DExportResult ExportLayoutToPdf\(/ {in_fn=0}
  in_fn {print}
' "$file")

if [[ -z "$viewer_scope" ]]; then
  echo "ERROR: could not locate ExportViewer2DToPdf scope" >&2
  exit 1
fi

forbidden='findLegendSvg\(|appendPerastageSvgSymbolObject\(|\<symbolScale\>'
if printf '%s\n' "$viewer_scope" | rg -n "$forbidden" >/dev/null; then
  echo "ERROR: ExportViewer2DToPdf must not depend on layout-only SVG helper names." >&2
  printf '%s\n' "$viewer_scope" | rg -n "$forbidden" >&2 || true
  exit 1
fi

echo "OK: ExportViewer2DToPdf does not use layout-only SVG helper names."

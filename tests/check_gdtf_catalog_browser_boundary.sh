#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if rg -n '#include.*(wx/|gui/|gdtfnet|mainwindow)' \
    "$root/core/gdtf_catalog_browser_model.h" "$root/core/gdtf_catalog_browser_model.cpp" \
    "$root/mvr/gdtf_catalog_parser.h" "$root/mvr/gdtf_catalog_parser.cpp"; then
  echo "Catalog parsing and browser models must remain GUI/network independent." >&2
  exit 1
fi
if rg -n 'ParseCatalog|SearchIndex|CatalogEntriesEquivalent|GdtfCatalogDetailsPanel' \
    "$root/gui/mainwindow.cpp" "$root/gui/mainwindow_menu.cpp"; then
  echo "MainWindow must delegate catalog parsing, search, and presentation." >&2
  exit 1
fi
if rg -n 'ParseCatalog|g_cachedCatalog|NormalizeForGdtfMatch|FilterCatalogEntries' \
    "$root/gui/gdtf_share_download_workflow.cpp"; then
  echo "The catalog workflow must reuse service snapshots rather than parse/search." >&2
  exit 1
fi
if rg -n 'ParseCatalog|GetCatalog|DownloadRevision|gdtf_archive_reader' \
    "$root/gui/gdtf_catalog_details_panel.cpp"; then
  echo "Catalog details must consume metadata without downloads or archive reads." >&2
  exit 1
fi

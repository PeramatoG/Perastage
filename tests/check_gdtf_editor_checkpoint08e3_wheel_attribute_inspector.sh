#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
require_file() { test -f "$root/$1" || { echo "missing $1" >&2; exit 1; }; }
require_rg() { rg -q "$1" "$root/$2" || { echo "missing pattern $1 in $2" >&2; exit 1; }; }
require_file core/gdtf/gdtf_wheel_catalog.h
require_file core/gdtf/gdtf_wheel_catalog.cpp
require_file core/gdtf/gdtf_color_cie.h
require_file core/gdtf/gdtf_dmx_inspector.h
require_file core/gdtf/gdtf_dmx_inspector.cpp
require_file gui/gdtf/gdtf_resource_bitmap_cache.h
require_file gui/gdtf/gdtf_resource_bitmap_cache.cpp
require_rg "SetWheelInspectionCallback" gui/fixtureeditdialog.cpp
require_rg "GDTF wheels" gui/fixtureeditdialog.cpp
require_rg "InspectGdtfDmxValue" gui/gdtf/gdtf_modes_panel.cpp
require_rg "SetInspectionData" gui/gdtf/gdtf_modes_panel.cpp
require_file gui/gdtf/gdtf_wheel_inspector_panel.cpp
require_file gui/gdtf/gdtf_wheel_inspector_panel.h
require_rg "WheelSlotIndex" core/gdtf/gdtf_dmx_inspector.cpp
require_rg "wxSlider" gui/gdtf/gdtf_modes_panel.cpp
require_rg "FormatInspectionValueSummary" gui/gdtf/gdtf_modes_panel.cpp
require_rg "FormatInspectionRange" gui/gdtf/gdtf_modes_panel.cpp
require_rg "wxTE_WORDWRAP" gui/gdtf/gdtf_modes_panel.cpp
require_rg "UpdateInspectionSliderRange" gui/gdtf/gdtf_modes_panel.cpp
require_rg "SetRange" gui/gdtf/gdtf_modes_panel.cpp
require_rg "GdtfWheelInspectorDetailRow" gui/gdtf/gdtf_wheel_inspector_panel.h
require_rg "DMX inspection details" gui/gdtf/gdtf_wheel_inspector_panel.cpp
require_rg "wxScrolledWindow" gui/gdtf/gdtf_wheel_inspector_panel.h
require_rg "ApplyDetailRows" gui/gdtf/gdtf_wheel_inspector_panel.cpp
require_rg "BuildActiveInspectorRows" gui/gdtf/gdtf_modes_panel.cpp
require_rg "selectedInspectionDetails" gui/gdtf/gdtf_modes_panel.h
! rg -q "detailsCtrl" "$root/gui/gdtf/gdtf_modes_panel.cpp"
require_rg "FindWheel\(activeFunction->wheel\)" core/gdtf/gdtf_dmx_inspector.cpp
require_rg "ReadGdtfArchiveResource" core/gdtf_archive_reader.cpp
require_rg "TryReadExplodedGdtfResource" core/gdtf_archive_reader.cpp
require_rg "GetCachedGdtfExtractionDirectory" viewer3d/gdtfloader.h
require_rg "ResourceNotFound" core/gdtf_archive_reader.h
require_rg "filesystemFallback" core/gdtf_archive_reader.h
require_rg "wxImage" gui/gdtf/gdtf_resource_bitmap_cache.cpp
require_rg "FindHandler" gui/gdtf/gdtf_resource_bitmap_cache.cpp
! rg -q "tinyxml|XMLDocument|GdtfEditSession|Apply|Save|Undo|Art-Net|sACN|XmlWriter|XMLPrinter" "$root/gui/gdtf/gdtf_resource_bitmap_cache.cpp"
require_rg "GdtfWheelAttributeInspector" tests/CMakeLists.txt
require_rg "check_gdtf_editor_checkpoint08e2_mode_browser" tests/CMakeLists.txt
require_rg "check_no_configmanager_get_in_gui" tests/CMakeLists.txt
require_rg "check_perastage_tree_modules" tests/CMakeLists.txt
require_rg "Truss Modes" docs/internal/gdtf_wheel_attribute_inspector.md
require_rg "ReadGdtfArchiveResource" gui/fixtureeditdialog.cpp
require_rg "resourceRoots" gui/fixtureeditdialog.cpp
require_rg "hasThumbnail" gui/gdtf/gdtf_wheel_inspector_panel.h
require_rg "hasPreview" gui/gdtf/gdtf_wheel_inspector_panel.h
require_rg "GdtfBitmapDecodeResult" gui/gdtf/gdtf_resource_bitmap_cache.h
require_rg "decoded" gui/fixtureeditdialog.cpp
require_rg "ComposePreviewBitmap" gui/gdtf/gdtf_resource_bitmap_cache.cpp
require_rg "CreateSwatchBitmap" gui/gdtf/gdtf_wheel_inspector_panel.cpp
require_rg "OnSlotSelected" gui/gdtf/gdtf_wheel_inspector_panel.cpp
require_rg "GetOrCreate" gui/fixtureeditdialog.cpp

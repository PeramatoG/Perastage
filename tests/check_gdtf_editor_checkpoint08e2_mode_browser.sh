#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep
python3 - <<'PY'
from pathlib import Path

def require(condition, message):
    if not condition:
        raise SystemExit(message)

modes_h = Path('gui/gdtf/gdtf_modes_panel.h').read_text()
modes = Path('gui/gdtf/gdtf_modes_panel.cpp').read_text()
model_h = Path('gui/gdtf/gdtf_mode_data_view_model.h').read_text()
model = Path('gui/gdtf/gdtf_mode_data_view_model.cpp').read_text()
presenter = Path('gui/gdtf/gdtf_mode_browser_presenter.cpp').read_text()
core_h = Path('core/gdtf/gdtf_mode_channel_browser.h').read_text()
core = Path('core/gdtf/gdtf_mode_channel_browser.cpp').read_text()
fixture_h = Path('gui/fixtureeditdialog.h').read_text()
fixture = Path('gui/fixtureeditdialog.cpp').read_text()
prefs = Path('gui/gdtf/gdtf_editor_layout_preferences.cpp').read_text()
truss = Path('gui/trusseditdialog.cpp').read_text()
cmake_gui = Path('gui/CMakeLists.txt').read_text()
cmake_core = Path('core/CMakeLists.txt').read_text()

require('wxDataViewCtrl' in modes and 'AppendTextColumn("Item"' in modes, 'Mode browser must use wxDataViewCtrl with columns.')
require('channelListCtrl' not in modes_h + modes, 'Old multiline channel control must be removed.')
require('class GdtfModeDataViewModel : public wxDataViewModel' in model_h, 'Hierarchical wxDataViewModel must exist.')
require('SetValue' in model and 'return false' in model, 'GUI model must be read-only.')
require('tinyxml2' not in model_h + model and 'ReadGdtfArchive' not in model_h + model, 'GUI model must not parse XML/archive data.')
require('#include <wx/' not in core_h + core and 'wx' not in core_h, 'Core parser must have no wxWidgets dependency.')
for token in ['DMXMode', 'DMXChannel', 'LogicalChannel', 'ChannelFunction', 'ChannelSet', 'SubChannelSet', 'ParseGdtfDmxValue']:
    require(token in core_h + core, f'Core hierarchy/parser missing {token}.')
require('GeometryReference' in core and 'DMXOffset' in core and 'geometryReferenceIndex' in core_h + core, 'Core reader must expand GeometryReference DMX offsets for matrix fixtures.')
for col in ['Item', 'DMX range', 'Physical range', 'Unit']:
    require(col in modes, f'Missing browser column {col}.')
require('Channel function' not in modes, 'Mode browser must not expose the removed Channel function column.')
require('detailsCtrl' in modes_h + modes and 'UpdateDetails' in modes, 'Details inspector must exist.')
summary = Path('gui/gdtf/gdtf_channel_summary_panel.cpp').read_text()
editor = Path('gui/gdtf/gdtf_editor_panel.cpp').read_text()
require('wxTE_MULTILINE | wxTE_READONLY' in summary and 'channelSummaryPanel' in editor, 'Legacy quick channel summary panel must be restored below physical properties.')
require('BuildPerByteChannelFunctionNames' in presenter and 'FormatGroupedChannelFunctions' in presenter, 'Summary must show per-byte functions without adding a browser Channel function column.')
require('address' not in Path('gui/gdtf/gdtf_mode_browser_presenter.h').read_text(), 'Browser presentation must not keep removed Channel function column data.')
require('HasContainerColumns' not in model_h + model, 'Browser model must keep default container columns to avoid slow nested redraws.')
require('SetBrowserSplitterRatio' in modes_h + modes and 'GetBrowserSplitterRatio' in modes_h + modes, 'Browser ratio API must exist.')
require('gdtf_editor/fixture/mode_browser_ratio' in prefs, 'Browser ratio persistence key must exist.')
require('gdtfConfiguration.modes.visible = false' in truss, 'Truss must keep Modes hidden.')
require('cachedModeChannelDocument' in fixture_h + fixture and 'ReloadModeChannelDocument' in fixture, 'Fixture Edit must cache parsed hierarchical document.')
require('GdtfModeDataViewModel::Item' in modes and 'wxDATAVIEW_CELL_INERT' in modes, 'Columns must be inert and read-only.')
for forbidden in ['ImageList', 'ColourPicker', 'DMX simulation', 'SetDropTarget']:
    require(forbidden not in modes_h + modes + model_h + model, f'08E2 must not introduce {forbidden}.')
require('wxSlider' in modes_h + modes and Path('tests/check_gdtf_editor_checkpoint08e3_wheel_attribute_inspector.sh').exists(), '08E3 owns the read-only DMX inspection slider.')
require('gdtf_mode_channel_browser.cpp' in cmake_core, 'Core browser reader must be registered.')
require('gdtf_mode_data_view_model.cpp' in cmake_gui and 'gdtf_mode_browser_presenter.cpp' in cmake_gui, 'GUI browser files must be registered.')
for guard in ['check_gdtf_editor_checkpoint08e1_layout.sh', 'check_gdtf_editor_panel_boundary.sh', 'check_gdtf_reusable_panels_boundary.sh']:
    require(Path('tests', guard).exists(), f'Required earlier guard is missing: {guard}.')
print('OK: GDTF editor Checkpoint 08E2 mode browser guard passed.')
PY

#!/usr/bin/env bash
set -euo pipefail
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/test_tool_requirements.sh"
require_ripgrep

python3 - <<'PY'
from pathlib import Path

fixture_h = Path('gui/fixtureeditdialog.h').read_text()
fixture = Path('gui/fixtureeditdialog.cpp').read_text()
truss_h = Path('gui/trusseditdialog.h').read_text()
truss = Path('gui/trusseditdialog.cpp').read_text()
panel_h = Path('gui/gdtf/gdtf_editor_panel.h').read_text()
panel = Path('gui/gdtf/gdtf_editor_panel.cpp').read_text()
modes = Path('gui/gdtf/gdtf_modes_panel.cpp').read_text()
metrics = Path('gui/gdtf/gdtf_editor_visual_metrics.h').read_text()
prefs_h = Path('gui/gdtf/gdtf_editor_layout_preferences.h').read_text()
prefs = Path('gui/gdtf/gdtf_editor_layout_preferences.cpp').read_text()
cmake = Path('gui/CMakeLists.txt').read_text()


def require(condition, message):
    if not condition:
        raise SystemExit(message)

require('wxSplitterWindow* contextSplitter' in fixture_h and 'wxSplitterWindow* visualSplitter' in fixture_h,
        'Fixture Edit must own splitter members for context/main/visual layout.')
require('new wxSplitterWindow(contentPanel' in fixture and 'new wxSplitterWindow(workspacePanel' in fixture,
        'Fixture Edit must build splitter-based compact context/main/visual panes.')
require('wxScrolledWindow(contextSplitter' in fixture and 'Fixture instance' in fixture,
        'Fixture Edit must use a compact scrollable Fixture instance pane.')
require('wxNotebook* visualNotebook' in fixture_h and 'new wxNotebook' in fixture,
        'Fixture Edit must use a native visual-resource notebook.')
for tab in ['Preview', 'Symbols']:
    require(f'AddPage' in fixture and tab in fixture, f'Missing Fixture visual-resource tab: {tab}.')
require(fixture.count('visualNotebook->AddPage') == 2,
        'Fixture visual-resource notebook must have exactly Preview and Symbols tabs.')
require('fixtureImagePreview = new wxStaticBitmap(previewPage' in fixture,
        'Fixture image must be placed below the 3D preview on the Preview tab.')
require('officialSymbolPreview' in fixture_h and 'LoadGdtfOfficialSvgSymbol' in fixture,
        'Symbols tab must include an official GDTF SVG thumbnail preview above Perastage symbols.')
require('symbolRootSizer->Add(officialSymbolPreview, 1' in fixture and 'symbolRootSizer->Add(symbolSizer, 1' in fixture,
        'Official SVG thumbnail area must receive the same vertical proportion as the Top/Front/Side symbol area.')
require(fixture.count('new GdtfEditorPanel(') == 1,
        'Fixture Edit must instantiate exactly one GdtfEditorPanel.')
for child in ['GdtfMetadataPanel', 'GdtfTypeIdentityPanel', 'GdtfPhysicalPropertiesPanel', 'GdtfModesPanel']:
    require(child not in fixture_h and f'new {child}' not in fixture,
            f'Fixture host must not directly own or construct {child}.')

require('wxSplitterWindow *contextSplitter' in truss_h,
        'Truss Edit must own the compact context splitter.')
require('new wxSplitterWindow(contentPanel' in truss and 'GetWorkspaceHeaderHost' in truss,
        'Truss Edit must build compact context plus GDTF workspace-header preview layout.')
require('wxScrolledWindow(contextSplitter' in truss and 'MVR instance' in truss,
        'Truss Edit must use a compact scrollable MVR instance pane.')
require('preview = new FixturePreviewPanel(previewHost)' in truss,
        'Truss Edit must place preview in the third GDTF column above physical properties.')
require(truss.count('new GdtfEditorPanel(') == 1,
        'Truss Edit must instantiate exactly one GdtfEditorPanel.')
for child in ['GdtfMetadataPanel', 'GdtfTypeIdentityPanel', 'GdtfPhysicalPropertiesPanel', 'GdtfModesPanel']:
    require(child not in truss_h and f'new {child}' not in truss,
            f'Truss host must not directly own or construct {child}.')

for token in ['enum class GdtfEditorSection', 'enum class GdtfEditorPane',
              'struct GdtfEditorSectionPlacement', 'twoPaneOrder',
              'SetTwoPaneSplitterRatio', 'GetTwoPaneSplitterRatio']:
    require(token in panel_h, f'GdtfEditorPanel missing typed placement API token: {token}.')
require('wxSplitterWindow *twoPaneSplitter' in panel_h and 'SplitVertically(overviewPane, workspacePane' in panel,
        'GdtfEditorPanel two-pane layout must use a native splitter.')
require('class GdtfEditorFlatSection' in panel and 'wxStaticLine' in panel,
        'GdtfEditorPanel must use flatter title/line sections instead of nested static boxes.')
require('wxStaticBoxSizer' not in panel_h + panel,
        'GdtfEditorPanel must not reintroduce heavy static-box section borders.')
require(panel.count('new GdtfMetadataPanel(') == 1 and panel.count('new GdtfTypeIdentityPanel(') == 1 and
        panel.count('new GdtfPhysicalPropertiesPanel(') == 1 and panel.count('new GdtfModesPanel(') == 1,
        'GdtfEditorPanel must construct each reusable child panel exactly once.')
require('ConfigManager' not in panel_h + panel,
        'Reusable GdtfEditorPanel must remain presentation-only and ConfigManager-free.')

fixture_order = fixture.find('gdtfConfiguration.twoPaneOrder')
require(fixture_order >= 0, 'Fixture must configure typed two-pane section order.')
fixture_block = fixture[fixture_order:fixture.find('};', fixture_order)+2]
expected_fixture = ['GdtfEditorSection::TypeIdentity', 'GdtfEditorSection::Metadata',
                    'GdtfEditorSection::PhysicalProperties', 'GdtfEditorSection::ChannelSummary',
                    'GdtfEditorSection::Modes']
positions = [fixture_block.find(token) for token in expected_fixture]
require(all(pos >= 0 for pos in positions) and positions == sorted(positions),
        'Fixture section order must be Type, Metadata, Physical in overview and Modes in workspace.')
require(fixture_block.count('GdtfEditorPane::Workspace') == 1 and 'GdtfEditorSection::Modes' in fixture_block,
        'Fixture browser must be isolated in the workspace pane while the quick summary stays in overview.')

truss_order = truss.find('gdtfConfiguration.twoPaneOrder')
require(truss_order >= 0, 'Truss must configure typed two-pane section order.')
truss_block = truss[truss_order:truss.find('};', truss_order)+2]
expected_truss = ['GdtfEditorSection::TypeIdentity', 'GdtfEditorSection::Metadata',
                  'GdtfEditorSection::PhysicalProperties']
positions = [truss_block.find(token) for token in expected_truss]
require(all(pos >= 0 for pos in positions) and positions == sorted(positions),
        'Truss section order must be Type, Metadata in overview and Physical in workspace.')
require('GdtfEditorPane::Workspace, GdtfEditorSection::PhysicalProperties' in truss_block,
        'Truss physical properties must occupy the third GDTF column under the preview.')
require('gdtfConfiguration.channelSummary.visible = false' in truss and 'gdtfConfiguration.modes.visible = false' in truss,
        'Truss Edit must keep Modes and channel summary hidden.')

require('wxDataViewCtrl' in modes and 'Mode and channel browser' in modes,
        'GdtfModesPanel must use the 08E2 read-only mode/channel browser after 08E1.')
for forbidden in ['wxTreeCtrl', 'wxSlider', 'GdtfWheel']:
    require(forbidden not in fixture + truss + panel + modes,
            f'08E1 must not introduce future browser/resource controls: {forbidden}.')
for forbidden in ['GdtfApplyRequest', 'ProjectFixtureGdtfApplyAdapter', 'ProjectTrussGdtfApplyAdapter', 'GdtfEditSession']:
    require(forbidden not in panel_h + panel, f'Reusable panels must not depend on apply/session type {forbidden}.')
require('ApplyChanges' in fixture and 'ApplyChanges' in truss,
        'Existing host Apply methods must remain host-owned.')

for token in ['OuterMargin', 'PaneGap', 'MinimumContextPaneWidth', 'MinimumPreviewHeight', 'FromDIP']:
    require(token in metrics, f'Shared visual metrics missing token: {token}.')
require('gdtf_editor_layout_preferences.cpp' in cmake,
        'Layout preference helper must be registered in gui/CMakeLists.txt.')
for token in ['LoadFixtureLayoutPreferences', 'SaveFixtureLayoutPreferences',
              'LoadTrussLayoutPreferences', 'SaveTrussLayoutPreferences',
              'ClampDialogSize', 'RatioToSash', 'SashToRatio']:
    require(token in prefs_h + prefs, f'Layout preference helper missing token: {token}.')
for key in ['gdtf_editor/fixture/dialog_width', 'gdtf_editor/fixture/context_ratio',
            'gdtf_editor/fixture/visual_ratio', 'gdtf_editor/fixture/gdtf_ratio',
            'gdtf_editor/fixture/visual_tab', 'gdtf_editor/truss/dialog_width',
            'gdtf_editor/truss/context_ratio', 'gdtf_editor/truss/preview_ratio',
            'gdtf_editor/truss/gdtf_ratio']:
    require(key in prefs, f'Missing independent persisted layout key: {key}.')
require('SaveLayoutPreferences();' in fixture and 'SaveLayoutPreferences();' in truss,
        'Both dialogs must save layout preferences on close/OK/Cancel paths.')
require('SetBackgroundColour' not in panel, 'Reusable GDTF panels must not introduce custom backgrounds.')

for guard in ['check_gdtf_editor_panel_boundary.sh', 'check_gdtf_editor_checkpoint08a_binding.sh',
              'check_gdtf_editor_checkpoint08b_fixture_apply_adapter.sh', 'check_gdtf_editor_checkpoint08c_truss_apply_adapter.sh',
              'check_gdtf_editor_checkpoint08d_stabilization.sh']:
    require(Path('tests', guard).exists(), f'Checkpoint 07-08D guard is missing: {guard}.')

print('OK: GDTF editor Checkpoint 08E1 layout guard passed.')
PY

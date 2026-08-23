#include "layoutviewpresets.h"
#include "main_toolbar_pane_policy.h"

#include <cassert>
#include <string>
#include <unordered_map>

namespace {

using PaneState = std::unordered_map<std::string, bool>;

// Applies the same visibility contract used by a canonical manual view action.
void ApplyCanonicalPreset(PaneState &state, const std::string &mode) {
  const LayoutViewPreset *preset = LayoutViewPresetRegistry::GetPreset(mode);
  assert(preset);
  for (const std::string &pane : preset->showPanes)
    state[pane] = true;
  for (const std::string &pane : preset->hidePanes)
    state[pane] = false;
  for (const std::string_view toolbar : gui::kAlwaysVisibleToolbarPanes)
    state[std::string(toolbar)] = true;
}

// Verifies the complete canonical 3D pane visibility contract.
void Assert3D(const PaneState &state) {
  assert(state.at("3DViewport"));
  assert(!state.at("2DViewport"));
  assert(!state.at("2DRenderOptions"));
  assert(!state.at("LayoutPanel"));
  assert(!state.at("LayoutViewer"));
  for (const std::string pane : {"DataNotebook", "Console", "LayerPanel",
                                 "SummaryPanel", "RiggingPanel"})
    assert(state.at(pane));
  for (const std::string_view toolbar : gui::kAlwaysVisibleToolbarPanes)
    assert(state.at(std::string(toolbar)));
}

// Verifies the complete canonical 2D pane visibility contract.
void Assert2D(const PaneState &state) {
  assert(!state.at("3DViewport"));
  assert(state.at("2DViewport"));
  assert(state.at("2DRenderOptions"));
  assert(!state.at("LayoutPanel"));
  assert(!state.at("LayoutViewer"));
  for (const std::string pane : {"DataNotebook", "Console", "LayerPanel",
                                 "SummaryPanel", "RiggingPanel"})
    assert(state.at(pane));
  for (const std::string_view toolbar : gui::kAlwaysVisibleToolbarPanes)
    assert(state.at(std::string(toolbar)));
}

// Verifies the complete canonical Layout Mode pane visibility contract.
void AssertLayout(const PaneState &state) {
  assert(!state.at("3DViewport"));
  assert(!state.at("2DViewport"));
  assert(!state.at("2DRenderOptions"));
  assert(state.at("LayoutPanel"));
  assert(state.at("LayoutViewer"));
  for (const std::string pane : {"DataNotebook", "Console", "LayerPanel",
                                 "SummaryPanel", "RiggingPanel"})
    assert(!state.at(pane));
  for (const std::string_view toolbar : gui::kAlwaysVisibleToolbarPanes)
    assert(state.at(std::string(toolbar)));
}

} // namespace

// Exercises canonical pane transitions from deliberately contaminated states.
int main() {
  PaneState state;
  for (const std::string pane :
       {"3DViewport", "2DViewport", "2DRenderOptions", "LayoutPanel",
        "LayoutViewer", "DataNotebook", "Console", "LayerPanel", "SummaryPanel",
        "RiggingPanel", "FileToolbar", "EditToolbar", "LayoutViewsToolbar",
        "ToolsToolbar", "LayoutToolbar"})
    state[pane] = false;

  ApplyCanonicalPreset(state, "layout_mode_view");
  AssertLayout(state);
  ApplyCanonicalPreset(state, "3d_layout_view");
  Assert3D(state);
  ApplyCanonicalPreset(state, "layout_mode_view");
  AssertLayout(state);
  ApplyCanonicalPreset(state, "3d_layout_view");
  Assert3D(state);

  ApplyCanonicalPreset(state, "layout_mode_view");
  ApplyCanonicalPreset(state, "2d_layout_view");
  Assert2D(state);
  ApplyCanonicalPreset(state, "layout_mode_view");
  AssertLayout(state);
  ApplyCanonicalPreset(state, "2d_layout_view");
  Assert2D(state);

  state["DataNotebook"] = false;
  for (const std::string_view toolbar : gui::kAlwaysVisibleToolbarPanes)
    state[std::string(toolbar)] = false;
  for (const std::string_view toolbar : gui::kAlwaysVisibleToolbarPanes)
    state[std::string(toolbar)] = true;
  assert(!state.at("DataNotebook"));
  for (const std::string_view toolbar : gui::kAlwaysVisibleToolbarPanes)
    assert(state.at(std::string(toolbar)));
  return 0;
}

/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "patch/dmx_patch_panel.h"
#include "patch/dmx_patch_canvas.h"
#include "configmanager.h"
#include "position_selection.h"
#include "selection_movement_settings.h"

void DmxPatchPanel::OnHoverHighlight(wxThreadEvent &event) {
  hoverHighlight_ = event.GetPayload<gui::FixtureHoverHighlight>();
  RefreshHighlights();
  event.Skip();
}
void DmxPatchPanel::RefreshHighlights() {
  std::map<std::string, wxColour> colors;
  const auto &scene = static_cast<const ConfigManager &>(config_).GetScene();
  for (const auto &uuid : scene_grouping::ExpandSelectionForGroupHighlights(
           scene, position_selection::SelectionFrom(config_),
           selection_movement_settings::LoadInteractiveTransformPolicy(config_)))
    colors[uuid] = gui::FixtureSelectionColor();
  // Match table precedence: selection wins over hover and related hover.
  if (!hoverHighlight_.primary.empty())
    colors.try_emplace(hoverHighlight_.primary, gui::FixtureHoverColor());
  for (const auto &uuid : hoverHighlight_.related)
    colors.try_emplace(uuid, gui::FixtureRelatedHoverColor());
  canvas_->SetHighlights(std::move(colors));
}

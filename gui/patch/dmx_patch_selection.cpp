/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "patch/dmx_patch_panel.h"
#include "patch/dmx_patch_canvas.h"
#include "configmanager.h"
#include "position_selection.h"
#include "selection_movement_settings.h"
#include "../viewer3d/interaction/scene_selection_policy.h"

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

void DmxPatchPanel::PickFixtures(const std::vector<std::string> &uuids, bool shift, bool control) {
  if (!publishSelection_) return;
  using namespace viewer3d::interaction;
  const auto &scene = static_cast<const ConfigManager &>(config_).GetScene();
  const auto policy = selection_movement_settings::LoadInteractiveTransformPolicy(config_);
  TypedSelection clicked;
  for (const auto &uuid : uuids) {
    if (!scene.fixtures.contains(uuid)) continue;
    const auto target = BuildClickSelection(scene, uuid, policy);
    clicked.fixtures = AddSelectionUuids(std::move(clicked.fixtures), target.fixtures);
    clicked.trusses = AddSelectionUuids(std::move(clicked.trusses), target.trusses);
    clicked.supports = AddSelectionUuids(std::move(clicked.supports), target.supports);
    clicked.sceneObjects = AddSelectionUuids(std::move(clicked.sceneObjects), target.sceneObjects);
  }
  if (!uuids.empty() && clicked.fixtures.empty()) return;
  auto selection = position_selection::SelectionFrom(config_);
  const bool additive = shift || control;
  selection.fixtures = ResolveClickedSelection(selection.fixtures, clicked.fixtures, additive, control);
  selection.trusses = ResolveClickedSelection(selection.trusses, clicked.trusses, additive, control);
  selection.supports = ResolveClickedSelection(selection.supports, clicked.supports, additive, control);
  selection.sceneObjects = ResolveClickedSelection(selection.sceneObjects, clicked.sceneObjects, additive, control);
  if (!additive) selection.groupObjects.clear();
  publishSelection_(selection);
}

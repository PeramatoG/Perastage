/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#include "context_menu_model.h"

#include <algorithm>
#include <cstdlib>

namespace viewport_context {
Model Build(const Input &input) {
  Model model;
  model.selection = input.selection;
  if (input.interactionActive || input.dragged)
    return model;
  const bool hit = input.hit.kind != Kind::Empty && !input.hit.uuid.empty();
  if (hit && std::find(input.selection.begin(), input.selection.end(),
                       input.hit) == input.selection.end()) {
    model.replaceSelection = true;
    model.selection = input.clickedSelection.empty()
                          ? std::vector<Target>{input.hit}
                          : input.clickedSelection;
  }
  const auto &c = input.capabilities;
  const auto add = [&](bool available, Action action) {
    if (available) model.actions.push_back(action);
  };
  add(hit && input.hit.kind == Kind::Fixture && c.editFixture, Action::EditFixture);
  add(hit && input.hit.kind == Kind::Fixture && c.editPatch, Action::EditPatch);
  add(hit && input.hit.kind == Kind::Truss && c.editTruss, Action::EditTruss);
  add(hit && input.hit.kind == Kind::SceneObject && c.editPrimitive, Action::EditPrimitive);
  add(hit && input.hit.kind == Kind::SceneObject && c.convertToTruss, Action::ConvertToTruss);
  add(hit && (input.hit.kind == Kind::Fixture || input.hit.kind == Kind::Truss), Action::SelectType);
  add(hit && (input.hit.kind == Kind::Fixture || input.hit.kind == Kind::Truss), Action::SelectPosition);
  const bool selected = !model.selection.empty();
  add(selected && c.positionTool, Action::PositionTool);
  add(model.selection.size() >= 2 && c.group, Action::Group);
  add(selected && c.ungroup, Action::Ungroup);
  add(selected && c.duplicate, Action::Duplicate);
  add(selected && c.remove, Action::Delete);
  add(c.renderStyle, Action::RenderStyle);
  add(c.exportImage, Action::ExportImage);
  add(!hit && c.selectionShortcuts, Action::SelectionShortcuts);
  return model;
}

void ClickGesture::Begin(int x, int y, bool blocked) {
  x_ = x; y_ = y; active_ = true; blocked_ = blocked; moved_ = false;
}
void ClickGesture::Move(int x, int y) {
  if (active_ && (std::abs(x - x_) > 3 || std::abs(y - y_) > 3)) moved_ = true;
}
bool ClickGesture::Finish() {
  const bool show = active_ && !blocked_ && !moved_;
  Cancel();
  return show;
}
void ClickGesture::Cancel() { active_ = blocked_ = moved_ = false; }
} // namespace viewport_context

/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#pragma once

#include <string>
#include <vector>

namespace viewport_context {
enum class Kind { Empty, Fixture, Truss, Support, SceneObject, Group };
struct Target {
  Kind kind = Kind::Empty;
  std::string uuid;
  bool operator==(const Target &) const = default;
};
enum class Action {
  EditFixture, EditPatch, EditTruss, EditPrimitive, ConvertToTruss,
  SelectType, SelectPosition, PositionTool, Group, Ungroup, Copy, Delete,
  RenderStyle, ExportImage, SelectionShortcuts
};
struct Capabilities {
  bool editFixture = false, editPatch = false, editTruss = false;
  bool editPrimitive = false, convertToTruss = false, positionTool = false;
  bool group = false, ungroup = false, copy = false, remove = false;
  bool renderStyle = false, exportImage = false;
  bool selectionShortcuts = false;
};
struct Input {
  Target hit;
  // Effective membership includes promoted groups; order is never rewritten.
  std::vector<Target> selection;
  std::vector<Target> clickedSelection;
  bool interactionActive = false;
  bool dragged = false;
  Capabilities capabilities;
};
struct Model {
  bool replaceSelection = false;
  std::vector<Target> selection;
  std::vector<Action> actions;
};
Model Build(const Input &input);

// Tracks only the current right-button gesture, independently of navigation.
class ClickGesture {
public:
  void Begin(int x, int y, bool blocked = false);
  void Move(int x, int y);
  bool Finish();
  void Cancel();
private:
  int x_ = 0, y_ = 0;
  bool active_ = false, blocked_ = false, moved_ = false;
};
} // namespace viewport_context

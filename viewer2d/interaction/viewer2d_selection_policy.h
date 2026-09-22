/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */

#pragma once

#include "viewer2d_interaction_scope_policy.h"
#include "viewer2d_interaction_session.h"

namespace viewer2d::interaction {

struct ClickSelectionInput {
  bool found = false;
  SceneElementKind kind = SceneElementKind::None;
  std::string uuid;
  bool shiftDown = false;
  bool controlDown = false;
  bool crossTable = false;
  SelectionBuckets current;
};

struct ClickSelectionDecision {
  bool clearAll = false;
  bool additive = false;
  SceneElementKind changedKind = SceneElementKind::None;
  SelectionBuckets selection;
  std::vector<std::string> viewerSelection;
};

struct DragSelectionDecision {
  bool valid = false;
  DragTarget target = DragTarget::None;
  bool usesCurrentSelection = false;
  std::vector<std::string> activeUuids;
  SelectionBuckets selection;
};

// Computes click and drag selection decisions without applying UI mutations.
class Viewer2DSelectionPolicy {
public:
  // Computes typed click selection and deterministic merged viewer selection.
  static ClickSelectionDecision DecideClick(const ClickSelectionInput &input);
  // Prepares the typed selection used by a drag gesture.
  static DragSelectionDecision PrepareDrag(DragTarget target,
                                           const std::string &clickedUuid,
                                           const SelectionBuckets &current);
  // Maps a scene-element kind to the existing drag-target representation.
  static DragTarget DragTargetFor(SceneElementKind kind);
};

} // namespace viewer2d::interaction

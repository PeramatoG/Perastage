/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */

#include "viewer2d_selection_policy.h"

#include <algorithm>

namespace viewer2d::interaction {
namespace {

// Returns the mutable typed bucket corresponding to a scene-element kind.
std::vector<std::string> &BucketFor(SelectionBuckets &selection,
                                    SceneElementKind kind) {
  switch (kind) {
  case SceneElementKind::Fixture:
    return selection.fixtures;
  case SceneElementKind::Truss:
    return selection.trusses;
  case SceneElementKind::Support:
    return selection.supports;
  case SceneElementKind::SceneObject:
    return selection.sceneObjects;
  case SceneElementKind::None:
    return selection.fixtures;
  }
  return selection.fixtures;
}

// Returns the typed bucket corresponding to a drag target.
const std::vector<std::string> &BucketFor(const SelectionBuckets &selection,
                                          DragTarget target) {
  switch (target) {
  case DragTarget::Fixtures:
    return selection.fixtures;
  case DragTarget::Trusses:
    return selection.trusses;
  case DragTarget::Supports:
    return selection.supports;
  case DragTarget::SceneObjects:
    return selection.sceneObjects;
  case DragTarget::None:
    return selection.fixtures;
  }
  return selection.fixtures;
}

// Appends typed buckets in the established viewer-selection order.
std::vector<std::string> MergeSelection(const SelectionBuckets &selection) {
  std::vector<std::string> merged;
  for (const auto *bucket : {&selection.fixtures, &selection.trusses,
                             &selection.supports, &selection.sceneObjects})
    merged.insert(merged.end(), bucket->begin(), bucket->end());
  return merged;
}

} // namespace

// Computes typed click selection and deterministic merged viewer selection.
ClickSelectionDecision
Viewer2DSelectionPolicy::DecideClick(const ClickSelectionInput &input) {
  ClickSelectionDecision result;
  result.selection = input.current;
  if (!input.found || input.kind == SceneElementKind::None) {
    result.clearAll = true;
    result.selection = {};
    return result;
  }

  result.additive = input.shiftDown || input.controlDown;
  result.changedKind = input.kind;
  auto &bucket = BucketFor(result.selection, input.kind);
  if (!result.additive)
    bucket = {input.uuid};
  else {
    const auto found = std::find(bucket.begin(), bucket.end(), input.uuid);
    if (found == bucket.end())
      bucket.push_back(input.uuid);
    else if (!input.controlDown)
      bucket.erase(found);
  }

  result.viewerSelection =
      input.crossTable ? MergeSelection(result.selection) : bucket;
  return result;
}

// Prepares the typed selection used by a drag gesture.
DragSelectionDecision
Viewer2DSelectionPolicy::PrepareDrag(DragTarget target,
                                     const std::string &clickedUuid,
                                     const SelectionBuckets &current) {
  DragSelectionDecision result;
  result.target = target;
  if (target == DragTarget::None || clickedUuid.empty())
    return result;

  const auto &activeBucket = BucketFor(current, target);
  result.usesCurrentSelection =
      std::find(activeBucket.begin(), activeBucket.end(), clickedUuid) !=
      activeBucket.end();
  result.activeUuids = result.usesCurrentSelection
                           ? activeBucket
                           : std::vector<std::string>{clickedUuid};
  if (result.usesCurrentSelection) {
    result.selection = current;
  } else {
    switch (target) {
    case DragTarget::Fixtures:
      result.selection.fixtures = {clickedUuid};
      break;
    case DragTarget::Trusses:
      result.selection.trusses = {clickedUuid};
      break;
    case DragTarget::Supports:
      result.selection.supports = {clickedUuid};
      break;
    case DragTarget::SceneObjects:
      result.selection.sceneObjects = {clickedUuid};
      break;
    case DragTarget::None:
      break;
    }
  }
  result.valid = true;
  return result;
}

// Maps a scene-element kind to the existing drag-target representation.
DragTarget Viewer2DSelectionPolicy::DragTargetFor(SceneElementKind kind) {
  switch (kind) {
  case SceneElementKind::Fixture:
    return DragTarget::Fixtures;
  case SceneElementKind::Truss:
    return DragTarget::Trusses;
  case SceneElementKind::Support:
    return DragTarget::Supports;
  case SceneElementKind::SceneObject:
    return DragTarget::SceneObjects;
  case SceneElementKind::None:
    return DragTarget::None;
  }
  return DragTarget::None;
}

} // namespace viewer2d::interaction

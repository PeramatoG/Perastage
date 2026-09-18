/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "viewer2d_interaction_session.h"

#include "../../viewer_common/viewport_mouse_navigation.h"

#include <cmath>
#include <utility>

namespace viewer2d::interaction {
namespace {
constexpr int kSelectionDragStartThresholdPixels = 3;
constexpr long kSelectionDragDelayMilliseconds = 150;
} // namespace

// Starts a primary-button gesture in viewport-navigation mode.
bool Viewer2DInteractionSession::BeginPrimary(PointerPosition position) {
  if (mode != DragMode::None || middleMousePanning)
    return false;
  ResetGesture();
  mode = DragMode::View;
  lastPointer = position;
  return true;
}

// Starts an exclusive middle-button viewport-pan gesture when permitted.
bool Viewer2DInteractionSession::BeginPan(PointerPosition position,
                                          bool continuousPlacementActive) {
  if (middleMousePanning ||
      (mode != DragMode::None &&
       !(continuousPlacementActive && mode == DragMode::Selection)))
    return false;
  if (!viewport_navigation::CanBeginViewer2DPan(mode == DragMode::None,
                                                continuousPlacementActive))
    return false;
  middleMousePanning = true;
  draggedSincePress = false;
  mode = DragMode::View;
  lastPointer = position;
  return true;
}

// Starts temporary left-button viewport navigation during placement.
bool Viewer2DInteractionSession::BeginPlacementNavigation(
    PointerPosition position) {
  if (middleMousePanning || mode != DragMode::Selection)
    return false;
  draggedSincePress = false;
  mode = DragMode::View;
  lastPointer = position;
  return true;
}

// Starts rectangle selection and records its cross-table selection intent.
void Viewer2DInteractionSession::BeginRectangleSelection(
    PointerPosition position, bool acrossAllTables) {
  mode = DragMode::RectSelection;
  rectangleActive = true;
  rectangleAcrossAllTables = acrossAllTables;
  rectangleStart = position;
  rectangleEnd = position;
}

// Starts selection movement for the supplied typed selection.
void Viewer2DInteractionSession::BeginSelectionDrag(
    DragTarget dragTarget, std::vector<std::string> activeSelection,
    SelectionBuckets typedSelection) {
  ResetGesture();
  mode = DragMode::Selection;
  target = dragTarget;
  activeUuids = std::move(activeSelection);
  selection = std::move(typedSelection);
}

// Resolves selection movement after delay, distance, and axis constraints.
SelectionDragMotion Viewer2DInteractionSession::ResolveSelectionMotion(
    PointerPosition position, long elapsedMilliseconds, bool axisConstrained) {
  if (mode != DragMode::Selection ||
      elapsedMilliseconds < kSelectionDragDelayMilliseconds)
    return {};

  int dx = position.x - lastPointer.x;
  int dy = position.y - lastPointer.y;
  if (!selectionMoved && std::abs(dx) < kSelectionDragStartThresholdPixels &&
      std::abs(dy) < kSelectionDragStartThresholdPixels)
    return {};

  if (axisConstrained) {
    if (axis == DragAxis::None &&
        (std::abs(dx) >= kSelectionDragStartThresholdPixels ||
         std::abs(dy) >= kSelectionDragStartThresholdPixels)) {
      axis = std::abs(dx) >= std::abs(dy) ? DragAxis::Horizontal
                                          : DragAxis::Vertical;
    }
    if (axis == DragAxis::Horizontal)
      dy = 0;
    else if (axis == DragAxis::Vertical)
      dx = 0;
  }

  lastPointer = position;
  return {dx != 0 || dy != 0, dx, dy};
}

// Updates the active rectangle-selection endpoint.
void Viewer2DInteractionSession::UpdateRectangle(PointerPosition position) {
  rectangleEnd = position;
  draggedSincePress = true;
}

// Records viewport movement for click suppression.
void Viewer2DInteractionSession::MarkNavigationMoved(PointerPosition position) {
  lastPointer = position;
  draggedSincePress = true;
}

// Records that a selection transform was applied during the gesture.
void Viewer2DInteractionSession::MarkSelectionMoved() {
  selectionMoved = true;
  draggedSincePress = true;
}

// Records that undo history has already been captured for the gesture.
void Viewer2DInteractionSession::MarkUndoPushed() {
  selectionUndoPushed = true;
}

// Clears the active axis constraint without ending the gesture.
void Viewer2DInteractionSession::ClearAxis() { axis = DragAxis::None; }

// Finishes a temporary middle-button pan and restores placement selection.
void Viewer2DInteractionSession::EndPan(bool continuousPlacementActive) {
  middleMousePanning = false;
  mode = viewport_navigation::ShouldResumeViewer2DSelection(
             continuousPlacementActive)
             ? DragMode::Selection
             : DragMode::None;
  draggedSincePress = false;
}

// Restores selection mode after left-button placement navigation.
void Viewer2DInteractionSession::EndPlacementNavigation() {
  mode = DragMode::Selection;
  draggedSincePress = false;
}

// Completes a gesture while retaining its click-suppression outcome.
void Viewer2DInteractionSession::CompleteGesture() {
  const bool suppressClick = draggedSincePress;
  ResetGesture();
  draggedSincePress = suppressClick;
}

// Clears click suppression after the panel consumes the pointer outcome.
void Viewer2DInteractionSession::ConsumePointerOutcome() {
  draggedSincePress = false;
}

// Clears every gesture marker so stale middle-button state cannot survive.
void Viewer2DInteractionSession::ResetGesture() {
  mode = DragMode::None;
  axis = DragAxis::None;
  target = DragTarget::None;
  activeUuids.clear();
  selection = {};
  selectionMoved = false;
  selectionUndoPushed = false;
  draggedSincePress = false;
  rectangleActive = false;
  rectangleAcrossAllTables = false;
  middleMousePanning = false;
}

// Cancels transient state after mouse capture is lost.
void Viewer2DInteractionSession::Cancel(bool continuousPlacementActive) {
  ResetGesture();
  if (continuousPlacementActive)
    mode = DragMode::Selection;
}

} // namespace viewer2d::interaction

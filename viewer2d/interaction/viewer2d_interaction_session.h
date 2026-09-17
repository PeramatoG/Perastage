/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <string>
#include <vector>

namespace viewer2d::interaction {

struct PointerPosition {
  int x = 0;
  int y = 0;
};

enum class DragMode { None, View, Selection, RectSelection };
enum class DragAxis { None, Horizontal, Vertical };
enum class DragTarget { None, Fixtures, Trusses, Supports, SceneObjects };

struct SelectionBuckets {
  std::vector<std::string> fixtures;
  std::vector<std::string> trusses;
  std::vector<std::string> supports;
  std::vector<std::string> sceneObjects;
};

struct SelectionDragMotion {
  bool active = false;
  int deltaX = 0;
  int deltaY = 0;
};

// Owns GUI-independent transient state for Viewer2D pointer gestures.
class Viewer2DInteractionSession {
public:
  // Starts a primary-button gesture in viewport-navigation mode.
  void BeginPrimary(PointerPosition position);
  // Starts an exclusive middle-button viewport-pan gesture when permitted.
  bool BeginPan(PointerPosition position, bool continuousPlacementActive);
  // Starts rectangle selection and records its cross-table selection intent.
  void BeginRectangleSelection(PointerPosition position, bool acrossAllTables);
  // Starts selection movement for the supplied typed selection.
  void BeginSelectionDrag(DragTarget target,
                          std::vector<std::string> activeUuids,
                          SelectionBuckets selection);
  // Resolves selection movement after delay, distance, and axis constraints.
  SelectionDragMotion ResolveSelectionMotion(PointerPosition position,
                                             long elapsedMilliseconds,
                                             bool axisConstrained);
  // Updates the active rectangle-selection endpoint.
  void UpdateRectangle(PointerPosition position);
  // Records viewport movement for click suppression.
  void MarkNavigationMoved(PointerPosition position);
  // Finishes a temporary middle-button pan and restores placement selection.
  void EndPan(bool continuousPlacementActive);
  // Clears selection and rectangle state while leaving neutral pointer data.
  void ResetGesture();
  // Cancels transient state after mouse capture is lost.
  void Cancel(bool continuousPlacementActive);

  DragMode mode = DragMode::None;
  DragAxis axis = DragAxis::None;
  DragTarget target = DragTarget::None;
  std::vector<std::string> activeUuids;
  SelectionBuckets selection;
  bool selectionMoved = false;
  bool selectionUndoPushed = false;
  bool middleMousePanning = false;
  bool draggedSincePress = false;
  bool rectangleActive = false;
  bool rectangleAcrossAllTables = false;
  PointerPosition rectangleStart;
  PointerPosition rectangleEnd;
  PointerPosition lastPointer;
};

} // namespace viewer2d::interaction

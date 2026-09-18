#include "../viewer2d/interaction/viewer2d_interaction_session.h"

#include <cassert>

// Characterizes navigation, selection-drag, and rectangle gesture sessions.
int main() {
  using namespace viewer2d::interaction;

  Viewer2DInteractionSession session;
  assert(session.BeginPan({10, 20}, false));
  assert(session.mode == DragMode::View);
  assert(session.middleMousePanning);
  session.MarkNavigationMoved({14, 25});
  assert(session.draggedSincePress);
  session.EndPan(false);
  assert(session.mode == DragMode::None);
  assert(!session.middleMousePanning);
  assert(!session.draggedSincePress);

  session.BeginSelectionDrag(DragTarget::Fixtures, {"fixture"}, {});
  assert(!session.BeginPan({0, 0}, false));
  assert(session.BeginPan({0, 0}, true));
  session.EndPan(true);
  assert(session.mode == DragMode::Selection);

  session.BeginPrimary({10, 10});
  session.BeginRectangleSelection({10, 10}, true);
  assert(session.mode == DragMode::RectSelection);
  assert(session.rectangleActive);
  assert(session.rectangleAcrossAllTables);
  session.UpdateRectangle({30, 40});
  assert(session.rectangleEnd.x == 30 && session.rectangleEnd.y == 40);
  assert(session.draggedSincePress);
  session.ResetGesture();
  assert(!session.rectangleActive);
  assert(!session.rectangleAcrossAllTables);
  assert(!session.middleMousePanning);

  SelectionBuckets selection;
  selection.fixtures = {"fixture-a", "fixture-b"};
  session.BeginPrimary({100, 100});
  session.BeginSelectionDrag(DragTarget::Fixtures, selection.fixtures,
                             selection);
  assert(!session.ResolveSelectionMotion({120, 100}, 149, true).active);
  assert(!session.ResolveSelectionMotion({102, 101}, 150, true).active);
  const SelectionDragMotion horizontal =
      session.ResolveSelectionMotion({104, 102}, 150, true);
  assert(horizontal.active && horizontal.deltaX == 4 && horizontal.deltaY == 0);
  assert(session.axis == DragAxis::Horizontal);
  session.MarkSelectionMoved();
  session.MarkUndoPushed();
  assert(session.activeUuids.size() == 2);
  assert(session.selection.fixtures.size() == 2);

  session.CompleteGesture();
  assert(session.mode == DragMode::None);
  assert(session.axis == DragAxis::None);
  assert(session.activeUuids.empty());
  assert(!session.selectionMoved && !session.selectionUndoPushed);
  assert(session.draggedSincePress);
  session.ConsumePointerOutcome();
  assert(!session.draggedSincePress);

  session.BeginPrimary({10, 10});
  session.BeginSelectionDrag(DragTarget::Trusses, {"truss"}, {});
  const SelectionDragMotion vertical =
      session.ResolveSelectionMotion({11, 15}, 150, true);
  assert(vertical.active && vertical.deltaX == 0 && vertical.deltaY == 5);
  assert(session.axis == DragAxis::Vertical);
  session.MarkSelectionMoved();
  session.MarkUndoPushed();

  session.Cancel(false);
  assert(session.mode == DragMode::None);
  assert(session.axis == DragAxis::None);
  assert(session.target == DragTarget::None);
  assert(session.activeUuids.empty());
  assert(session.selection.fixtures.empty());
  assert(!session.selectionMoved && !session.selectionUndoPushed);
  assert(!session.middleMousePanning);

  session.BeginPrimary({0, 0});
  session.BeginSelectionDrag(DragTarget::SceneObjects, {"object"}, {});
  const SelectionDragMotion free =
      session.ResolveSelectionMotion({5, 4}, 150, false);
  assert(free.active && free.deltaX == 5 && free.deltaY == 4);
  assert(session.axis == DragAxis::None);
  session.MarkSelectionMoved();
  session.Cancel(true);
  assert(session.mode == DragMode::Selection);
  assert(!session.selectionMoved && !session.selectionUndoPushed);

  session.BeginPlacementNavigation({4, 5});
  assert(!session.middleMousePanning);
  session.MarkNavigationMoved({8, 9});
  session.EndPlacementNavigation();
  assert(session.mode == DragMode::Selection);
  assert(!session.draggedSincePress);

  // Interleaved buttons cannot replace an active gesture or its capture.
  session.Cancel(false);
  assert(session.BeginPan({1, 2}, false));
  assert(!session.BeginPrimary({3, 4}));
  assert(!session.BeginPan({3, 4}, false));
  assert(session.middleMousePanning && session.mode == DragMode::View);
  session.Cancel(false);
  assert(!session.middleMousePanning && session.mode == DragMode::None);

  assert(session.BeginPrimary({5, 6}));
  assert(!session.BeginPan({7, 8}, false));
  assert(session.mode == DragMode::View && !session.middleMousePanning);
  session.ResetGesture();

  // Continuous placement permits pan only from its neutral selection state.
  session.Cancel(true);
  assert(session.BeginPan({9, 10}, true));
  assert(!session.BeginPlacementNavigation({9, 10}));
  session.EndPan(true);
  assert(session.mode == DragMode::Selection && !session.middleMousePanning);
  assert(session.BeginPlacementNavigation({11, 12}));
  assert(!session.BeginPan({11, 12}, true));
  session.Cancel(true);
  assert(session.mode == DragMode::Selection && !session.middleMousePanning);

  return 0;
}

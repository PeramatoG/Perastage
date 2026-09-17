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
  assert(!session.draggedSincePress);

  session.mode = DragMode::Selection;
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
  session.selectionMoved = true;
  session.selectionUndoPushed = true;
  assert(session.activeUuids.size() == 2);
  assert(session.selection.fixtures.size() == 2);

  session.Cancel(false);
  assert(session.mode == DragMode::None);
  assert(session.axis == DragAxis::None);
  assert(session.target == DragTarget::None);
  assert(session.activeUuids.empty());
  assert(session.selection.fixtures.empty());
  assert(!session.selectionMoved && !session.selectionUndoPushed);

  session.BeginPrimary({0, 0});
  session.BeginSelectionDrag(DragTarget::SceneObjects, {"object"}, {});
  const SelectionDragMotion free =
      session.ResolveSelectionMotion({5, 4}, 150, false);
  assert(free.active && free.deltaX == 5 && free.deltaY == 4);
  assert(session.axis == DragAxis::None);
  session.Cancel(true);
  assert(session.mode == DragMode::Selection);

  return 0;
}

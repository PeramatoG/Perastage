#include "../viewer2d/interaction/viewer2d_line_point_selection_session.h"

#include <cassert>

// Characterizes neutral two-point line-selection lifecycle state.
int main() {
  using namespace viewer2d::interaction;

  const WorldPoint start{1.0f, 2.0f, 3.0f};
  const WorldPoint end{4.0f, 5.0f, 6.0f};
  const WorldPoint first{2.0f, 3.0f, 4.0f};
  const WorldPoint second{3.0f, 4.0f, 5.0f};
  Viewer2DLinePointSelectionSession session;
  assert(!session.IsActive());
  assert(!session.FirstPoint());
  assert(!session.PreviewPoint());
  assert(!session.ShouldConsumeNextMouseUp());

  session.Begin(start, end);
  assert(session.IsActive());
  assert(session.LineStart() == start);
  assert(session.LineEnd() == end);
  session.UpdatePreview(first);
  assert(session.PreviewPoint() == first);
  assert(!session.AcceptPoint(first));
  assert(session.IsActive());
  assert(session.FirstPoint() == first);
  assert(session.PreviewPoint() == first);

  session.MarkConsumeNextMouseUp();
  assert(session.ShouldConsumeNextMouseUp());
  assert(session.ConsumeNextMouseUp());
  assert(!session.ShouldConsumeNextMouseUp());
  assert(!session.ConsumeNextMouseUp());

  const auto completed = session.AcceptPoint(second);
  assert(completed);
  assert(completed->first == first);
  assert(completed->second == second);
  assert(!session.IsActive());
  assert(!session.FirstPoint());
  assert(!session.PreviewPoint());

  session.Begin(end, start);
  assert(session.LineStart() == end);
  assert(session.LineEnd() == start);
  assert(!session.FirstPoint());
  assert(!session.PreviewPoint());
  session.UpdatePreview(second);
  session.MarkConsumeNextMouseUp();
  session.Reset();
  assert(!session.IsActive());
  assert(!session.FirstPoint());
  assert(!session.PreviewPoint());
  assert(!session.ShouldConsumeNextMouseUp());

  session.Begin(start, end);
  assert(!session.FirstPoint());
  assert(!session.PreviewPoint());
  return 0;
}

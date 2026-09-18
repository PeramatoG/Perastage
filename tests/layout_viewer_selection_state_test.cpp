#include "layout_viewer_selection_state.h"

#include <cassert>

namespace {
using namespace gui::layoutselection;

// Verifies explicit selection transitions and neutral-state invariants.
void TestSelectionState() {
  LayoutViewerSelectionState state;
  assert(state.Current() == LayoutElementRef{});
  assert(state.Select(LayoutElementKind::View2D, 4));
  assert(state.IsSelected(LayoutElementKind::View2D, 4));
  assert(!state.Select(LayoutElementKind::View2D, 4));
  assert(state.Select(LayoutElementKind::Legend, 4));
  assert(state.Select(LayoutElementKind::Legend, 8));
  assert(state.Select(LayoutElementKind::None, 8));
  assert(state.Current() == LayoutElementRef{});
  assert(!state.Select(LayoutElementKind::Text, -1));
  state.Select(LayoutElementKind::Image, 3);
  assert(state.Clear());
  assert(!state.Clear());
}

// Verifies retained and category-prioritized default selection decisions.
void TestDefaultSelection() {
  ElementRefsByKind ids{{{{1}}, {{2}}, {{3}}, {{4}}, {{5}}}};
  assert(RetainOrChooseDefault({LayoutElementKind::Text, 4}, ids) ==
         (LayoutElementRef{LayoutElementKind::Text, 4}));
  assert(RetainOrChooseDefault({}, ids) ==
         (LayoutElementRef{LayoutElementKind::View2D, 1}));
  ids[0].clear();
  assert(RetainOrChooseDefault({}, ids).kind == LayoutElementKind::Legend);
  ids[1].clear();
  assert(RetainOrChooseDefault({}, ids).kind == LayoutElementKind::EventTable);
  ids[2].clear();
  assert(RetainOrChooseDefault({}, ids).kind == LayoutElementKind::Text);
  ids[3].clear();
  assert(RetainOrChooseDefault({}, ids).kind == LayoutElementKind::Image);
  ids[4].clear();
  assert(RetainOrChooseDefault({}, ids) == LayoutElementRef{});
}

// Verifies mixed-type stable ordering, range, and front/back targets.
void TestZOrder() {
  ElementsByKind source;
  source[0] = {{10, 2}, {11, -3}};
  source[1] = {{20, 2}};
  source[2] = {{30, 0}};
  source[3] = {{40, 2}};
  source[4] = {{50, -1}};
  const auto ordered = BuildStableZOrder(source);
  assert(ordered.size() == 6);
  assert(ordered[0].element.id == 11);
  assert(ordered[1].element.id == 50);
  assert(ordered[2].element.id == 30);
  assert(ordered[3].element ==
         (LayoutElementRef{LayoutElementKind::View2D, 10}));
  assert(ordered[4].element.kind == LayoutElementKind::Legend);
  assert(ordered[5].element.kind == LayoutElementKind::Text);
  assert(ordered.rbegin()->element.id == 40);
  assert((GetZIndexRange(ordered) == std::pair<int, int>(-3, 2)));
  assert(BringToFrontTarget(ordered) == 3);
  assert(SendToBackTarget(ordered) == -4);
  assert((GetZIndexRange({}) == std::pair<int, int>(0, 0)));
}
} // namespace

// Runs the GUI-independent selection and Z-order policy checks.
int main() {
  TestSelectionState();
  TestDefaultSelection();
  TestZOrder();
  return 0;
}

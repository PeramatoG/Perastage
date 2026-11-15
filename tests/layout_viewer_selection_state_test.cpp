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
  LayoutViewerSelectionState state;
  const LayoutElementRef a{LayoutElementKind::View2D,1}, b{LayoutElementKind::Text,1}, c{LayoutElementKind::Image,2};
  state.Select(a.kind,a.id);
  assert(state.Add(b) && state.Add(c)); // Shift-add; typed IDs may overlap.
  assert(!state.Add(a) && state.Current()==c);
  assert((state.Ordered()==std::vector<LayoutElementRef>{a,b,c}));
  assert(state.Toggle(b) && !state.Matches(b)); // Ctrl-toggle.
  assert(state.Toggle(b) && state.Current()==b);
  assert((state.Ordered()==std::vector<LayoutElementRef>{a,c,b}));
  state.Remove(b); assert(state.Current()==c);
  ElementRefsByKind valid;
  valid[0]={1}; valid[3]={1};
  state.Retain(valid); assert(state.Current()==a && state.Ordered().size()==1);
  const std::vector<SelectionFrame> frames{{a,10,10,30,30},{b,20,20,30,30},{c,100,100,10,10}};
  assert(HitFrame(frames,25,25)==b); // Highest stable Z wins overlap.
  assert(HitFrame(frames,12,12)==a);
  assert(HitFrame(frames,90,90)==LayoutElementRef{});
  assert((IntersectingFrames(frames,0,0,22,22)==std::vector<LayoutElementRef>{a,b}));
  assert((IntersectingFrames(frames,22,22,-22,-22)==std::vector<LayoutElementRef>{a,b}));
  assert((IntersectingFrames(frames,0,0,200,200)==std::vector<LayoutElementRef>{a,b,c}));
  assert(state.Replace({c,a,c,b}) && state.Current()==b);
  assert((state.Ordered()==std::vector<LayoutElementRef>{c,a,b}));
  assert(!state.Replace({c,a,b}));
  state.Clear(); // Page replacement must clear even when IDs are reused.
  assert(state.Current()==LayoutElementRef{});
  return 0;
}

#include "../viewer3d/interaction/navigation_interaction_policy.h"
#include "../viewer3d/interaction/selection_interaction_policy.h"

#include <cassert>
#include <vector>

// Verifies camera, wheel, drag-threshold, and typed-selection policies.
int main() {
  using namespace viewer3d::interaction;

  const auto orbit = ResolveCameraDrag(
      {NavigationMode::Orbit, {10, 20}, {14, 26}, true, false, {false, false}});
  assert(orbit && orbit->action == CameraDragIntent::Action::Orbit);
  assert(orbit->horizontal == 2.0f && orbit->vertical == -3.0f);
  const auto invertedOrbit = ResolveCameraDrag(
      {NavigationMode::Orbit, {10, 20}, {14, 26}, true, false, {true, true}});
  assert(invertedOrbit && invertedOrbit->horizontal == -2.0f &&
         invertedOrbit->vertical == 3.0f);

  const auto pan = ResolveCameraDrag(
      {NavigationMode::Pan, {10, 20}, {14, 26}, false, true, {}});
  assert(pan && pan->action == CameraDragIntent::Action::Pan);
  assert(pan->horizontal == -0.04f && pan->vertical == 0.06f);
  assert(!ResolveCameraDrag(
      {NavigationMode::None, {10, 20}, {14, 26}, true, true, {}}));
  assert(!ResolveCameraDrag(
      {NavigationMode::Orbit, {10, 20}, {10, 20}, true, false, {}}));

  assert(ResolveWheelZoomSteps(120, 120) == -1.0f);
  assert(ResolveWheelZoomSteps(-240, 120) == 2.0f);
  assert(!ResolveWheelZoomSteps(120, 0));
  assert(!ResolveWheelZoomSteps(0, 120));
  assert(!HasSelectionDragStarted(2, -2, 3));
  assert(HasSelectionDragStarted(3, 0, 3));
  assert(HasSelectionDragStarted(0, -3, 3));

  NavigationSession session;
  assert(!session.IsActive() && !session.HasMoved());
  session.Begin(NavigationMode::Pan);
  assert(session.IsActive() && session.GetMode() == NavigationMode::Pan);
  session.MarkMoved();
  assert(session.HasMoved());
  session.End();
  assert(!session.IsActive() && session.HasMoved());
  session.ClearMoved();
  assert(!session.HasMoved());
  session.Begin(NavigationMode::Orbit);
  session.Reset();
  assert(!session.IsActive() && !session.HasMoved() &&
         session.GetMode() == NavigationMode::None);

  const std::vector<std::string> current{"a", "b"};
  const std::vector<std::string> group{"b", "c"};
  assert((ResolveClickedSelection(current, group, false, false) == group));
  assert((ResolveClickedSelection(current, group, true, true) ==
          std::vector<std::string>{"a", "b", "c"}));
  assert((ResolveClickedSelection(current, group, true, false) ==
          std::vector<std::string>{"a", "b", "c"}));
  assert((ResolveClickedSelection({"a", "b", "c"}, group, true, false) ==
          std::vector<std::string>{"a"}));

  const TypedSelection typed{{"b", "a"}, {"c"}, {"a"}, {"d"}};
  assert((FlattenTypedSelection(typed) ==
          std::vector<std::string>{"a", "b", "c", "d"}));
}

#include "../viewer_common/viewport_mouse_navigation.h"

#include <cassert>
#include <optional>
#include <string_view>

// Verifies gesture routing, orbit inversion independence, and preference defaults.
int main() {
  using namespace viewport_navigation;

  assert(ResolveViewer3DAction(MouseButton::Left, false) ==
         Viewer3DAction::Orbit);
  assert(ResolveViewer3DAction(MouseButton::Left, true) == Viewer3DAction::Pan);
  assert(ResolveViewer3DAction(MouseButton::Middle, false) ==
         Viewer3DAction::Pan);
  assert(!IsExclusiveViewer2DPan(MouseButton::Left));
  assert(IsExclusiveViewer2DPan(MouseButton::Middle));

  assert((ResolveOrbitDeltas(2.0f, -3.0f, false, false) ==
          std::pair(2.0f, -3.0f)));
  assert((ResolveOrbitDeltas(2.0f, -3.0f, true, false) ==
          std::pair(-2.0f, -3.0f)));
  assert((ResolveOrbitDeltas(2.0f, -3.0f, false, true) ==
          std::pair(2.0f, 3.0f)));
  assert((ResolveOrbitDeltas(2.0f, -3.0f, true, true) ==
          std::pair(-2.0f, 3.0f)));

  assert(!ReadBooleanPreference(std::optional<std::string>{}));
  assert(!ReadBooleanPreference(std::string("0")));
  assert(ReadBooleanPreference(std::string("1")));
}

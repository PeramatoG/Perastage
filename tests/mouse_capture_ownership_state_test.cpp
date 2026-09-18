#include "../gui/mouse_capture_ownership.h"

#include <cassert>

// Verifies balanced capture ownership transitions without requiring wx GUI.
int main() {
  ui::MouseCaptureOwnershipState state;
  assert(!state.IsOwned());
  assert(state.MarkAcquired());
  assert(state.IsOwned());
  assert(!state.MarkAcquired());
  assert(state.ConsumeForRelease(true));
  assert(!state.IsOwned());
  assert(!state.ConsumeForRelease(true));

  assert(state.MarkAcquired());
  assert(state.AbandonOnLoss());
  assert(!state.IsOwned());
  assert(!state.AbandonOnLoss());
  assert(!state.ConsumeForRelease(true));

  assert(state.MarkAcquired());
  assert(!state.ConsumeForRelease(false));
  assert(!state.IsOwned());
  return 0;
}

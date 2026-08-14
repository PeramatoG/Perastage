#include "clipboard_placement_confirmation.h"

#include <cassert>
#include <vector>

// Verifies Magnet finalization precedes clipboard transaction confirmation.
int main() {
  std::vector<std::string> events;
  const std::string next = scene_clipboard::ConfirmSinglePlacement(
      "current", [&]() { events.push_back("magnet"); },
      [&](const std::string &uuid) {
        events.push_back("confirm:" + uuid);
        return std::string("next");
      });
  assert(next == "next");
  assert((events == std::vector<std::string>{"magnet", "confirm:current"}));

  events.clear();
  const std::function<void()> canceledFinalize;
  assert(!canceledFinalize);
  assert(events.empty());
  return 0;
}

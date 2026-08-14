#pragma once

#include <functional>
#include <string>

namespace scene_clipboard {

// Finalizes transient placement state before committing one clipboard clone.
inline std::string ConfirmSinglePlacement(
    const std::string &uuid, const std::function<void()> &finalizePlacement,
    const std::function<std::string(const std::string &)> &commit) {
  if (finalizePlacement)
    finalizePlacement();
  return commit ? commit(uuid) : std::string{};
}

} // namespace scene_clipboard

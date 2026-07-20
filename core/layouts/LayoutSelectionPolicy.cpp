#include "LayoutSelectionPolicy.h"

#include <algorithm>

namespace layouts {

// Chooses the row that should remain active when the layout selection is repaired.
std::optional<int> ChooseLayoutSelectionRow(const LayoutSelectionRequest &request) {
  if (request.layoutNames.empty())
    return std::nullopt;
  if (!request.currentLayout.empty()) {
    const auto it = std::find(request.layoutNames.begin(), request.layoutNames.end(),
                              request.currentLayout);
    if (it != request.layoutNames.end())
      return static_cast<int>(std::distance(request.layoutNames.begin(), it));
  }
  if (request.preferredRow >= 0) {
    if (request.preferredRow < static_cast<int>(request.layoutNames.size()))
      return request.preferredRow;
    return static_cast<int>(request.layoutNames.size() - 1);
  }
  return 0;
}

} // namespace layouts

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace layouts {

struct LayoutSelectionRequest {
  std::vector<std::string> layoutNames;
  std::string currentLayout;
  int preferredRow = -1;
};

std::optional<int> ChooseLayoutSelectionRow(const LayoutSelectionRequest &request);

} // namespace layouts

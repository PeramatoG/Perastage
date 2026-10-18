/* This file is part of Perastage. */
#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace mvr {
struct ModelDependencies {
  std::vector<std::string> references;
  std::string error;
};
// Shared dependency inspection; callers choose their preservation policy.
ModelDependencies InspectModelDependencies(const std::filesystem::path &path);
} // namespace mvr

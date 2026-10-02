#pragma once

#include <filesystem>
#include <optional>

// Supplies caller-owned resources without changing canonical format policy.
struct MvrExportEnvironment {
  std::optional<std::filesystem::path> fixtureFallbackGdtfPath;
};

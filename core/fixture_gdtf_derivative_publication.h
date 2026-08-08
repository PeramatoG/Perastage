#pragma once

#include <filesystem>
#include <string>

namespace fixture_gdtf {

struct PreparedDerivative {
  std::filesystem::path workingPath;
  std::filesystem::path publishedPath;
  std::string publishedReference;
};

// Prepares a private working copy and its eventual project publication target.
bool PrepareProjectDerivative(const std::filesystem::path &sourcePath,
                              const std::filesystem::path &projectBasePath,
                              const std::filesystem::path &canonicalFileName,
                              PreparedDerivative &prepared,
                              std::string &errorMessage);

// Validates and atomically publishes a prepared canonical derivative.
bool PublishPreparedDerivative(const PreparedDerivative &prepared,
                               std::string &errorMessage);

// Removes private working storage without touching a published derivative.
void DiscardPreparedDerivative(const PreparedDerivative &prepared);

} // namespace fixture_gdtf

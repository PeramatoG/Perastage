#pragma once

#include <filesystem>
#include <string>

namespace fixture_gdtf {

struct PreparedDerivative {
  std::filesystem::path workingPath;
  std::filesystem::path publishedPath;
  std::string publishedReference;
};

// Exact standard mutation revisions establish provenance without SVG markers.
bool HasPerastageStandardSvgRevision(const std::filesystem::path &sourcePath);

// Canonicalizes a source archive through a private copy and atomically publishes it.
bool PublishCanonicalGdtfCopy(const std::filesystem::path &sourcePath,
                              const std::filesystem::path &publishedPath,
                              std::string &errorMessage);

// Prepares a private copy for an explicitly owned publication target.
bool PrepareOwnedDerivative(const std::filesystem::path &sourcePath,
                            const std::filesystem::path &publishedPath,
                            const std::string &publishedReference,
                            PreparedDerivative &prepared,
                            std::string &errorMessage);

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

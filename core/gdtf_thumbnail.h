#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace gdtf {

struct ThumbnailResource {
  std::string archivePath;
  bool svg = false;
  bool compatibilityFallback = false;
};

// Exact canonical root SVG/PNG candidates precede permissive legacy imports.
std::vector<ThumbnailResource> InspectGdtfThumbnails(const std::filesystem::path &source);

} // namespace gdtf

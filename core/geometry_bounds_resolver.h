#pragma once

#include "geometry_bounds.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

// Resolves and caches CPU-side bounds using the renderers' existing mesh loaders.
class GeometryBoundsResolver {
public:
  // Resolves bounds for a supported 3DS or GLB resource.
  static std::optional<GeometryBounds>
  Resolve(const std::filesystem::path &path, std::string *diagnostic = nullptr);

  // Returns the number of physical parses, for cache regression tests.
  static std::size_t ParseCountForTesting();

  // Clears process-local cached measurements, for deterministic tests.
  static void ClearForTesting();
};

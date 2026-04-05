#pragma once

#include "LayoutCollection.h"

#include <string>
#include <vector>

namespace layouts {

struct LayoutDefaultsLoadResult {
  std::vector<LayoutDefinition> layouts;
  int filesScanned = 0;
  int filesImported = 0;
};

LayoutDefaultsLoadResult LoadLayoutDefaultsFromLibrary(
    const std::string &librarySubdir = "default_layouts");

} // namespace layouts

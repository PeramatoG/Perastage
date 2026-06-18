#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "truss.h"

namespace gui {

struct TrussCreationSource {
  std::string displayName;
  std::string definitionPath;
};

std::vector<TrussCreationSource> CollectTrussCreationSources(
    const std::unordered_map<std::string, Truss> &trusses,
    const std::string &sceneBasePath);

} // namespace gui

#pragma once

#include <string>

namespace gui {

std::string PreserveSceneResourceReferenceForTableSync(
    const std::string &basePath, const std::string &currentRef,
    const std::string &candidateRef, const std::string &displayOnlyRef = {});

} // namespace gui

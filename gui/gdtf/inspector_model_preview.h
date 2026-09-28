#pragma once

#include "mesh.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gui::inspection {

// Prepares texture-free model geometry without touching wx or OpenGL objects.
std::optional<Mesh> PrepareInspectorModel(
    const std::vector<std::uint8_t> &bytes, const std::string &archivePath,
    std::string *error = nullptr);

} // namespace gui::inspection

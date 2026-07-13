#pragma once

#include <filesystem>
#include <string>

namespace gui {

std::string BuildGdtfSourceFingerprint(const std::filesystem::path &path);

} // namespace gui

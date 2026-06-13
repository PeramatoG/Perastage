#pragma once

#include <filesystem>
#include <string>

namespace PathUtils {

std::filesystem::path PathFromUtf8(const std::string &text);
std::string PathToUtf8(const std::filesystem::path &path);

} // namespace PathUtils

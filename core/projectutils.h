#pragma once

#include <string>
#include <optional>

namespace ProjectUtils {
    inline constexpr const char* PROJECT_EXTENSION = ".pstg";

    std::string GetLastProjectPathFile();
    bool SaveLastProjectPath(const std::string& path);
    std::optional<std::string> LoadLastProjectPath();

    // Returns the path to a library subdirectory if it exists, otherwise empty.
    std::string GetDefaultLibraryPath(const std::string& subdir);
}


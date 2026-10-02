#pragma once

#include <filesystem>
#include <string>

namespace gdtf_filename_policy {

// Builds the canonical Perastage filename from explicit fixture identity.
std::string BuildCanonicalFileName(const std::string &manufacturer,
                                   const std::string &model,
                                   const std::string &fallbackStem = {});

// Builds the canonical Perastage filename from a GDTF source archive.
std::string BuildCanonicalFileName(const std::filesystem::path &sourcePath);

// Reports whether a filename carries the canonical Perastage comment segment.
bool IsPerastageNamedFile(const std::filesystem::path &path);

} // namespace gdtf_filename_policy

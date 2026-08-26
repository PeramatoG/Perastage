#pragma once

#include <filesystem>
#include <string>

namespace gdtf_download_filename {

// Builds a portable human-readable filename from authoritative catalog identity.
std::string BuildReadableFileName(const std::string &manufacturer,
                                  const std::string &fixtureName);

// Chooses a deterministic collision-safe destination while preserving readable identity.
std::filesystem::path ChooseDestination(
    const std::filesystem::path &directory, const std::string &manufacturer,
    const std::string &fixtureName, const std::string &revisionId);

} // namespace gdtf_download_filename

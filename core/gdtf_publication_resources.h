#pragma once

#include "gdtf_canonicalizer.h"

#include <filesystem>
#include <string>
#include <vector>

namespace fixture_gdtf {

// Plans removal of positively recognized historical private symbol extensions.
// Reading and ordinary canonicalization never apply this publication-only plan.
bool BuildStandardPublicationResourceMutations(
    const std::filesystem::path &sourcePath,
    std::vector<GdtfCanonicalizer::ResourceMutation> &mutations,
    std::string &errorMessage);

} // namespace fixture_gdtf

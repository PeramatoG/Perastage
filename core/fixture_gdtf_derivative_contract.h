#pragma once

#include <string>

namespace fixture_gdtf {

// Validates exchange-resource ownership independently of SVG completeness.
// Authored unusable SVGs may remain unchanged while other missing views are added.
// Legacy private extensions are readable inputs, never valid publication output.
bool ValidatePublishedDerivative(const std::string &path,
                                 std::string &errorMessage);

} // namespace fixture_gdtf

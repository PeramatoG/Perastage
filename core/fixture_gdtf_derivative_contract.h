#pragma once

#include <string>

namespace fixture_gdtf {

// Validates the four-view contract required by a published Perastage derivative.
bool ValidatePublishedDerivative(const std::string &path,
                                 std::string &errorMessage);

} // namespace fixture_gdtf

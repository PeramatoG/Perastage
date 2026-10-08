#pragma once

#include <string>

namespace fixture_gdtf {

// Accepts complete standard Top/Side/Front or internal Perastage four-view sets.
// Bottom belongs only to the internal Perastage set.
bool ValidatePublishedDerivative(const std::string &path,
                                 std::string &errorMessage);

} // namespace fixture_gdtf

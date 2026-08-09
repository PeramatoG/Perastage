#include "fixture_gdtf_derivative_contract.h"

#include "symbols/PerastageSvgSymbol.h"
#include <string>

namespace fixture_gdtf {

// Validates the four-view contract required by a published Perastage derivative.
bool ValidatePublishedDerivative(const std::string &path,
                                 std::string &errorMessage) {
  RequiredFixtureSvgSetInspection inspection;
  if (!InspectRequiredFixtureSvgSet(path, inspection) || !inspection.usable) {
    errorMessage = inspection.diagnostic.empty()
                       ? "Could not inspect the fixture derivative symbols."
                       : inspection.diagnostic;
    return false;
  }
  errorMessage.clear();
  return true;
}

} // namespace fixture_gdtf

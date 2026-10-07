#include "fixture_gdtf_derivative_contract.h"

#include "symbols/fixture_symbol_resource_contract.h"
#include <string>

namespace fixture_gdtf {

// Keeps standard and internal Perastage completeness independent.
bool ValidatePublishedDerivative(const std::string &path,
                                 std::string &errorMessage) {
  FixtureSymbolResourceInspection inspection;
  if (!InspectFixtureSymbolResources(path, inspection) ||
      (!inspection.standardViewsUsable && !inspection.perastageViewsUsable)) {
    errorMessage = inspection.diagnostic.empty()
                       ? "Could not inspect the fixture derivative symbols."
                       : inspection.diagnostic;
    return false;
  }
  errorMessage.clear();
  return true;
}

} // namespace fixture_gdtf

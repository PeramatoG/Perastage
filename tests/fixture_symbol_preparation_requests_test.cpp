#include "symbols/fixture_symbol_preparation_requests.h"

#include <cassert>
#include <string>

// Verifies missing-symbol notifications remain immediate and GUI-independent.
int main() {
  int requests = 0;
  std::string path;
  std::string mode;
  symbols::SetFixtureSymbolPreparationRequestHandler(
      [&](const std::string &requestedPath, const std::string &requestedMode) {
        ++requests;
        path = requestedPath;
        mode = requestedMode;
      });
  symbols::RequestFixtureSymbolPreparation("/fixture.gdtf", "Exact Mode");
  assert(requests == 1);
  assert(path == "/fixture.gdtf");
  assert(mode == "Exact Mode");
  symbols::SetFixtureSymbolPreparationRequestHandler({});
  symbols::RequestFixtureSymbolPreparation("/ignored.gdtf", "Exact Mode");
  assert(requests == 1);
  return 0;
}

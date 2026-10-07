#include "symbols/fixture_symbol_resource_contract.h"

// This target has no toolkit or renderer include directories or libraries.
int main() {
  FixtureSymbolResourceInspection resources;
  return resources.standardViewsUsable ? 1 : 0;
}

#include "symbols/fixture_symbol_preview_model.h"
#include "symbols/fixture_symbol_resolution.h"

// This target has no toolkit or renderer include directories or libraries.
int main() {
  FixtureSymbolResourceInspection resources;
  const auto result = ResolveFixtureSymbolView(resources, SymbolViewKind::Bottom,
      FixtureSymbolResolutionPurpose::StandardGdtf);
  return result.usable ? 1 : 0;
}

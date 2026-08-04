#include "symbols/fixture_symbol_svg_cache.h"

#include <cassert>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace {

// Creates valid distinguishable symbol data for cache contract tests.
PerastageSvgSymbolData MakeSymbol(double width, SymbolViewKind view) {
  PerastageSvgSymbolData data;
  data.viewKind = view;
  data.viewBoxWidth = width;
  data.viewBoxHeight = 10.0;
  data.strokes.push_back({{{0.0, 0.0}, {width, 10.0}}});
  return data;
}

// Verifies failures are retried and positive values are content-qualified.
void TestMissMutationAndFingerprintCoherence() {
  std::unordered_map<std::string, PerastageSvgSymbolData> available;
  symbol_cache::FixtureSymbolSvgCache cache(
      [&](const std::string &, SymbolViewKind view, PerastageSvgSymbolData &out,
          std::string *) {
        const auto it = available.find(std::to_string(static_cast<int>(view)));
        if (it == available.end())
          return false;
        out = it->second;
        return true;
      });
  symbol_cache::FixtureSymbolSvgRequest request{
      "fixture.gdtf", SymbolViewKind::Top, "fp-a", "id-a"};
  assert(!cache.LookupOrLoad(request));
  available[std::to_string(static_cast<int>(SymbolViewKind::Top))] =
      MakeSymbol(10.0, SymbolViewKind::Top);
  cache.InvalidatePath("fixture.gdtf");
  const auto first = cache.LookupOrLoad(request);
  assert(first && first->viewBoxWidth == 10.0);

  available[std::to_string(static_cast<int>(SymbolViewKind::Top))] =
      MakeSymbol(20.0, SymbolViewKind::Top);
  request.semanticFingerprint = "fp-b";
  const auto second = cache.LookupOrLoad(request);
  assert(second && second->viewBoxWidth == 20.0);
  assert(first->viewBoxWidth == 10.0);
}

// Verifies views, identities, aliases, and lifecycle invalidation remain
// separate.
void TestStructuredKeysAndSafeHandles() {
  int loads = 0;
  symbol_cache::FixtureSymbolSvgCache cache(
      [&](const std::string &, SymbolViewKind view, PerastageSvgSymbolData &out,
          std::string *) {
        ++loads;
        out = MakeSymbol(view == SymbolViewKind::Top ? 10.0 : 30.0, view);
        return true;
      });
  const auto temp =
      std::filesystem::temp_directory_path() / "perastage_fixture_symbol_cache";
  const auto path = temp / "fixture.gdtf";
  symbol_cache::FixtureSymbolSvgRequest top{path.string(), SymbolViewKind::Top,
                                            "fp", "identity-a"};
  const auto first = cache.LookupOrLoad(top);
  top.physicalGdtfPath = (temp / "." / "fixture.gdtf").string();
  assert(cache.LookupOrLoad(top) == first);
  assert(loads == 1);

  auto front = top;
  front.view = SymbolViewKind::Front;
  assert(cache.LookupOrLoad(front)->viewBoxWidth == 30.0);
  auto otherIdentity = top;
  otherIdentity.generationIdentityKey = "identity-b";
  assert(cache.LookupOrLoad(otherIdentity));
  assert(loads == 3);

  cache.InvalidateGenerationIdentity("identity-a");
  assert(first->viewBoxWidth == 10.0);
  assert(cache.LookupOrLoad(top));
  cache.Clear();
  assert(cache.GetStats().entries == 0);
}

} // namespace

// Supplies the production loader symbol while focused tests inject their
// loader.
bool LoadPerastageSvgSymbolFromGdtf(const std::string &, SymbolViewKind,
                                    PerastageSvgSymbolData &, std::string *) {
  return false;
}

namespace symbol_cache {

// Supplies the fingerprint invalidation symbol for the focused cache test.
void InvalidateGdtfSemanticFingerprintCache(const std::string &) {}

// Supplies the fingerprint clear symbol for the focused cache test.
void ClearGdtfSemanticFingerprintCache() {}

} // namespace symbol_cache

// Runs managed fixture SVG cache regression coverage.
int main() {
  TestMissMutationAndFingerprintCoherence();
  TestStructuredKeysAndSafeHandles();
  return 0;
}

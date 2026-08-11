#include "symbols/fixture_symbol_svg_cache.h"

#include <cassert>
#include <filesystem>
#include <fstream>
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

// Writes a revision-visible archive placeholder for cache-key tests.
void WriteRevision(const std::filesystem::path &path,
                   const std::string &content) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << content;
}

// Verifies failures are retried and explicit invalidation exposes replacements.
void TestMissMutationAndInvalidation() {
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
  symbol_cache::FixtureSymbolSvgRequest request{"fixture.gdtf",
                                                SymbolViewKind::Top};
  assert(!cache.LookupOrLoad(request));
  available[std::to_string(static_cast<int>(SymbolViewKind::Top))] =
      MakeSymbol(10.0, SymbolViewKind::Top);
  cache.InvalidatePath("fixture.gdtf");
  const auto first = cache.LookupOrLoad(request);
  assert(first && first->viewBoxWidth == 10.0);

  available[std::to_string(static_cast<int>(SymbolViewKind::Top))] =
      MakeSymbol(20.0, SymbolViewKind::Top);
  cache.InvalidatePath("fixture.gdtf");
  const auto second = cache.LookupOrLoad(request);
  assert(second && second->viewBoxWidth == 20.0);
  assert(first->viewBoxWidth == 10.0);
}

// Verifies aliases, views, bounded file revisions, and lifecycle clears.
void TestStructuredKeysAndSafeHandles() {
  int loads = 0;
  symbol_cache::FixtureSymbolSvgCache cache(
      [&](const std::string &, SymbolViewKind view, PerastageSvgSymbolData &out,
          std::string *) {
        ++loads;
        out =
            MakeSymbol(view == SymbolViewKind::Top ? loads * 10.0 : 30.0, view);
        return true;
      });
  const auto temp =
      std::filesystem::temp_directory_path() / "perastage_fixture_symbol_cache";
  const auto path = temp / "fixture.gdtf";
  std::filesystem::remove_all(temp);
  WriteRevision(path, "first");
  symbol_cache::FixtureSymbolSvgRequest top{path.string(), SymbolViewKind::Top};
  const auto first = cache.LookupOrLoad(top);
  top.physicalGdtfPath = (temp / "." / "fixture.gdtf").string();
  assert(cache.LookupOrLoad(top) == first);
  assert(loads == 1);

  auto front = top;
  front.view = SymbolViewKind::Front;
  assert(cache.LookupOrLoad(front)->viewBoxWidth == 30.0);
  assert(loads == 2);

  WriteRevision(path, "second revision");
  const auto revised = cache.LookupOrLoad(top);
  assert(revised && revised != first && revised->viewBoxWidth == 30.0);
  assert(loads == 3);
  assert(first->viewBoxWidth == 10.0);

  cache.Clear();
  assert(cache.GetStats().entries == 0);
  std::filesystem::remove_all(temp);
}
} // namespace

// Supplies the production loader symbol while focused tests inject their
// loader.
bool LoadPerastageSvgSymbolFromGdtf(const std::string &, SymbolViewKind,
                                    PerastageSvgSymbolData &, std::string *) {
  return false;
}

// Runs managed fixture SVG cache regression coverage.
int main() {
  TestMissMutationAndInvalidation();
  TestStructuredKeysAndSafeHandles();
  return 0;
}

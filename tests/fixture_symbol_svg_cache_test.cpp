#include "symbols/fixture_symbol_svg_cache.h"
#include "symbols/fixture_symbol_resource_revision.h"

#include <cassert>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

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
          std::string *, FixtureSymbolResolutionPurpose) {
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
          std::string *, FixtureSymbolResolutionPurpose) {
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

  auto standard = top;
  standard.purpose = FixtureSymbolResolutionPurpose::StandardGdtf;
  const auto standardHandle = cache.LookupOrLoad(standard);
  assert(standardHandle != first);
  assert(cache.LookupOrLoad(standard) == standardHandle);
  assert(loads == 3);

  WriteRevision(path, "second revision");
  const auto revised = cache.LookupOrLoad(top);
  assert(revised && revised != first && revised->viewBoxWidth == 40.0);
  assert(loads == 4);
  assert(first->viewBoxWidth == 10.0);

  cache.Clear();
  assert(cache.GetStats().entries == 0);
  std::filesystem::remove_all(temp);
}

// Verifies provenance changes invalidate generation identity even when the
// parsed SVG geometry remains identical.
void TestSymbolProvenanceSemanticFingerprint() {
  const auto entry = [](const std::string &path, const std::string &content) {
    return symbol_cache::GdtfSemanticFingerprintEntry{
        path, {content.begin(), content.end()}};
  };
  const std::string authored =
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10 10\">"
      "<polygon points=\"0,0 10,0 10,10\"/></svg>";
  const std::string generated =
      "<svg xmlns=\"http://www.w3.org/2000/svg\" "
      "data-perastage-symbol-version=\"1\" viewBox=\"0 0 10 10\">"
      "<polygon points=\"0,0 10,0 10,10\"/></svg>";
  std::vector<symbol_cache::GdtfSemanticFingerprintEntry> entries = {
      entry("description.xml", "<GDTF><FixtureType/></GDTF>"),
      entry("models/svg/base.svg", authored),
      entry("models/svg_side/base.svg", authored),
      entry("models/svg_front/base.svg", authored)};
  std::string error;
  const auto standard =
      symbol_cache::ComputeGdtfSemanticFingerprintFromEntries(entries, error);
  assert(!standard.empty() && error.empty());
  for (std::size_t i = 1; i < entries.size(); ++i) {
    entries[i].bytes.assign(generated.begin(), generated.end());
    const auto marked =
        symbol_cache::ComputeGdtfSemanticFingerprintFromEntries(entries, error);
    assert(!marked.empty() && error.empty() && marked != standard);
    entries[i].bytes.assign(authored.begin(), authored.end());
  }
  entries.push_back(entry("models/svg/base_bottom.svg", authored));
  const auto withLegacyBottom =
      symbol_cache::ComputeGdtfSemanticFingerprintFromEntries(entries, error);
  assert(!withLegacyBottom.empty() && error.empty() &&
         withLegacyBottom != standard);
  entries.back().bytes.assign(generated.begin(), generated.end());
  const auto withGeneratedBottom =
      symbol_cache::ComputeGdtfSemanticFingerprintFromEntries(entries, error);
  assert(!withGeneratedBottom.empty() && error.empty() &&
         withGeneratedBottom != withLegacyBottom);
  entries.back() = entry("models/svg_bottom/base.svg", authored);
  const auto withLegacyBottomDirectory =
      symbol_cache::ComputeGdtfSemanticFingerprintFromEntries(entries, error);
  assert(!withLegacyBottomDirectory.empty() && error.empty() &&
         withLegacyBottomDirectory != standard);
  entries.back().bytes.assign(generated.begin(), generated.end());
  const auto withMarkedBottomDirectory =
      symbol_cache::ComputeGdtfSemanticFingerprintFromEntries(entries, error);
  assert(!withMarkedBottomDirectory.empty() && error.empty() &&
         withMarkedBottomDirectory != withLegacyBottomDirectory);

  entries.pop_back();
  constexpr std::array<const char *, 4> internalViews = {
      "top", "side", "front", "bottom"};
  for (const char *view : internalViews) {
    entries.push_back(entry(std::string("perastage/symbols/base/") + view +
                                ".svg",
                            authored));
    const auto internal =
        symbol_cache::ComputeGdtfSemanticFingerprintFromEntries(entries, error);
    assert(!internal.empty() && error.empty() && internal != standard);
    entries.back().bytes.assign(generated.begin(), generated.end());
    const auto markedInternal =
        symbol_cache::ComputeGdtfSemanticFingerprintFromEntries(entries, error);
    assert(!markedInternal.empty() && error.empty() &&
           markedInternal != internal);
    std::string changedContent = generated;
    changedContent.insert(changedContent.find("viewBox"),
                          "data-perastage-offset-x-mm=\"23\" ");
    entries.back().bytes.assign(changedContent.begin(), changedContent.end());
    const auto changedInternal =
        symbol_cache::ComputeGdtfSemanticFingerprintFromEntries(entries, error);
    assert(!changedInternal.empty() && error.empty() &&
           changedInternal != markedInternal);
    entries.pop_back();
  }
}
} // namespace

// Supplies the production loader symbol while focused tests inject their
// loader.
bool LoadPerastageSvgSymbolFromGdtf(const std::string &, SymbolViewKind,
                                    PerastageSvgSymbolData &, std::string *, FixtureSymbolResolutionPurpose) {
  return false;
}

// Runs managed fixture SVG cache regression coverage.
int main() {
  TestMissMutationAndInvalidation();
  TestStructuredKeysAndSafeHandles();
  TestSymbolProvenanceSemanticFingerprint();
  return 0;
}

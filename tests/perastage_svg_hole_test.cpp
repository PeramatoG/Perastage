#include "symbols/Symbol2DSvg.h"
#include "symbols/PerastageSvgSymbol.h"
#include "support/gdtf_test_fixture_builder.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <locale>
#include <string>

#include <tinyxml2.h>
#include <wx/init.h>

namespace {

struct ArchiveScope {
  std::filesystem::path path = std::filesystem::temp_directory_path() /
      ("perastage_svg_holes_" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()) + ".gdtf");
  ~ArchiveScope() {
    std::error_code error;
    std::filesystem::remove(path, error);
  }
};

symbols::Symbol2D MakeSymbol(size_t holes) {
  symbols::Symbol2D symbol;
  symbol.bounds = {{-10, -20}, {90, 80}, true};
  symbol.fill.push_back({{{-10, -20}, {90, -20}, {90, 80}, {-10, 80}}, {}});
  for (size_t i = 0; i < holes; ++i) {
    const float x = 10 + 30 * static_cast<float>(i);
    // Match the outer winding to prove even-odd filling is winding-independent.
    symbol.fill.front().holes.push_back({{x, 0}, {x + 10, 0},
                                         {x + 10, 10}, {x, 10}});
  }
  return symbol;
}

std::string Serialize(const symbols::Symbol2D &symbol) {
  std::string svg, error;
  assert(symbols::SerializeSymbolToSvg(symbol, svg, error));
  assert(error.empty());
  std::string repeated;
  assert(symbols::SerializeSymbolToSvg(symbol, repeated, error));
  assert(svg == repeated);
  return svg;
}

// Uses the production archive reader, including provenance and offset handling.
void CheckRoundTrip(const symbols::Symbol2D &symbol) {
  const std::string svg = Serialize(symbol);
  assert(svg.find("fill=\"#ffffff\"") == std::string::npos);
  assert(svg.find("fill-rule=\"evenodd\"") != std::string::npos);
  tinyxml2::XMLDocument document;
  assert(document.Parse(svg.c_str()) == tinyxml2::XML_SUCCESS);
  auto *root = document.FirstChildElement("svg");
  assert(std::string(root->Attribute("version")) == "1.1");
  assert(std::string(root->Attribute("viewBox")) == "0 0 100 100");
  root->SetAttribute(kPerastageSymbolVersionAttribute,
                     kCurrentPerastageSymbolResourceVersion);
  root->SetAttribute(kPerastageSymbolOffsetXAttribute, 10.0);
  root->SetAttribute(kPerastageSymbolOffsetYAttribute, 20.0);
  size_t shapeCount = 0, strokeCount = 0;
  for (auto *element = root->FirstChildElement(); element;
       element = element->NextSiblingElement()) {
    const std::string name = element->Name();
    if (name == "polyline") {
      ++strokeCount;
      assert(std::string(element->Attribute("fill")) == "none");
      assert(std::string(element->Attribute("stroke")) == "#000000");
      assert(element->FloatAttribute("stroke-width") == symbol.strokeWidthPx);
    } else {
      ++shapeCount;
      assert(std::string(element->Attribute("fill")) == "#e0e0e0");
      assert(std::string(element->Attribute("stroke")) == "none");
    }
  }
  assert(shapeCount == symbol.fill.size());
  assert(strokeCount == symbol.strokes.size());
  tinyxml2::XMLPrinter printer;
  document.Print(&printer);
  ArchiveScope archive;
  auto builder = tests::gdtf::BuildMinimalValidFixture().WithModelResource("main");
  for (auto view : {SymbolViewKind::Top, SymbolViewKind::Bottom,
                    SymbolViewKind::Front, SymbolViewKind::Left})
    builder.WithArchiveEntry(BuildPerastageFixtureSymbolPath("main", view),
                             printer.CStr());
  builder.WriteArchive(archive.path);
  FixtureSymbolResourceInspection inspection;
  assert(InspectFixtureSymbolResources(archive.path.string(), inspection));
  assert(inspection.perastageViewsUsable);
  for (auto view : {SymbolViewKind::Top, SymbolViewKind::Bottom,
                    SymbolViewKind::Front, SymbolViewKind::Left}) {
    PerastageSvgSymbolData parsed;
    assert(LoadPerastageSvgSymbolFromGdtf(
        archive.path.string(), view, parsed, nullptr,
        FixtureSymbolResolutionPurpose::InternalRendering));
    assert(parsed.viewKind == view && !parsed.usedViewFallback);
    assert(parsed.provenance == FixtureSymbolProvenance::GeneratedPerastage);
    assert(parsed.resourceSet == FixtureSymbolResourceSet::Perastage);
    assert(parsed.sourcePath == BuildPerastageFixtureSymbolPath("main", view));
    assert(parsed.viewBoxWidth == 100 && parsed.viewBoxHeight == 100);
    assert(parsed.offsetXmm == 10 && parsed.offsetYmm == 20);
    assert(parsed.fills.size() == symbol.fill.size());
    auto checkRing = [&](const symbols::Polyline2D &before,
                         const std::vector<PerastageSvgPoint> &after) {
      assert(before.size() == after.size());
      for (size_t i = 0; i < before.size(); ++i) {
        assert(after[i].x == before[i].x - symbol.bounds.min.x);
        assert(after[i].y == symbol.bounds.max.y - before[i].y);
      }
    };
    for (size_t i = 0; i < symbol.fill.size(); ++i) {
      checkRing(symbol.fill[i].outer, parsed.fills[i].points);
      assert(parsed.fills[i].holes.size() == symbol.fill[i].holes.size());
      for (size_t h = 0; h < symbol.fill[i].holes.size(); ++h)
        checkRing(symbol.fill[i].holes[h], parsed.fills[i].holes[h]);
    }
    assert(parsed.strokes.size() == symbol.strokes.size());
    for (size_t i = 0; i < symbol.strokes.size(); ++i)
      checkRing(symbol.strokes[i], parsed.strokes[i].points);
  }
}

void CheckLegacyAndMalformedPaths() {
  ArchiveScope archive;
  const std::string legacy =
      "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100' "
      "data-perastage-symbol-version='1' data-perastage-offset-x-mm='10' "
      "data-perastage-offset-y-mm='20'><g>"
      "<polygon points='0,0 100,0 100,100 0,100' fill='#e0e0e0'/>"
      "<polygon points='20,20 30,20 30,30 20,30' fill='#ffffff'/>"
      "<polygon points='50,50 60,50 60,60 50,60' style='fill: white;'/>"
      "<polyline points='0,0 10,10' fill='none' stroke='#000000'/></g></svg>";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("main")
      .WithArchiveEntry(BuildPerastageFixtureSymbolPath("main", SymbolViewKind::Top),
                         legacy).WriteArchive(archive.path);
  PerastageSvgSymbolData parsed;
  assert(LoadPerastageSvgSymbolFromGdtf(
      archive.path.string(), SymbolViewKind::Top, parsed, nullptr,
      FixtureSymbolResolutionPurpose::InternalRendering));
  assert(parsed.fills.size() == 1 && parsed.fills[0].holes.size() == 2);
  assert(parsed.fills[0].holes[0][0].x == 20);
  assert(parsed.fills[0].holes[1][0].x == 50);
  assert(parsed.strokes.size() == 1);
  assert(parsed.offsetXmm == 10 && parsed.offsetYmm == 20);
  assert(parsed.provenance == FixtureSymbolProvenance::GeneratedPerastage);

  // Older derivatives stored the same masks at the official Top resource path.
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("main")
      .WithArchiveEntry("models/svg/main.svg", legacy).WriteArchive(archive.path);
  assert(LoadPerastageSvgSymbolFromGdtf(
      archive.path.string(), SymbolViewKind::Top, parsed, nullptr,
      FixtureSymbolResolutionPurpose::InternalRendering));
  assert(parsed.sourcePath == "models/svg/main.svg");
  assert(parsed.provenance == FixtureSymbolProvenance::LegacyPerastage);
  assert(parsed.fills.size() == 1 && parsed.fills[0].holes.size() == 2);
  assert(parsed.fills[0].holes[0][0].x == 20);
  assert(parsed.fills[0].holes[1][0].x == 50);
  assert(parsed.strokes.size() == 1);

  // The limited Perastage path grammar must not change authored SVG recovery.
  const std::string authored =
      "<svg viewBox='0 0 100 100'><polygon points='0,0 100,0 0,100'/>"
      "<path fill-rule='evenodd' d='M 0,0 C 10,0 0,10 0,0 Z'/></svg>";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("main")
      .WithArchiveEntry("models/svg/main.svg", authored).WriteArchive(archive.path);
  assert(LoadPerastageSvgSymbolFromGdtf(
      archive.path.string(), SymbolViewKind::Top, parsed, nullptr,
      FixtureSymbolResolutionPurpose::StandardGdtf));
  assert(parsed.provenance == FixtureSymbolProvenance::AuthoredGdtf);
  assert(parsed.fills.size() == 1 && parsed.fills[0].holes.empty());

  // Malformed emitted-subset paths must not become partial, usable symbols.
  for (const char *data : {"", "M 0,0 L 10,0 Z", "M 0,0 L 10,0 L 0,10",
                           "M 0,0 C 10,0 0,10 0,0 Z",
                           "M 0,0 L 10,0 L 0,10 Z M 1,1 L nan,1 L 1,2 Z",
                           "M 0,0 L 10,0 L 0,10 Z junk"}) {
    const std::string invalid =
        "<svg viewBox='0 0 10 10' data-perastage-symbol-version='1'>"
        "<polyline points='0,0 10,10'/><path fill-rule='evenodd' d='" +
        std::string(data) + "'/></svg>";
    tests::gdtf::BuildMinimalValidFixture().WithModelResource("main")
        .WithArchiveEntry(BuildPerastageFixtureSymbolPath("main", SymbolViewKind::Top),
                           invalid).WriteArchive(archive.path);
    assert(!LoadPerastageSvgSymbolFromGdtf(
        archive.path.string(), SymbolViewKind::Top, parsed, nullptr,
        FixtureSymbolResolutionPurpose::InternalRendering));
  }
}

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());
  CheckRoundTrip(MakeSymbol(1));
  CheckRoundTrip(MakeSymbol(2));
  auto independent = MakeSymbol(1);
  independent.fill.front().outer = {{-10, -20}, {30, -20}, {30, 80}, {-10, 80}};
  independent.fill.insert(independent.fill.begin(),
                          {{{60, -10}, {70, -10}, {70, 0}}, {}});
  independent.fill.push_back({{{60, 50}, {70, 50}, {70, 60}}, {}});
  CheckRoundTrip(independent);
  independent.strokes = {{{-10, -20}, {90, 80}}, {{10, 0}, {20, 10}, {10, 0}}};
  independent.strokeWidthPx = 1.25f;
  independent.fill[1].holes[0][0] = {10.25f, 0.5f};
  CheckRoundTrip(independent);
  struct DecimalComma : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
  };
  const auto expected = Serialize(independent);
  const auto previousLocale = std::locale();
  std::locale::global(std::locale(previousLocale, new DecimalComma));
  assert(Serialize(independent) == expected);
  std::locale::global(previousLocale);
  CheckLegacyAndMalformedPaths();
  return 0;
}

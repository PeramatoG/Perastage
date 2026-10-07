#include "fixture_symbol_source.h"
#include "symbols/fixture_symbol_availability.h"

#include <cassert>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <utility>

#include <wx/filename.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

namespace {

// Creates a temporary GDTF archive from one fixture-type XML body.
std::string MakeGdtf(const std::string &fixtureBody,
                     std::initializer_list<const char *> storedSymbolEntries = {},
                     std::initializer_list<std::pair<std::string, std::string>>
                         additionalResources = {}) {
  wxFileName temporary(wxFileName::CreateTempFileName("symbol_source_"));
  const std::string path = temporary.GetFullPath().ToStdString() + ".gdtf";
  wxRemoveFile(temporary.GetFullPath());
  wxFFileOutputStream file(path);
  wxZipOutputStream archive(file);
  archive.PutNextEntry("description.xml");
  const std::string xml =
      "<?xml version=\"1.0\"?><GDTF DataVersion=\"1.2\"><FixtureType "
      "Name=\"SourceTest\">" +
      fixtureBody + "</FixtureType></GDTF>";
  archive.Write(xml.data(), xml.size());
  if (storedSymbolEntries.size() != 0) {
    const std::string svg =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10 10\">"
        "<polygon points=\"0,0 10,0 10,10\"/></svg>";
    for (const char *entry : storedSymbolEntries) {
      archive.PutNextEntry(entry);
      archive.Write(svg.data(), svg.size());
    }
  }
  for (const auto &[entry, svg] : additionalResources) {
    archive.PutNextEntry(entry);
    archive.Write(svg.data(), svg.size());
  }
  archive.Close();
  return path;
}

// Verifies mode-specific classification without treating runtime fallback as geometry.
int RunClassificationChecks() {
  const std::string placeholder = MakeGdtf(
      "<Models><Model Name=\"Base\"/></Models><Geometries><Geometry "
      "Name=\"Root\" Model=\"Base\"/></Geometries><DMXModes><DMXMode "
      "Name=\"Standard\" Geometry=\"Root\"/></DMXModes>");
  const auto placeholderResult =
      symbols::InspectFixtureSymbolSource(placeholder, "Standard");
  assert(placeholderResult.source ==
         symbols::FixtureSymbolSource::PerastageFallback);
  assert(!placeholderResult.renderableGeometry);

  const std::string stored = MakeGdtf(
      "<Models><Model Name=\"Base\"/></Models><Geometries><Geometry "
      "Name=\"Root\" Model=\"Base\"/></Geometries><DMXModes><DMXMode "
      "Name=\"Standard\" Geometry=\"Root\"/></DMXModes>",
      {"models/svg/Base.svg", "models/svg_front/Base.svg",
       "models/svg_side/Base.svg"});
  const auto storedResult =
      symbols::InspectFixtureSymbolSource(stored, "Standard");
  assert(storedResult.source == symbols::FixtureSymbolSource::StoredGdtfSvg);
  const auto bottomFallback =
      symbol_cache::LoadUsableFixtureSymbol(stored, SymbolViewKind::Bottom);
  assert(bottomFallback && bottomFallback->viewKind == SymbolViewKind::Top);
  assert(bottomFallback->sourcePath == "models/svg/Base.svg");
  assert(bottomFallback->provenance == FixtureSymbolProvenance::AuthoredGdtf);
  assert(bottomFallback->usedViewFallback);

  const std::string topOnly = MakeGdtf(
      "<Models><Model Name=\"Base\"/></Models><Geometries><Geometry "
      "Name=\"Root\" Model=\"Base\"/></Geometries><DMXModes><DMXMode "
      "Name=\"Standard\" Geometry=\"Root\"/></DMXModes>",
      {"models/svg/Base.svg"});
  assert(symbols::InspectFixtureSymbolSource(topOnly, "Standard").source ==
         symbols::FixtureSymbolSource::PerastageFallback);
  const auto authoredTop =
      symbol_cache::LoadUsableFixtureSymbol(topOnly, SymbolViewKind::Top);
  assert(authoredTop && authoredTop->sourcePath == "models/svg/Base.svg");
  assert(authoredTop->provenance == FixtureSymbolProvenance::AuthoredGdtf);
  assert(!authoredTop->usedViewFallback);
  const auto frontFallback =
      symbol_cache::LoadUsableFixtureSymbol(topOnly, SymbolViewKind::Front);
  assert(frontFallback && frontFallback->viewKind == SymbolViewKind::Top);
  assert(frontFallback->provenance == FixtureSymbolProvenance::AuthoredGdtf);
  assert(frontFallback->usedViewFallback);

  const std::string topSide = MakeGdtf(
      "<Models><Model Name=\"Base\"/></Models><Geometries><Geometry "
      "Name=\"Root\" Model=\"Base\"/></Geometries><DMXModes><DMXMode "
      "Name=\"Standard\" Geometry=\"Root\"/></DMXModes>",
      {"models/svg/Base.svg", "models/svg_side/Base.svg"});
  assert(symbols::InspectFixtureSymbolSource(topSide, "Standard").source ==
         symbols::FixtureSymbolSource::PerastageFallback);

  const std::string bottomOnly = MakeGdtf(
      "<Models><Model Name=\"Base\"/></Models><Geometries><Geometry "
      "Name=\"Root\" Model=\"Base\"/></Geometries><DMXModes><DMXMode "
      "Name=\"Standard\" Geometry=\"Root\"/></DMXModes>",
      {"models/svg/Base_bottom.svg"});
  assert(symbols::InspectFixtureSymbolSource(bottomOnly, "Standard").source ==
         symbols::FixtureSymbolSource::PerastageFallback);

  const std::string generatedTop =
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 20 10\" "
      "data-perastage-symbol-version=\"1\" data-perastage-offset-x-mm=\"-7\" "
      "data-perastage-offset-y-mm=\"8\">"
      "<polygon points=\"0,0 20,0 20,10\"/></svg>";
  const std::string generatedFront =
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 30 10\" "
      "data-perastage-symbol-version=\"1\" data-perastage-offset-x-mm=\"-9\" "
      "data-perastage-offset-y-mm=\"10\">"
      "<polygon points=\"0,0 30,0 30,10\"/></svg>";
  const std::string mixedResources = MakeGdtf(
      "<Models><Model Name=\"Base\" SVGOffsetX=\"101\" SVGOffsetY=\"102\" "
      "SVGFrontOffsetX=\"103\" SVGFrontOffsetY=\"104\"/></Models>"
      "<Geometries><Geometry Name=\"Root\" Model=\"Base\"/></Geometries>"
      "<DMXModes><DMXMode Name=\"Standard\" Geometry=\"Root\"/></DMXModes>",
      {"models/svg/Base.svg", "models/svg_front/Base.svg",
       "models/svg_side/Base.svg"},
      {{"perastage/symbols/Base/top.svg", generatedTop},
       {"perastage/symbols/Base/front.svg", generatedFront}});
  const auto internalFront =
      symbol_cache::LoadUsableFixtureSymbol(mixedResources, SymbolViewKind::Front);
  assert(internalFront && internalFront->sourcePath ==
                              "perastage/symbols/Base/front.svg");
  assert(internalFront->provenance == FixtureSymbolProvenance::GeneratedPerastage);
  assert(internalFront->viewBoxWidth == 30.0);
  assert(internalFront->offsetXmm == -9.0 && internalFront->offsetYmm == 10.0);
  assert(!internalFront->usedViewFallback);
  const auto internalTopFallback =
      symbol_cache::LoadUsableFixtureSymbol(mixedResources, SymbolViewKind::Bottom);
  assert(internalTopFallback && internalTopFallback->sourcePath ==
                                    "perastage/symbols/Base/top.svg");
  assert(internalTopFallback->provenance ==
         FixtureSymbolProvenance::GeneratedPerastage);
  assert(internalTopFallback->viewKind == SymbolViewKind::Top);
  assert(internalTopFallback->offsetXmm == -7.0 &&
         internalTopFallback->offsetYmm == 8.0);
  assert(internalTopFallback->usedViewFallback);

  const std::string invalidInternal = MakeGdtf(
      "<Models><Model Name=\"Base\"/></Models>",
      {"models/svg_front/Base.svg"},
      {{"perastage/symbols/Base/front.svg", "<svg>"}});
  const auto authoredFrontFallback = symbol_cache::LoadUsableFixtureSymbol(
      invalidInternal, SymbolViewKind::Front);
  assert(authoredFrontFallback &&
         authoredFrontFallback->sourcePath == "models/svg_front/Base.svg");
  assert(authoredFrontFallback->resourceSet == FixtureSymbolResourceSet::StandardGdtf);
  assert(authoredFrontFallback->provenance == FixtureSymbolProvenance::AuthoredGdtf);
  assert(!authoredFrontFallback->usedViewFallback);

  const std::string legacyAlternate = MakeGdtf(
      "<Models><Model Name=\"Base\"/></Models>", {},
      {{"models/svg/Base_bottom.svg", "<svg>"},
       {"models/svg_bottom/Base.svg", generatedTop}});
  const auto alternateBottom = symbol_cache::LoadUsableFixtureSymbol(
      legacyAlternate, SymbolViewKind::Bottom);
  assert(alternateBottom && alternateBottom->sourcePath == "models/svg_bottom/Base.svg");
  assert(alternateBottom->provenance == FixtureSymbolProvenance::LegacyPerastage);
  assert(alternateBottom->resourceSet == FixtureSymbolResourceSet::Perastage);
  assert(!alternateBottom->usedViewFallback);

  const std::string primitive = MakeGdtf(
      "<Models><Model Name=\"Base\" PrimitiveType=\"Cube\"/></Models>"
      "<Geometries><Geometry Name=\"Root\" Model=\"Base\"/></Geometries>"
      "<DMXModes><DMXMode Name=\"Standard\" Geometry=\"Root\"/>"
      "</DMXModes>");
  const auto primitiveResult =
      symbols::InspectFixtureSymbolSource(primitive, "Standard");
  assert(primitiveResult.source ==
         symbols::FixtureSymbolSource::RenderableGdtfGeometry);

  const std::string dimensions = MakeGdtf(
      "<Models><Model Name=\"Base\" Length=\"0.4\" Width=\"0.3\" "
      "Height=\"0.2\"/></Models><Geometries><Geometry Name=\"Root\" "
      "Model=\"Base\"/></Geometries><DMXModes><DMXMode Name=\"Standard\" "
      "Geometry=\"Root\"/></DMXModes>");
  assert(symbols::InspectFixtureSymbolSource(dimensions, "Standard").source ==
         symbols::FixtureSymbolSource::RenderableGdtfGeometry);

  const std::string brokenFile = MakeGdtf(
      "<Models><Model Name=\"Base\" File=\"missing\"/></Models>"
      "<Geometries><Geometry Name=\"Root\" Model=\"Base\"/></Geometries>"
      "<DMXModes><DMXMode Name=\"Standard\" Geometry=\"Root\"/>"
      "</DMXModes>");
  assert(symbols::InspectFixtureSymbolSource(brokenFile, "Standard").source ==
         symbols::FixtureSymbolSource::PerastageFallback);

  const std::string modes = MakeGdtf(
      "<Models><Model Name=\"Real\" PrimitiveType=\"Cube\"/><Model "
      "Name=\"Empty\"/></Models><Geometries><Geometry Name=\"RealRoot\" "
      "Model=\"Real\"/><Geometry Name=\"EmptyRoot\" Model=\"Empty\"/>"
      "</Geometries><DMXModes><DMXMode Name=\"RealMode\" "
      "Geometry=\"RealRoot\"/><DMXMode Name=\"EmptyMode\" "
      "Geometry=\"EmptyRoot\"/></DMXModes>");
  assert(symbols::InspectFixtureSymbolSource(modes, "RealMode").source ==
         symbols::FixtureSymbolSource::RenderableGdtfGeometry);
  assert(symbols::InspectFixtureSymbolSource(modes, "EmptyMode").source ==
         symbols::FixtureSymbolSource::PerastageFallback);

  for (const auto &path :
       {placeholder, stored, topOnly, topSide, bottomOnly, mixedResources,
        invalidInternal, legacyAlternate,
        primitive, dimensions, brokenFile, modes}) {
    std::error_code error;
    std::filesystem::remove(path, error);
  }
  return 0;
}

} // namespace

// Runs fixture symbol source regression checks.
int main() { return RunClassificationChecks(); }

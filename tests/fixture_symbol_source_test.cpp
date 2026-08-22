#include "fixture_symbol_source.h"

#include <cassert>
#include <filesystem>
#include <string>

#include <wx/filename.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

namespace {

// Creates a temporary GDTF archive from one fixture-type XML body.
std::string MakeGdtf(const std::string &fixtureBody,
                     bool includeStoredSymbols = false) {
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
  if (includeStoredSymbols) {
    const std::string svg =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10 10\">"
        "<polygon points=\"0,0 10,0 10,10\"/></svg>";
    for (const std::string entry : {"models/svg/Base.svg",
                                    "models/svg/Base_bottom.svg",
                                    "models/svg_front/Base.svg",
                                    "models/svg_side/Base.svg"}) {
      archive.PutNextEntry(entry);
      archive.Write(svg.data(), svg.size());
    }
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
      true);
  const auto storedResult =
      symbols::InspectFixtureSymbolSource(stored, "Standard");
  assert(storedResult.source == symbols::FixtureSymbolSource::StoredGdtfSvg);

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
       {placeholder, stored, primitive, dimensions, brokenFile, modes}) {
    std::error_code error;
    std::filesystem::remove(path, error);
  }
  return 0;
}

} // namespace

// Runs fixture symbol source regression checks.
int main() { return RunClassificationChecks(); }

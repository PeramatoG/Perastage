#include "support/gdtf_test_fixture_builder.h"
#include <cassert>
#include <filesystem>
#include <memory>
#include <string>
#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
namespace fs = std::filesystem;
// Reads description.xml from a generated GDTF archive.
static std::string ReadDescription(const fs::path &archivePath) {
  wxFileInputStream input(archivePath.generic_string()); assert(input.IsOk()); wxZipInputStream zip(input); std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) { if (entry->GetName().ToStdString() != "description.xml") continue; std::string content; char buffer[1024]; while (zip.Read(buffer, sizeof(buffer)).LastRead() > 0) content.append(buffer, zip.LastRead()); return content; }
  return {};
}
// Verifies the reusable builder creates deterministic minimal GDTF archives.
int main() {
  wxInitializer initializer; assert(initializer.IsOk()); const fs::path root = fs::temp_directory_path() / "perastage_gdtf_builder_test"; fs::remove_all(root); fs::create_directories(root);
  const fs::path archiveA = root / "a.gdtf"; const fs::path archiveB = root / "b.gdtf"; tests::gdtf::BuildMinimalValidFixture().WriteArchive(archiveA); tests::gdtf::BuildMinimalValidFixture().WriteArchive(archiveB);
  const std::string xmlA = ReadDescription(archiveA); const std::string xmlB = ReadDescription(archiveB); assert(xmlA == xmlB);
  tinyxml2::XMLDocument doc; assert(doc.Parse(xmlA.c_str()) == tinyxml2::XML_SUCCESS); auto *fixtureType = doc.FirstChildElement("GDTF")->FirstChildElement("FixtureType");
  assert(std::string(fixtureType->Attribute("FixtureTypeID")) == tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId); assert(fixtureType->FirstChildElement("AttributeDefinitions") != nullptr); assert(fixtureType->FirstChildElement("Geometries") != nullptr); assert(fixtureType->FirstChildElement("DMXModes") != nullptr); fs::remove_all(root);
}

#include "support/gdtf_test_fixture_builder.h"
#include "wx_path_utils.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;

// Removes a temporary directory when the test scope exits.
class ScopedTempDir {
public:
  explicit ScopedTempDir(std::string name)
      : path(fs::temp_directory_path() / (name + "_" + std::to_string(std::rand()))) {
    fs::remove_all(path);
    fs::create_directories(path);
  }

  ~ScopedTempDir() { fs::remove_all(path); }

  fs::path path;
};

// Reads all bytes from a file.
static std::string ReadBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  assert(input.is_open());
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

// Reads entries from a generated GDTF archive.
static std::vector<std::pair<std::string, std::string>> ReadEntries(const fs::path &archivePath) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(archivePath));
  assert(input.IsOk());

  wxZipInputStream zip(input);
  std::vector<std::pair<std::string, std::string>> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    std::string content;
    char buffer[1024];
    while (zip.Read(buffer, sizeof(buffer)).LastRead() > 0) {
      content.append(buffer, zip.LastRead());
    }
    entries.emplace_back(entry->GetName().ToStdString(), content);
  }
  return entries;
}

// Verifies the generated XML describes a standards-valid one-address fixture.
static void VerifyOneAddressFixtureXml(const std::string &xml) {
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(xml.c_str()) == tinyxml2::XML_SUCCESS);

  auto *gdtf = doc.FirstChildElement("GDTF");
  assert(gdtf != nullptr);
  assert(std::string(gdtf->Attribute("DataVersion")) == "1.2");

  auto *fixtureType = gdtf->FirstChildElement("FixtureType");
  assert(fixtureType != nullptr);
  assert(std::string(fixtureType->Attribute("FixtureTypeID")) ==
         tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId);
  assert(fixtureType->FirstChildElement("AttributeDefinitions") != nullptr);
  assert(fixtureType->FirstChildElement("Wheels") != nullptr);
  assert(fixtureType->FirstChildElement("PhysicalDescriptions") != nullptr);
  assert(fixtureType->FirstChildElement("Models") != nullptr);
  assert(fixtureType->FirstChildElement("Geometries") != nullptr);

  auto *dmxMode = fixtureType->FirstChildElement("DMXModes")->FirstChildElement("DMXMode");
  assert(dmxMode != nullptr);
  assert(std::string(dmxMode->Attribute("Geometry")) == "Root");

  auto *dmxChannel = dmxMode->FirstChildElement("DMXChannels")->FirstChildElement("DMXChannel");
  assert(dmxChannel != nullptr);
  assert(std::string(dmxChannel->Attribute("Offset")) == "1");
  assert(dmxChannel->NextSiblingElement("DMXChannel") == nullptr);
}

// Verifies the reusable builder creates deterministic minimal GDTF archives.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  ScopedTempDir root("perastage_gdtf_builder_test");
  const fs::path archiveA = root.path / "a.gdtf";
  const fs::path archiveB = root.path / "b.gdtf";

  tests::gdtf::BuildMinimalValidFixture().WriteArchive(archiveA);
  tests::gdtf::BuildMinimalValidFixture().WriteArchive(archiveB);

  assert(ReadBytes(archiveA) == ReadBytes(archiveB));

  const auto entries = ReadEntries(archiveA);
  assert(entries.size() == 1);
  assert(entries.front().first == "description.xml");
  VerifyOneAddressFixtureXml(entries.front().second);

  bool rejectedTraversal = false;
  try {
    tests::gdtf::BuildMinimalValidFixture().WriteArchive(root.path / "safe..name.gdtf");
  } catch (...) {
    rejectedTraversal = true;
  }
  assert(!rejectedTraversal);
}

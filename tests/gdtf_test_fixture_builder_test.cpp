#include "support/gdtf_test_fixture_builder.h"
#include "wx_path_utils.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <tinyxml2.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;

// Reports a test failure with context for CI logs.
static bool Fail(const std::string &message) {
  std::cerr << "ERROR: " << message << std::endl;
  return false;
}

// Creates a unique temporary directory for this test process.
static fs::path CreateUniqueTempDir(const std::string &prefix) {
  for (int attempt = 0; attempt < 100; ++attempt) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path candidate = fs::temp_directory_path() /
                         (prefix + "_" + std::to_string(stamp) + "_" +
                          std::to_string(attempt));
    std::error_code ec;
    if (fs::create_directories(candidate, ec) && !ec)
      return candidate;
  }
  return {};
}

// Removes a temporary directory when the test scope exits.
class ScopedTempDir {
public:
  explicit ScopedTempDir(const std::string &prefix) : path(CreateUniqueTempDir(prefix)) {}
  ~ScopedTempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  fs::path path;
};

// Reads all bytes from a file.
static std::string ReadBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open())
    return {};
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct ArchiveEntryInfo {
  std::string name;
  std::string content;
  wxDateTime timestamp;
  int mode = 0;
  int method = -1;
};

// Reads entries from a generated GDTF archive.
static std::vector<ArchiveEntryInfo> ReadEntries(const fs::path &archivePath) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(archivePath));
  if (!input.IsOk())
    return {};

  wxZipInputStream zip(input);
  std::vector<ArchiveEntryInfo> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    ArchiveEntryInfo info;
    info.name = entry->GetName().ToStdString();
    info.timestamp = entry->GetDateTime();
    info.mode = entry->GetMode();
    info.method = entry->GetMethod();
    char buffer[1024];
    while (zip.Read(buffer, sizeof(buffer)).LastRead() > 0)
      info.content.append(buffer, zip.LastRead());
    entries.push_back(std::move(info));
  }
  return entries;
}

// Verifies the generated XML describes a standards-valid one-address fixture.
static bool VerifyOneAddressFixtureXml(const std::string &xml) {
  tinyxml2::XMLDocument doc;
  if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS)
    return Fail("Generated description.xml did not parse");
  auto *gdtf = doc.FirstChildElement("GDTF");
  if (!gdtf || std::string(gdtf->Attribute("DataVersion")) != "1.2")
    return Fail("Generated fixture DataVersion is not 1.2");
  auto *fixtureType = gdtf->FirstChildElement("FixtureType");
  if (!fixtureType || std::string(fixtureType->Attribute("FixtureTypeID")) !=
                          tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId)
    return Fail("Generated fixture has the wrong FixtureTypeID");
  for (const char *section : {"AttributeDefinitions", "Wheels", "PhysicalDescriptions",
                              "Models", "Geometries", "DMXModes"}) {
    if (!fixtureType->FirstChildElement(section))
      return Fail(std::string("Generated fixture is missing section: ") + section);
  }
  auto *dmxMode = fixtureType->FirstChildElement("DMXModes")->FirstChildElement("DMXMode");
  if (!dmxMode || std::string(dmxMode->Attribute("Geometry")) != "Root")
    return Fail("Generated DMX mode does not reference Root geometry");
  auto *dmxChannel = dmxMode->FirstChildElement("DMXChannels")->FirstChildElement("DMXChannel");
  if (!dmxChannel || std::string(dmxChannel->Attribute("Offset")) != "1" ||
      dmxChannel->NextSiblingElement("DMXChannel"))
    return Fail("Generated fixture is not exactly one physical DMX address");
  return true;
}

// Verifies model dimensions use deterministic decimal serialization.
static bool VerifyModelDimensions() {
  const std::string xml = tests::gdtf::BuildMinimalValidFixture()
                              .WithModelDimensionsMeters(3.0f, 0.4f, 0.4f)
                              .BuildDescriptionXml();
  tinyxml2::XMLDocument doc;
  if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS)
    return Fail("Dimension fixture XML did not parse");
  const auto *model = doc.FirstChildElement("GDTF")
                          ->FirstChildElement("FixtureType")
                          ->FirstChildElement("Models")
                          ->FirstChildElement("Model");
  if (!model || std::string(model->Attribute("Length")) != "3.0" ||
      std::string(model->Attribute("Width")) != "0.4" ||
      std::string(model->Attribute("Height")) != "0.4")
    return Fail("Model dimensions were not serialized deterministically");
  return true;
}

// Verifies the test-support archive path validator accepts and rejects expected paths.
static bool VerifyArchivePathValidation(const fs::path &root) {
  for (const std::string &path : {"safe..name.png", "models/safe..name.glb"}) {
    if (!tests::gdtf::IsPortableArchiveEntryPathForTesting(path))
      return Fail("Expected archive entry path to be accepted: " + path);
    tests::gdtf::WriteArchiveEntryForTesting(root / (path + ".gdtf"), path);
  }
  for (const std::string &path : {"../escape.xml", "models/../escape.glb", "/absolute.xml",
                                  "C:/absolute.xml", "models\\escape.glb", "", ".",
                                  "models//empty.glb", "models/./dot.glb"}) {
    if (tests::gdtf::IsPortableArchiveEntryPathForTesting(path))
      return Fail("Expected archive entry path to be rejected: " + path);
  }
  return true;
}

// Verifies the reusable builder creates deterministic minimal GDTF archives.
int main() {
  wxInitializer initializer;
  if (!initializer.IsOk())
    return Fail("wxWidgets initialization failed") ? 0 : 1;

  ScopedTempDir root("perastage_gdtf_builder_test");
  if (root.path.empty())
    return Fail("Could not create a unique temporary directory") ? 0 : 1;
  const fs::path archiveA = root.path / "a.gdtf";
  const fs::path archiveB = root.path / "b.gdtf";

  tests::gdtf::BuildMinimalValidFixture().WriteArchive(archiveA);
  tests::gdtf::BuildMinimalValidFixture().WriteArchive(archiveB);
  if (ReadBytes(archiveA) != ReadBytes(archiveB))
    return Fail("Generated archives are not byte-identical") ? 0 : 1;

  const auto entries = ReadEntries(archiveA);
  if (entries.size() != 1 || entries.front().name != "description.xml")
    return Fail("Generated archive does not contain exactly one root description.xml") ? 0 : 1;
  if (!entries.front().timestamp.IsEqualTo(wxDateTime(1, wxDateTime::Jan, 2026, 0, 0, 0)))
    return Fail("Generated archive entry timestamp is not fixed") ? 0 : 1;
  if ((entries.front().mode & 0777) != 0644)
    return Fail("Generated archive entry permissions are not fixed") ? 0 : 1;
  if (!VerifyOneAddressFixtureXml(entries.front().content))
    return 1;
  if (!VerifyModelDimensions())
    return 1;
  const std::string glb = tests::gdtf::BuildMinimalValidGlb();
  if (glb.size() != 48 || glb.substr(0, 4) != "glTF")
    return Fail("Minimal GLB payload has an invalid header or length") ? 0 : 1;
  if (!VerifyArchivePathValidation(root.path))
    return 1;
  return 0;
}

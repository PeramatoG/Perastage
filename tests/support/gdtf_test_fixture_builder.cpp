#include "gdtf_test_fixture_builder.h"
#include "wx_path_utils.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include <wx/datetime.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace tests::gdtf {
namespace {

struct ArchiveEntry {
  std::string path;
  std::string bytes;
};

// Returns true when the path has a Windows drive prefix.
bool HasDrivePrefix(const std::string &path) {
  return path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) &&
         path[1] == ':';
}

// Rejects archive paths that are not portable GDTF-relative paths.
void ValidateArchivePath(const std::string &path) {
  if (path.empty() || path.front() == '/' || path.find('\\') != std::string::npos ||
      HasDrivePrefix(path)) {
    throw std::invalid_argument("GDTF archive entry path is not portable: " + path);
  }

  std::stringstream stream(path);
  std::string component;
  while (std::getline(stream, component, '/')) {
    if (component.empty() || component == "." || component == "..") {
      throw std::invalid_argument("GDTF archive entry path is not portable: " + path);
    }
  }
}

// Creates fixed ZIP metadata for deterministic test archives.
wxZipEntry *CreateDeterministicEntry(const std::string &path) {
  auto *entry = new wxZipEntry(wxString::FromUTF8(path));
  wxDateTime timestamp(1, wxDateTime::Jan, 2026, 0, 0, 0);
  entry->SetDateTime(timestamp);
  entry->SetMode(0644);
  return entry;
}

// Writes a deterministic archive from ordered entries.
void WriteArchiveEntries(const std::filesystem::path &archivePath,
                         std::vector<ArchiveEntry> entries) {
  std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    return left.path < right.path;
  });

  std::filesystem::create_directories(archivePath.parent_path());
  wxFileOutputStream output(WxPathUtils::WxStringFromFilesystemPath(archivePath));
  if (!output.IsOk()) {
    throw std::runtime_error("Could not open GDTF archive for writing: " +
                             archivePath.string());
  }

  wxZipOutputStream zip(output, 0);
  for (const auto &entry : entries) {
    ValidateArchivePath(entry.path);
    if (!zip.PutNextEntry(CreateDeterministicEntry(entry.path))) {
      throw std::runtime_error("Could not create GDTF archive entry: " + entry.path);
    }
    zip.Write(entry.bytes.data(), entry.bytes.size());
  }
  zip.Close();
}

} // namespace

// Initializes a deterministic minimal GDTF 1.2 builder.
FixtureBuilder::FixtureBuilder()
    : modeName("Default"), modeGeometry("Root"),
      fixtureName("Perastage Minimal 1ch"), manufacturer("Perastage"),
      fixtureTypeId(kMinimalFixtureTypeId) {}

// Builds a deterministic minimal GDTF 1.2 fixture.
FixtureBuilder BuildMinimalValidFixture() {
  return FixtureBuilder{};
}

// Overrides the fixture identity fields while retaining a canonical structure.
FixtureBuilder &FixtureBuilder::WithFixtureIdentity(std::string name,
                                                    std::string maker,
                                                    std::string typeId) {
  fixtureName = std::move(name);
  manufacturer = std::move(maker);
  fixtureTypeId = std::move(typeId);
  return *this;
}

// Overrides the default DMX mode and geometry reference.
FixtureBuilder &FixtureBuilder::WithDmxMode(std::string name, std::string geometry) {
  modeName = std::move(name);
  modeGeometry = std::move(geometry);
  return *this;
}

// Assigns the standard archive resource basename referenced by the model.
FixtureBuilder &FixtureBuilder::WithModelResource(std::string fileBase) {
  modelFileBase = std::move(fileBase);
  return *this;
}

// Adds basic category signal metadata for category-related tests.
FixtureBuilder &FixtureBuilder::WithFixtureCategorySignals() {
  categorySignals = true;
  return *this;
}

// Adds an extra portable archive entry that must be preserved by rewrite tests.
FixtureBuilder &FixtureBuilder::WithArchiveEntry(std::string path, std::string bytes) {
  archiveEntries.emplace_back(std::move(path), std::move(bytes));
  return *this;
}

// Builds the description.xml payload for the fixture.
std::string FixtureBuilder::BuildDescriptionXml() const {
  const std::string category = categorySignals ? " FixtureTypeCategory=\"Conventional\"" : "";
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<GDTF DataVersion=\"1.2\">\n"
         "  <FixtureType Name=\"" + fixtureName + "\" ShortName=\"" + fixtureName +
         "\" Manufacturer=\"" + manufacturer +
         "\" Description=\"Minimal canonical GDTF 1.2 fixture for tests\" FixtureTypeID=\"" +
         fixtureTypeId + category +
         "\">\n"
         "    <AttributeDefinitions><ActivationGroups/><FeatureGroups><FeatureGroup Name=\"Dimmer\" Pretty=\"Dimmer\"><Feature Name=\"Dimmer\"/></FeatureGroup></FeatureGroups><Attributes><Attribute Name=\"Dimmer\" Pretty=\"Dimmer\" Feature=\"Dimmer.Dimmer\" PhysicalUnit=\"LuminousIntensity\"/></Attributes></AttributeDefinitions>\n"
         "    <Wheels/>\n"
         "    <PhysicalDescriptions><Emitters/><Filters/><ColorSpace Mode=\"sRGB\"/><DMXProfiles/><CRIs/><Connectors/></PhysicalDescriptions>\n"
         "    <Models><Model Name=\"Body\" Length=\"0.1\" Width=\"0.1\" Height=\"0.1\" PrimitiveType=\"Cube\"" +
         (modelFileBase.empty() ? std::string{} : " File=\"" + modelFileBase + "\"") +
         "/></Models>\n"
         "    <Geometries><Geometry Name=\"Root\" Model=\"Body\"/></Geometries>\n"
         "    <DMXModes><DMXMode Name=\"" +
         modeName + "\" Geometry=\"" + modeGeometry +
         "\"><DMXChannels><DMXChannel Offset=\"1\" Geometry=\"Root\"><LogicalChannel Attribute=\"Dimmer\"><ChannelFunction Name=\"Dimmer\" Attribute=\"Dimmer\" Default=\"0/1\" DMXFrom=\"0/1\"/></LogicalChannel></DMXChannel></DMXChannels></DMXMode></DMXModes>\n"
         "  </FixtureType>\n"
         "</GDTF>\n";
}

// Writes the fixture as a deterministic GDTF ZIP archive.
void FixtureBuilder::WriteArchive(const std::filesystem::path &archivePath) const {
  std::vector<ArchiveEntry> entries = {{"description.xml", BuildDescriptionXml()}};
  for (const auto &entry : archiveEntries)
    entries.push_back({entry.first, entry.second});
  WriteArchiveEntries(archivePath, std::move(entries));
}

// Writes an archive with intentionally missing mandatory sections.
void WriteMissingMandatorySectionsArchive(const std::filesystem::path &archivePath) {
  WriteArchiveEntries(archivePath,
                      {{"description.xml",
                        "<GDTF DataVersion=\"1.2\"><FixtureType FixtureTypeID=\"12345678-1234-4234-9234-123456789abc\"/></GDTF>"}});
}

// Writes an archive with an intentionally invalid FixtureTypeID.
void WriteInvalidGuidArchive(const std::filesystem::path &archivePath) {
  auto builder = FixtureBuilder{};
  auto xml = builder.BuildDescriptionXml();
  xml.replace(xml.find(FixtureBuilder::kMinimalFixtureTypeId),
              std::string(FixtureBuilder::kMinimalFixtureTypeId).size(), "not-a-guid");
  WriteArchiveEntries(archivePath, {{"description.xml", xml}});
}

// Writes an archive with intentionally malformed XML.
void WriteMalformedXmlArchive(const std::filesystem::path &archivePath) {
  WriteArchiveEntries(archivePath, {{"description.xml", "<GDTF DataVersion=\"1.2\"><FixtureType>"}});
}

// Returns whether a test archive entry path is portable and relative.
bool IsPortableArchiveEntryPathForTesting(const std::string &path) {
  try {
    ValidateArchivePath(path);
    return true;
  } catch (const std::invalid_argument &) {
    return false;
  }
}

// Writes a single test archive entry after applying test-support path validation.
void WriteArchiveEntryForTesting(const std::filesystem::path &archivePath,
                                 const std::string &entryPath) {
  WriteArchiveEntries(archivePath, {{entryPath, "test"}});
}

} // namespace tests::gdtf

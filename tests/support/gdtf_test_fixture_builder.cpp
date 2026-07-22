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
FixtureBuilder::FixtureBuilder() : modeName("Default"), modeGeometry("Root") {}

// Builds a deterministic minimal GDTF 1.2 fixture.
FixtureBuilder BuildMinimalValidFixture() {
  return FixtureBuilder{};
}

// Overrides the default DMX mode and geometry reference.
FixtureBuilder &FixtureBuilder::WithDmxMode(std::string name, std::string geometry) {
  modeName = std::move(name);
  modeGeometry = std::move(geometry);
  return *this;
}

// Adds basic category signal metadata for category-related tests.
FixtureBuilder &FixtureBuilder::WithFixtureCategorySignals() {
  categorySignals = true;
  return *this;
}

// Builds the description.xml payload for the fixture.
std::string FixtureBuilder::BuildDescriptionXml() const {
  const std::string category = categorySignals ? " FixtureTypeCategory=\"Conventional\"" : "";
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<GDTF DataVersion=\"1.2\">\n"
         "  <FixtureType Name=\"Perastage Minimal 1ch\" ShortName=\"Minimal 1ch\" Manufacturer=\"Perastage\" FixtureTypeID=\"" +
         std::string(kMinimalFixtureTypeId) + category +
         "\">\n"
         "    <AttributeDefinitions><ActivationGroups/><FeatureGroups><FeatureGroup Name=\"Dimmer\" Pretty=\"Dimmer\"><Feature Name=\"Dimmer\"/></FeatureGroup></FeatureGroups><Attributes><Attribute Name=\"Dimmer\" Pretty=\"Dimmer\" Feature=\"Dimmer.Dimmer\" PhysicalUnit=\"LuminousIntensity\"/></Attributes></AttributeDefinitions>\n"
         "    <Wheels/>\n"
         "    <PhysicalDescriptions><Emitters/><Filters/><ColorSpace Mode=\"sRGB\"/><DMXProfiles/><CRIs/><Connectors/></PhysicalDescriptions>\n"
         "    <Models><Model Name=\"Body\" Length=\"0.1\" Width=\"0.1\" Height=\"0.1\" PrimitiveType=\"Cube\"/></Models>\n"
         "    <Geometries><Geometry Name=\"Root\" Model=\"Body\"/></Geometries>\n"
         "    <DMXModes><DMXMode Name=\"" +
         modeName + "\" Geometry=\"" + modeGeometry +
         "\"><DMXChannels><DMXChannel Offset=\"1\" Geometry=\"Root\"><LogicalChannel Attribute=\"Dimmer\"><ChannelFunction Name=\"Dimmer\" Attribute=\"Dimmer\" DMXFrom=\"0/1\"/></LogicalChannel></DMXChannel></DMXChannels></DMXMode></DMXModes>\n"
         "  </FixtureType>\n"
         "</GDTF>\n";
}

// Writes the fixture as a deterministic GDTF ZIP archive.
void FixtureBuilder::WriteArchive(const std::filesystem::path &archivePath) const {
  WriteArchiveEntries(archivePath, {{"description.xml", BuildDescriptionXml()}});
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

} // namespace tests::gdtf

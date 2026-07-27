#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "gdtf_archive_reader.h"
#include "gdtf_description_reader.h"
#include "gdtf_metadata_summary.h"
#include "filesystem_path_utils.h"
#include "wx_path_utils.h"

namespace fs = std::filesystem;

// Reads a whole binary file into memory for ZIP metadata assertions.
static std::vector<unsigned char> ReadBinary(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return std::vector<unsigned char>((std::istreambuf_iterator<char>(input)),
                                    {});
}

// Writes a whole binary file after in-place ZIP metadata patching.
static void WriteBinary(const fs::path &path,
                        const std::vector<unsigned char> &data) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char *>(data.data()),
               static_cast<std::streamsize>(data.size()));
}

// Reads a little-endian 32-bit integer from ZIP test data.
static uint32_t ReadLe32(const std::vector<unsigned char> &data,
                         size_t offset) {
  return static_cast<uint32_t>(data[offset]) |
         (static_cast<uint32_t>(data[offset + 1]) << 8) |
         (static_cast<uint32_t>(data[offset + 2]) << 16) |
         (static_cast<uint32_t>(data[offset + 3]) << 24);
}

// Reads a little-endian 16-bit integer from ZIP test data.
static uint16_t ReadLe16(const std::vector<unsigned char> &data,
                         size_t offset) {
  return static_cast<uint16_t>(data[offset]) |
         (static_cast<uint16_t>(data[offset + 1]) << 8);
}

// Writes a little-endian 16-bit integer to ZIP test data.
static void WriteLe16(std::vector<unsigned char> &data, size_t offset,
                      uint16_t value) {
  data[offset] = static_cast<unsigned char>(value & 0xff);
  data[offset + 1] = static_cast<unsigned char>((value >> 8) & 0xff);
}

struct ZipNamePatchCounts {
  size_t localRecords = 0;
  size_t centralRecords = 0;
};

// Patches one exact ZIP entry identity in its local and central records.
static ZipNamePatchCounts PatchEntryNameAsInvalidFlaggedUtf8(
    std::vector<unsigned char> &data, const std::string &entryName) {
  ZipNamePatchCounts counts;
  for (size_t i = 0; i + 30 <= data.size(); ++i) {
    const uint32_t signature = ReadLe32(data, i);
    const bool local = signature == 0x04034b50;
    const bool central = signature == 0x02014b50;
    if (!local && !central)
      continue;
    const size_t headerSize = local ? 30 : 46;
    if (i + headerSize > data.size())
      continue;
    const size_t nameLengthOffset = i + (local ? 26 : 28);
    const size_t nameOffset = i + headerSize;
    const uint16_t nameLength = ReadLe16(data, nameLengthOffset);
    if (nameLength != entryName.size() ||
        nameOffset + nameLength > data.size() ||
        !std::equal(entryName.begin(), entryName.end(),
                    data.begin() + nameOffset,
                    [](char expected, unsigned char actual) {
                      return static_cast<unsigned char>(expected) == actual;
                    }))
      continue;
    data[nameOffset] = 0xff;
    const size_t flagOffset = i + (local ? 6 : 8);
    WriteLe16(data, flagOffset,
              static_cast<uint16_t>(ReadLe16(data, flagOffset) | (1u << 11)));
    if (local)
      ++counts.localRecords;
    else
      ++counts.centralRecords;
  }
  return counts;
}

// Forces or clears the ZIP UTF-8 filename flag in local and central headers.
static void PatchUtf8Flags(const fs::path &path, bool enabled) {
  std::vector<unsigned char> data = ReadBinary(path);
  for (size_t i = 0; i + 46 <= data.size(); ++i) {
    const uint32_t signature = ReadLe32(data, i);
    if (signature != 0x04034b50 && signature != 0x02014b50)
      continue;
    const size_t flagOffset = i + (signature == 0x04034b50 ? 6 : 8);
    uint16_t flags = ReadLe16(data, flagOffset);
    if (enabled)
      flags = static_cast<uint16_t>(flags | (1u << 11));
    else
      flags = static_cast<uint16_t>(flags & ~(1u << 11));
    WriteLe16(data, flagOffset, flags);
  }
  WriteBinary(path, data);
}

// Reports whether one raw central-directory identity has the expected UTF-8 flag.
static bool CentralDirectoryEntryUtf8FlagMatches(const fs::path &path,
                                                 const std::string &entryName,
                                                 bool expected) {
  const std::vector<unsigned char> data = ReadBinary(path);
  for (size_t i = 0; i + 46 <= data.size(); ++i) {
    if (ReadLe32(data, i) != 0x02014b50)
      continue;
    const uint16_t nameLength = ReadLe16(data, i + 28);
    const size_t nameOffset = i + 46;
    if (nameLength != entryName.size() ||
        nameOffset + nameLength > data.size() ||
        !std::equal(entryName.begin(), entryName.end(),
                    data.begin() + nameOffset,
                    [](char expectedByte, unsigned char actualByte) {
                      return static_cast<unsigned char>(expectedByte) ==
                             actualByte;
                    }))
      continue;
    const bool hasFlag = (ReadLe16(data, i + 8) & (1u << 11)) != 0;
    return hasFlag == expected;
  }
  return false;
}

// Writes a ZIP/GDTF archive with the requested text entries.
static bool
WriteArchive(const fs::path &path,
             const std::vector<std::pair<std::string, std::string>> &entries) {
  wxFileOutputStream output(WxPathUtils::WxStringFromFilesystemPath(path));
  if (!output.IsOk())
    return false;
  wxZipOutputStream zip(output);
  for (const auto &[name, contents] : entries) {
    auto *entry = new wxZipEntry(wxString::FromUTF8(name.c_str()));
    entry->SetMethod(wxZIP_METHOD_DEFLATE);
    zip.PutNextEntry(entry);
    zip.Write(contents.data(), contents.size());
    zip.CloseEntry();
  }
  zip.Close();
  return true;
}

// Returns a minimal GDTF XML document with caller-provided FixtureType content.
static std::string GdtfXml(const std::string &fixtureTypeContent) {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
         "<GDTF DataVersion=\"1.2\"><FixtureType Name=\"Demo\" "
         "Manufacturer=\"Perastage\" ShortName=\"DemoShort\" "
         "LongName=\"Demo Long\" Description=\"Test fixture\" "
         "FixtureTypeID=\"FTID-1\" Thumbnail=\"thumb.png\" "
         "CreateDate=\"2026-01-02T03:04:05\">" +
         fixtureTypeContent + "</FixtureType></GDTF>";
}

// Reports whether an archive result contains the requested diagnostic code.
static bool HasArchiveDiagnostic(const gdtf::ArchiveReadResult &read,
                                 gdtf::ArchiveDiagnosticCode code) {
  for (const auto &diagnostic : read.diagnostics) {
    if (diagnostic.code == code)
      return true;
  }
  return false;
}

// Reports whether a description snapshot contains the requested diagnostic
// code.
static bool
HasDescriptionDiagnostic(const gdtf::GdtfDescriptionSnapshot &snapshot,
                         gdtf::DescriptionDiagnosticCode code) {
  for (const auto &diagnostic : snapshot.diagnostics) {
    if (diagnostic.code == code)
      return true;
  }
  return false;
}

// Verifies two floating point values are close enough for parsed metadata
// tests.
static bool NearlyEqual(float left, float right) {
  return std::fabs(left - right) < 0.001f;
}

// Verifies basic archive opening and description lookup behavior.
static void TestArchiveLookup(const fs::path &dir) {
  const fs::path valid = dir / "valid.gdtf";
  assert(WriteArchive(valid, {{"description.xml", GdtfXml("")}}));
  gdtf::ArchiveReadResult read = gdtf::ReadGdtfArchive(valid);
  assert(read.Success());
  assert(read.descriptionEntryPath == "description.xml");
  assert(read.entries.size() == 1);

  const fs::path upper = dir / "upper.gdtf";
  assert(WriteArchive(upper, {{"Folder/DESCRIPTION.XML", GdtfXml("")}}));
  read = gdtf::ReadGdtfArchive(upper);
  assert(read.Success());
  assert(read.descriptionEntryPath == "Folder/DESCRIPTION.XML");
  assert(read.usedCompatibilityDescriptionFallback);

  const fs::path missing = dir / "missing.gdtf";
  assert(WriteArchive(missing, {{"models/model.glb", "model"}}));
  read = gdtf::ReadGdtfArchive(missing);
  assert(!read.Success());
  assert(!read.diagnostics.empty());
  assert(read.diagnostics.back().code ==
         gdtf::ArchiveDiagnosticCode::MissingDescriptionXml);

  const fs::path rootWins = dir / "root_wins.gdtf";
  assert(WriteArchive(rootWins, {{"description.xml", GdtfXml("")},
                                 {"Nested/DESCRIPTION.XML", GdtfXml("")}}));
  read = gdtf::ReadGdtfArchive(rootWins);
  assert(read.Success());
  assert(read.descriptionEntryPath == "description.xml");

  const fs::path duplicate = dir / "duplicate.gdtf";
  assert(WriteArchive(duplicate, {{"A/description.xml", GdtfXml("")},
                                  {"B/DESCRIPTION.XML", GdtfXml("")}}));
  read = gdtf::ReadGdtfArchive(duplicate);
  assert(!read.Success());
  bool foundAmbiguous = false;
  for (const auto &diagnostic : read.diagnostics)
    foundAmbiguous |=
        diagnostic.code == gdtf::ArchiveDiagnosticCode::AmbiguousDescriptionXml;
  assert(foundAmbiguous);

  const fs::path zeroBytes = dir / "zero_bytes.gdtf";
  assert(WriteArchive(zeroBytes, {{"description.xml", ""}}));
  read = gdtf::ReadGdtfArchive(zeroBytes);
  assert(!read.Success());
  assert(read.descriptionEntryPath == "description.xml");
  assert(HasArchiveDiagnostic(
      read, gdtf::ArchiveDiagnosticCode::EmptyDescriptionXml));
  assert(!HasArchiveDiagnostic(
      read, gdtf::ArchiveDiagnosticCode::MissingDescriptionXml));

  const fs::path whitespace = dir / "whitespace.gdtf";
  assert(WriteArchive(whitespace, {{"description.xml", " \n\t"}}));
  read = gdtf::ReadGdtfArchive(whitespace);
  assert(!read.Success());
  assert(read.descriptionEntryPath == "description.xml");
  assert(HasArchiveDiagnostic(
      read, gdtf::ArchiveDiagnosticCode::EmptyDescriptionXml));
  assert(!HasArchiveDiagnostic(
      read, gdtf::ArchiveDiagnosticCode::MissingDescriptionXml));
}

// Verifies malformed and incomplete description documents are diagnosed.
static void TestDescriptionFailures() {
  gdtf::GdtfDescriptionSnapshot malformed =
      gdtf::ReadGdtfDescription("<GDTF><FixtureType>");
  assert(!malformed.Success());
  assert(malformed.diagnostics.front().code ==
         gdtf::DescriptionDiagnosticCode::MalformedXml);

  gdtf::GdtfDescriptionSnapshot missingRoot =
      gdtf::ReadGdtfDescription("<FixtureType/>");
  assert(!missingRoot.Success());
  assert(missingRoot.diagnostics.front().code ==
         gdtf::DescriptionDiagnosticCode::MissingRoot);

  gdtf::GdtfDescriptionSnapshot missingFixture =
      gdtf::ReadGdtfDescription("<GDTF DataVersion=\"1.2\"/>");
  assert(!missingFixture.Success());
  assert(missingFixture.diagnostics.front().code ==
         gdtf::DescriptionDiagnosticCode::MissingFixtureType);
}

// Verifies explicit and WiringObject fallback power parsing.
static void TestPowerParsing() {
  auto readPower = [](const std::string &content) {
    return gdtf::ReadGdtfDescription(GdtfXml(content));
  };

  gdtf::GdtfDescriptionSnapshot snapshot =
      readPower("<PhysicalDescriptions><Properties>"
                "<PowerConsumption Value=\"120\"/>"
                "</Properties></PhysicalDescriptions>");
  assert(snapshot.powerConsumptionWPresent);
  assert(NearlyEqual(snapshot.powerConsumptionW, 120.0f));

  snapshot = readPower("<PhysicalDescriptions><Properties>"
                       "<PowerConsumption Value=\"120\"/>"
                       "<PowerConsumption Value=\"30.5\"/>"
                       "</Properties></PhysicalDescriptions>");
  assert(snapshot.powerConsumptionWPresent);
  assert(NearlyEqual(snapshot.powerConsumptionW, 150.5f));

  snapshot = readPower(
      "<PhysicalDescriptions><Properties>"
      "<PowerConsumption Value=\"120\"/>"
      "</Properties></PhysicalDescriptions>"
      "<Geometries><Geometry>"
      "<WiringObject ComponentType=\"Consumer\" ElectricalPayLoad=\"50\"/>"
      "</Geometry></Geometries>");
  assert(snapshot.powerConsumptionWPresent);
  assert(NearlyEqual(snapshot.powerConsumptionW, 120.0f));

  snapshot = readPower(
      "<Geometries><Geometry>"
      "<WiringObject ComponentType=\"Consumer\" ElectricalPayLoad=\"50\"/>"
      "</Geometry></Geometries>");
  assert(snapshot.powerConsumptionWPresent);
  assert(NearlyEqual(snapshot.powerConsumptionW, 50.0f));

  snapshot = readPower(
      "<Geometries><Geometry><Geometry>"
      "<WiringObject ComponentType=\"Consumer\" ElectricalPayLoad=\"25\"/>"
      "</Geometry></Geometry></Geometries>");
  assert(snapshot.powerConsumptionWPresent);
  assert(NearlyEqual(snapshot.powerConsumptionW, 25.0f));

  snapshot = readPower(
      "<Geometries><Geometry>"
      "<WiringObject ComponentType=\"Consumer\" ElectricalPayLoad=\"20\"/>"
      "<WiringObject ComponentType=\"Consumer\" ElectricalPayload=\"22\"/>"
      "<WiringObject ComponentType=\"Source\" ElectricalPayLoad=\"999\"/>"
      "<WiringObject ComponentType=\"Consumer\" ElectricalPayLoad=\"0\"/>"
      "<WiringObject ComponentType=\"Consumer\" ElectricalPayLoad=\"-1\"/>"
      "<WiringObject ComponentType=\"Consumer\" ElectricalPayLoad=\"invalid\"/>"
      "</Geometry></Geometries>");
  assert(snapshot.powerConsumptionWPresent);
  assert(NearlyEqual(snapshot.powerConsumptionW, 42.0f));
}

// Verifies ordered metadata, revisions, and DMX modes are preserved.
static void TestOrderedDocumentData() {
  const std::string xml =
      GdtfXml("<Revisions>"
              "<Revision Text=\"Initial\" Date=\"2026-01-03T04:05:06\" "
              "UserID=\"7\" ModifiedBy=\"A\"/>"
              "<Revision Text=\"Updated\" Date=\"2026-01-04T05:06:07Z\" "
              "UserID=\"8\" ModifiedBy=\"B\"/>"
              "</Revisions>"
              "<DMXModes><DMXMode Name=\"Standard\"/><DMXMode "
              "Name=\"Extended\"/></DMXModes>");
  gdtf::GdtfDescriptionSnapshot snapshot = gdtf::ReadGdtfDescription(xml);
  assert(snapshot.Success());
  assert(snapshot.dataVersion == "1.2");
  assert(snapshot.fixtureTypeName == "Demo");
  assert(snapshot.manufacturer == "Perastage");
  assert(snapshot.revisions.size() == 2);
  assert(snapshot.revisions[0].text == "Initial");
  assert(snapshot.revisions[1].modifiedBy == "B");
  assert((snapshot.dmxModeNames ==
          std::vector<std::string>{"Standard", "Extended"}));
}

// Verifies repeated wheels and ordered gobo slot references remain independent.
static void TestGoboWheelCollections() {
  const std::string xml =
      GdtfXml("<Wheels>"
              "<Wheel Name=\"Gobo Wheel A\">"
              "<Slot Name=\"Open A\" MediaFileName=\"a_open\"/>"
              "<Slot Name=\"Breakup A\" MediaFileName=\"a_breakup\"/>"
              "</Wheel>"
              "<Wheel Name=\"Gobo Wheel B\">"
              "<Slot Name=\"Open B\" MediaFileName=\"b_open\"/>"
              "<Slot Name=\"Dots B\" MediaFileName=\"b_dots\"/>"
              "</Wheel>"
              "</Wheels>");
  gdtf::GdtfDescriptionSnapshot snapshot = gdtf::ReadGdtfDescription(
      xml, {"description.xml", "wheels/a_open.png", "wheels/a_breakup.png",
            "wheels/b_open.png", "wheels/b_dots.png"});
  assert(snapshot.Success());
  assert(snapshot.wheels.size() == 2);
  assert(snapshot.wheels[0].name == "Gobo Wheel A");
  assert(snapshot.wheels[1].name == "Gobo Wheel B");
  assert(snapshot.wheels[0].slots.size() == 2);
  assert(snapshot.wheels[1].slots.size() == 2);
  assert(snapshot.wheels[0].slots[1].mediaFileName == "a_breakup");
  assert(snapshot.wheels[1].slots[1].mediaFileName == "b_dots");
  assert(!HasDescriptionDiagnostic(
      snapshot, gdtf::DescriptionDiagnosticCode::MissingWheelMediaResource));
}

// Verifies GDTF wheel MediaFileName resource lookup uses wheels basenames.
static void TestWheelMediaResolution() {
  const std::string xml = GdtfXml("<Wheels><Wheel Name=\"Gobo\"><Slot "
                                  "Name=\"Slot\" MediaFileName=\"Gobo1\"/>"
                                  "</Wheel></Wheels>");

  gdtf::GdtfDescriptionSnapshot snapshot =
      gdtf::ReadGdtfDescription(xml, {"description.xml", "wheels/Gobo1.png"});
  assert(snapshot.Success());
  assert(!HasDescriptionDiagnostic(
      snapshot, gdtf::DescriptionDiagnosticCode::MissingWheelMediaResource));
  assert(snapshot.wheels[0].slots[0].mediaFileName == "Gobo1");

  snapshot =
      gdtf::ReadGdtfDescription(xml, {"description.xml", "wheels/Gobo1.svg"});
  assert(snapshot.Success());
  assert(!HasDescriptionDiagnostic(
      snapshot, gdtf::DescriptionDiagnosticCode::MissingWheelMediaResource));

  snapshot = gdtf::ReadGdtfDescription(
      xml, {"description.xml", "resources/Gobo1.png"});
  assert(HasDescriptionDiagnostic(
      snapshot, gdtf::DescriptionDiagnosticCode::MissingWheelMediaResource));

  snapshot = gdtf::ReadGdtfDescription(
      xml, {"description.xml", "wheels/Gobo1.png", "wheels/Gobo1.svg"});
  assert(HasDescriptionDiagnostic(
      snapshot, gdtf::DescriptionDiagnosticCode::AmbiguousWheelMediaResource));

  snapshot =
      gdtf::ReadGdtfDescription(xml, {"description.xml", "wheels/gobo1.png"});
  assert(HasDescriptionDiagnostic(
      snapshot,
      gdtf::DescriptionDiagnosticCode::NonCanonicalWheelMediaCaseMatch));
}

// Verifies unknown content remains non-fatal and metadata summary stays
// compatible.
static void TestUnknownUnicodeAndSummary(const fs::path &dir) {
  const std::string xml =
      GdtfXml("<CustomVendorNode CustomAttribute=\"kept\"/>"
              "<Revisions>"
              "<Revision Text=\"Initial\" Date=\"2026-01-03T04:05:06\" "
              "UserID=\"42\" ModifiedBy=\"Tester\"/>"
              "<Revision Text=\"Updated\" Date=\"2026-01-04T05:06:07Z\" "
              "UserID=\"43\" ModifiedBy=\"Maintainer\"/>"
              "</Revisions>");
  gdtf::GdtfDescriptionSnapshot snapshot = gdtf::ReadGdtfDescription(xml);
  assert(snapshot.Success());
  bool foundUnknown = false;
  for (const auto &diagnostic : snapshot.diagnostics)
    foundUnknown |=
        diagnostic.code == gdtf::DescriptionDiagnosticCode::UnknownElement;
  assert(foundUnknown);

  const fs::path unicodePath =
      dir / PathUtils::PathFromUtf8("metadata_ñ_测试.gdtf");
  assert(WriteArchive(unicodePath, {{std::string("assets/ñ.txt"), "ok"},
                                    {"description.xml", xml}}));
  gdtf::ArchiveReadResult read = gdtf::ReadGdtfArchive(unicodePath);
  assert(read.Success());
  bool foundUnicodeEntry = false;
  for (const auto &entry : read.entries)
    foundUnicodeEntry |= entry.path == std::string("assets/ñ.txt");
  assert(foundUnicodeEntry);

  GdtfMetadataSummary summary;
  assert(LoadGdtfMetadataSummary(unicodePath, summary));
  assert(summary.manufacturer == "Perastage");
  assert(summary.description == "Test fixture");
  assert(summary.creationDate == "2026-01-02 03:04:05");
  assert(summary.revision == "Updated");
  assert(summary.lastModified == "2026-01-04 05:06:07");
  assert(summary.userId == "43");
  assert(summary.modifiedBy == "Maintainer");
  assert(summary.version == "1.2");
}

// Verifies ZIP filename UTF-8 compatibility fallback and strict invalid-name
// diagnostics.
static void TestUnicodeZipFilenameCompatibility(const fs::path &dir) {
  const std::string unicodeName =
      "wheels/191130000002 FINE 360 BEAM Φ113金属图案盘二 02.png";
  const std::string mediaName =
      "191130000002 FINE 360 BEAM Φ113金属图案盘二 02";
  const std::string xml = GdtfXml(
      "<Wheels><Wheel Name=\"Gobo\"><Slot Name=\"Unicode\" MediaFileName=\"" +
      mediaName +
      "\"/></Wheel></Wheels>"
      "<DMXModes><DMXMode Name=\"16BT \"/></DMXModes>");

  const fs::path missingFlag = dir / "unicode_missing_flag.gdtf";
  assert(WriteArchive(missingFlag,
                      {{unicodeName, "png"}, {"description.xml", xml}}));
  PatchUtf8Flags(missingFlag, false);
  gdtf::ArchiveReadResult read = gdtf::ReadGdtfArchive(missingFlag);
  assert(read.Success());
  assert(read.utf8FlagMissingEntryCount == 1);
  assert(HasArchiveDiagnostic(read,
                              gdtf::ArchiveDiagnosticCode::Utf8FallbackUsed));
  bool foundUnicodeEntry = false;
  for (const auto &entry : read.entries) {
    foundUnicodeEntry |=
        entry.path == unicodeName && entry.nameUsedUtf8CompatibilityFallback;
  }
  assert(foundUnicodeEntry);
  assert(read.descriptionEntryPath == "description.xml");

  std::vector<std::string> paths;
  for (const auto &entry : read.entries)
    paths.push_back(entry.path);
  gdtf::GdtfDescriptionSnapshot snapshot =
      gdtf::ReadGdtfDescription(read.descriptionXml, paths);
  assert(snapshot.Success());
  assert(snapshot.dmxModeNames.size() == 1);
  assert(snapshot.dmxModeNames[0] == "16BT ");
  assert(!HasDescriptionDiagnostic(
      snapshot, gdtf::DescriptionDiagnosticCode::MissingWheelMediaResource));

  const fs::path utf8Flag = dir / "unicode_with_flag.gdtf";
  assert(
      WriteArchive(utf8Flag, {{unicodeName, "png"}, {"description.xml", xml}}));
  PatchUtf8Flags(utf8Flag, true);
  read = gdtf::ReadGdtfArchive(utf8Flag);
  assert(read.Success());
  assert(read.utf8FlagMissingEntryCount == 0);
  assert(!HasArchiveDiagnostic(read,
                               gdtf::ArchiveDiagnosticCode::Utf8FallbackUsed));

  const fs::path asciiNoFlag = dir / "ascii_no_flag.gdtf";
  assert(WriteArchive(asciiNoFlag, {{"wheels/plain.png", "png"},
                                    {"description.xml", GdtfXml("")}}));
  PatchUtf8Flags(asciiNoFlag, false);
  read = gdtf::ReadGdtfArchive(asciiNoFlag);
  assert(read.Success());
  assert(read.utf8FlagMissingEntryCount == 0);
  assert(!HasArchiveDiagnostic(read,
                               gdtf::ArchiveDiagnosticCode::Utf8FallbackUsed));

  auto assertMalformedByteArchive =
      [&](const std::string &fileName,
          const std::vector<std::pair<std::string, std::string>> &entries,
          const std::string &corruptedName) {
        const fs::path archivePath = dir / fileName;
        assert(WriteArchive(archivePath, entries));
        std::vector<unsigned char> data = ReadBinary(archivePath);
        const ZipNamePatchCounts counts =
            PatchEntryNameAsInvalidFlaggedUtf8(data, corruptedName);
        assert(counts.localRecords == 1);
        assert(counts.centralRecords == 1);
        WriteBinary(archivePath, data);
        const std::vector<unsigned char> patched = ReadBinary(archivePath);
        assert(patched == data);

        const gdtf::ArchiveReadResult malformedRead =
            gdtf::ReadGdtfArchive(archivePath);
        assert(!malformedRead.Success());
        assert(HasArchiveDiagnostic(
            malformedRead,
            gdtf::ArchiveDiagnosticCode::FilenameDecodeFailed));
        assert(malformedRead.entries.empty());
        assert(malformedRead.descriptionXml.empty());
      };

  assertMalformedByteArchive(
      "invalid_before_description.gdtf",
      {{unicodeName, "png"}, {"description.xml", xml}}, unicodeName);
  assertMalformedByteArchive(
      "invalid_after_description.gdtf",
      {{"description.xml", xml}, {unicodeName, "png"}}, unicodeName);
  assertMalformedByteArchive(
      "invalid_among_multiple.gdtf",
      {{"models/body.glb", "model"}, {unicodeName, "png"},
       {"description.xml", xml}},
      unicodeName);
  assertMalformedByteArchive(
      "invalid_description.gdtf",
      {{"description.xml", xml}, {"models/body.glb", "model"}},
      "description.xml");
  assertMalformedByteArchive(
      "valid_unicode_and_invalid_resource.gdtf",
      {{"wheels/ñ.png", "valid"}, {unicodeName, "invalid"},
       {"description.xml", xml}},
      unicodeName);
}

// Verifies Perastage-created ZIP entries carry UTF-8 filename metadata for
// Unicode names.
static void TestUnicodeZipWriteMetadata(const fs::path &dir) {
  const fs::path path = dir / "unicode_written.gdtf";
  assert(WriteArchive(path, {{"wheels/Φ113金属图案盘二.png", "png"},
                             {"description.xml", GdtfXml("")}}));
  assert(CentralDirectoryEntryUtf8FlagMatches(
      path, "wheels/Φ113金属图案盘二.png", true));
  gdtf::ArchiveReadResult read = gdtf::ReadGdtfArchive(path);
  assert(read.Success());
  assert(read.utf8FlagMissingEntryCount == 0);
}

// Verifies Unicode fallback filenames extract to native filesystem paths.
static void TestUnicodeZipExtractionCompatibility(const fs::path &dir) {
  const std::string unicodeName =
      "wheels/191130000002 FINE 360 BEAM Φ113金属图案盘二 02.png";
  const fs::path archivePath = dir / "unicode_extract_missing_flag.gdtf";
  assert(WriteArchive(
      archivePath, {{unicodeName, "png"}, {"description.xml", GdtfXml("")}}));
  PatchUtf8Flags(archivePath, false);

  const fs::path extractedRoot = dir / "extracted_unicode";
  gdtf::ArchiveReadResult result =
      gdtf::ExtractGdtfArchive(archivePath, extractedRoot);
  assert(!result.entries.empty());
  assert(result.utf8FlagMissingEntryCount == 1);
  assert(HasArchiveDiagnostic(result,
                              gdtf::ArchiveDiagnosticCode::Utf8FallbackUsed));
  std::u8string relativeUtf8;
  relativeUtf8.reserve(unicodeName.size());
  for (char ch : unicodeName)
    relativeUtf8.push_back(static_cast<char8_t>(ch));
  assert(fs::is_regular_file(extractedRoot / fs::path(relativeUtf8)));
  assert(fs::is_regular_file(extractedRoot / "description.xml"));
}

// Runs focused read-layer regression tests using temporary archives only.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());
  const fs::path dir = fs::temp_directory_path() / "gdtf_read_services_test";
  fs::remove_all(dir);
  fs::create_directories(dir);

  TestArchiveLookup(dir);
  TestDescriptionFailures();
  TestPowerParsing();
  TestOrderedDocumentData();
  TestGoboWheelCollections();
  TestWheelMediaResolution();
  TestUnknownUnicodeAndSummary(dir);
  TestUnicodeZipFilenameCompatibility(dir);
  TestUnicodeZipWriteMetadata(dir);
  TestUnicodeZipExtractionCompatibility(dir);

  fs::remove_all(dir);
  return 0;
}

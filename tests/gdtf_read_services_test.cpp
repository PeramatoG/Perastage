#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "gdtf_archive_reader.h"
#include "gdtf_description_reader.h"
#include "gdtf_metadata_summary.h"

namespace fs = std::filesystem;

// Writes a ZIP/GDTF archive with the requested text entries.
static bool WriteArchive(const fs::path &path,
                         const std::vector<std::pair<std::string, std::string>> &entries) {
  wxFileOutputStream output(wxString::FromUTF8(path.generic_string().c_str()));
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
    foundAmbiguous |= diagnostic.code ==
                      gdtf::ArchiveDiagnosticCode::AmbiguousDescriptionXml;
  assert(foundAmbiguous);
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

// Verifies ordered metadata, revisions, and DMX modes are preserved.
static void TestOrderedDocumentData() {
  const std::string xml = GdtfXml(
      "<Revisions>"
      "<Revision Text=\"Initial\" Date=\"2026-01-03T04:05:06\" UserID=\"7\" ModifiedBy=\"A\"/>"
      "<Revision Text=\"Updated\" Date=\"2026-01-04T05:06:07Z\" UserID=\"8\" ModifiedBy=\"B\"/>"
      "</Revisions>"
      "<DMXModes><DMXMode Name=\"Standard\"/><DMXMode Name=\"Extended\"/></DMXModes>");
  gdtf::GdtfDescriptionSnapshot snapshot = gdtf::ReadGdtfDescription(xml);
  assert(snapshot.Success());
  assert(snapshot.dataVersion == "1.2");
  assert(snapshot.fixtureTypeName == "Demo");
  assert(snapshot.manufacturer == "Perastage");
  assert(snapshot.revisions.size() == 2);
  assert(snapshot.revisions[0].text == "Initial");
  assert(snapshot.revisions[1].modifiedBy == "B");
  assert((snapshot.dmxModeNames == std::vector<std::string>{"Standard", "Extended"}));
}

// Verifies repeated wheels and ordered gobo slot references remain independent.
static void TestGoboWheelCollections() {
  const std::string xml = GdtfXml(
      "<Wheels>"
      "<Wheel Name=\"Gobo Wheel A\">"
      "<Slot Name=\"Open A\" MediaFileName=\"wheels/a_open.png\"/>"
      "<Slot Name=\"Breakup A\" MediaFileName=\"wheels/a_breakup.png\"/>"
      "</Wheel>"
      "<Wheel Name=\"Gobo Wheel B\">"
      "<Slot Name=\"Open B\" MediaFileName=\"wheels/b_open.png\"/>"
      "<Slot Name=\"Dots B\" MediaFileName=\"wheels/b_dots.png\"/>"
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
  assert(snapshot.wheels[0].slots[1].mediaFileName == "wheels/a_breakup.png");
  assert(snapshot.wheels[1].slots[1].mediaFileName == "wheels/b_dots.png");
}

// Verifies unknown content remains non-fatal and metadata summary stays compatible.
static void TestUnknownUnicodeAndSummary(const fs::path &dir) {
  const std::string xml = GdtfXml(
      "<CustomVendorNode CustomAttribute=\"kept\"/>"
      "<Revisions>"
      "<Revision Text=\"Initial\" Date=\"2026-01-03T04:05:06\" UserID=\"42\" ModifiedBy=\"Tester\"/>"
      "<Revision Text=\"Updated\" Date=\"2026-01-04T05:06:07Z\" UserID=\"43\" ModifiedBy=\"Maintainer\"/>"
      "</Revisions>");
  gdtf::GdtfDescriptionSnapshot snapshot = gdtf::ReadGdtfDescription(xml);
  assert(snapshot.Success());
  bool foundUnknown = false;
  for (const auto &diagnostic : snapshot.diagnostics)
    foundUnknown |= diagnostic.code == gdtf::DescriptionDiagnosticCode::UnknownElement;
  assert(foundUnknown);

  const fs::path unicodePath = dir / std::string("metadata_ñ_测试.gdtf");
  assert(WriteArchive(unicodePath, {{std::string("assets/ñ.txt"), "ok"}, {"description.xml", xml}}));
  gdtf::ArchiveReadResult read = gdtf::ReadGdtfArchive(unicodePath);
  assert(read.Success());
  bool foundUnicodeEntry = false;
  for (const auto &entry : read.entries)
    foundUnicodeEntry |= entry.path == std::string("assets/ñ.txt");
  assert(foundUnicodeEntry);

  GdtfMetadataSummary summary;
  assert(LoadGdtfMetadataSummary(unicodePath.generic_string(), summary));
  assert(summary.manufacturer == "Perastage");
  assert(summary.description == "Test fixture");
  assert(summary.creationDate == "2026-01-02 03:04:05");
  assert(summary.revision == "Updated");
  assert(summary.lastModified == "2026-01-04 05:06:07");
  assert(summary.userId == "43");
  assert(summary.modifiedBy == "Maintainer");
  assert(summary.version == "1.2");
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
  TestOrderedDocumentData();
  TestGoboWheelCollections();
  TestUnknownUnicodeAndSummary(dir);

  fs::remove_all(dir);
  return 0;
}

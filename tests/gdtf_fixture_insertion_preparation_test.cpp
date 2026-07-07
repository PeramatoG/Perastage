#include "gdtf_fixture_insertion_preparation.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <vector>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;

namespace {

// Writes a ZIP/GDTF archive with the requested text entries.
bool WriteArchive(const fs::path &path,
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

// Reads a whole file into memory for non-mutating checks.
std::string ReadFile(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

// Returns a minimal GDTF XML document with caller-provided FixtureType content.
std::string GdtfXml(const std::string &fixtureTypeContent) {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
         "<GDTF DataVersion=\"1.2\"><FixtureType Name=\"Demo\" "
         "Manufacturer=\"Perastage\" ShortName=\"DemoShort\">" +
         fixtureTypeContent + "</FixtureType></GDTF>";
}

// Returns a DMXModes XML block preserving caller-provided mode names.
std::string Modes(std::initializer_list<const char *> names) {
  std::string xml = "<DMXModes>";
  for (const char *name : names)
    xml += std::string("<DMXMode Name=\"") + name + "\"/>";
  xml += "</DMXModes>";
  return xml;
}

// Reports whether a preparation contains a diagnostic code.
bool HasDiagnostic(const gdtf::GdtfFixtureInsertionPreparation &preparation,
                   const std::string &code) {
  for (const auto &diagnostic : preparation.diagnostics) {
    if (diagnostic.code == code)
      return true;
  }
  return false;
}

// Verifies canonical and ordered mode preparation behavior.
void AssertCanonicalPreparation(const fs::path &dir) {
  const fs::path oneMode = dir / "one_mode.gdtf";
  assert(WriteArchive(oneMode, {{"description.xml", GdtfXml(Modes({"Mode 1"}))}}));
  auto preparation = gdtf::PrepareGdtfFixtureInsertion(oneMode);
  assert(preparation.success);
  assert(preparation.fixtureDisplayName == "Demo");
  assert(preparation.dmxModeNames.size() == 1);
  assert(preparation.dmxModeNames[0] == "Mode 1");
  assert(preparation.standardsCompliantForCheckedRead);
  assert(!preparation.tolerantNonStandardInputAccepted);

  const fs::path severalModes = dir / "several_modes.gdtf";
  assert(WriteArchive(severalModes,
                      {{"description.xml", GdtfXml(Modes({"A", "B", "C"}))}}));
  preparation = gdtf::PrepareGdtfFixtureInsertion(severalModes);
  assert(preparation.success);
  assert((preparation.dmxModeNames == std::vector<std::string>{"A", "B", "C"}));
}

// Verifies tolerant description.xml fallback rules.
void AssertTolerantDescriptionLookup(const fs::path &dir) {
  const fs::path upper = dir / "upper.gdtf";
  assert(WriteArchive(upper, {{"DESCRIPTION.XML", GdtfXml(Modes({"Mode"}))}}));
  auto preparation = gdtf::PrepareGdtfFixtureInsertion(upper);
  assert(preparation.success);
  assert(preparation.tolerantNonStandardInputAccepted);
  assert(HasDiagnostic(preparation, "archive-non-canonical-description"));

  const fs::path nested = dir / "nested.gdtf";
  assert(WriteArchive(nested, {{"folder/description.xml", GdtfXml(Modes({"Mode"}))}}));
  preparation = gdtf::PrepareGdtfFixtureInsertion(nested);
  assert(preparation.success);
  assert(preparation.tolerantNonStandardInputAccepted);

  const fs::path rootWins = dir / "root_wins.gdtf";
  assert(WriteArchive(rootWins, {{"description.xml", GdtfXml(Modes({"Root"}))},
                                 {"folder/description.xml", GdtfXml(Modes({"Nested"}))}}));
  preparation = gdtf::PrepareGdtfFixtureInsertion(rootWins);
  assert(preparation.success);
  assert(preparation.dmxModeNames[0] == "Root");
  assert(!preparation.tolerantNonStandardInputAccepted);
}

// Verifies invalid archives and invalid description documents fail clearly.
void AssertFailureDiagnostics(const fs::path &dir) {
  const fs::path ambiguous = dir / "ambiguous.gdtf";
  assert(WriteArchive(ambiguous, {{"a/description.xml", GdtfXml(Modes({"A"}))},
                                  {"b/description.xml", GdtfXml(Modes({"B"}))}}));
  auto preparation = gdtf::PrepareGdtfFixtureInsertion(ambiguous);
  assert(!preparation.success);
  assert(HasDiagnostic(preparation, "archive-ambiguous-description"));

  const fs::path missing = dir / "missing_description.gdtf";
  assert(WriteArchive(missing, {{"models/model.glb", "model"}}));
  preparation = gdtf::PrepareGdtfFixtureInsertion(missing);
  assert(!preparation.success);
  assert(HasDiagnostic(preparation, "archive-missing-description"));

  const fs::path emptyDescription = dir / "empty_description.gdtf";
  assert(WriteArchive(emptyDescription, {{"description.xml", "   \n\t"}}));
  preparation = gdtf::PrepareGdtfFixtureInsertion(emptyDescription);
  assert(!preparation.success);
  assert(HasDiagnostic(preparation, "archive-empty-description"));

  const fs::path malformed = dir / "malformed.gdtf";
  assert(WriteArchive(malformed, {{"description.xml", "<GDTF><FixtureType>"}}));
  preparation = gdtf::PrepareGdtfFixtureInsertion(malformed);
  assert(!preparation.success);
  assert(HasDiagnostic(preparation, "description-malformed-xml"));

  const fs::path missingRoot = dir / "missing_root.gdtf";
  assert(WriteArchive(missingRoot, {{"description.xml", "<FixtureType/>"}}));
  preparation = gdtf::PrepareGdtfFixtureInsertion(missingRoot);
  assert(!preparation.success);
  assert(HasDiagnostic(preparation, "description-missing-root"));

  const fs::path missingFixtureType = dir / "missing_fixture_type.gdtf";
  assert(WriteArchive(missingFixtureType, {{"description.xml", "<GDTF/>"}}));
  preparation = gdtf::PrepareGdtfFixtureInsertion(missingFixtureType);
  assert(!preparation.success);
  assert(HasDiagnostic(preparation, "description-missing-fixture-type"));

  const fs::path noModes = dir / "no_modes.gdtf";
  assert(WriteArchive(noModes, {{"description.xml", GdtfXml("")}}));
  preparation = gdtf::PrepareGdtfFixtureInsertion(noModes);
  assert(!preparation.success);
  assert(HasDiagnostic(preparation, "description-missing-dmx-modes"));

  const fs::path invalidZip = dir / "invalid_zip.gdtf";
  { std::ofstream output(invalidZip, std::ios::binary); output << "not a zip"; }
  preparation = gdtf::PrepareGdtfFixtureInsertion(invalidZip);
  assert(!preparation.success);

  const fs::path emptyFile = dir / "empty_file.gdtf";
  { std::ofstream output(emptyFile, std::ios::binary); }
  preparation = gdtf::PrepareGdtfFixtureInsertion(emptyFile);
  assert(!preparation.success);
  assert(HasDiagnostic(preparation, "source-empty-or-unreadable"));

  preparation = gdtf::PrepareGdtfFixtureInsertion(dir / "missing_file.gdtf");
  assert(!preparation.success);
  assert(HasDiagnostic(preparation, "source-missing"));
}

// Verifies Unicode paths and unsafe archive paths are handled deterministically.
void AssertUnicodeAndUnsafePaths(const fs::path &dir) {
  const fs::path unicode = dir / "Perastage_ñ_fixture.gdtf";
  assert(WriteArchive(unicode, {{"assets/ñ.txt", "ok"},
                                {"description.xml", GdtfXml(Modes({"Mode"}))}}));
  auto preparation = gdtf::PrepareGdtfFixtureInsertion(unicode);
  assert(preparation.success);

  const fs::path unsafe = dir / "unsafe.gdtf";
  assert(WriteArchive(unsafe, {{"../description.xml", GdtfXml(Modes({"Mode"}))}}));
  preparation = gdtf::PrepareGdtfFixtureInsertion(unsafe);
  assert(!preparation.success);
  assert(HasDiagnostic(preparation, "archive-unsafe-entry-path"));
}

// Verifies preparation does not mutate archive bytes or timestamps.
void AssertPreparationIsReadOnly(const fs::path &dir) {
  const fs::path archive = dir / "readonly.gdtf";
  assert(WriteArchive(archive, {{"description.xml", GdtfXml(Modes({"Mode"}))}}));
  const std::string beforeBytes = ReadFile(archive);
  const auto beforeTime = fs::last_write_time(archive);
  auto preparation = gdtf::PrepareGdtfFixtureInsertion(archive);
  assert(preparation.success);
  assert(ReadFile(archive) == beforeBytes);
  assert(fs::last_write_time(archive) == beforeTime);
}

} // namespace

// Runs generated-archive regression tests for GDTF fixture insertion preparation.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path dir = fs::temp_directory_path() /
                       "gdtf_fixture_insertion_preparation_test";
  fs::remove_all(dir);
  fs::create_directories(dir);

  AssertCanonicalPreparation(dir);
  AssertTolerantDescriptionLookup(dir);
  AssertFailureDiagnostics(dir);
  AssertUnicodeAndUnsafePaths(dir);
  AssertPreparationIsReadOnly(dir);
  return 0;
}

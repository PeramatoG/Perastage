#include "gdtf_archive_reader.h"
#include "gdtf_description_reader.h"
#include "inspection/gdtf_inspection.h"
#include "wx_path_utils.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;
using perastage::inspection::DiagnosticClassification;
using perastage::inspection::GdtfInspectionResult;
using perastage::inspection::GdtfReadStatus;
using perastage::inspection::ValidationLayer;
using perastage::inspection::ValidationStatus;

namespace {

// Returns deterministic XML that exercises every semantic family in INS-110.
std::string CompleteXml() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<GDTF DataVersion=\"1.2\"><FixtureType Name=\"Inspect Fixture\" "
         "Manufacturer=\"Perastage\" ShortName=\"Inspect\" "
         "LongName=\"Inspection Fixture\" Description=\"Reader parity\" "
         "FixtureTypeID=\"12345678-1234-4234-9234-123456789abc\" "
         "Thumbnail=\"thumb\" CreateDate=\"2026-01-02T03:04:05\" "
         "RefFT=\"rev-a\"><Revisions><Revision Text=\"Initial\" "
         "Date=\"2026-01-02T03:04:05\" UserID=\"7\" ModifiedBy=\"Test\"/>"
         "</Revisions><Wheels><Wheel Name=\"Color\"><Slot Name=\"Blue\" "
         "MediaFileName=\"blue\" Gobo=\"wheels/blue.png\"/></Wheel></Wheels>"
         "<PhysicalDescriptions><Properties><Weight Value=\"12.5\"/>"
         "<OperatingTemperature Low=\"0\" High=\"40\"/></Properties>"
         "</PhysicalDescriptions><Models><Model Name=\"Body\" "
         "PrimitiveType=\"Cube\"/></Models><Geometries><Geometry Name=\"Root\" "
         "Model=\"Body\"/></Geometries><DMXModes><DMXMode Name=\"Mode A\" "
         "Geometry=\"Root\"/><DMXMode Name=\"Mode B\" Geometry=\"Root\"/>"
         "</DMXModes></FixtureType></GDTF>\n";
}

// Writes a deterministic ZIP with the supplied ordered text entries.
void WriteArchive(
    const fs::path &path,
    const std::vector<std::pair<std::string, std::string>> &entries) {
  wxFileOutputStream output(WxPathUtils::WxStringFromFilesystemPath(path));
  assert(output.IsOk());
  wxZipOutputStream zip(output, 0);
  for (const auto &[name, contents] : entries) {
    auto *entry = new wxZipEntry();
    entry->SetName(wxString::FromUTF8(name), wxPATH_UNIX);
    assert(zip.PutNextEntry(entry));
    zip.Write(contents.data(), contents.size());
  }
  zip.Close();
}

// Reads all source bytes so inspection immutability can be asserted.
std::vector<unsigned char> ReadBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {(std::istreambuf_iterator<char>(input)), {}};
}

// Reports whether an adapted diagnostic has a code and classification.
bool HasDiagnostic(const GdtfInspectionResult &result, const std::string &code,
                   DiagnosticClassification classification) {
  return std::any_of(result.inspection.diagnostics.begin(),
                     result.inspection.diagnostics.end(),
                     [&](const auto &diagnostic) {
                       return diagnostic.code == code &&
                              diagnostic.classification == classification;
                     });
}

// Returns one validation stage from a GDTF inspection result.
const perastage::inspection::ValidationResult &
Validation(const GdtfInspectionResult &result, ValidationLayer layer) {
  const auto found = std::find_if(
      result.validation.begin(), result.validation.end(),
      [layer](const auto &validation) { return validation.layer == layer; });
  assert(found != result.validation.end());
  return *found;
}

// Returns a minimal GDTF 1.2 document accepted by both compared schemas.
std::string StandardsValidXml() {
  return "<GDTF DataVersion=\"1.2\"><FixtureType Name=\"Fixture\" "
         "Manufacturer=\"Perastage\" Description=\"Valid fixture\" "
         "FixtureTypeID=\"12345678-1234-4234-9234-123456789abc\">"
         "<AttributeDefinitions><FeatureGroups/><Attributes/>"
         "</AttributeDefinitions><Geometries><Geometry Name=\"Root\"/>"
         "</Geometries><DMXModes><DMXMode Name=\"Mode\" Geometry=\"Root\">"
         "<DMXChannels/></DMXMode></DMXModes></FixtureType></GDTF>";
}

// Verifies schema requirements without changing tolerant reader success policy.
void TestValidationLayers(const fs::path &directory) {
  const fs::path validPath = directory / "standards-valid.gdtf";
  WriteArchive(validPath, {{"description.xml", StandardsValidXml()}});
  const GdtfInspectionResult valid =
      perastage::inspection::InspectGdtf(validPath);
  assert(Validation(valid, ValidationLayer::XmlWellFormedness).status ==
         ValidationStatus::Valid);
  assert(Validation(valid, ValidationLayer::Schema).status ==
         ValidationStatus::Valid);

  const fs::path invalidPath = directory / "standards-invalid.gdtf";
  WriteArchive(invalidPath,
               {{"description.xml",
                 "<GDTF DataVersion=\"1.2\"><FixtureType Name=\"Fixture\" "
                 "Manufacturer=\"Perastage\" Description=\"Invalid\" "
                 "FixtureTypeID=\"12345678-1234-4234-9234-123456789abc\">"
                 "<Geometries/><DMXModes/></FixtureType></GDTF>"}});
  const GdtfInspectionResult invalid =
      perastage::inspection::InspectGdtf(invalidPath);
  assert(Validation(invalid, ValidationLayer::XmlWellFormedness).status ==
         ValidationStatus::Valid);
  assert(Validation(invalid, ValidationLayer::Schema).status ==
         ValidationStatus::Invalid);

  const fs::path compatiblePath = directory / "standards-compatible.gdtf";
  WriteArchive(compatiblePath,
               {{"nested/DESCRIPTION.XML", StandardsValidXml()}});
  const GdtfInspectionResult compatible =
      perastage::inspection::InspectGdtf(compatiblePath);
  assert(compatible.status == GdtfReadStatus::CompatibilityAccepted);
  assert(Validation(compatible, ValidationLayer::Schema).status ==
         ValidationStatus::Valid);
  assert(HasDiagnostic(compatible, "gdtf.archive.non_canonical_description_xml",
                       DiagnosticClassification::Compatibility));
}

// Verifies canonical inspection, source preservation, and direct-reader parity.
void TestCanonicalAndParity(const fs::path &directory) {
  const fs::path path = directory / "canonical.gdtf";
  const std::string xml = CompleteXml();
  WriteArchive(path, {{"description.xml", xml},
                      {"wheels/blue.png", "png"},
                      {"models/\xC3\xA9"
                       "clairage.glb",
                       "glb"}});
  const std::vector<unsigned char> before = ReadBytes(path);

  const GdtfInspectionResult inspected =
      perastage::inspection::InspectGdtf(path);
  assert(inspected.Success());
  assert(inspected.status == GdtfReadStatus::Canonical);
  assert(inspected.packageInventory);
  assert(inspected.packageInventory->entries.size() == 3);
  assert(inspected.packageInventory->canonicalRootDocumentPresent);
  assert(inspected.packageInventory->entries.back().displayPath ==
         "models/\xC3\xA9"
         "clairage.glb");
  assert(inspected.document);
  const auto &archive = inspected.document->Archive();
  const auto &description = inspected.document->Description();
  assert(archive.descriptionXml == xml);
  assert(archive.descriptionEntryPath == "description.xml");
  assert(archive.standardsCompliantDescriptionLocation);
  assert(!archive.usedCompatibilityDescriptionFallback);
  assert(description.dataVersion == "1.2");
  assert(description.fixtureTypeName == "Inspect Fixture");
  assert(description.manufacturer == "Perastage");
  assert(description.weightKgPresent && description.weightKg == 12.5f);
  assert(description.dmxModeNames ==
         std::vector<std::string>({"Mode A", "Mode B"}));
  assert(description.wheels.size() == 1);
  assert(description.wheels.front().slots.front().mediaFileName == "blue");
  assert(description.wheels.front().slots.front().resourceReferences ==
         std::vector<std::string>({"wheels/blue.png"}));

  const gdtf::ArchiveReadResult directArchive = gdtf::ReadGdtfArchive(path);
  const gdtf::GdtfDescriptionSnapshot directDescription =
      gdtf::ReadGdtfDescription(directArchive.descriptionXml,
                                {"description.xml", "wheels/blue.png",
                                 "models/\xC3\xA9"
                                 "clairage.glb"});
  assert(archive.descriptionXml == directArchive.descriptionXml);
  assert(archive.descriptionEntryPath == directArchive.descriptionEntryPath);
  assert(description.fixtureTypeName == directDescription.fixtureTypeName);
  assert(description.dmxModeNames == directDescription.dmxModeNames);
  assert(description.wheels.front().slots.front().resourceReferences ==
         directDescription.wheels.front().slots.front().resourceReferences);
  assert(ReadBytes(path) == before);

  const GdtfInspectionResult repeated =
      perastage::inspection::InspectGdtf(path);
  assert(repeated.document->Description().dmxModeNames ==
         description.dmxModeNames);
  assert(repeated.inspection.diagnostics.size() ==
         inspected.inspection.diagnostics.size());
}

// Verifies accepted noncanonical description selection is explicit.
void TestCompatibilityDescription(const fs::path &directory) {
  const fs::path path = directory / "compatible.gdtf";
  WriteArchive(path, {{"nested/DESCRIPTION.XML", CompleteXml()}});
  const GdtfInspectionResult inspected =
      perastage::inspection::InspectGdtf(path);
  assert(inspected.Success());
  assert(inspected.status == GdtfReadStatus::CompatibilityAccepted);
  assert(inspected.document->Archive().descriptionEntryPath ==
         "nested/DESCRIPTION.XML");
  assert(!inspected.document->Archive().standardsCompliantDescriptionLocation);
  assert(inspected.document->Archive().usedCompatibilityDescriptionFallback);
  assert(HasDiagnostic(inspected, "gdtf.archive.non_canonical_description_xml",
                       DiagnosticClassification::Compatibility));
}

// Verifies malformed XML retains package inventory and becomes a neutral fatal.
void TestMalformedXml(const fs::path &directory) {
  const fs::path path = directory / "malformed.gdtf";
  WriteArchive(path, {{"description.xml", "<GDTF><FixtureType>"}});
  const std::vector<unsigned char> before = ReadBytes(path);
  const GdtfInspectionResult inspected =
      perastage::inspection::InspectGdtf(path);
  assert(inspected.packageInventory);
  assert(!inspected.Success());
  assert(HasDiagnostic(inspected, "gdtf.description.malformed_xml",
                       DiagnosticClassification::General));
  assert(Validation(inspected, ValidationLayer::XmlWellFormedness).status ==
         ValidationStatus::Invalid);
  assert(Validation(inspected, ValidationLayer::Schema).status ==
         ValidationStatus::NotRun);
  assert(ReadBytes(path) == before);
}

// Verifies existing missing-mode and resource diagnostics remain structured.
void TestSemanticDiagnostics(const fs::path &directory) {
  const fs::path path = directory / "semantic.gdtf";
  const std::string xml =
      "<GDTF DataVersion=\"1.2\"><FixtureType Name=\"Incomplete\">"
      "<Wheels><Wheel Name=\"Gobo\"><Slot Name=\"One\" "
      "MediaFileName=\"missing\"/></Wheel></Wheels></FixtureType></GDTF>";
  WriteArchive(path, {{"description.xml", xml}, {"marker.txt", "x"}});
  const GdtfInspectionResult inspected =
      perastage::inspection::InspectGdtf(path);
  assert(inspected.Success());
  assert(HasDiagnostic(inspected, "gdtf.description.missing_dmx_modes",
                       DiagnosticClassification::Standards));
  assert(HasDiagnostic(inspected,
                       "gdtf.description.missing_wheel_media_resource",
                       DiagnosticClassification::General));
}

// Verifies case-compatible wheel resources are classified as compatibility.
void TestWheelCaseCompatibility(const fs::path &directory) {
  const fs::path path = directory / "wheel-case.gdtf";
  WriteArchive(path,
               {{"description.xml", CompleteXml()}, {"WHEELS/BLUE.PNG", "x"}});
  const GdtfInspectionResult inspected =
      perastage::inspection::InspectGdtf(path);
  assert(inspected.Success());
  assert(inspected.status == GdtfReadStatus::CompatibilityAccepted);
  assert(HasDiagnostic(inspected,
                       "gdtf.description.non_canonical_wheel_media_case_match",
                       DiagnosticClassification::Compatibility));
}

// Verifies non-GDTF inputs stop after neutral package classification.
void TestUnsupportedInput(const fs::path &directory) {
  const fs::path path = directory / "scene.mvr";
  WriteArchive(path,
               {{"GeneralSceneDescription.xml", "<GeneralSceneDescription/>"}});
  const GdtfInspectionResult inspected =
      perastage::inspection::InspectGdtf(path);
  assert(!inspected.Success());
  assert(!inspected.document);
  assert(HasDiagnostic(inspected, "gdtf.input.unsupported_package_kind",
                       DiagnosticClassification::General));
}

} // namespace

// Runs focused production-path GDTF inspection coverage.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());
  const fs::path directory =
      fs::temp_directory_path() / "perastage_gdtf_inspection_test";
  fs::remove_all(directory);
  fs::create_directories(directory);
  TestCanonicalAndParity(directory);
  TestValidationLayers(directory);
  TestCompatibilityDescription(directory);
  TestMalformedXml(directory);
  TestSemanticDiagnostics(directory);
  TestWheelCaseCompatibility(directory);
  TestUnsupportedInput(directory);
  fs::remove_all(directory);
  return 0;
}

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
using perastage::inspection::Diagnostic;
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

// Compares every stable field of two adapted diagnostics.
void AssertDiagnosticEqual(const Diagnostic &left, const Diagnostic &right) {
  assert(left.severity == right.severity);
  assert(left.domain == right.domain);
  assert(left.classification == right.classification);
  assert(left.code == right.code);
  assert(left.message == right.message);
  assert(left.location.has_value() == right.location.has_value());
  if (left.location) {
    assert(left.location->sourcePath == right.location->sourcePath);
    assert(left.location->packageEntry == right.location->packageEntry);
    assert(left.location->xmlPath == right.location->xmlPath);
    assert(left.location->line == right.location->line);
    assert(left.location->column == right.location->column);
  }
}

// Compares all stable public GDTF facts except intentional source identity.
void AssertStableResultEqual(const GdtfInspectionResult &left,
                             const GdtfInspectionResult &right) {
  assert(left.status == right.status);
  assert(left.Success() == right.Success());
  assert(left.inspection.request.sourcePath ==
         right.inspection.request.sourcePath);
  assert(left.inspection.diagnostics.size() ==
         right.inspection.diagnostics.size());
  for (std::size_t index = 0; index < left.inspection.diagnostics.size();
       ++index)
    AssertDiagnosticEqual(left.inspection.diagnostics[index],
                          right.inspection.diagnostics[index]);
  assert(left.packageInventory.has_value() ==
         right.packageInventory.has_value());
  if (left.packageInventory) {
    assert(left.packageInventory->kind == right.packageInventory->kind);
    assert(left.packageInventory->canonicalRootDocumentPresent ==
           right.packageInventory->canonicalRootDocumentPresent);
    assert(left.packageInventory->entries.size() ==
           right.packageInventory->entries.size());
    for (std::size_t index = 0; index < left.packageInventory->entries.size();
         ++index) {
      const auto &a = left.packageInventory->entries[index];
      const auto &b = right.packageInventory->entries[index];
      assert(a.displayPath == b.displayPath);
      assert(a.normalizedPath == b.normalizedPath);
      assert(a.extension == b.extension);
      assert(a.type == b.type);
      assert(a.uncompressedSize == b.uncompressedSize);
      assert(a.sizeKnown == b.sizeKnown);
      assert(a.pathSafe == b.pathSafe);
    }
  }
  assert(left.validation.size() == right.validation.size());
  for (std::size_t index = 0; index < left.validation.size(); ++index) {
    const auto &a = left.validation[index];
    const auto &b = right.validation[index];
    assert(a.layer == b.layer);
    assert(a.status == b.status);
    assert(a.schema.has_value() == b.schema.has_value());
    if (a.schema) {
      assert(a.schema->format == b.schema->format);
      assert(a.schema->formatVersion == b.schema->formatVersion);
      assert(a.schema->schemaVersion == b.schema->schemaVersion);
      assert(a.schema->provenance == b.schema->provenance);
      assert(a.schema->sourceRevision == b.schema->sourceRevision);
    }
    assert(a.diagnostics.size() == b.diagnostics.size());
    for (std::size_t diagnostic = 0; diagnostic < a.diagnostics.size();
         ++diagnostic)
      AssertDiagnosticEqual(a.diagnostics[diagnostic],
                            b.diagnostics[diagnostic]);
  }
  assert(left.document.has_value() == right.document.has_value());
  if (!left.document)
    return;
  const auto &leftArchive = left.document->Archive();
  const auto &rightArchive = right.document->Archive();
  const auto &leftDescription = left.document->Description();
  const auto &rightDescription = right.document->Description();
  assert(leftArchive.descriptionXml == rightArchive.descriptionXml);
  assert(leftArchive.descriptionEntryPath == rightArchive.descriptionEntryPath);
  assert(leftArchive.entries.size() == rightArchive.entries.size());
  for (std::size_t index = 0; index < leftArchive.entries.size(); ++index) {
    const auto &a = leftArchive.entries[index];
    const auto &b = rightArchive.entries[index];
    assert(a.path == b.path);
    assert(a.size == b.size);
    assert(a.sizeKnown == b.sizeKnown);
    assert(a.directory == b.directory);
    assert(a.nameUsedUtf8CompatibilityFallback ==
           b.nameUsedUtf8CompatibilityFallback);
  }
  assert(leftArchive.diagnostics.size() == rightArchive.diagnostics.size());
  for (std::size_t index = 0; index < leftArchive.diagnostics.size(); ++index) {
    const auto &a = leftArchive.diagnostics[index];
    const auto &b = rightArchive.diagnostics[index];
    assert(a.code == b.code);
    assert(a.message == b.message);
    assert(a.entryPath == b.entryPath);
  }
  assert(leftArchive.usedCompatibilityDescriptionFallback ==
         rightArchive.usedCompatibilityDescriptionFallback);
  assert(leftArchive.standardsCompliantDescriptionLocation ==
         rightArchive.standardsCompliantDescriptionLocation);
  assert(leftArchive.utf8FlagMissingEntryCount ==
         rightArchive.utf8FlagMissingEntryCount);
  assert(leftDescription.dataVersion == rightDescription.dataVersion);
  assert(leftDescription.fixtureTypeName == rightDescription.fixtureTypeName);
  assert(leftDescription.manufacturer == rightDescription.manufacturer);
  assert(leftDescription.shortName == rightDescription.shortName);
  assert(leftDescription.longName == rightDescription.longName);
  assert(leftDescription.description == rightDescription.description);
  assert(leftDescription.fixtureTypeId == rightDescription.fixtureTypeId);
  assert(leftDescription.thumbnail == rightDescription.thumbnail);
  assert(leftDescription.createDate == rightDescription.createDate);
  assert(leftDescription.revision == rightDescription.revision);
  assert(leftDescription.weightKgPresent == rightDescription.weightKgPresent);
  assert(leftDescription.weightKg == rightDescription.weightKg);
  assert(leftDescription.powerConsumptionWPresent ==
         rightDescription.powerConsumptionWPresent);
  assert(leftDescription.powerConsumptionW ==
         rightDescription.powerConsumptionW);
  assert(leftDescription.modelColorHex == rightDescription.modelColorHex);
  assert(leftDescription.trussCrossSectionType ==
         rightDescription.trussCrossSectionType);
  assert(leftDescription.trussCrossSection ==
         rightDescription.trussCrossSection);
  assert(leftDescription.revisions.size() == rightDescription.revisions.size());
  for (std::size_t index = 0; index < leftDescription.revisions.size();
       ++index) {
    const auto &a = leftDescription.revisions[index];
    const auto &b = rightDescription.revisions[index];
    assert(a.text == b.text);
    assert(a.date == b.date);
    assert(a.userId == b.userId);
    assert(a.modifiedBy == b.modifiedBy);
  }
  assert(leftDescription.dmxModeNames == rightDescription.dmxModeNames);
  assert(leftDescription.wheels.size() == rightDescription.wheels.size());
  for (std::size_t wheel = 0; wheel < leftDescription.wheels.size(); ++wheel) {
    assert(leftDescription.wheels[wheel].name ==
           rightDescription.wheels[wheel].name);
    assert(leftDescription.wheels[wheel].slots.size() ==
           rightDescription.wheels[wheel].slots.size());
    for (std::size_t slot = 0;
         slot < leftDescription.wheels[wheel].slots.size(); ++slot) {
      const auto &a = leftDescription.wheels[wheel].slots[slot];
      const auto &b = rightDescription.wheels[wheel].slots[slot];
      assert(a.name == b.name);
      assert(a.mediaFileName == b.mediaFileName);
      assert(a.resourceReferences == b.resourceReferences);
    }
  }
  assert(leftDescription.diagnostics.size() ==
         rightDescription.diagnostics.size());
  for (std::size_t index = 0; index < leftDescription.diagnostics.size();
       ++index) {
    const auto &a = leftDescription.diagnostics[index];
    const auto &b = rightDescription.diagnostics[index];
    assert(a.code == b.code);
    assert(a.message == b.message);
    assert(a.path == b.path);
  }
  assert(left.document->Modes() == right.document->Modes());
  assert(left.document->RepeatedFamilies().size() ==
         right.document->RepeatedFamilies().size());
  for (std::size_t index = 0; index < left.document->RepeatedFamilies().size();
       ++index) {
    assert(left.document->RepeatedFamilies()[index].familyKind ==
           right.document->RepeatedFamilies()[index].familyKind);
    assert(left.document->RepeatedFamilies()[index].names ==
           right.document->RepeatedFamilies()[index].names);
  }
  assert(left.document->Valid() == right.document->Valid());
}

// Compares source identity for repeated use of the same GDTF entry point.
void AssertSourceIdentityEqual(const GdtfInspectionResult &left,
                               const GdtfInspectionResult &right) {
  assert(left.document.has_value() == right.document.has_value());
  if (left.document) {
    assert(left.document->SourcePath() == right.document->SourcePath());
    assert(left.document->SourceFilePresent() ==
           right.document->SourceFilePresent());
  }
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

// Finds a classification in all public GDTF diagnostics.
bool HasClassification(const GdtfInspectionResult &result,
                       DiagnosticClassification classification) {
  const auto matches = [classification](const Diagnostic &diagnostic) {
    return diagnostic.classification == classification;
  };
  if (std::any_of(result.inspection.diagnostics.begin(),
                  result.inspection.diagnostics.end(), matches))
    return true;
  for (const auto &validation : result.validation) {
    if (std::any_of(validation.diagnostics.begin(),
                    validation.diagnostics.end(), matches))
      return true;
  }
  return false;
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
  assert(!HasClassification(valid, DiagnosticClassification::Compatibility));

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
  assert(HasClassification(invalid, DiagnosticClassification::Standards));
  assert(!HasClassification(invalid, DiagnosticClassification::Compatibility));

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
  assert(!HasClassification(compatible, DiagnosticClassification::Standards));
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
  const gdtf::GdtfDocument directDocument = gdtf::LoadGdtfDocument(path);
  const gdtf::GdtfResourceReadResult directResource =
      gdtf::ReadGdtfArchiveResource(path, "wheels/blue.png");
  assert(archive.descriptionXml == directArchive.descriptionXml);
  assert(archive.descriptionEntryPath == directArchive.descriptionEntryPath);
  assert(description.fixtureTypeName == directDescription.fixtureTypeName);
  assert(description.dataVersion == directDescription.dataVersion);
  assert(description.manufacturer == directDescription.manufacturer);
  assert(description.dmxModeNames == directDescription.dmxModeNames);
  assert(description.wheels.front().slots.front().resourceReferences ==
         directDescription.wheels.front().slots.front().resourceReferences);
  assert(directDocument.Valid());
  assert(directDocument.Description().fixtureTypeId ==
         description.fixtureTypeId);
  assert(directResource.Success());
  assert(directResource.entryPath == "wheels/blue.png");
  assert(directResource.bytes == std::vector<unsigned char>({'p', 'n', 'g'}));
  assert(ReadBytes(path) == before);

  const GdtfInspectionResult repeated =
      perastage::inspection::InspectGdtf(path);
  AssertStableResultEqual(inspected, repeated);
  AssertSourceIdentityEqual(inspected, repeated);
}

// Verifies file and owned-byte parity, determinism, Unicode, and non-mutation.
void TestOwnedBytesParity(const fs::path &directory) {
  const fs::path unicodeDirectory =
      directory / fs::path(std::u8string(u8"fixture-path-é"));
  fs::create_directories(unicodeDirectory);
  const fs::path path = unicodeDirectory / "parity.gdtf";
  std::string xml = CompleteXml();
  const std::string asciiReference =
      "MediaFileName=\"blue\" Gobo=\"wheels/blue.png\"";
  const std::string unicodeReference =
      "MediaFileName=\"蓝色\" Gobo=\"wheels/蓝色.png\"";
  assert(xml.find(asciiReference) != std::string::npos);
  xml.replace(xml.find(asciiReference), asciiReference.size(),
              unicodeReference);
  WriteArchive(path, {{"description.xml", xml},
                      {"wheels/蓝色.png", "unicode-image"},
                      {"models/nested/灯具.glb", "model"}});
  const std::vector<unsigned char> bytes = ReadBytes(path);
  const std::vector<unsigned char> originalBytes = bytes;
  const gdtf::ArchiveReadResult directArchive = gdtf::ReadGdtfArchive(path);
  assert(std::any_of(
      directArchive.entries.begin(), directArchive.entries.end(),
      [](const auto &entry) { return entry.path == "wheels/蓝色.png"; }));
  const gdtf::GdtfDescriptionSnapshot directDescription =
      gdtf::ReadGdtfDescription(
          directArchive.descriptionXml,
          {"description.xml", "wheels/蓝色.png", "models/nested/灯具.glb"});
  assert(directDescription.wheels.front().slots.front().resourceReferences ==
         std::vector<std::string>{"wheels/蓝色.png"});
  const gdtf::GdtfResourceReadResult directResource =
      gdtf::ReadGdtfArchiveResource(path, "wheels/蓝色.png");
  assert(directResource.Success());
  assert(directResource.entryPath == "wheels/蓝色.png");
  assert(directResource.bytes ==
         std::vector<unsigned char>({'u', 'n', 'i', 'c', 'o', 'd', 'e', '-',
                                     'i', 'm', 'a', 'g', 'e'}));
  const perastage::inspection::Request logicalRequest{path};
  const GdtfInspectionResult fromFile =
      perastage::inspection::InspectGdtf(path);
  const GdtfInspectionResult fromBytes =
      perastage::inspection::InspectGdtf(bytes, logicalRequest);
  const GdtfInspectionResult repeated =
      perastage::inspection::InspectGdtf(bytes, logicalRequest);
  AssertStableResultEqual(fromFile, fromBytes);
  AssertStableResultEqual(fromBytes, repeated);
  AssertSourceIdentityEqual(fromBytes, repeated);
  assert(fromFile.document->Description()
             .wheels.front()
             .slots.front()
             .resourceReferences ==
         directDescription.wheels.front().slots.front().resourceReferences);
  assert(fromFile.document->SourceFilePresent());
  assert(fromFile.document->SourcePath() == path);
  assert(!fromBytes.document->SourceFilePresent());
  assert(fromBytes.document->SourcePath().empty());
  assert(bytes == ReadBytes(path));
  assert(bytes == originalBytes);
}

// Verifies standards and compatibility findings remain distinct when mixed.
void TestMixedDiagnosticClassifications(const fs::path &directory) {
  const fs::path path = directory / "mixed.gdtf";
  WriteArchive(path, {{"nested/DESCRIPTION.XML",
                       "<GDTF DataVersion=\"1.2\"><FixtureType Name=\"Mixed\" "
                       "Manufacturer=\"Perastage\"/></GDTF>"}});
  const GdtfInspectionResult inspected =
      perastage::inspection::InspectGdtf(path);
  assert(HasDiagnostic(inspected, "gdtf.archive.non_canonical_description_xml",
                       DiagnosticClassification::Compatibility));
  assert(HasDiagnostic(inspected, "gdtf.description.missing_dmx_modes",
                       DiagnosticClassification::Standards));
  assert(HasClassification(inspected, DiagnosticClassification::Compatibility));
  assert(HasClassification(inspected, DiagnosticClassification::Standards));
}

// Verifies malformed owned inputs return structured fatal results repeatedly.
void TestMalformedOwnedInputMatrix() {
  const std::vector<std::vector<unsigned char>> corpus = {
      {},
      {'n', 'o', 't', '-', 'z', 'i', 'p'},
      {'P', 'K', 3, 4, 0, 0, 0, 0},
  };
  for (std::size_t index = 0; index < corpus.size(); ++index) {
    const perastage::inspection::Request request{
        fs::path("malformed-" + std::to_string(index) + ".gdtf")};
    const auto before = corpus[index];
    const GdtfInspectionResult first =
        perastage::inspection::InspectGdtf(corpus[index], request);
    const GdtfInspectionResult second =
        perastage::inspection::InspectGdtf(corpus[index], request);
    assert(!first.Success());
    assert(first.inspection.HasFatalDiagnostics());
    AssertStableResultEqual(first, second);
    assert(corpus[index] == before);
  }
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
  TestOwnedBytesParity(directory);
  TestMixedDiagnosticClassifications(directory);
  TestMalformedOwnedInputMatrix();
  TestValidationLayers(directory);
  TestCompatibilityDescription(directory);
  TestMalformedXml(directory);
  TestSemanticDiagnostics(directory);
  TestWheelCaseCompatibility(directory);
  TestUnsupportedInput(directory);
  fs::remove_all(directory);
  return 0;
}

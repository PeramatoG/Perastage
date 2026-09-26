#include "inspection/inspection_report_json_serializer.h"

#include "json.hpp"

#include <cassert>
#include <string>

using namespace perastage::inspection;
using namespace perastage::inspection::serialization;

namespace {

// Creates a structured diagnostic for ordering and classification assertions.
Diagnostic Finding(DiagnosticSeverity severity,
                   DiagnosticClassification classification, std::string code) {
  return {severity,        DiagnosticDomain::Content,   classification,
          std::move(code), "Synthetic report finding.", std::nullopt};
}

// Creates a shared package inventory used by complete report checks.
PackageInventory Package(PackageKind kind) {
  PackageInventory inventory;
  inventory.kind = kind;
  inventory.canonicalRootDocumentPresent = true;
  inventory.entries.push_back(
      {"root.xml", "root.xml", ".xml", PackageEntryType::File, 42, true, true});
  return inventory;
}

// Creates one fully populated resource descriptor for report checks.
ResourceDescriptor Resource(PackageKind kind) {
  ResourceDescriptor resource;
  resource.displayPath = "資料/resource.xml";
  resource.normalizedPath = resource.displayPath;
  resource.packageKind = kind;
  resource.size = 42;
  resource.sizeKnown = true;
  resource.pathSafe = true;
  resource.kind = ResourceKind::XmlText;
  resource.rawReadSupported = true;
  resource.textPreviewSupported = true;
  return resource;
}

// Creates one validation layer with an ordered standards diagnostic.
ValidationResult Validation() {
  ValidationResult validation;
  validation.layer = ValidationLayer::Schema;
  validation.status = ValidationStatus::Invalid;
  validation.diagnostics.push_back(Finding(DiagnosticSeverity::Warning,
                                           DiagnosticClassification::Standards,
                                           "schema.warning"));
  return validation;
}

// Verifies the complete deterministic MVR report projection.
void TestMvrReport() {
  MvrInspectionResult result;
  result.inspection.request.sourcePath =
      std::filesystem::path(u8"資料/灯具-ñ.mvr");
  result.inspection.diagnostics = {
      Finding(DiagnosticSeverity::Information,
              DiagnosticClassification::General, "report.first"),
      Finding(DiagnosticSeverity::Warning,
              DiagnosticClassification::Compatibility, "report.second")};
  result.packageInventory = Package(PackageKind::Mvr);
  result.validation = {Validation()};
  result.snapshot.emplace();
  auto &snapshot = *result.snapshot;
  snapshot.versionMajor = 1;
  snapshot.versionMinor = 6;
  snapshot.provider = "Pérastage";
  snapshot.providerVersion = "1.7";
  snapshot.sceneDescriptionEntry = "GeneralSceneDescription.xml";
  snapshot.sceneDescriptionXml = "<GeneralSceneDescription/>";
  snapshot.embeddedGdtfEntries = {"fixtures/灯具.gdtf"};
  snapshot.referencedResources = {{"geometry", "models/fixture.glb"}};
  snapshot.fixtures.push_back({"fixture",
                               "fixture-1",
                               "灯具",
                               "layer-1",
                               "Main",
                               {},
                               "fixtures/灯具.gdtf",
                               {}});
  snapshot.nodeCounts = {{"layers", 1}, {"fixtures", 1}};

  const std::vector<ResourceDescriptor> resources = {
      Resource(PackageKind::Mvr)};
  const std::string first = SerializeMvrReportToJson(result, resources);
  const std::string second = SerializeMvrReportToJson(result, resources);
  const nlohmann::json json = nlohmann::json::parse(first);

  assert(first == second);
  assert(json.at("schema_version") == kInspectionJsonSchemaVersion);
  assert(json.at("format") == "MVR");
  assert(json.at("request").at("source_path") == "資料/灯具-ñ.mvr");
  assert(json.at("diagnostics").at(0).at("code") == "report.first");
  assert(json.at("diagnostics").at(1).at("classification") == "compatibility");
  assert(json.at("package").at("entries").at(0).at("display_path") ==
         "root.xml");
  assert(json.at("resources").at(0).at("kind") == "xml_text");
  assert(json.at("validation").at(0).at("layer") == "schema");
  assert(json.at("validation")
             .at(0)
             .at("diagnostics")
             .at(0)
             .at("classification") == "standards");
  const auto &serialized = json.at("snapshot");
  assert(serialized.at("version_major") == 1);
  assert(serialized.at("provider") == "Pérastage");
  assert(serialized.at("root_xml") == "<GeneralSceneDescription/>");
  assert(serialized.at("fixtures").at(0).at("name") == "灯具");
  assert(serialized.at("node_counts").at(1).at("count") == 1);
  assert(serialized.at("embedded_gdtfs").at(0) == "fixtures/灯具.gdtf");
  assert(serialized.at("referenced_resources").at(0).at("archive_path") ==
         "models/fixture.glb");
  assert(first.find("workspace") == std::string::npos);
}

// Verifies the complete deterministic GDTF report projection.
void TestGdtfReport() {
  GdtfInspectionResult result;
  result.inspection.request.sourcePath = "fixture.gdtf";
  result.inspection.diagnostics = {
      Finding(DiagnosticSeverity::Warning,
              DiagnosticClassification::Compatibility, "gdtf.compatibility")};
  result.packageInventory = Package(PackageKind::Gdtf);
  result.validation = {Validation()};
  result.status = GdtfReadStatus::CompatibilityAccepted;

  gdtf::ArchiveReadResult archive;
  archive.descriptionXml = "<GDTF/>";
  archive.descriptionEntryPath = "Description.xml";
  archive.usedCompatibilityDescriptionFallback = true;
  archive.standardsCompliantDescriptionLocation = false;
  archive.utf8FlagMissingEntryCount = 2;
  gdtf::GdtfDescriptionSnapshot description;
  description.dataVersion = "1.2";
  description.fixtureTypeName = "Fixture";
  description.manufacturer = "Perastage";
  description.shortName = "Short";
  description.longName = "Long";
  description.description = "Complete report";
  description.revisions = {{"Initial", "2026-01-01", "7", "Tester"}};
  description.dmxModeNames = {"Mode A", "Mode B"};
  description.wheels = {{"Color", {{"Red", "red.png", {"wheels/red.png"}}}},
                        {"Gobo", {}}};
  result.document.emplace(std::move(archive), std::move(description));

  const std::vector<ResourceDescriptor> resources = {
      Resource(PackageKind::Gdtf)};
  const std::string first = SerializeGdtfReportToJson(result, resources);
  const std::string second = SerializeGdtfReportToJson(result, resources);
  const nlohmann::json json = nlohmann::json::parse(first);
  const auto &document = json.at("document");

  assert(first == second);
  assert(json.at("schema_version") == kInspectionJsonSchemaVersion);
  assert(json.at("format") == "GDTF");
  assert(json.at("status") == "compatibility_accepted");
  assert(document.at("root_xml") == "<GDTF/>");
  assert(document.at("manufacturer") == "Perastage");
  assert(document.at("revisions").at(0).at("text") == "Initial");
  assert(document.at("wheels")
             .at(0)
             .at("slots")
             .at(0)
             .at("resource_references")
             .at(0) == "wheels/red.png");
  assert(document.at("valid") == true);
  assert(document.at("modes") == nlohmann::json({"Mode A", "Mode B"}));
  assert(document.at("repeated_families") ==
         nlohmann::json::parse(
             R"([{"family_kind":"wheel","names":["Color","Gobo"]}])"));
  const auto &archiveJson = document.at("archive");
  assert(archiveJson.at("description_entry_path") == "Description.xml");
  assert(archiveJson.at("used_compatibility_description_fallback") == true);
  assert(archiveJson.at("standards_compliant_description_location") == false);
  assert(archiveJson.at("utf8_flag_missing_entry_count") == 2);
  assert(json.at("package").at("kind") == "gdtf");
  assert(json.at("resources").at(0).at("raw_read_supported") == true);
  assert(json.at("validation").at(0).at("status") == "invalid");
  assert(first.find("workspace") == std::string::npos);
}

} // namespace

// Runs complete report serializer characterization independently of CLI
// presentation.
int main() {
  TestMvrReport();
  TestGdtfReport();
}

#include "inspection/inspection_json_serializer.h"
#include "inspection_outcome.h"
#include "inspection_text_formatter.h"

#include "json.hpp"

#include <iostream>
#include <string>

namespace {

// Reports a failed presentation assertion with its stable label.
bool Expect(bool condition, const char *label) {
  if (!condition)
    std::cerr << "Failed: " << label << '\n';
  return condition;
}

// Creates one diagnostic with the requested structured classification.
perastage::inspection::Diagnostic
Finding(perastage::inspection::DiagnosticSeverity severity,
        perastage::inspection::DiagnosticClassification classification) {
  return {severity,
          perastage::inspection::DiagnosticDomain::Content,
          classification,
          "test.finding",
          "Synthetic finding.",
          std::nullopt};
}

// Verifies the independently testable exit-code classification contract.
bool CheckOutcome() {
  using namespace perastage;
  inspection::Result clean{{"clean.mvr"}, {}};
  inspection::ValidationResult layer;
  bool passed = true;
  passed &= Expect(cli::ClassifyInspectionOutcome(clean, {}, true) == 0,
                   "clean outcome");
  clean.diagnostics.push_back(
      Finding(inspection::DiagnosticSeverity::Information,
              inspection::DiagnosticClassification::General));
  passed &= Expect(cli::ClassifyInspectionOutcome(clean, {}, true) == 0,
                   "information outcome");
  clean.diagnostics = {
      Finding(inspection::DiagnosticSeverity::Warning,
              inspection::DiagnosticClassification::Compatibility)};
  passed &= Expect(cli::ClassifyInspectionOutcome(clean, {}, true) == 1,
                   "compatibility warning outcome");
  layer.diagnostics = {
      Finding(inspection::DiagnosticSeverity::Error,
              inspection::DiagnosticClassification::Standards)};
  passed &= Expect(cli::ClassifyInspectionOutcome({{"error.mvr"}, {}},
                                                  {&layer, 1}, true) == 1,
                   "inspectable standards error outcome");
  clean.diagnostics = {Finding(inspection::DiagnosticSeverity::Fatal,
                               inspection::DiagnosticClassification::General)};
  passed &= Expect(cli::ClassifyInspectionOutcome(clean, {}, false) == 3,
                   "fatal outcome");
  return passed;
}

// Verifies deterministic human formatters from synthetic structured facts.
bool CheckFormatters() {
  using namespace perastage;
  inspection::PackageInventory inventory;
  inventory.entries.push_back({"description.xml", "description.xml", ".xml",
                               inspection::PackageEntryType::File, 12, true,
                               true});
  const std::string expectedInventory =
      "file | description.xml | size=12 | path=safe\n";
  bool passed = Expect(cli::FormatInventory(inventory) == expectedInventory,
                       "inventory golden");

  inspection::ResourceDescriptor resource;
  resource.displayPath = "description.xml";
  resource.normalizedPath = "description.xml";
  resource.size = 12;
  resource.sizeKnown = true;
  resource.pathSafe = true;
  resource.kind = inspection::ResourceKind::XmlText;
  resource.rawReadSupported = true;
  resource.textPreviewSupported = true;
  passed &= Expect(cli::FormatResources({resource}) ==
                       "description.xml | kind=xml-text | size=12 | "
                       "raw-read=yes | text-preview=yes | path=safe\n",
                   "resources golden");

  inspection::MvrInspectionResult mvr;
  mvr.inspection.request.sourcePath = "Unicode/escena-ñ.mvr";
  mvr.packageInventory = inventory;
  mvr.snapshot.emplace();
  mvr.snapshot->versionMajor = 1;
  mvr.snapshot->versionMinor = 6;
  mvr.snapshot->provider = "Pérastage";
  mvr.snapshot->providerVersion = "1";
  mvr.snapshot->sceneDescriptionXml = "<GeneralSceneDescription/>";
  mvr.snapshot->nodeCounts = {{"layers", 1}, {"fixtures", 2}};
  const std::string summary = cli::FormatMvrSummary(mvr, {resource});
  passed &= Expect(summary.find("Format: MVR\nStatus: inspected\n") == 0,
                   "MVR summary golden prefix");
  passed &= Expect(summary.find("Provider: Pérastage\n") != std::string::npos,
                   "MVR Unicode summary");

  mvr.inspection.diagnostics.push_back(
      Finding(inspection::DiagnosticSeverity::Warning,
              inspection::DiagnosticClassification::Compatibility));
  const std::string diagnostics = cli::FormatDiagnostics(mvr.inspection, {});
  passed &= Expect(diagnostics == "warning | compatibility | content | "
                                  "test.finding | Synthetic finding.\n",
                   "diagnostics golden");

  inspection::GdtfInspectionResult gdtf;
  gdtf.inspection.request.sourcePath = "fixture.gdtf";
  gdtf.packageInventory = inventory;
  gdtf.status = inspection::GdtfReadStatus::Canonical;
  gdtf::ArchiveReadResult archive;
  archive.descriptionXml = "<GDTF/>";
  gdtf::GdtfDescriptionSnapshot description;
  description.dataVersion = "1.2";
  description.manufacturer = "Perastage";
  description.fixtureTypeName = "Fixture";
  description.shortName = "Short";
  description.longName = "Long";
  description.dmxModeNames = {"Mode"};
  description.wheels.push_back({"Color", {}});
  gdtf.document.emplace(std::move(archive), std::move(description));
  const std::string gdtfSummary = cli::FormatGdtfSummary(gdtf, {resource});
  passed &= Expect(gdtfSummary.find("Format: GDTF\nStatus: canonical\n") == 0,
                   "GDTF summary golden prefix");
  passed &= Expect(
      gdtfSummary.find("Manufacturer: Perastage\nFixture type: Fixture\n") !=
          std::string::npos,
      "GDTF summary golden facts");
  return passed;
}

// Verifies complete deterministic JSON additions and retained XML.
bool CheckJson() {
  using namespace perastage;
  inspection::MvrInspectionResult result;
  result.inspection.request.sourcePath = "Unicode/escena-ñ.mvr";
  result.snapshot.emplace();
  result.snapshot->versionMajor = 1;
  result.snapshot->versionMinor = 6;
  result.snapshot->provider = "Pérastage";
  result.snapshot->sceneDescriptionXml = "<GeneralSceneDescription/>";
  result.snapshot->fixtures.push_back(
      {"fixture", "2", "Fixture", "1", "Layer", {}, "fixture.gdtf", {}});
  result.snapshot->nodeCounts.push_back({"fixtures", 1});
  const std::string first =
      inspection::serialization::SerializeMvrReportToJson(result, {});
  const std::string second =
      inspection::serialization::SerializeMvrReportToJson(result, {});
  const auto json = nlohmann::json::parse(first);
  bool passed = Expect(first == second, "deterministic JSON");
  passed &= Expect(json["schema_version"] == 1 && json["format"] == "MVR",
                   "JSON report identity");
  passed &= Expect(json["snapshot"]["root_xml"] == "<GeneralSceneDescription/>",
                   "retained XML JSON");
  passed &= Expect(first.find("workspace") == std::string::npos,
                   "no temporary path JSON");
  inspection::GdtfInspectionResult gdtf;
  gdtf.inspection.request.sourcePath = "fixture.gdtf";
  gdtf.status = inspection::GdtfReadStatus::Canonical;
  gdtf::ArchiveReadResult archive;
  archive.descriptionXml = "<GDTF/>";
  gdtf::GdtfDescriptionSnapshot description;
  description.dataVersion = "1.2";
  description.fixtureTypeName = "Fixture";
  gdtf.document.emplace(std::move(archive), std::move(description));
  const auto gdtfJson = nlohmann::json::parse(
      inspection::serialization::SerializeGdtfReportToJson(gdtf, {}));
  passed &= Expect(gdtfJson["format"] == "GDTF" &&
                       gdtfJson["document"]["root_xml"] == "<GDTF/>",
                   "GDTF report JSON");
  return passed;
}

} // namespace

// Runs CLI inspection outcome, formatter, and serialization checks.
int main() {
  return CheckOutcome() && CheckFormatters() && CheckJson() ? 0 : 1;
}

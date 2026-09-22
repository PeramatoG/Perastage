#include "inspection/mvr_inspection.h"

#include "mvr_import_package.h"
#include "mvrimporter.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <utility>

namespace perastage::inspection {
namespace {

// Appends a neutral diagnostic with optional package-entry context.
void AddDiagnostic(MvrInspectionResult &result, DiagnosticSeverity severity,
                   DiagnosticDomain domain,
                   DiagnosticClassification classification,
                   std::string code, std::string message,
                   std::optional<std::string> entry = std::nullopt) {
  Diagnostic diagnostic{severity, domain, classification, std::move(code),
                        std::move(message), std::nullopt};
  DiagnosticLocation location;
  location.sourcePath = result.inspection.request.sourcePath;
  location.packageEntry = std::move(entry);
  diagnostic.location = std::move(location);
  result.inspection.diagnostics.push_back(std::move(diagnostic));
}

// Reads a file without changing its contents or filesystem metadata.
std::vector<std::uint8_t> ReadBytes(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

// Converts a filesystem path to its unchanged UTF-8 byte representation.
std::string PathUtf8(const std::filesystem::path &path) {
  const std::u8string value = path.u8string();
  return {value.begin(), value.end()};
}

// Reads the selected scene XML while its temporary package lease is alive.
std::string ReadText(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

// Adds a unique scene resource reference to an ordered set.
void AddReference(std::set<std::pair<std::string, std::string>> &references,
                  const char *kind, const std::string &path) {
  if (!path.empty() && !std::filesystem::path(path).is_absolute())
    references.emplace(kind, mvr::NormalizeImportArchivePath(path));
}

// Derives deterministic summaries from the authoritative parsed scene.
MvrInspectionSnapshot BuildSnapshot(const MvrImportResult &parsed,
                                    const mvr::ImportPackage &package,
                                    const PackageInventory &inventory) {
  const MvrScene &scene = parsed.scene;
  MvrInspectionSnapshot snapshot;
  snapshot.versionMajor = scene.versionMajor;
  snapshot.versionMinor = scene.versionMinor;
  snapshot.provider = scene.provider;
  snapshot.providerVersion = scene.providerVersion;
  snapshot.sceneDescriptionEntry = PathUtf8(
      std::filesystem::relative(package.sceneXmlPath, package.rootPath));
  snapshot.sceneDescriptionXml = ReadText(package.sceneXmlPath);
  for (const PackageEntry &entry : inventory.entries) {
    if (entry.type == PackageEntryType::File && entry.pathSafe &&
        entry.extension == ".gdtf" && entry.normalizedPath)
      snapshot.embeddedGdtfEntries.push_back(*entry.normalizedPath);
  }

  std::set<std::pair<std::string, std::string>> references;
  for (const auto &[id, fixture] : scene.fixtures) {
    (void)id;
    AddReference(references, "gdtf", fixture.gdtfSpec);
  }
  for (const auto &[id, truss] : scene.trusses) {
    (void)id;
    AddReference(references, "gdtf", truss.gdtfSpec);
    AddReference(references, "geometry", truss.symbolFile);
    AddReference(references, "model", truss.modelFile);
  }
  for (const auto &[id, support] : scene.supports) {
    (void)id;
    AddReference(references, "gdtf", support.gdtfSpec);
    AddReference(references, "model", support.modelFile);
  }
  for (const auto &[id, object] : scene.sceneObjects) {
    (void)id;
    AddReference(references, "geometry", object.modelFile);
    for (const GeometryInstance &geometry : object.geometries)
      AddReference(references, "geometry", geometry.modelFile);
  }
  for (const auto &[kind, path] : references)
    snapshot.referencedResources.push_back({kind, path});

  snapshot.nodeCounts = {{"layers", scene.layers.size()},
                         {"fixtures", scene.fixtures.size()},
                         {"trusses", scene.trusses.size()},
                         {"supports", scene.supports.size()},
                         {"scene_objects", scene.sceneObjects.size()},
                         {"group_objects", scene.groupObjects.size()},
                         {"positions", scene.positions.size()},
                         {"symdefs", scene.symdefGeometries.size()}};
  return snapshot;
}

// Adapts established importer diagnostics without changing their messages.
void AppendImporterDiagnostics(MvrInspectionResult &result,
                               const MvrImportResult &parsed) {
  for (const MvrImportDiagnostic &source : parsed.diagnostics) {
    AddDiagnostic(result, DiagnosticSeverity::Warning,
                  DiagnosticDomain::Content,
                  DiagnosticClassification::Compatibility,
                  "mvr.import." + source.code, source.message);
  }
}

} // namespace

// Reports whether package and semantic reads produced a usable snapshot.
bool MvrInspectionResult::Success() const {
  return snapshot.has_value() && !inspection.HasFatalDiagnostics();
}

// Inspects owned bytes through package acquisition and parse-only importing.
MvrInspectionResult InspectMvrBytes(const std::vector<std::uint8_t> &bytes,
                                    const Request &request) {
  MvrInspectionResult result;
  result.inspection.request = request;
  if (bytes.empty()) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Input,
                  DiagnosticClassification::General, "mvr.input.empty",
                  "The MVR input is empty.");
    return result;
  }

  std::vector<MvrImportDiagnostic> packageDiagnostics;
  std::optional<mvr::ImportPackage> package =
      mvr::AcquireImportPackage(bytes, packageDiagnostics);
  for (const MvrImportDiagnostic &source : packageDiagnostics) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
                  DiagnosticClassification::Standards,
                  "mvr.package." + source.code, source.message);
  }
  if (!package) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
                  DiagnosticClassification::General,
                  "mvr.package.read_failed",
                  "The MVR package could not be safely read.");
    return result;
  }

  MvrImportResult parsed;
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  options.allowDummyFallback = false;
  MvrImporter importer;
  if (!importer.ImportFromBuffer(bytes, parsed, MvrImportMode::ParseOnly,
                                 options)) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Xml,
                  DiagnosticClassification::General, "mvr.xml.parse_failed",
                  "GeneralSceneDescription.xml could not be parsed.",
                  PathUtf8(std::filesystem::relative(package->sceneXmlPath,
                                                     package->rootPath)));
    return result;
  }
  AppendImporterDiagnostics(result, parsed);

  PackageInventory inventory;
  inventory.kind = PackageKind::Mvr;
  if (!request.sourcePath.empty()) {
    PackageInspectionResult packageResult = InspectPackage(request);
    for (Diagnostic &diagnostic : packageResult.inspection.diagnostics)
      result.inspection.diagnostics.push_back(std::move(diagnostic));
    if (packageResult.inventory)
      inventory = std::move(*packageResult.inventory);
  }
  result.packageInventory = inventory;
  result.snapshot = BuildSnapshot(parsed, *package, inventory);
  if (result.snapshot->sceneDescriptionEntry !=
      "GeneralSceneDescription.xml") {
    AddDiagnostic(result, DiagnosticSeverity::Warning,
                  DiagnosticDomain::Package,
                  DiagnosticClassification::Compatibility,
                  "mvr.package.non_canonical_scene_description",
                  "A case-insensitive legacy scene-description filename was accepted.",
                  result.snapshot->sceneDescriptionEntry);
  }
  return result;
}

// Inspects one filesystem MVR without modifying the source package.
MvrInspectionResult InspectMvr(const Request &request) {
  PackageInspectionResult package = InspectPackage(request);
  if (package.inspection.HasFatalDiagnostics()) {
    MvrInspectionResult result;
    result.inspection = std::move(package.inspection);
    result.packageInventory = std::move(package.inventory);
    return result;
  }
  MvrInspectionResult result = InspectMvrBytes(ReadBytes(request.sourcePath), request);
  if (package.inventory)
    result.packageInventory = std::move(package.inventory);
  return result;
}

// Wraps a filesystem path in the neutral MVR inspection request.
MvrInspectionResult InspectMvr(const std::filesystem::path &sourcePath) {
  return InspectMvr(Request{sourcePath});
}

} // namespace perastage::inspection

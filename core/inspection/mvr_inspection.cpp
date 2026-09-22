#include "inspection/mvr_inspection.h"

#include "archive_entry_path.h"
#include "mvr_import_package.h"
#include "mvr_read_service.h"

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
                   DiagnosticClassification classification, std::string code,
                   std::string message,
                   std::optional<std::string> entry = std::nullopt) {
  Diagnostic diagnostic{severity,           domain,
                        classification,     std::move(code),
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

// Returns descriptors in stable UUID order for any scene map.
template <typename Map, typename Convert>
std::vector<MvrSceneNodeDescriptor> SortedDescriptors(const Map &nodes,
                                                      Convert convert) {
  std::vector<MvrSceneNodeDescriptor> descriptors;
  descriptors.reserve(nodes.size());
  for (const auto &[uuid, node] : nodes) {
    (void)uuid;
    descriptors.push_back(convert(node));
  }
  std::sort(descriptors.begin(), descriptors.end(),
            [](const auto &left, const auto &right) {
              return left.uuid < right.uuid;
            });
  return descriptors;
}

// Converts group child references to stable UUID order.
std::vector<std::string> GroupChildUuids(const GroupObject &group) {
  std::vector<std::string> children;
  children.reserve(group.children.size());
  for (const GroupObjectChildRef &child : group.children)
    children.push_back(child.uuid);
  std::sort(children.begin(), children.end());
  return children;
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
    AddReference(references, "gdtf",
                 fixture.originalMvrGdtfSpec.empty()
                     ? fixture.gdtfSpec
                     : fixture.originalMvrGdtfSpec);
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

  snapshot.layers = SortedDescriptors(scene.layers, [](const Layer &layer) {
    std::vector<std::string> children = layer.childUUIDs;
    std::sort(children.begin(), children.end());
    return MvrSceneNodeDescriptor{"layer", layer.uuid, layer.name,         {},
                                  {},      {},         std::move(children)};
  });
  snapshot.fixtures =
      SortedDescriptors(scene.fixtures, [](const Fixture &node) {
        const std::string reference = node.originalMvrGdtfSpec.empty()
                                          ? node.gdtfSpec
                                          : node.originalMvrGdtfSpec;
        return MvrSceneNodeDescriptor{"fixture",
                                      node.uuid,
                                      node.instanceName,
                                      node.layer,
                                      node.parentGroupUuid,
                                      reference,
                                      {}};
      });
  snapshot.trusses = SortedDescriptors(scene.trusses, [](const Truss &node) {
    const std::string reference =
        !node.gdtfSpec.empty() ? node.gdtfSpec : node.symbolFile;
    return MvrSceneNodeDescriptor{
        "truss",   node.uuid, node.name, node.layer, node.parentGroupUuid,
        reference, {}};
  });
  snapshot.supports =
      SortedDescriptors(scene.supports, [](const Support &node) {
        const std::string reference =
            !node.gdtfSpec.empty() ? node.gdtfSpec : node.modelFile;
        return MvrSceneNodeDescriptor{
            "support", node.uuid, node.name, node.layer, node.parentGroupUuid,
            reference, {}};
      });
  snapshot.sceneObjects =
      SortedDescriptors(scene.sceneObjects, [](const SceneObject &node) {
        return MvrSceneNodeDescriptor{"scene_object",
                                      node.uuid,
                                      node.name,
                                      node.layer,
                                      node.parentGroupUuid,
                                      node.GetPrimaryModel(),
                                      {}};
      });
  snapshot.groupObjects =
      SortedDescriptors(scene.groupObjects, [](const GroupObject &node) {
        return MvrSceneNodeDescriptor{"group_object",
                                      node.uuid,
                                      node.name,
                                      node.layer,
                                      node.parentGroupUuid,
                                      {},
                                      GroupChildUuids(node)};
      });

  snapshot.nodeCounts = {{"layers", snapshot.layers.size()},
                         {"fixtures", snapshot.fixtures.size()},
                         {"trusses", snapshot.trusses.size()},
                         {"supports", snapshot.supports.size()},
                         {"scene_objects", snapshot.sceneObjects.size()},
                         {"group_objects", snapshot.groupObjects.size()},
                         {"positions", scene.positions.size()},
                         {"symdefs", scene.symdefGeometries.size()}};
  return snapshot;
}

// Maps known tolerant-reader findings and defaults unknown codes to General.
DiagnosticClassification ImportClassification(const std::string &code) {
  static const std::set<std::string> compatibilityCodes = {
      "legacy_perastage_metadata_missing_version"};
  static const std::set<std::string> standardsCodes = {
      "multiple_root_userdata", "invalid_root_userdata_child",
      "missing_userdata_provider"};
  if (compatibilityCodes.contains(code))
    return DiagnosticClassification::Compatibility;
  if (standardsCodes.contains(code))
    return DiagnosticClassification::Standards;
  return DiagnosticClassification::General;
}

// Adapts established importer diagnostics without changing their messages.
void AppendImporterDiagnostics(MvrInspectionResult &result,
                               const MvrImportResult &parsed) {
  for (const MvrImportDiagnostic &source : parsed.diagnostics) {
    AddDiagnostic(result, DiagnosticSeverity::Warning,
                  DiagnosticDomain::Content, ImportClassification(source.code),
                  "mvr.import." + source.code, source.message);
  }
}

// Reports safely provable missing archive resources without resolving online.
void AppendMissingResourceDiagnostics(MvrInspectionResult &result) {
  if (!result.snapshot || !result.packageInventory)
    return;
  std::set<std::string> packaged;
  for (const PackageEntry &entry : result.packageInventory->entries) {
    if (entry.pathSafe && entry.normalizedPath &&
        entry.type == PackageEntryType::File)
      packaged.insert(*entry.normalizedPath);
  }
  for (const MvrResourceReference &reference :
       result.snapshot->referencedResources) {
    const std::string normalized =
        archive::NormalizeEntrySeparators(reference.archivePath);
    if (normalized.empty() ||
        archive::IsUnsafeNormalizedEntryPath(normalized) ||
        packaged.contains(normalized))
      continue;
    AddDiagnostic(result, DiagnosticSeverity::Warning,
                  DiagnosticDomain::Content, DiagnosticClassification::General,
                  "mvr.resource.missing_packaged_resource",
                  "A referenced packaged resource is missing from the MVR.",
                  normalized);
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

  PackageInspectionResult packageInventory =
      InspectPackage(bytes, PackageKind::Mvr, request);
  result.inspection = std::move(packageInventory.inspection);
  result.packageInventory = std::move(packageInventory.inventory);
  if (result.inspection.HasFatalDiagnostics())
    return result;

  std::vector<MvrImportDiagnostic> packageDiagnostics;
  std::optional<mvr::ImportPackage> package =
      mvr::AcquireImportPackage(bytes, packageDiagnostics);
  for (const MvrImportDiagnostic &source : packageDiagnostics) {
    AddDiagnostic(result,
                  package ? DiagnosticSeverity::Warning
                          : DiagnosticSeverity::Fatal,
                  DiagnosticDomain::Package, DiagnosticClassification::General,
                  "mvr.package." + source.code, source.message);
  }
  if (!package) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Package,
                  DiagnosticClassification::General, "mvr.package.read_failed",
                  "The MVR package could not be safely read.");
    return result;
  }

  MvrImportResult parsed;
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  options.allowDummyFallback = false;
  if (!mvr::ReadAcquiredMvrPackage(*package, parsed, options)) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Xml,
                  DiagnosticClassification::General, "mvr.xml.parse_failed",
                  "GeneralSceneDescription.xml could not be parsed.",
                  PathUtf8(std::filesystem::relative(package->sceneXmlPath,
                                                     package->rootPath)));
    return result;
  }
  AppendImporterDiagnostics(result, parsed);

  result.snapshot = BuildSnapshot(parsed, *package, *result.packageInventory);
  if (result.snapshot->sceneDescriptionEntry != "GeneralSceneDescription.xml") {
    AddDiagnostic(
        result, DiagnosticSeverity::Warning, DiagnosticDomain::Package,
        DiagnosticClassification::Compatibility,
        "mvr.package.non_canonical_scene_description",
        "A case-insensitive legacy scene-description filename was accepted.",
        result.snapshot->sceneDescriptionEntry);
  }
  AppendMissingResourceDiagnostics(result);
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
  if (!package.inventory || package.inventory->kind != PackageKind::Mvr) {
    MvrInspectionResult result;
    result.inspection = std::move(package.inspection);
    result.packageInventory = std::move(package.inventory);
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Input,
                  DiagnosticClassification::General,
                  "mvr.input.unsupported_package_kind",
                  "The input package is not an MVR file.");
    return result;
  }
  return InspectMvrBytes(ReadBytes(request.sourcePath), request);
}

// Wraps a filesystem path in the neutral MVR inspection request.
MvrInspectionResult InspectMvr(const std::filesystem::path &sourcePath) {
  return InspectMvr(Request{sourcePath});
}

} // namespace perastage::inspection

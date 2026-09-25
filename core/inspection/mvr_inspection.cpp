#include "inspection/mvr_inspection.h"

#include "archive_entry_path.h"
#include "mvr_import_package.h"
#include "mvr_read_service.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <unordered_map>
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

// Returns the authored layer UUID retained for one parsed node.
std::string LayerUuidFor(const mvr::MvrReadContext &context,
                         const std::string &nodeUuid) {
  const auto found = context.layerUuidByNodeUuid.find(nodeUuid);
  return found == context.layerUuidByNodeUuid.end() ? std::string{}
                                                    : found->second;
}

// Derives deterministic summaries from the authoritative parsed scene.
MvrInspectionSnapshot BuildSnapshot(const MvrImportResult &parsed,
                                    const mvr::MvrReadContext &readContext,
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
  for (const auto &[uuid, geometries] : scene.symdefGeometries) {
    (void)uuid;
    for (const SymdefGeometry &geometry : geometries)
      AddReference(references, "geometry", geometry.file);
  }
  for (const auto &[uuid, file] : scene.symdefFiles) {
    (void)uuid;
    AddReference(references, "geometry", file);
  }
  for (const auto &[kind, path] : references)
    snapshot.referencedResources.push_back({kind, path});

  for (const auto &[layerUuid, layerName] :
       readContext.authoredLayerNameByUuid) {
    std::vector<std::string> children;
    const auto found = readContext.directChildUuidsByLayerUuid.find(layerUuid);
    if (found != readContext.directChildUuidsByLayerUuid.end())
      children = found->second;
    std::sort(children.begin(), children.end());
    snapshot.layers.push_back(
        {"layer", layerUuid, layerName, {}, {}, {}, {}, std::move(children)});
  }
  std::sort(snapshot.layers.begin(), snapshot.layers.end(),
            [](const auto &left, const auto &right) {
              return left.uuid < right.uuid;
            });
  snapshot.fixtures =
      SortedDescriptors(scene.fixtures, [&](const Fixture &node) {
        const std::string reference = node.originalMvrGdtfSpec.empty()
                                          ? node.gdtfSpec
                                          : node.originalMvrGdtfSpec;
        return MvrSceneNodeDescriptor{
            "fixture",         node.uuid,
            node.instanceName, LayerUuidFor(readContext, node.uuid),
            node.layer,        node.parentGroupUuid,
            reference,         {}};
      });
  snapshot.trusses = SortedDescriptors(scene.trusses, [&](const Truss &node) {
    const std::string reference =
        !node.gdtfSpec.empty() ? node.gdtfSpec : node.symbolFile;
    return MvrSceneNodeDescriptor{
        "truss",    node.uuid,
        node.name,  LayerUuidFor(readContext, node.uuid),
        node.layer, node.parentGroupUuid,
        reference,  {}};
  });
  snapshot.supports =
      SortedDescriptors(scene.supports, [&](const Support &node) {
        const std::string reference =
            !node.gdtfSpec.empty() ? node.gdtfSpec : node.modelFile;
        return MvrSceneNodeDescriptor{
            "support",  node.uuid,
            node.name,  LayerUuidFor(readContext, node.uuid),
            node.layer, node.parentGroupUuid,
            reference,  {}};
      });
  snapshot.sceneObjects =
      SortedDescriptors(scene.sceneObjects, [&](const SceneObject &node) {
        return MvrSceneNodeDescriptor{"scene_object",
                                      node.uuid,
                                      node.name,
                                      LayerUuidFor(readContext, node.uuid),
                                      node.layer,
                                      node.parentGroupUuid,
                                      node.GetPrimaryModel(),
                                      {}};
      });
  snapshot.groupObjects =
      SortedDescriptors(scene.groupObjects, [&](const GroupObject &node) {
        return MvrSceneNodeDescriptor{"group_object",
                                      node.uuid,
                                      node.name,
                                      LayerUuidFor(readContext, node.uuid),
                                      node.layer,
                                      node.parentGroupUuid,
                                      {},
                                      GroupChildUuids(node)};
      });
  for (const MvrOpaqueUserDataBlock &block : scene.opaqueUserDataBlocks) {
    snapshot.foreignUserData.push_back(
        {block.provider, block.version, block.xml});
  }

  for (const auto &[uuid, name] : scene.positions)
    snapshot.positions.push_back({"position", uuid, name, {}, {}, {}, {}, {}});
  std::sort(snapshot.positions.begin(), snapshot.positions.end(),
            [](const auto &left, const auto &right) {
              return left.uuid < right.uuid;
            });
  std::set<std::string> symdefUuids;
  for (const auto &[uuid, value] : scene.symdefGeometries) {
    (void)value;
    symdefUuids.insert(uuid);
  }
  for (const auto &[uuid, value] : scene.symdefFiles) {
    (void)value;
    symdefUuids.insert(uuid);
  }
  for (const auto &[uuid, value] : scene.symdefTypes) {
    (void)value;
    symdefUuids.insert(uuid);
  }
  for (const auto &[uuid, value] : scene.symdefMatrices) {
    (void)value;
    symdefUuids.insert(uuid);
  }
  for (const std::string &uuid : symdefUuids) {
    std::set<std::string> retainedResources;
    const auto geometries = scene.symdefGeometries.find(uuid);
    if (geometries != scene.symdefGeometries.end()) {
      for (const SymdefGeometry &geometry : geometries->second)
        if (!geometry.file.empty())
          retainedResources.insert(geometry.file);
    }
    const auto file = scene.symdefFiles.find(uuid);
    if (file != scene.symdefFiles.end() && !file->second.empty())
      retainedResources.insert(file->second);
    std::vector<std::string> resources(retainedResources.begin(),
                                       retainedResources.end());
    snapshot.symdefs.push_back({uuid, std::move(resources)});
  }

  snapshot.nodeCounts = {{"layers", snapshot.layers.size()},
                         {"fixtures", snapshot.fixtures.size()},
                         {"trusses", snapshot.trusses.size()},
                         {"supports", snapshot.supports.size()},
                         {"scene_objects", snapshot.sceneObjects.size()},
                         {"group_objects", snapshot.groupObjects.size()},
                         {"positions", snapshot.positions.size()},
                         {"symdefs", snapshot.symdefs.size()}};
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

// Completes inspection from one inventory and one acquired package.
static MvrInspectionResult CompleteMvrInspection(
    PackageInspectionResult packageInventory,
    std::optional<mvr::ImportPackage> package,
    const std::vector<MvrImportDiagnostic> &packageDiagnostics) {
  MvrInspectionResult result;
  result.inspection = std::move(packageInventory.inspection);
  result.packageInventory = std::move(packageInventory.inventory);
  if (result.inspection.HasFatalDiagnostics())
    return result;
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
  mvr::MvrReadContext readContext;
  MvrImportOptions options;
  options.promptConflicts = false;
  options.applyDictionary = false;
  options.allowDummyFallback = false;
  if (!mvr::ReadAcquiredMvrPackage(*package, parsed, options, nullptr,
                                   &readContext)) {
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Xml,
                  DiagnosticClassification::General, "mvr.xml.parse_failed",
                  "GeneralSceneDescription.xml could not be parsed.",
                  PathUtf8(std::filesystem::relative(package->sceneXmlPath,
                                                     package->rootPath)));
    return result;
  }
  AppendImporterDiagnostics(result, parsed);
  result.snapshot =
      BuildSnapshot(parsed, readContext, *package, *result.packageInventory);
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

// Inspects owned bytes through one inventory and one package acquisition.
MvrInspectionResult InspectMvrBytes(const std::vector<std::uint8_t> &bytes,
                                    const Request &request) {
  if (bytes.empty()) {
    MvrInspectionResult result;
    result.inspection.request = request;
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Input,
                  DiagnosticClassification::General, "mvr.input.empty",
                  "The MVR input is empty.");
    return result;
  }
  PackageInspectionResult inventory =
      InspectPackage(bytes, PackageKind::Mvr, request);
  std::vector<MvrImportDiagnostic> diagnostics;
  std::optional<mvr::ImportPackage> package;
  if (!inventory.inspection.HasFatalDiagnostics())
    package = mvr::AcquireImportPackage(bytes, diagnostics);
  return CompleteMvrInspection(std::move(inventory), std::move(package),
                               diagnostics);
}

// Inspects one filesystem MVR through one inventory and package acquisition.
MvrInspectionResult InspectMvr(const Request &request) {
  PackageInspectionResult inventory = InspectPackage(request);
  if (!inventory.inspection.HasFatalDiagnostics() &&
      (!inventory.inventory || inventory.inventory->kind != PackageKind::Mvr)) {
    MvrInspectionResult result;
    result.inspection = std::move(inventory.inspection);
    result.packageInventory = std::move(inventory.inventory);
    AddDiagnostic(result, DiagnosticSeverity::Fatal, DiagnosticDomain::Input,
                  DiagnosticClassification::General,
                  "mvr.input.unsupported_package_kind",
                  "The input package is not an MVR file.");
    return result;
  }
  std::vector<MvrImportDiagnostic> diagnostics;
  std::optional<mvr::ImportPackage> package;
  if (!inventory.inspection.HasFatalDiagnostics())
    package = mvr::AcquireImportPackage(request.sourcePath, diagnostics);
  return CompleteMvrInspection(std::move(inventory), std::move(package),
                               diagnostics);
}

// Wraps a filesystem path in the neutral MVR inspection request.
MvrInspectionResult InspectMvr(const std::filesystem::path &sourcePath) {
  return InspectMvr(Request{sourcePath});
}

} // namespace perastage::inspection

#include "mvr_identifier_preservation.h"
#include "mvr_standard_preservation.h"
#include "mvr_fixture_addresses.h"
#include "fixture_patch_service.h"
#include "fixture_network_reference.h"
#include "mvr_class_preservation.h"
#include "mvr_symdef_preservation.h"

#include "filesystem_path_utils.h"
#include "mvr_matrix_precision.h"
#include "uuidutils.h"

#include <tinyxml2.h>

#include <filesystem>
#include <algorithm>
#include <cctype>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace mvr {
namespace {
using Names = std::unordered_set<std::string>;

// Only known discarded structures are rejected; absence is never an error.
const std::unordered_map<std::string, Names> unsupportedChildren = {
    {"AUXData", {"MappingDefinition"}},
    {"Fixture", {"CastShadow", "ChildPosition", "Protocols",
                 "Alignments", "CustomCommands", "Overwrites", "Connections",
                 "Mappings", "Gobo"}},
    {"SceneObject", {"GDTFSpec", "GDTFMode", "CastShadow",
                     "Addresses", "Alignments", "CustomCommands", "Overwrites",
                     "Connections", "CustomId", "CustomIdType"}},
    {"Truss", {"CastShadow", "Addresses", "Alignments",
               "CustomCommands", "Overwrites", "Connections", "ChildPosition"}},
    {"Support", {"CastShadow", "Alignments",
                 "CustomCommands", "Overwrites", "Connections"}}};

// Character references and CDATA can encode insignificant XML whitespace.
bool IsEmptyAssignment(const tinyxml2::XMLElement &element) {
  if (element.FirstAttribute()) return false;
  for (auto *node = element.FirstChild(); node; node = node->NextSibling()) {
    if (node->ToComment()) continue;
    const auto *text = node->ToText();
    if (!text || std::string_view(text->Value()).find_first_not_of(" \t\r\n") !=
                     std::string_view::npos)
      return false;
  }
  return true;
}

void Record(MvrScene &scene, tinyxml2::XMLElement &element,
            const std::string &owner, const std::string &location,
            const std::string &reason) {
  tinyxml2::XMLPrinter printer(nullptr, true);
  element.Accept(&printer);
  scene.preservationIssues.push_back(
      {element.Name(), owner, location, printer.CStr(), reason,
       "Keep the original MVR; use an application that preserves this structure."});
}

void Scan(tinyxml2::XMLElement &element, MvrScene &scene,
          std::string owner, const std::string &location,
          const std::string &geometricOwner = {}) {
  const std::string name = element.Name();
  mvr_identifiers::ScanSource(element, scene, location);
  if (const char *uuid = element.Attribute("uuid")) owner = uuid;
  if (name == "Fixture" || name == "Support") ScanFixtureAddresses(element, scene);
  if (name == "Layer") {
    const auto source = ReadMatrixSource(element, scene);
    if (source && !CanonicalLayerMatrixIsElevation(source->projection, source))
      Record(scene, *element.FirstChildElement("Matrix"), owner, location + "/Matrix",
             "MVR 1.6 Layer Matrix permits vertical elevation only.");
  }
  if (name == "FixtureTypeId") {
    Record(scene, element, owner, location,
           "Legacy FixtureTypeId has no preserved canonical equivalent.");
    return;
  }
  if (name == "VideoScreen" || name == "Projector") {
    Record(scene, element, owner, location, "This scene node has no durable representation.");
    return;
  }
  if (element.Attribute("multipatch")) {
    Record(scene, element, owner, location + "/@multipatch",
           "Multipatch ownership is not retained.");
    return;
  }
  // Root UserData already has a dedicated validated preservation service.
  if (name == "UserData") {
    if (location != "/GeneralSceneDescription/UserData")
      for (auto *data = element.FirstChildElement("Data"); data;
           data = data->NextSiblingElement("Data"))
      {
        std::string provider = data->Attribute("provider") ? data->Attribute("provider") : "";
        std::transform(provider.begin(), provider.end(), provider.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (provider != "perastage")
          Record(scene, *data, owner, location,
                 "Foreign object-level UserData has no preservation path.");
      }
    return;
  }
  if (name == "FocusPoint" && !element.FirstChildElement("Geometries"))
    Record(scene, element, owner, location, "FocusPoint requires a Geometries container.");
  std::string geometryOwner = geometricOwner;
  if (name == "FocusPoint" || name == "SceneObject" || name == "Support")
    geometryOwner = name;
  if (name == "Symbol" && !geometryOwner.empty()) {
    Record(scene, element, owner, location,
           "Geometry expansion would discard Symbol identity and Symdef relationships.");
    return;
  }
  if (name == "Addresses" && (!element.Parent()->ToElement() ||
      (std::string_view(element.Parent()->ToElement()->Name()) != "Fixture" &&
       std::string_view(element.Parent()->ToElement()->Name()) != "Support"))) {
    auto *address = element.FirstChildElement("Address");
    if (element.FirstChildElement("Network") ||
        (address && (address->NextSiblingElement("Address") ||
                     address->IntAttribute("break", 0) != 0)))
      Record(scene, element, owner, location,
             "Network assignments and multiple or nonzero DMX breaks are not retained.");
  }
  for (auto *child = element.FirstChildElement(); child;
       child = child->NextSiblingElement()) {
    const std::string childName = child->Name();
    const std::string childLocation = location + "/" + childName;
    const auto rule = unsupportedChildren.find(name);
    if (rule != unsupportedChildren.end() && rule->second.contains(childName)) {
      static const Names emptyContainers = {"Alignments", "Protocols", "CustomCommands",
          "Overwrites", "Connections", "Mappings", "Addresses"};
      if (emptyContainers.contains(childName) && IsEmptyAssignment(*child))
        continue;
      Record(scene, *child, owner, childLocation,
             "The current serializer omits or changes this standard field.");
      continue;
    }
    if (childName == "ChildList" && child->FirstChildElement() &&
        name != "Layer" && name != "GroupObject" && name != "Symdef") {
      Record(scene, *child, owner, childLocation,
             "Children of parametric objects would be reparented.");
      continue;
    }
    if (name == "Symdef" && childName == "ChildList" && child->FirstChildElement("Symbol"))
      Record(scene, *child, owner, childLocation,
             "Nested Symdef references are not retained.");
    Scan(*child, scene, owner, childLocation, geometryOwner);
  }
}

MvrExportDiagnostic Block(const std::string &type, const std::string &uuid,
                          const std::string &location, const std::string &reason) {
  return {MvrExportDiagnosticCode::StandardDataPreservationBlocked,
          MvrExportDiagnosticSeverity::Error, MvrExportDiagnosticImpact::ExportFailed,
          true, type, {}, uuid, location,
          "MVR preservation blocked at " + location + " (uuid=" + uuid + "): " +
              reason + " Keep the original MVR; no lossy override is available."};
}
} // namespace

void RecordStandardPreservationRisks(tinyxml2::XMLElement &root, MvrScene &scene) {
  ReadMvrClasses(root, scene);
  Scan(root, scene, {}, "/GeneralSceneDescription");
}

std::vector<MvrExportDiagnostic> CheckStandardPreservation(const MvrScene &scene) {
  auto result = CheckMvrClasses(scene);
  const auto symbols = CheckMvrSymdefs(scene);
  result.insert(result.end(), symbols.begin(), symbols.end());
  if (scene.versionMajor != 1 || scene.versionMinor > 6)
    result.push_back(Block("GeneralSceneDescription", {}, "/@verMajor,verMinor",
                           "Unsupported MVR version cannot be downgraded safely."));
  for (const auto &issue : scene.preservationIssues) {
    auto diagnostic = Block(issue.node, issue.objectUuid, issue.sourceLocation,
                            issue.reason + " " + issue.recommendedAction);
    if (issue.kind == MvrPreservationIssueKind::ResourceFailure) {
      diagnostic.code = MvrExportDiagnosticCode::ResourceMissing;
      diagnostic.resourceName = issue.originalInformation;
    }
    result.push_back(std::move(diagnostic));
  }
  for (const auto &[uuid, layer] : scene.layers)
    if (!CanonicalLayerMatrixIsElevation(layer.transform, layer.matrixSource))
      result.push_back(Block("Layer", uuid, "Layer/Matrix",
                             "MVR 1.6 Layer Matrix permits finite vertical elevation only."));
  std::unordered_set<std::string> focusUuids;
  for (const auto &[uuid, object] : scene.sceneObjects) {
    if (object.kind != SceneObjectKind::FocusPoint) continue;
    if (const auto canonical = CanonicalizeUuid(uuid); !canonical.empty())
      focusUuids.insert(canonical);
    auto checkGeometry = [&](const std::string &file, bool requiresSymbol) {
      auto path = PathUtils::PathFromUtf8(file);
      if (!path.is_absolute()) path = PathUtils::PathFromUtf8(scene.basePath) / path;
      std::error_code error;
      if (requiresSymbol || !std::filesystem::is_regular_file(path, error))
        result.push_back(Block("FocusPoint", uuid, "Geometries",
                               "Focus geometry cannot be preserved: " + file));
    };
    for (const auto &geometry : object.geometries)
      checkGeometry(geometry.modelFile, !geometry.sourceSymdefUuid.empty());
    if (object.geometries.empty() && !object.modelFile.empty())
      checkGeometry(object.modelFile, false);
  }
  std::unordered_map<std::string, std::optional<fixture_network::WireNames>> networkEvidence;
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (const auto patch = perastage::patch::ResolveFixturePatch(fixture); !patch)
      result.push_back(Block("Fixture", uuid, "Fixture/Addresses", patch.detail));
    if (fixture.dmxAddresses && !fixture.dmxAddresses->networks.empty()) {
      const auto &networkSpec = fixture.gdtfSpec.empty() ? fixture.originalMvrGdtfSpec : fixture.gdtfSpec;
      auto gdtfPath = PathUtils::PathFromUtf8(networkSpec);
      if (!gdtfPath.is_absolute()) gdtfPath = PathUtils::PathFromUtf8(scene.basePath) / gdtfPath;
      auto [evidence, inserted] = networkEvidence.try_emplace(PathUtils::PathToUtf8(gdtfPath));
      if (inserted) evidence->second = fixture_network::ReadWireNames(gdtfPath);
      const auto network = fixture_network::CheckReferences(fixture, evidence->second);
      if (!network) {
        auto diagnostic = Block("Fixture", uuid, "Fixture/Addresses/Network", network.detail);
        if (network.conflict == fixture_network::Conflict::UnavailableGdtf) {
          diagnostic.code = MvrExportDiagnosticCode::ResourceMissing;
          diagnostic.resourceName = networkSpec;
        }
        result.push_back(std::move(diagnostic));
      }
    }
    if (!fixture.focus.empty() &&
        !focusUuids.contains(CanonicalizeUuid(fixture.focus)))
      result.push_back(Block("Fixture", uuid, "Focus",
                             "Unresolved FocusPoint reference: " + fixture.focus));
  }
  for (const auto &[uuid, support] : scene.supports) {
    if (const auto addresses = perastage::patch::InspectFixtureAddresses(support); !addresses)
      result.push_back(Block("Support", uuid, "Support/Addresses", addresses.detail));
    auto path = PathUtils::PathFromUtf8(support.gdtfSpec);
    if (!path.is_absolute()) path = PathUtils::PathFromUtf8(scene.basePath) / path;
    if (!support.gdtfSpec.empty()) {
      if (support.gdtfMode.empty()) {
        result.push_back(Block("Support", uuid, "Support/GDTFMode",
            "A linked GDTF requires an explicit mode; no replacement is guessed."));
        continue;
      }
      std::error_code error;
      if (!std::filesystem::is_regular_file(path, error)) {
        auto diagnostic = Block("Support", uuid, "Support/GDTFSpec",
            "The authored GDTF resource is unavailable; no fallback is substituted.");
        diagnostic.code = MvrExportDiagnosticCode::ResourceMissing;
        diagnostic.resourceName = support.gdtfSpec;
        result.push_back(std::move(diagnostic));
        continue;
      }
    } else if (!support.gdtfMode.empty()) {
      result.push_back(Block("Support", uuid, "Support/GDTFSpec",
          "The authored GDTF mode has no linked definition and cannot be omitted."));
      continue;
    }
    if (!support.dmxAddresses || support.dmxAddresses->networks.empty()) continue;
    if (const auto binding = fixture_network::CheckBindingReference(support, support.gdtfSpec); !binding) {
      result.push_back(Block("Support", uuid, "Support/GDTFSpec", binding.detail));
      continue;
    }
    auto [evidence, inserted] = networkEvidence.try_emplace(PathUtils::PathToUtf8(path));
    if (inserted) evidence->second = fixture_network::ReadWireNames(path);
    const auto network = fixture_network::CheckReferences(support, evidence->second);
    if (!network) {
      auto diagnostic = Block("Support", uuid, "Support/Addresses/Network", network.detail);
      if (network.conflict == fixture_network::Conflict::UnavailableGdtf) {
        diagnostic.code = MvrExportDiagnosticCode::ResourceMissing;
        diagnostic.resourceName = support.gdtfSpec;
      }
      result.push_back(std::move(diagnostic));
    }
  }
  return result;
}
} // namespace mvr

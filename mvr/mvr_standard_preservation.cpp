#include "mvr_identifier_preservation.h"
#include "mvr_standard_preservation.h"
#include "mvr_class_preservation.h"
#include "mvr_symdef_preservation.h"

#include "filesystem_path_utils.h"
#include "matrixutils.h"
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
    {"Layer", {"Matrix"}},
    {"Fixture", {"CastShadow", "ChildPosition", "Protocols",
                 "Alignments", "CustomCommands", "Overwrites", "Connections",
                 "Mappings", "Gobo"}},
    {"SceneObject", {"GDTFSpec", "GDTFMode", "CastShadow",
                     "Addresses", "Alignments", "CustomCommands", "Overwrites",
                     "Connections", "CustomId", "CustomIdType"}},
    {"Truss", {"CastShadow", "Addresses", "Alignments",
               "CustomCommands", "Overwrites", "Connections", "ChildPosition"}},
    {"Support", {"CastShadow", "Addresses", "Alignments",
                 "CustomCommands", "Overwrites", "Connections",
                 "CustomIdType", "CustomId"}}};

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
  if (name == "Addresses") {
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
      if (name == "Layer" && childName == "Matrix") {
        Matrix matrix;
        const auto identity = MatrixUtils::Identity();
        if (child->GetText() && MatrixUtils::ParseMatrix(child->GetText(), matrix) &&
            matrix.u == identity.u && matrix.v == identity.v &&
            matrix.w == identity.w && matrix.o == identity.o)
          continue;
      }
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
  for (const auto &[uuid, fixture] : scene.fixtures)
    if (!fixture.focus.empty() &&
        !focusUuids.contains(CanonicalizeUuid(fixture.focus)))
      result.push_back(Block("Fixture", uuid, "Focus",
                             "Unresolved FocusPoint reference: " + fixture.focus));
  return result;
}
} // namespace mvr

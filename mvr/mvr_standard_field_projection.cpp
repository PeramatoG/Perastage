#include "mvr_standard_field_projection.h"
#include "mvr_identifier_preservation.h"
#include "archive_entry_path.h"
#include "inspection/xml_schema_validation.h"
#include "uuidutils.h"

#include <tinyxml2.h>
#include <algorithm>
#include <map>
#include <string_view>
#include <unordered_map>

namespace mvr {
namespace {
using Element = tinyxml2::XMLElement;
struct Description { std::string parent, type; bool auxiliary = false; };
Description Describe(const MvrPreservationIssue &issue) {
  if (issue.kind != MvrPreservationIssueKind::StandardData) return {};
  const auto slash = issue.sourceLocation.rfind('/');
  if (slash == std::string::npos || issue.sourceLocation.substr(slash + 1) != issue.node) return {};
  const auto prefix = issue.sourceLocation.substr(0, slash);
  const auto parent = prefix.substr(prefix.rfind('/') + 1);
  if (parent == "AUXData" && issue.node == "MappingDefinition") return {parent, "MappingDefinition", true};
  if (parent != "Fixture" && parent != "Truss" && parent != "Support" && parent != "SceneObject") return {};
  if (CanonicalizeUuid(issue.objectUuid).empty()) return {};
  if (issue.reason == mvr_identifiers::kLexicalIntegerReason &&
      (issue.node == "FixtureIDNumeric" || issue.node == "UnitNumber" ||
       issue.node == "CustomId" || issue.node == "CustomIdType"))
    return {parent, "xs:integer"};
  static const std::map<std::string, std::string> types = {
      {"CastShadow","xs:boolean"}, {"ChildPosition","xs:string"},
      {"FixtureTypeId","positiveinteger"}, {"Protocols","Protocols"},
      {"Alignments","Alignments"}, {"CustomCommands","CustomCommands"},
      {"Overwrites","Overwrites"}, {"Connections","Connections"},
      {"Mappings","Mappings"}, {"Gobo","Gobo"}, {"GDTFSpec","FileName"},
      {"GDTFMode","xs:string"}, {"Addresses","Addresses"},
      {"CustomId","xs:integer"}, {"CustomIdType","xs:integer"}};
  const auto type = types.find(issue.node);
  if (type == types.end()) return {};
  // Only fields known to be omitted by the current owning writer, never a
  // malformed assignment, identity, source transform or resource failure.
  if (issue.reason != "The current serializer omits or changes this standard field." &&
      !(issue.node == "FixtureTypeId" && issue.reason == "Legacy FixtureTypeId has no preserved canonical equivalent.")) return {};
  return {parent, type->second};
}
bool Parse(const MvrPreservationIssue &issue, tinyxml2::XMLDocument &doc) {
  if (issue.originalInformation.empty() || issue.originalInformation.size() > 2 * 1024 * 1024) return false;
  if (doc.Parse(issue.originalInformation.data(), issue.originalInformation.size()) != tinyxml2::XML_SUCCESS) return false;
  const auto *root = doc.RootElement();
  return root && issue.node == root->Name() && doc.FirstChild() == root && !root->NextSibling();
}
Element *FindUuid(Element *element, const std::string &uuid) {
  if (!element) return nullptr;
  if (std::string_view(element->Name()) == "UserData") return nullptr;
  if (const auto *value = element->Attribute("uuid"); value && CanonicalizeUuid(value) == CanonicalizeUuid(uuid)) return element;
  for (auto *child = element->FirstChildElement(); child; child = child->NextSiblingElement())
    if (auto *found = FindUuid(child, uuid)) return found;
  return nullptr;
}
const std::vector<std::string> &Order(const std::string &parent) {
  static const std::map<std::string, std::vector<std::string>> orders = {
      {"Fixture",
       {"Matrix",        "Classing",   "GDTFSpec",       "GDTFMode",
        "Focus",         "CastShadow", "DMXInvertPan",   "DMXInvertTilt",
        "Position",      "Function",   "FixtureID",      "FixtureIDNumeric",
        "FixtureTypeId", "UnitNumber", "ChildPosition",  "Addresses",
        "Protocols",     "Alignments", "CustomCommands", "Overwrites",
        "Connections",   "Color",      "CustomIdType",   "CustomId",
        "Mappings",      "Gobo",       "ChildList"}},
      {"Truss",
       {"Matrix",           "Classing",      "Position",       "Geometries",
        "Function",         "GDTFSpec",      "GDTFMode",       "CastShadow",
        "Addresses",        "Alignments",    "CustomCommands", "Overwrites",
        "Connections",      "ChildPosition", "ChildList",      "FixtureID",
        "FixtureIDNumeric", "UnitNumber",    "CustomIdType",   "CustomId"}},
      {"Support",
       {"Matrix",        "Classing",    "Position",     "Geometries",
        "Function",      "ChainLength", "GDTFSpec",     "GDTFMode",
        "CastShadow",    "Addresses",   "Alignments",   "CustomCommands",
        "Overwrites",    "Connections", "FixtureID",    "FixtureIDNumeric",
        "FixtureTypeId", "UnitNumber",  "CustomIdType", "CustomId",
        "ChildList"}},
      {"SceneObject",
       {"Matrix", "Classing", "Geometries", "GDTFSpec", "GDTFMode",
        "CastShadow", "Addresses", "Alignments", "CustomCommands", "Overwrites",
        "Connections", "FixtureID", "FixtureIDNumeric", "FixtureTypeId",
        "UnitNumber", "CustomId", "CustomIdType", "ChildList"}},
      {"AUXData", {"Class", "Position", "Symdef", "MappingDefinition"}}};
  return orders.at(parent);
}
void InsertOrdered(Element &parent, Element *child) {
  const auto &order = Order(parent.Name());
  const auto rank = [&](const char *name) { return std::find(order.begin(), order.end(), name) - order.begin(); };
  tinyxml2::XMLNode *previous = nullptr;
  for (auto *existing = parent.FirstChildElement(); existing; existing = existing->NextSiblingElement()) {
    if (rank(existing->Name()) > rank(child->Name())) break;
    previous = existing;
  }
  if (previous) parent.InsertAfterChild(previous, child);
  else parent.InsertFirstChild(child);
}
MvrExportDiagnostic Failure(const MvrPreservationIssue &issue, const std::string &detail,
                            MvrExportDiagnosticCode code = MvrExportDiagnosticCode::StandardDataPreservationBlocked) {
  return {code, MvrExportDiagnosticSeverity::Error, MvrExportDiagnosticImpact::ExportFailed,
          true, issue.node, {}, issue.objectUuid, issue.sourceLocation, detail};
}
MvrExportDiagnostic Omitted(
    const MvrPreservationIssue &issue, const std::string &detail,
    MvrExportDiagnosticCode code = MvrExportDiagnosticCode::InternalRecovery) {
  return {code,
          MvrExportDiagnosticSeverity::Warning,
          MvrExportDiagnosticImpact::DataOmitted,
          true,
          issue.node,
          {},
          issue.objectUuid,
          issue.sourceLocation,
          detail + "; omitted source field=" + issue.originalInformation};
}
} // namespace

bool CanProjectStandardSourceField(const MvrPreservationIssue &issue) {
  const auto description = Describe(issue);
  if (description.type.empty()) return false;
  tinyxml2::XMLDocument parsed;
  if (!Parse(issue, parsed)) return false;
  // Reuse the pinned, network-denying schema validator instead of treating
  // well-formed XML as proof of a valid standard field. No new schema fields.
  auto schema = perastage::inspection::Mvr16Schema();
  const auto end = schema.xsd.rfind("</xs:schema>");
  if (end == std::string::npos) return false;
  schema.xsd.insert(end, "<xs:element name=\"" + issue.node + "\" type=\"" + description.type + "\"/>");
  const auto validation = perastage::inspection::ValidateXmlAgainstSchema(issue.originalInformation, schema);
  return validation.xml.status == perastage::inspection::ValidationStatus::Valid &&
         validation.schema.status == perastage::inspection::ValidationStatus::Valid;
}

StandardFieldProjectionResult ProjectStandardSourceFields(
    tinyxml2::XMLDocument &document, const MvrScene &scene,
    const StandardFieldResource &registerResource) {
  StandardFieldProjectionResult result;
  std::unordered_set<std::string> projected;
  for (const auto &issue : scene.preservationIssues) {
    if (!CanProjectStandardSourceField(issue)) continue;
    const auto description = Describe(issue);
    Element *owner = FindUuid(document.RootElement(), issue.objectUuid);
    if (description.auxiliary) {
      auto *sceneNode = document.RootElement()->FirstChildElement("Scene");
      owner = sceneNode ? sceneNode->FirstChildElement("AUXData") : nullptr;
      if (sceneNode && !owner) { owner = document.NewElement("AUXData"); sceneNode->InsertFirstChild(owner); }
    }
    if (!owner) continue; // Explicit deletion of a modeled owner deletes its fields.
    if (description.parent != owner->Name()) {
      result.diagnostics.push_back(
          Omitted(issue, "Retained edited native owner kind; incompatible "
                         "source-only field omitted."));
      continue;
    }
    if (issue.reason == mvr_identifiers::kLexicalIntegerReason) {
      auto *existing = owner->FirstChildElement(issue.node.c_str());
      int native = 0;
      if (existing &&
          !mvr_identifiers::ParseInteger(
              existing->GetText() ? existing->GetText() : "", native)) {
        result.diagnostics.push_back(
            Failure(issue, "Malformed native identifier projection."));
        continue;
      }
      if (!mvr_identifiers::RetainedIntegerText(scene, issue.objectUuid,
                                                issue.node, native))
        continue;
      if (existing)
        owner->DeleteChild(existing);
    }
    const auto identity = issue.objectUuid + "\n" + issue.sourceLocation;
    if (!description.auxiliary && (!projected.insert(identity).second || owner->FirstChildElement(issue.node.c_str()))) {
      result.diagnostics.push_back(Omitted(
          issue,
          "Retained native value over conflicting source-only field at " +
              issue.sourceLocation));
      continue;
    }
    tinyxml2::XMLDocument source; if (!Parse(issue, source)) continue;
    auto *field = source.RootElement()->DeepClone(&document)->ToElement();
    std::vector<Element *> pending{field};
    bool resourcesValid = true;
    while (!pending.empty()) {
      auto *node = pending.back(); pending.pop_back();
      for (const char *attribute :
           {"uuid", "symdef", "toObject", "linkedDef", "multipatch"})
        if (const char *value = node->Attribute(attribute))
          if (auto id = CanonicalizeUuid(value); !id.empty())
            node->SetAttribute(attribute, id.c_str());
      const std::string name = node->Name();
      const auto transfer = [&](const std::string &raw, bool gdtf, const char *attribute) {
        if (perastage::archive::IsUnsafeNormalizedEntryPath(raw) || raw.find('\\') != std::string::npos) {
          result.diagnostics.push_back(Omitted(
              issue, "Omitted field with unsafe source resource path: " + raw,
              MvrExportDiagnosticCode::ResourceMissing));
          resourcesValid = false;
          return;
        }
        const auto registered = registerResource(issue.objectUuid, raw, gdtf);
        if (registered.empty()) {
          result.diagnostics.push_back(Omitted(
              issue, "Omitted field with unavailable source resource: " + raw,
              MvrExportDiagnosticCode::ResourceMissing));
          resourcesValid = false;
          return;
        }
        if (attribute) node->SetAttribute(attribute, registered.c_str()); else node->SetText(registered.c_str());
        result.resources.insert(registered);
      };
      if (const auto *file = node->Attribute("fileName")) transfer(file, false, "fileName");
      if ((name == "GDTFSpec" || name == "Gobo") && node->GetText())
        transfer(node->GetText(), name == "GDTFSpec", nullptr);
      if (name == "Source" && node->Attribute("type") && std::string(node->Attribute("type")) == "File" && node->GetText()) transfer(node->GetText(), false, nullptr);
      for (auto *child = node->FirstChildElement(); child; child = child->NextSiblingElement()) pending.push_back(child);
    }
    if (resourcesValid) InsertOrdered(*owner, field);
    else document.DeleteNode(field);
  }
  // Retained standard relationships are references, not opaque strings to clear.
  std::vector<Element *> pending{document.RootElement()};
  while (!pending.empty()) {
    auto *node = pending.back(); pending.pop_back();
    if (std::string_view(node->Name()) == "UserData") continue;
    bool removed = false;
    for (const char *attribute : {"toObject", "linkedDef"}) {
      const auto *reference = node->Attribute(attribute);
      const auto *target =
          reference ? FindUuid(document.RootElement(), reference) : nullptr;
      const auto kind =
          target ? std::string_view(target->Name()) : std::string_view{};
      const bool correctType =
          std::string_view(attribute) == "linkedDef"
              ? kind == "MappingDefinition"
              : kind == "Fixture" || kind == "Truss" || kind == "Support" ||
                    kind == "GroupObject" || kind == "SceneObject" ||
                    kind == "FocusPoint" || kind == "VideoScreen" ||
                    kind == "Projector";
      if (reference && !correctType) {
        const std::string field = node->Name();
        if (field != "Connection" && field != "Mapping") {
          result.diagnostics.push_back(
              {MvrExportDiagnosticCode::StructuralValidationFailed,
               MvrExportDiagnosticSeverity::Error,
               MvrExportDiagnosticImpact::ExportFailed,
               true,
               field,
               {},
               {},
               attribute,
               "Unexpected unresolvable reference owner after canonical "
               "repair."});
          continue;
        }
        tinyxml2::XMLPrinter evidence(nullptr, true);
        node->Accept(&evidence);
        result.diagnostics.push_back(
            {MvrExportDiagnosticCode::ReferenceCleared,
             MvrExportDiagnosticSeverity::Warning,
             MvrExportDiagnosticImpact::DataOmitted,
             true,
             field,
             {},
             {},
             attribute,
             "Omitted unresolved or wrong-type published relationship: " +
                 std::string(evidence.CStr())});
        node->Parent()->DeleteChild(node);
        removed = true;
        break;
      }
    }
    if (removed)
      continue;
    for (auto *child = node->FirstChildElement(); child; child = child->NextSiblingElement()) pending.push_back(child);
  }
  return result;
}
void ReconcileStandardSourceFields(MvrScene &scene,
    const std::function<std::string(const std::string &)> &mapUuid,
    const std::function<std::string(const std::string &)> &mapResource) {
  const auto remap = [&](const std::string &value) {
    const auto direct = mapUuid(value);
    if (direct != value) return direct;
    const auto canonical = CanonicalizeUuid(value);
    return canonical.empty() ? direct : mapUuid(canonical);
  };
  std::vector<MvrPreservationIssue> failures;
  for (auto &issue : scene.preservationIssues) {
    if (!CanProjectStandardSourceField(issue)) continue;
    if (mapUuid) issue.objectUuid = remap(issue.objectUuid);
    tinyxml2::XMLDocument source;
    if (!Parse(issue, source)) continue;
    std::vector<Element *> pending{source.RootElement()};
    while (!pending.empty()) {
      auto *node = pending.back(); pending.pop_back();
      if (mapUuid) for (const char *attribute : {"uuid", "toObject", "linkedDef"})
        if (const auto *value = node->Attribute(attribute)) node->SetAttribute(attribute, remap(value).c_str());
      const std::string name = node->Name();
      if (mapResource && ((name == "GDTFSpec") ||
          (name == "Source" && node->Attribute("type") && std::string(node->Attribute("type")) == "File"))) {
        const std::string raw = node->GetText() ? node->GetText() : "";
        const auto rewritten = mapResource(raw);
        if (rewritten.empty()) failures.push_back({issue.node, issue.objectUuid, issue.sourceLocation,
            raw, "Retained source resource could not be copied during merge.", "Restore the source resource.",
            MvrPreservationIssueKind::ResourceFailure});
        else node->SetText(rewritten.c_str());
      }
      for (auto *child = node->FirstChildElement(); child; child = child->NextSiblingElement()) pending.push_back(child);
    }
    tinyxml2::XMLPrinter printer(nullptr, true); source.RootElement()->Accept(&printer);
    issue.originalInformation = printer.CStr();
  }
  scene.preservationIssues.insert(scene.preservationIssues.end(), failures.begin(), failures.end());
}
} // namespace mvr

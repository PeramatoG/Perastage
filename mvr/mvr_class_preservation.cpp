/* This file is part of Perastage. */
#include "mvr_class_preservation.h"
#include "uuidutils.h"
#include <tinyxml2.h>
#include <map>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace mvr {
namespace {
using Element = tinyxml2::XMLElement;

// Provider payloads own their UUID namespace and are never interpreted here.
template <typename Visitor>
void Visit(Element &element, Visitor visitor,
           const std::string &path = "/GeneralSceneDescription") {
  if (std::string_view(element.Name()) == "UserData") return;
  visitor(element, path);
  for (auto *child = element.FirstChildElement(); child;
       child = child->NextSiblingElement())
    Visit(*child, visitor, path + "/" + child->Name());
}

// Published Table 1 forbids the nil UUID, even though the XSD accepts it.
std::string ClassUuid(const std::string &value) {
  auto canonical = CanonicalizeUuid(value);
  return canonical == "00000000-0000-0000-0000-000000000000" ? std::string{} : canonical;
}

std::string Uuid(Element &element) {
  const auto *uuid = element.Attribute("uuid");
  return uuid ? ClassUuid(uuid) : std::string{};
}

bool HasContent(Element &element) {
  if (element.FirstChildElement()) return true;
  for (auto *child = element.FirstChild(); child; child = child->NextSibling()) {
    if (const auto *text = child->ToText())
      for (const char ch : std::string_view(text->Value()))
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') return true;
  }
  return false;
}

void Record(MvrScene &scene, Element &element, const std::string &owner,
            const std::string &path, const std::string &reason) {
  tinyxml2::XMLPrinter printer(nullptr, true);
  element.Accept(&printer);
  scene.preservationIssues.push_back(
      {element.Name(), owner, path, printer.CStr(), reason,
       "Keep the original MVR; resolve the class definition or reference at its source."});
}

MvrExportDiagnostic Block(const std::string &uuid, const std::string &reason) {
  return {MvrExportDiagnosticCode::StandardDataPreservationBlocked,
          MvrExportDiagnosticSeverity::Error, MvrExportDiagnosticImpact::ExportFailed,
          true, "Class/Classing", {}, uuid, "Scene/AUXData/Class",
          "MVR class preservation blocked (uuid=" + uuid + "): " + reason};
}

template <typename Visitor>
void VisitObjects(const MvrScene &scene, Visitor visitor) {
  const auto visit = [&](const auto &objects) {
    for (const auto &[key, object] : objects) visitor(key, object);
  };
  visit(scene.fixtures); visit(scene.trusses); visit(scene.supports);
  visit(scene.sceneObjects); visit(scene.groupObjects);
}

std::map<std::string, std::string> Assignments(const MvrScene &scene) {
  std::map<std::string, std::string> result;
  VisitObjects(scene, [&](const auto &key, const auto &object) {
    if (!object.classing.empty()) result[CanonicalizeUuid(key)] = object.classing;
  });
  return result;
}
} // namespace

std::string ReadMvrClassing(Element &object) {
  const auto *element = object.FirstChildElement("Classing");
  return element && element->GetText() ? ClassUuid(element->GetText()) : std::string{};
}

void ReadMvrClasses(Element &root, MvrScene &scene) {
  std::unordered_map<std::string, int> identities;
  Visit(root, [&](Element &node, const auto &) {
    if (const auto uuid = Uuid(node); !uuid.empty()) ++identities[uuid];
  });
  Visit(root, [&](Element &node, const auto &path) {
    if (std::string_view(node.Name()) != "Class") return;
    const auto uuid = Uuid(node);
    const auto *parent = node.Parent() ? node.Parent()->ToElement() : nullptr;
    if (!parent || std::string_view(parent->Name()) != "AUXData" || uuid.empty() ||
        identities[uuid] != 1) {
      Record(scene, node, node.Attribute("uuid") ? node.Attribute("uuid") : "", path, "Class identity is missing, malformed, duplicated or shared with another standard node.");
      return;
    }
    bool supported = !HasContent(node);
    for (auto *attribute = node.FirstAttribute(); attribute; attribute = attribute->Next())
      supported &= std::string_view(attribute->Name()) == "uuid" ||
                   std::string_view(attribute->Name()) == "name";
    if (!supported)
      Record(scene, node, node.Attribute("uuid") ? node.Attribute("uuid") : "", path, "Class contains information outside the represented standard attributes.");
    MvrClass definition;
    if (const auto *name = node.Attribute("name")) definition.name = name;
    scene.classes.emplace(uuid, std::move(definition));
  });
  Visit(root, [&](Element &node, const auto &path) {
    auto *classing = node.FirstChildElement("Classing");
    if (!classing) return;
    const auto target = ReadMvrClassing(node);
    const auto owner = Uuid(node);
    static const std::unordered_set<std::string> owners = {
        "Fixture", "GroupObject", "FocusPoint", "SceneObject", "Truss", "Support"};
    if (!owners.contains(node.Name()) || owner.empty() || identities[owner] != 1 ||
        classing->NextSiblingElement("Classing") || classing->FirstAttribute() ||
        classing->FirstChildElement() || target.empty() || !scene.classes.contains(target))
      Record(scene, *classing, node.Attribute("uuid") ? node.Attribute("uuid") : "", path + "/Classing",
             "Classing has an ambiguous owner, unsupported content, or no unique Class target.");
  });
}

std::vector<MvrExportDiagnostic> CheckMvrClasses(const MvrScene &scene) {
  std::vector<MvrExportDiagnostic> result;
  std::unordered_set<std::string> occupied;
  const auto addKeys = [&](const auto &values) {
    for (const auto &[key, value] : values) occupied.insert(CanonicalizeUuid(key));
  };
  addKeys(scene.fixtures); addKeys(scene.trusses); addKeys(scene.supports);
  addKeys(scene.sceneObjects); addKeys(scene.groupObjects); addKeys(scene.layers);
  addKeys(scene.positions); addKeys(scene.symdefFiles); addKeys(scene.symdefTypes);
  addKeys(scene.symdefMatrices); addKeys(scene.symdefGeometries);
  for (const auto &[uuid, truss] : scene.trusses)
    if (!truss.sourceSymbolUuid.empty()) occupied.insert(CanonicalizeUuid(truss.sourceSymbolUuid));
  const auto addSymbols = [&](const auto &objects) {
    for (const auto &[uuid, object] : objects)
      for (const auto &geometry : object.geometries)
        if (!geometry.sourceSymbolUuid.empty()) occupied.insert(CanonicalizeUuid(geometry.sourceSymbolUuid));
  };
  addSymbols(scene.sceneObjects); addSymbols(scene.supports);
  for (const auto &[uuid, definition] : scene.classes)
    if (uuid.empty() || ClassUuid(uuid) != uuid || occupied.contains(uuid))
      result.push_back(Block(uuid, "Class UUID must be canonical and exclusively owned by its definition."));
  VisitObjects(scene, [&](const auto &key, const auto &object) {
    if (!object.classing.empty() && (!scene.classes.contains(object.classing) ||
        key != object.uuid || ClassUuid(key).empty()))
      result.push_back(Block(key, "Classing must retain its object identity and resolve to a Class in this scene."));
  });
  return result;
}

std::vector<MvrExportDiagnostic> CheckSerializedClasses(Element &sceneElement,
                                                       const MvrScene &scene) {
  std::unordered_map<std::string, MvrClass> definitions;
  std::map<std::string, std::string> assignments;
  std::unordered_map<std::string, int> identities;
  bool valid = true;
  Visit(sceneElement, [&](Element &node, const auto &) {
    if (const auto uuid = Uuid(node); !uuid.empty()) ++identities[uuid];
    if (std::string_view(node.Name()) == "Class") {
      MvrClass definition;
      if (const auto *name = node.Attribute("name")) definition.name = name;
      valid &= definitions.emplace(Uuid(node), definition).second;
    }
    if (auto *link = node.FirstChildElement("Classing")) {
      valid &= !link->NextSiblingElement("Classing") && !link->FirstAttribute() &&
               !link->FirstChildElement();
      valid &= assignments.emplace(Uuid(node), ReadMvrClassing(node)).second;
    }
  });
  for (const auto &[uuid, definition] : definitions) valid &= identities[uuid] == 1;
  if (!valid || definitions != scene.classes || assignments != Assignments(scene))
    return {Block({}, "Serialization changed class definitions or object assignments; output was rejected.")};
  return {};
}
} // namespace mvr

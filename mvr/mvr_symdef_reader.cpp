/* This file is part of Perastage. */
#include "mvr_symdef_preservation.h"
#include "matrixutils.h"
#include "uuidutils.h"
#include <tinyxml2.h>
#include <string_view>
#include <unordered_set>
#include <cmath>

namespace mvr {
namespace {
using Element = tinyxml2::XMLElement;
std::string Identity(const char *text) {
  const auto uuid = CanonicalizeUuid(text ? text : "");
  return uuid == "00000000-0000-0000-0000-000000000000" ? "" : uuid;
}
template <typename Visitor>
void Visit(Element &node, Visitor visitor, const std::string &path = "Scene") {
  if (std::string_view(node.Name()) == "UserData") return;
  visitor(node, path);
  for (auto *child = node.FirstChildElement(); child; child = child->NextSiblingElement())
    Visit(*child, visitor, path + "/" + child->Name());
}
void Record(MvrScene &scene, Element &element, const std::string &path,
            const std::string &owner, const std::string &reason) {
  tinyxml2::XMLPrinter printer(nullptr, true);
  element.Accept(&printer);
  scene.preservationIssues.push_back({element.Name(), owner, path, printer.CStr(), reason,
      "Keep the original MVR; this symbol structure cannot yet be saved without loss."});
}
bool Attributes(Element &node, const std::unordered_set<std::string> &allowed) {
  for (auto *attribute = node.FirstAttribute(); attribute; attribute = attribute->Next())
    if (!allowed.contains(attribute->Name())) return false;
  return true;
}
bool ReadMatrix(Element &node, Matrix &matrix) {
  matrix = MatrixUtils::Identity();
  auto *element = node.FirstChildElement("Matrix");
  if (!element) return true;
  if (element->NextSiblingElement("Matrix") || element->FirstAttribute() ||
      element->FirstChildElement() || !element->GetText() ||
      !MatrixUtils::ParseMatrix(element->GetText(), matrix)) return false;
  for (const auto &row : {matrix.u, matrix.v, matrix.w, matrix.o})
    for (float value : row) if (!std::isfinite(value)) return false;
  return SymdefMatrixPreservesSource(element->GetText(), matrix);
}
bool IdentityMatrix(const Matrix &matrix) {
  const auto id = MatrixUtils::Identity();
  return matrix.u == id.u && matrix.v == id.v && matrix.w == id.w && matrix.o == id.o;
}
bool OnlyMatrix(Element &node) {
  for (auto *child = node.FirstChildElement(); child; child = child->NextSiblingElement())
    if (std::string_view(child->Name()) != "Matrix") return false;
  return true;
}
} // namespace

void ReadMvrSymdefs(Element &sceneElement, MvrScene &scene,
                   const std::function<std::string(const std::string &)> &remapResource) {
  std::unordered_map<std::string, int> identities;
  Visit(sceneElement, [&](Element &node, const auto &) {
    if (auto uuid = Identity(node.Attribute("uuid")); !uuid.empty()) ++identities[uuid];
  });
  if (auto *aux = sceneElement.FirstChildElement("AUXData")) {
    for (auto *node = aux->FirstChildElement("Symdef"); node; node = node->NextSiblingElement("Symdef")) {
      const auto raw = node->Attribute("uuid") ? node->Attribute("uuid") : "";
      const auto uuid = Identity(raw);
      const std::string location = "Scene/AUXData/Symdef";
      bool rejected = false;
      auto reject = [&](const auto &reason) {
        if (!rejected) Record(scene, *node, location, raw, reason);
        rejected = true;
      };
      if (uuid.empty() || identities[uuid] != 1) {
        reject("Symdef identity is missing, nil, duplicated or shared with another standard node.");
        continue;
      }
      SymdefDefinition definition;
      if (const auto *name = node->Attribute("name")) definition.name = name;
      auto *children = node->FirstChildElement("ChildList");
      if (!Attributes(*node, {"uuid", "name"}) || !children ||
          children->NextSiblingElement("ChildList")) reject("Symdef attributes or ChildList cannot be represented.");
      for (auto *child = node->FirstChildElement(); child; child = child->NextSiblingElement())
        if (child != children) reject("Symdef contains an unrepresented child.");
      if (children && children->FirstAttribute()) reject("Symdef ChildList attributes are not represented.");
      for (auto *geo = children ? children->FirstChildElement() : nullptr; geo; geo = geo->NextSiblingElement()) {
        SymdefGeometry geometry;
        if (std::string_view(geo->Name()) != "Geometry3D" ||
            !Attributes(*geo, {"fileName"}) || !geo->Attribute("fileName") ||
            !OnlyMatrix(*geo) || !ReadMatrix(*geo, geometry.transform)) {
          reject("Only direct standard Geometry3D children are preserved; nested Symbol graphs remain protected.");
          continue;
        }
        geometry.file = remapResource(geo->Attribute("fileName"));
        if (const auto problem = SymdefResourceProblem(scene, geometry.file); !problem.empty())
          reject(problem);
        definition.geometries.push_back(std::move(geometry));
      }
      scene.symdefs.emplace(uuid, std::move(definition));
    }
  }
  Visit(sceneElement, [&](Element &node, const auto &path) {
    const auto owner = node.Attribute("uuid") ? node.Attribute("uuid") : "";
    if (std::string_view(node.Name()) == "Symbol") {
      const auto uuid = Identity(node.Attribute("uuid"));
      const auto target = Identity(node.Attribute("symdef"));
      Matrix matrix;
      if (uuid.empty() || identities[uuid] != 1 || target.empty() ||
          !scene.symdefs.contains(target) || !Attributes(node, {"uuid", "symdef"}) ||
          !OnlyMatrix(node) || !ReadMatrix(node, matrix))
        Record(scene, node, path, owner, "Symbol identity, definition reference or content cannot be preserved uniquely.");
      auto *parent = node.Parent() ? node.Parent()->ToElement() : nullptr;
      auto *container = parent && parent->Parent() ? parent->Parent()->ToElement() : nullptr;
      if (container && std::string_view(container->Name()) == "Truss" &&
          (!ReadMatrix(node, matrix) || !IdentityMatrix(matrix)))
        Record(scene, node, path, owner, "Nonidentity Truss Symbol placement is currently baked into the display transform.");
      if (container && std::string_view(container->Name()) == "Truss" && scene.symdefs.contains(target)) {
        const auto &geometries = scene.symdefs.at(target).geometries;
        if (!geometries.empty() && !IdentityMatrix(geometries.front().transform))
          Record(scene, node, path, owner, "The first definition geometry placement is currently baked into the Truss display transform.");
      }
    }
    bool containsSymbolTruss = false;
    if (std::string_view(node.Name()) == "GroupObject" || std::string_view(node.Name()) == "Truss") {
      Visit(node, [&](Element &descendant, const auto &) {
        if (std::string_view(descendant.Name()) != "Truss") return;
        auto *geometries = descendant.FirstChildElement("Geometries");
        containsSymbolTruss |= geometries && geometries->FirstChildElement("Symbol");
      });
    }
    if (containsSymbolTruss) {
      Matrix matrix;
      if (!ReadMatrix(node, matrix)) {
        auto *value = node.FirstChildElement("Matrix");
        Record(scene, value ? *value : node, path + "/Matrix", owner,
               "Symbol ancestor placement exceeds the current canonical coordinate precision or structure.");
      }
    }
    if (std::string_view(node.Name()) == "Truss") {
      if (auto *geometries = node.FirstChildElement("Geometries")) {
        auto *first = geometries->FirstChildElement();
        if (geometries->FirstAttribute() || (first && first->NextSiblingElement()))
          Record(scene, *geometries, path + "/Geometries", owner, "Multiple or mixed Truss geometry entries have no complete instance representation.");
        if (first && std::string_view(first->Name()) == "Geometry3D") {
          Matrix matrix;
          if (!ReadMatrix(*first, matrix) || !IdentityMatrix(matrix))
            Record(scene, *first, path + "/Geometries/Geometry3D", owner,
                   "Nonidentity direct Truss geometry placement is currently baked into the display transform.");
        }
      }
    }
  });
}
} // namespace mvr

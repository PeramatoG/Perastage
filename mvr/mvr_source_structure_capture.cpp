#include "mvr_source_structure_internal.h"
#include "mvr_matrix_precision.h"
#include "uuidutils.h"
#include "inspection/xml_schema_validation.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_set>

namespace mvr::source_structure {
std::string Xml(const Element &element) {
  tinyxml2::XMLPrinter printer(nullptr, true);
  element.Accept(&printer);
  return printer.CStr();
}
std::string Id(const Element &element) {
  return CanonicalizeUuid(element.Attribute("uuid") ? element.Attribute("uuid")
                                                    : "");
}
bool Equal(const Matrix &a, const Matrix &b) {
  return a.u == b.u && a.v == b.v && a.w == b.w && a.o == b.o;
}
bool ValidateType(const std::string &xml, const std::string &name,
                  const std::string &type) {
  auto schema = perastage::inspection::Mvr16Schema();
  auto end = schema.xsd.rfind("</xs:schema>");
  if (end == std::string::npos)
    return false;
  schema.xsd.insert(end, "<xs:element name=\"" + name + "\" type=\"" + type +
                             "\"/>");
  auto validation =
      perastage::inspection::ValidateXmlAgainstSchema(xml, schema);
  return validation.schema.status ==
         perastage::inspection::ValidationStatus::Valid;
}
// Existing scalar/assignment owners also implement published-standard policies
// where the pinned XSD disagrees (signed identifiers, IPv6, DMX breaks). This
// service validates only the geometry/topology it will restore for modeled
// nodes; their ordinary owners continue to validate every editable field.
bool ValidateOwnedPayload(Element &node, Element &shallow, bool modeled,
                          bool geometryAuthority) {
  const std::string kind = node.Name();
  if (!modeled)
    return ValidateType(Xml(shallow), kind, kind);
  if (geometryAuthority) {
    auto *geometry = shallow.FirstChildElement("Geometries");
    if (!geometry || geometry->NextSiblingElement("Geometries") ||
        !ValidateType(Xml(*geometry), "Geometries", "Geometries"))
      return false;
  }
  static const std::set<std::string> parametric = {"Fixture", "Truss",
                                                   "Support", "SceneObject"};
  if (const char *multipatch = shallow.Attribute("multipatch"))
    if (!parametric.contains(kind) ||
        (*multipatch && CanonicalizeUuid(multipatch).empty()))
      return false;
  if (auto *children = node.FirstChildElement("ChildList")) {
    if ((!parametric.contains(kind) && kind != "Layer" &&
         kind != "GroupObject") ||
        children->FirstAttribute() || children->NextSiblingElement("ChildList"))
      return false;
    for (auto *part = children->FirstChild(); part; part = part->NextSibling())
      if (part->ToText() &&
          std::string_view(part->Value()).find_first_not_of(" \t\r\n") !=
              std::string_view::npos)
        return false;
  }
  return true;
}
CurrentNode Current(const MvrScene &scene, const std::string &uuid) {
  CurrentNode result;
  const auto read = [&](const auto &objects) {
    const auto it = objects.find(uuid);
    if (it == objects.end())
      return;
    const auto &value = it->second;
    result.exists = true;
    result.world = value.transform;
    result.parent = value.parentGroupUuid;
    result.layer = value.layerUuid;
    if constexpr (requires { value.geometries; })
      result.geometries = value.geometries;
    if constexpr (requires { value.modelFile; })
      result.model = value.modelFile;
    if constexpr (requires { value.symbolFile; })
      if (!value.symbolFile.empty())
        result.model = value.symbolFile;
  };
  read(scene.fixtures);
  read(scene.trusses);
  read(scene.supports);
  read(scene.sceneObjects);
  read(scene.groupObjects);
  if (auto it = scene.layers.find(uuid); it != scene.layers.end()) {
    result.exists = true;
    result.world = it->second.transform;
    result.layer = uuid;
  }
  return result;
}
Element *Find(Element *root, const std::string &uuid) {
  if (!root || std::string_view(root->Name()) == "UserData")
    return nullptr;
  if (Id(*root) == CanonicalizeUuid(uuid))
    return root;
  for (auto *node = root->FirstChildElement(); node;
       node = node->NextSiblingElement())
    if (auto *found = Find(node, uuid))
      return found;
  return nullptr;
}
MvrExportDiagnostic Error(const std::string &uuid, const std::string &field,
                          const std::string &detail,
                          MvrExportDiagnosticCode code) {
  return {code,
          MvrExportDiagnosticSeverity::Error,
          MvrExportDiagnosticImpact::ExportFailed,
          true,
          "Source structure",
          {},
          uuid,
          field,
          detail};
}
// Double intermediates avoid an arbitrary determinant epsilon rejecting small
// but invertible authored scales. Unchanged singular placements need no
// inverse.
bool Inverse(const Matrix &m, Matrix &out) {
  const double a = m.u[0], b = m.v[0], c = m.w[0], d = m.u[1], e = m.v[1],
               f = m.w[1], g = m.u[2], h = m.v[2], i = m.w[2];
  const double det =
      a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
  if (!std::isfinite(det) || det == 0)
    return false;
  out.u = {float((e * i - f * h) / det), float((f * g - d * i) / det),
           float((d * h - e * g) / det)};
  out.v = {float((c * h - b * i) / det), float((a * i - c * g) / det),
           float((b * g - a * h) / det)};
  out.w = {float((b * f - c * e) / det), float((c * d - a * f) / det),
           float((a * e - b * d) / det)};
  for (size_t n = 0; n < 3; ++n)
    out.o[n] = -(out.u[n] * m.o[0] + out.v[n] * m.o[1] + out.w[n] * m.o[2]);
  for (const auto &row : {out.u, out.v, out.w, out.o})
    for (float value : row)
      if (!std::isfinite(value))
        return false;
  return true;
}
} // namespace mvr::source_structure
namespace mvr {
using namespace source_structure;
void IndexSourceStructure(MvrSourceStructure &source) {
  source.nodeIndex.clear();
  source.symbolOwnerIndex.clear();
  for (size_t index = 0; index < source.nodes.size(); ++index) {
    const auto &node = source.nodes[index];
    source.nodeIndex.try_emplace(node.uuid, index);
    if (!node.geometryAuthority)
      continue;
    tinyxml2::XMLDocument xml;
    xml.Parse(node.xml.c_str());
    if (auto *root = xml.RootElement())
      if (auto *geometries = root->FirstChildElement("Geometries"))
        for (auto *symbol = geometries->FirstChildElement("Symbol"); symbol;
             symbol = symbol->NextSiblingElement("Symbol"))
          source.symbolOwnerIndex.try_emplace(Id(*symbol), index);
  }
}
void CaptureSourceStructure(
    Element &root, MvrScene &scene,
    const std::function<std::string(const std::string &)> &remapResource) {
  bool structural = false;
  const auto inspect = [&](auto &&self, Element &node) -> void {
    const std::string kind = node.Name();
    if (kind == "UserData")
      return;
    structural |= kind == "Symbol" || kind == "VideoScreen" ||
                  kind == "Projector" || node.Attribute("multipatch");
    if (kind == "ChildList" && node.FirstChildElement()) {
      const auto *parent = node.Parent() ? node.Parent()->ToElement() : nullptr;
      if (parent && std::string(parent->Name()) != "Layer" &&
          std::string(parent->Name()) != "GroupObject" &&
          std::string(parent->Name()) != "Symdef")
        structural = true;
    }
    if (kind == "Geometries" && node.Parent() && node.Parent()->ToElement() &&
        std::string(node.Parent()->ToElement()->Name()) == "Truss")
      structural = true;
    for (auto *child = node.FirstChildElement(); child;
         child = child->NextSiblingElement())
      self(self, *child);
  };
  inspect(inspect, root);
  if (!structural)
    return;
  auto captured = std::make_shared<MvrSourceStructure>();
  bool needed = false;
  const auto visit = [&](auto &&self, Element &node, const std::string &parent,
                         const std::string &layer,
                         const Matrix &parentWorld) -> void {
    const std::string kind = node.Name();
    const auto id = Id(node);
    if (id.empty())
      return;
    MvrSourceNode value;
    value.uuid = id;
    value.kind = kind;
    value.parentUuid = parent;
    value.layerUuid = kind == "Layer" ? id : layer;
    value.localSource =
        node.FirstChildElement("Matrix")
            ? ReadCanonicalMatrixElement(*node.FirstChildElement("Matrix"))
            : ParseCanonicalMatrix("{1,0,0}{0,1,0}{0,0,1}{0,0,0}");
    value.authoredWorld = MatrixUtils::Multiply(
        parentWorld,
        value.localSource ? value.localSource->projection : Matrix{});
    auto current = Current(scene, id);
    value.modeled = current.exists;
    value.displayedWorld = current.world;
    value.projectedParentUuid = current.parent;
    value.projectedGeometries = current.geometries;
    value.projectedModel = current.model;
    auto *geometries = node.FirstChildElement("Geometries");
    value.geometryAuthority =
        geometries &&
        (kind == "Truss" || geometries->FirstChildElement("Symbol"));
    needed |= value.geometryAuthority || kind == "VideoScreen" ||
              kind == "Projector" || node.Attribute("multipatch");
    tinyxml2::XMLDocument shallow;
    auto *copy = shallow.NewElement(kind.c_str());
    shallow.InsertEndChild(copy);
    for (auto *attribute = node.FirstAttribute(); attribute;
         attribute = attribute->Next())
      copy->SetAttribute(attribute->Name(), attribute->Value());
    bool parametricChildren = false;
    for (auto *child = node.FirstChildElement(); child;
         child = child->NextSiblingElement()) {
      if (std::string(child->Name()) == "ChildList") {
        auto *list = shallow.NewElement("ChildList");
        copy->InsertEndChild(list);
        for (auto *attribute = child->FirstAttribute(); attribute;
             attribute = attribute->Next())
          list->SetAttribute(attribute->Name(), attribute->Value());
        parametricChildren = kind != "Layer" && kind != "GroupObject" &&
                             child->FirstChildElement();
        needed |= parametricChildren;
      } else
        copy->InsertEndChild(child->DeepClone(&shallow));
    }
    std::vector<Element *> pending{copy};
    while (!pending.empty()) {
      auto *part = pending.back();
      pending.pop_back();
      if (const auto *file = part->Attribute("fileName"))
        part->SetAttribute("fileName", remapResource(file).c_str());
      for (const char *key :
           {"uuid", "symdef", "toObject", "linkedDef", "multipatch"})
        if (const char *value = part->Attribute(key))
          if (auto id = CanonicalizeUuid(value); !id.empty())
            part->SetAttribute(key, id.c_str());
      const std::string name = part->Name();
      if ((name == "GDTFSpec" ||
           (name == "Source" && part->Attribute("type") &&
            std::string(part->Attribute("type")) == "File")) &&
          part->GetText())
        part->SetText(remapResource(part->GetText()).c_str());
      for (auto *child = part->FirstChildElement(); child;
           child = child->NextSiblingElement())
        pending.push_back(child);
    }
    value.xml = Xml(*copy);
    value.valid =
        value.localSource && ValidateOwnedPayload(node, *copy, value.modeled,
                                                  value.geometryAuthority);
    captured->nodes.push_back(value);
    if (auto *children = node.FirstChildElement("ChildList"))
      for (auto *child = children->FirstChildElement(); child;
           child = child->NextSiblingElement())
        self(self, *child, id, value.layerUuid, value.authoredWorld);
  };
  if (auto *layers = root.FirstChildElement("Layers"))
    for (auto *layer = layers->FirstChildElement("Layer"); layer;
         layer = layer->NextSiblingElement("Layer"))
      visit(visit, *layer, "", "", Matrix{});
  for (const auto &[id, definition] : scene.symdefs)
    needed |= !definition.symbols.empty();
  if (needed) {
    IndexSourceStructure(*captured);
    scene.sourceStructures.push_back(std::move(captured));
  }
}
const MvrSourceNode *FindSourceNode(const MvrScene &scene,
                                    const std::string &uuid) {
  const auto canonical = CanonicalizeUuid(uuid);
  for (const auto &source : scene.sourceStructures)
    if (source && source->version == 1) {
      if (const auto found = source->nodeIndex.find(canonical);
          found != source->nodeIndex.end())
        return &source->nodes[found->second];
      if (source->nodeIndex.empty())
        for (const auto &node : source->nodes)
          if (node.uuid == canonical)
            return &node;
    }
  return nullptr;
}
bool SourceOwnsGeometry(const MvrScene &scene, const std::string &uuid) {
  const auto *node = FindSourceNode(scene, uuid);
  return node && node->valid && node->geometryAuthority;
}
bool SourceNodeSurvives(const MvrScene &scene, const MvrSourceNode &node) {
  if (!scene.layers.contains(node.layerUuid))
    return false;
  if (node.modeled && !Current(scene, node.uuid).exists)
    return false;
  std::unordered_set<std::string> visited;
  auto parent = node.parentUuid;
  while (!parent.empty() && visited.insert(parent).second) {
    const auto *ancestor = FindSourceNode(scene, parent);
    if (!ancestor)
      break;
    if (ancestor->modeled && !Current(scene, parent).exists)
      return false;
    parent = ancestor->parentUuid;
  }
  return true;
}
bool CanRestoreSourceIssue(const MvrScene &scene,
                           const MvrPreservationIssue &issue) {
  if (issue.kind != MvrPreservationIssueKind::StandardData)
    return false;
  const auto *owner = FindSourceNode(scene, issue.objectUuid);
  if (owner && owner->valid) {
    if ((issue.node == "VideoScreen" || issue.node == "Projector") &&
        issue.reason == "This scene node has no durable representation.")
      return true;
    if (issue.node == "ChildList" &&
        issue.reason == "Children of parametric objects would be reparented.")
      return true;
    if (issue.sourceLocation.ends_with("/@multipatch") &&
        issue.reason == "Multipatch ownership is not retained.")
      return true;
    if (owner->geometryAuthority &&
        (issue.node == "Geometries" || issue.node == "Geometry3D") &&
        issue.reason.find("Truss geometry") != std::string::npos)
      return true;
  }
  if (issue.node == "Symbol") {
    for (const auto &source : scene.sourceStructures)
      if (source) {
        const auto found =
            source->symbolOwnerIndex.find(CanonicalizeUuid(issue.objectUuid));
        if (found != source->symbolOwnerIndex.end() &&
            source->nodes[found->second].valid)
          return true;
      }
  }
  if (issue.node == "ChildList" &&
      issue.reason == "Nested Symdef references are not retained.") {
    const auto it = scene.symdefs.find(CanonicalizeUuid(issue.objectUuid));
    return it != scene.symdefs.end() && it->second.sourceComplete;
  }
  return false;
}
std::vector<std::string> SourceStructureIdentities(const MvrScene &scene) {
  std::set<std::string> ids;
  for (const auto &source : scene.sourceStructures)
    if (source)
      for (const auto &node : source->nodes) {
        ids.insert(node.uuid);
        tinyxml2::XMLDocument xml;
        xml.Parse(node.xml.c_str());
        const auto visit = [&](auto &&self, Element *part) -> void {
          if (!part)
            return;
          if (auto id = Id(*part); !id.empty())
            ids.insert(id);
          for (auto *child = part->FirstChildElement(); child;
               child = child->NextSiblingElement())
            self(self, child);
        };
        visit(visit, xml.RootElement());
      }
  return {ids.begin(), ids.end()};
}
} // namespace mvr

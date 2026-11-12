#include "mvr_source_structure_internal.h"
#include "mvr_matrix_precision.h"
#include "archive_entry_path.h"
#include "uuidutils.h"
#include <algorithm>
#include <map>
#include <set>
#include <unordered_set>

namespace mvr {
using namespace source_structure;
namespace {
void RegisterResources(Element &root, const std::string &owner,
                       const SourceResource &resource,
                       StandardFieldProjectionResult &result,
                       bool nativeModelPaths = false) {
  const auto transfer = [&](Element &node, const std::string &path,
                            SourceResourceKind kind, const char *attribute) {
    if (path.empty() ||
        ((!nativeModelPaths || kind != SourceResourceKind::Model) &&
         (perastage::archive::IsUnsafeNormalizedEntryPath(path) ||
          path.find('\\') != std::string::npos))) {
      result.diagnostics.push_back(
          Error(owner, "Resource", "Unsafe source resource path: " + path,
                MvrExportDiagnosticCode::ResourceMissing));
      return;
    }
    const auto mapped = resource(owner, path, kind);
    if (mapped.empty()) {
      result.diagnostics.push_back(
          Error(owner, "Resource", "Source resource is unavailable: " + path,
                MvrExportDiagnosticCode::ResourceMissing));
      return;
    }
    if (attribute)
      node.SetAttribute(attribute, mapped.c_str());
    else
      node.SetText(mapped.c_str());
    result.resources.insert(mapped);
  };
  const auto visit = [&](auto &&self, Element &node) -> void {
    if (const auto *file = node.Attribute("fileName"))
      transfer(node, file, SourceResourceKind::Model, "fileName");
    const std::string name = node.Name();
    if (name == "GDTFSpec" && node.GetText())
      transfer(node, node.GetText(), SourceResourceKind::Gdtf, nullptr);
    if (name == "Source" && node.Attribute("type") &&
        std::string(node.Attribute("type")) == "File" && node.GetText())
      transfer(node, node.GetText(), SourceResourceKind::Media, nullptr);
    for (auto *child = node.FirstChildElement(); child;
         child = child->NextSiblingElement())
      self(self, *child);
  };
  visit(visit, root);
}
Element *ChildList(tinyxml2::XMLDocument &doc, Element &owner) {
  auto *list = owner.FirstChildElement("ChildList");
  if (!list) {
    list = doc.NewElement("ChildList");
    owner.InsertEndChild(list);
  }
  return list;
}
} // namespace
std::vector<MvrExportDiagnostic> CheckSourceStructure(const MvrScene &scene) {
  std::vector<MvrExportDiagnostic> errors;
  const auto validIdentity = [](const std::string &id) {
    return !id.empty() && CanonicalizeUuid(id) == id &&
           id != "00000000-0000-0000-0000-000000000000";
  };
  std::unordered_set<std::string> identities, sourceIds;
  const auto add = [&](const auto &values) {
    for (const auto &[id, value] : values)
      identities.insert(CanonicalizeUuid(id));
  };
  add(scene.layers);
  add(scene.fixtures);
  add(scene.trusses);
  add(scene.supports);
  add(scene.sceneObjects);
  add(scene.groupObjects);
  add(scene.classes);
  add(scene.positions);
  add(scene.symdefs);
  for (const auto &[id, definition] : scene.symdefs)
    for (const auto &symbol : definition.symbols)
      identities.insert(symbol.uuid);
  for (const auto &issue : scene.preservationIssues)
    if (issue.node == "MappingDefinition" &&
        CanProjectStandardSourceField(issue)) {
      tinyxml2::XMLDocument definition;
      definition.Parse(issue.originalInformation.c_str());
      if (definition.RootElement())
        identities.insert(Id(*definition.RootElement()));
    }
  std::vector<std::pair<std::string, std::string>> references;
  for (const auto &source : scene.sourceStructures)
    if (source)
      for (const auto &node : source->nodes) {
        if (!validIdentity(node.uuid))
          errors.push_back(Error(node.uuid, "uuid",
                                 "Invalid source node identity.",
                                 MvrExportDiagnosticCode::IdentityConflict));
        if (!sourceIds.insert(node.uuid).second)
          errors.push_back(
              Error(node.uuid, "uuid", "Duplicate source node identity."));
        if (!node.modeled && !identities.insert(node.uuid).second)
          errors.push_back(
              Error(node.uuid, "uuid",
                    "Source-only identity conflicts with an editable node."));
        if (!node.valid) {
          if (!node.modeled || node.geometryAuthority)
            errors.push_back(Error(node.uuid, node.kind,
                                   "Source structure is malformed or outside "
                                   "the standard node contract."));
          continue;
        }
        tinyxml2::XMLDocument xml;
        xml.Parse(node.xml.c_str());
        const auto visit = [&](auto &&self, Element &part, bool root) -> void {
          if (!root && part.Attribute("uuid"))
            if (!validIdentity(Id(part)) || !identities.insert(Id(part)).second)
              errors.push_back(
                  Error(Id(part), "uuid", "Duplicate source Symbol identity."));
          for (const char *attribute :
               {"symdef", "toObject", "linkedDef", "multipatch"})
            if (const char *ref = part.Attribute(attribute); ref && *ref)
              references.emplace_back(node.uuid, CanonicalizeUuid(ref));
          if (!node.modeled && SourceNodeSurvives(scene, node) &&
              (std::string_view(part.Name()) == "Classing" ||
               std::string_view(part.Name()) == "Position") &&
              part.GetText())
            references.emplace_back(node.uuid,
                                    CanonicalizeUuid(part.GetText()));
          for (auto *child = part.FirstChildElement(); child;
               child = child->NextSiblingElement())
            self(self, *child, false);
        };
        visit(visit, *xml.RootElement(), true);
      }
  for (const auto &[owner, reference] : references)
    if (!identities.contains(reference))
      errors.push_back(Error(owner, "Reference",
                             "Unresolved source reference: " + reference,
                             MvrExportDiagnosticCode::IdentityConflict));
  return errors;
}
StandardFieldProjectionResult
ProjectSourceStructure(tinyxml2::XMLDocument &document, const MvrScene &scene,
                       const SourceResource &resource,
                       const MvrExportOptions &options) {
  StandardFieldProjectionResult result;
  const bool mvrTrussPlacement = options.trussGeometryAuthority.value_or(
                                     MvrTrussGeometryAuthority::MvrGeometry) ==
                                 MvrTrussGeometryAuthority::MvrGeometry;
  const bool restoreTrussGeometry =
      mvrTrussPlacement &&
      options.trussGeometryExportMode == MvrTrussGeometryExportMode::Standard;
  if (scene.sourceStructures.empty())
    return result;
  auto *root = document.RootElement();
  std::map<std::string, const MvrSourceNode *> sources;
  std::map<std::string, Element *> elements;
  std::map<std::string, std::string> parents;
  for (const auto &source : scene.sourceStructures)
    if (source)
      for (const auto &node : source->nodes)
        sources.emplace(node.uuid, &node);
  // Snapshot the generated parent before moving any generated node.
  for (const auto &[id, node] : sources)
    if (auto *element = Find(root, id)) {
      elements[id] = element;
      auto *list = element->Parent() ? element->Parent()->ToElement() : nullptr;
      auto *owner =
          list && list->Parent() ? list->Parent()->ToElement() : nullptr;
      parents[id] = owner ? Id(*owner) : "";
    }
  for (const auto &[id, node] : sources)
    if (!node->modeled && node->valid && SourceNodeSurvives(scene, *node)) {
      tinyxml2::XMLDocument original;
      original.Parse(node->xml.c_str());
      auto *copy = original.RootElement()->DeepClone(&document)->ToElement();
      RegisterResources(*copy, id, resource, result);
      elements[id] = copy;
    }
  std::map<std::string, std::string> effectiveParents;
  for (const auto &[id, node] : sources)
    if (elements.contains(id)) {
      auto current = Current(scene, id);
      auto parent = node->parentUuid;
      if (node->modeled && (current.parent != node->projectedParentUuid ||
                            current.layer != node->layerUuid))
        parent = parents[id];
      std::set<std::string> visited;
      while (!parent.empty() && !elements.contains(parent) &&
             sources.contains(parent) && visited.insert(parent).second)
        parent = sources.at(parent)->parentUuid;
      effectiveParents[id] = parent;
    }
  std::map<std::string, Matrix> worlds;
  std::set<std::string> visiting;
  const auto world = [&](auto &&self, const std::string &id) -> Matrix {
    if (id.empty())
      return {};
    if (worlds.contains(id))
      return worlds.at(id);
    if (!visiting.insert(id).second) {
      result.diagnostics.push_back(
          Error(id, "ChildList", "Source hierarchy contains a cycle."));
      return {};
    }
    Matrix value;
    const auto current = Current(scene, id);
    const auto found = sources.find(id);
    if (found == sources.end())
      value = current.world;
    else if (current.exists) {
      const auto &node = *found->second;
      value = current.world;
      if (node.kind == "Truss" && node.geometryAuthority && mvrTrussPlacement) {
        if (Equal(value, node.displayedWorld))
          value = node.authoredWorld;
        else if (value.u == node.displayedWorld.u &&
                 value.v == node.displayedWorld.v &&
                 value.w == node.displayedWorld.w) {
          // Translation edits do not require inverting a possibly singular
          // authored display placement.
          const auto edited = value.o;
          value = node.authoredWorld;
          for (size_t axis = 0; axis < 3; ++axis)
            value.o[axis] += edited[axis] - node.displayedWorld.o[axis];
        } else {
          Matrix inverse;
          if (!Inverse(node.displayedWorld, inverse))
            result.diagnostics.push_back(
                Error(id, "Matrix",
                      "Edited geometry placement cannot be reconciled with a "
                      "singular display baseline."));
          else
            value = MatrixUtils::Multiply(MatrixUtils::Multiply(value, inverse),
                                          node.authoredWorld);
        }
      }
    } else if (found->second->localSource)
      value = MatrixUtils::Multiply(self(self, effectiveParents[id]),
                                    found->second->localSource->projection);
    visiting.erase(id);
    worlds[id] = value;
    return value;
  };
  for (const auto &[id, node] : sources)
    if (elements.contains(id)) {
      auto *element = elements.at(id);
      if (node->geometryAuthority && node->valid &&
          (node->kind != "Truss" || restoreTrussGeometry)) {
        tinyxml2::XMLDocument source;
        source.Parse(node->xml.c_str());
        auto *original = source.RootElement()->FirstChildElement("Geometries");
        if (original) {
          auto *geometry = original->DeepClone(&document)->ToElement();
          RegisterResources(*geometry, id, resource, result,
                            node->editedGeometry);
          if (auto *old = element->FirstChildElement("Geometries")) {
            element->InsertAfterChild(old, geometry);
            element->DeleteChild(old);
          } else {
            tinyxml2::XMLElement *previous = nullptr;
            for (auto *child = element->FirstChildElement(); child;
                 child = child->NextSiblingElement()) {
              const std::string_view name = child->Name();
              if (name != "Matrix" && name != "Classing" && name != "Position")
                break;
              previous = child;
            }
            if (previous)
              element->InsertAfterChild(previous, geometry);
            else
              element->InsertFirstChild(geometry);
          }
        }
      }
      if (node->valid) {
        tinyxml2::XMLDocument source;
        source.Parse(node->xml.c_str());
        if (const char *multipatch =
                source.RootElement()->Attribute("multipatch"))
          element->SetAttribute("multipatch",
                                CanonicalizeUuid(multipatch).c_str());
      }
      const auto parent = effectiveParents[id];
      // The ordinary writer already retains prepared local numeric provenance.
      // Recompute only when restoring a different owner or unbaking display
      // geometry; otherwise an ancestor edit would narrow unrelated matrices.
      const bool nativePlacement =
          node->modeled && parent == parents[id] &&
          (node->kind != "Truss" || !mvrTrussPlacement ||
           Equal(node->displayedWorld, node->authoredWorld));
      if (!nativePlacement) {
        const auto parentWorld = world(world, parent),
                   nodeWorld = world(world, id);
        Matrix local, inverse;
        Matrix originalParent;
        if (sources.contains(node->parentUuid))
          originalParent = sources.at(node->parentUuid)->authoredWorld;
        if (node->localSource && Equal(nodeWorld, node->authoredWorld) &&
            Equal(parentWorld, originalParent))
          local = node->localSource->projection;
        else if (Inverse(parentWorld, inverse)) {
          local = MatrixUtils::Multiply(inverse, nodeWorld);
          if (node->localSource && nodeWorld.u == node->authoredWorld.u &&
              nodeWorld.v == node->authoredWorld.v &&
              nodeWorld.w == node->authoredWorld.w &&
              parentWorld.u == originalParent.u &&
              parentWorld.v == originalParent.v &&
              parentWorld.w == originalParent.w) {
            local.u = node->localSource->projection.u;
            local.v = node->localSource->projection.v;
            local.w = node->localSource->projection.w;
          }
        } else {
          result.diagnostics.push_back(Error(
              id, "Matrix",
              "Edited hierarchy requires an invertible parent placement."));
          continue;
        }
        auto *matrix = element->FirstChildElement("Matrix");
        if (!matrix) {
          matrix = document.NewElement("Matrix");
          element->InsertFirstChild(matrix);
        }
        matrix->SetText(
            FormatCanonicalMatrix(local, node->localSource).c_str());
      }
      if (!parent.empty()) {
        auto *owner = elements.contains(parent) ? elements.at(parent)
                                                : Find(root, parent);
        if (owner)
          ChildList(document, *owner)->InsertEndChild(element);
        else
          result.diagnostics.push_back(Error(
              id, "ChildList", "Cannot resolve output parent: " + parent));
      }
    }
  return result;
}
} // namespace mvr

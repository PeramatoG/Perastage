/* This file is part of Perastage. */
#include "mvr_publication_repair.h"
#include "mvr_publication_text.h"
#include "mvr_import_reference_resolver.h"
#include "mvr_source_structure_internal.h"
#include "mvr_matrix_precision.h"
#include "uuidutils.h"
#include <algorithm>
#include <map>
#include <cmath>
namespace mvr::publication {
void RepairSymbols(MvrScene &scene, Findings &out) {
  MvrImportReferenceResolver identities;
  std::map<std::string, std::string> native;
  const auto collect = [&](const auto &values) {
    for (const auto &[id, value] : values)
      native[id] = id;
  };
  collect(scene.fixtures);
  collect(scene.trusses);
  collect(scene.supports);
  collect(scene.sceneObjects);
  collect(scene.groupObjects);
  collect(scene.layers);
  collect(scene.classes);
  collect(scene.positions);
  for (const auto &source : scene.sourceStructures)
    if (source)
      for (const auto &node : source->nodes)
        if (!node.modeled)
          native[node.uuid] = node.uuid;
  for (const auto &[id, value] : native)
    identities.ResolveStableUuid({"Native", {}, {}, {}, id, {}});
  std::map<std::string, std::string> definitionIds;
  std::map<std::string, SymdefDefinition> definitions(scene.symdefs.begin(),
                                                      scene.symdefs.end());
  scene.symdefs.clear();
  for (auto &[raw, definition] : definitions) {
    auto id = identities.ResolveStableUuid(
        {"Symdef", {}, definition.name.value_or(""), {}, raw, {}});
    if (definition.name && XmlText(*definition.name) != *definition.name) {
      Note(out, "Symdef", id, "@name",
           "Repaired malformed definition name encoding", *definition.name,
           MvrExportDiagnosticImpact::DataSubstituted);
      definition.name = XmlText(*definition.name);
    }
    definitionIds[raw] = id;
    scene.symdefs.emplace(id, std::move(definition));
    if (raw != id)
      Note(out, "Symdef", id, "@uuid", "Reassigned definition identity", raw,
           MvrExportDiagnosticImpact::IdentityChanged);
  }
  const auto definition = [&](const std::string &raw) {
    auto it = definitionIds.find(raw);
    const auto canonical = CanonicalizeUuid(raw);
    if (it == definitionIds.end() && !canonical.empty())
      it = definitionIds.find(canonical);
    return it == definitionIds.end() ? raw : it->second;
  };
  for (auto &issue : scene.preservationIssues)
    if (auto it = definitionIds.find(issue.objectUuid);
        it != definitionIds.end())
      issue.objectUuid = it->second;
  // An occurrence key is shared by retained XML and all expanded native
  // meshes of that instance. Repeated authored UUIDs remain separate instances.
  std::map<std::pair<std::string, std::string>, std::string> instances;
  const auto instance = [&](const std::string &owner, const std::string &raw) {
    // Retained XML uses canonical spelling; typed geometry can still carry
    // the authored spelling. Both projections must identify one occurrence.
    const auto canonical = CanonicalizeUuid(raw);
    const auto normalized = canonical.empty() ? raw : canonical;
    const auto key = std::pair{owner, normalized};
    if (instances.contains(key))
      return instances.at(key);
    const auto id =
        identities.ResolveStableUuid({"Symbol", owner, {}, {}, normalized, {}});
    instances[key] = id;
    if (id != raw)
      Note(out, "Symbol", owner, "Geometries/Symbol/@uuid",
           "Reassigned geometric instance identity to " + id, raw,
           MvrExportDiagnosticImpact::IdentityChanged);
    return id;
  };
  // Repair definition-instance identities before source occurrences in a stable
  // definition order, preserving repeated distinct instances independently.
  for (const auto &[raw, unused] : definitions) {
    auto &def = scene.symdefs.at(definition(raw));
    size_t index = 0;
    for (auto &symbol : def.symbols) {
      symbol.symdef = definition(symbol.symdef);
      symbol.uuid =
          instance(definition(raw) + "/Symbol[" + std::to_string(index++) + "]",
                   symbol.uuid);
    }
  }
  for (auto &original : scene.sourceStructures)
    if (original) {
      auto source = std::make_shared<MvrSourceStructure>(*original);
      for (auto &node : source->nodes) {
        size_t ordinal = 0;
        tinyxml2::XMLDocument xml;
        if (xml.Parse(node.xml.c_str()) != tinyxml2::XML_SUCCESS ||
            !xml.RootElement())
          continue;
        const auto visit = [&](auto &&self,
                               tinyxml2::XMLElement *element) -> void {
          if (std::string(element->Name()) == "Symbol") {
            const auto raw =
                element->Attribute("uuid") ? element->Attribute("uuid") : "";
            element->SetAttribute("uuid",
                                  instance(node.uuid + "/Symbol[" +
                                               std::to_string(ordinal++) + "]",
                                           raw)
                                      .c_str());
            const auto target = element->Attribute("symdef")
                                    ? element->Attribute("symdef")
                                    : "";
            element->SetAttribute("symdef", definition(target).c_str());
          }
          for (auto *child = element->FirstChildElement(); child;
               child = child->NextSiblingElement())
            self(self, child);
        };
        visit(visit, xml.RootElement());
        node.xml = source_structure::Xml(*xml.RootElement());
      }
      IndexSourceStructure(*source);
      original = std::move(source);
    }
  for (auto &[id, truss] : scene.trusses) {
    if (!truss.sourceSymdefUuid.empty())
      truss.sourceSymdefUuid = definition(truss.sourceSymdefUuid);
    if (!truss.sourceSymbolUuid.empty() ||
        truss.sourceRepresentation == Truss::GeometryRepresentation::SymbolSymdef)
      truss.sourceSymbolUuid =
          instance(id + "/Symbol[0]", truss.sourceSymbolUuid);
  }
  const auto occurrence = [](const std::string &owner, const std::string &key) {
    for (const auto *marker : {"/symbol/", "/support-symbol/"}) {
      auto at = key.find(marker);
      if (at == std::string::npos)
        continue;
      at += std::string(marker).size();
      auto end = key.find('/', at);
      return owner + "/Symbol[" + key.substr(at, end - at) + "]";
    }
    return owner + "/Symbol[0]";
  };
  const auto repairGeometry = [&](const std::string &owner, auto &geometry) {
    if (!geometry.sourceSymdefUuid.empty())
      geometry.sourceSymdefUuid = definition(geometry.sourceSymdefUuid);
    if (!geometry.sourceSymbolUuid.empty() || !geometry.sourceSymdefUuid.empty())
      geometry.sourceSymbolUuid = instance(
          occurrence(owner, geometry.instanceKey), geometry.sourceSymbolUuid);
  };
  const auto expanded = [&](auto &objects) {
    for (auto &[id, object] : objects)
      for (auto &geometry : object.geometries) {
        repairGeometry(id, geometry);
      }
  };
  expanded(scene.sceneObjects);
  expanded(scene.supports);
  for (auto &original : scene.sourceStructures)
    if (original) {
      auto source = std::make_shared<MvrSourceStructure>(*original);
      for (auto &node : source->nodes)
        for (auto &geometry : node.projectedGeometries)
          repairGeometry(node.uuid, geometry);
      original = std::move(source);
    }
  const auto matrix = [&](Matrix &value, std::optional<MvrMatrixSource> &source,
                          const std::string &owner) {
    bool invalid = false;
    const Matrix identity;
    for (auto pair :
         {std::pair{&value.u, &identity.u}, std::pair{&value.v, &identity.v},
          std::pair{&value.w, &identity.w}, std::pair{&value.o, &identity.o}})
      for (size_t i = 0; i < 3; ++i)
        if (!std::isfinite((*pair.first)[i])) {
          (*pair.first)[i] = (*pair.second)[i];
          invalid = true;
        }
    if (invalid) {
      Note(out, "Symdef", owner, "Matrix",
           "Replaced nonfinite geometry matrix components",
           "nonfinite components in source matrix",
           MvrExportDiagnosticImpact::DataSubstituted);
      source.reset();
    }
  };
  for (auto &[id, def] : scene.symdefs) {
    for (auto &geometry : def.geometries)
      matrix(geometry.transform, geometry.matrixSource, id);
    for (auto &symbol : def.symbols)
      matrix(symbol.transform, symbol.matrixSource, id);
  }
}
} // namespace mvr::publication

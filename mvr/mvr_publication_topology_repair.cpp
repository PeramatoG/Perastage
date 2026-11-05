/* This file is part of Perastage. */
#include "mvr_publication_repair.h"
#include "mvr_source_structure_internal.h"
#include "mvr_import_reference_resolver.h"
#include "mvr_matrix_precision.h"
#include "uuidutils.h"
#include "mvr_identity_recovery.h"
#include <map>
#include <algorithm>
#include <set>
namespace mvr::publication {
void ReconcileIdentityReferences(MvrScene &scene,
                                 const mvridentity::RecoveryResult &recovery) {
  std::map<std::pair<std::string, std::string>, std::set<std::string>> targets;
  for (const auto &item : recovery.diagnostics)
    if (!item.originalIdentity.empty() && !item.replacementIdentity.empty() &&
        item.originalIdentity != item.replacementIdentity)
      targets[{item.objectKind, item.originalIdentity}].insert(
          item.replacementIdentity);
  const auto remap = [&](const std::string &kind, std::string &id) {
    auto it = targets.find({kind, id});
    if (it != targets.end() && it->second.size() == 1)
      id = *it->second.begin();
  };
  for (auto &original : scene.sourceStructures)
    if (original) {
      auto source = std::make_shared<MvrSourceStructure>(*original);
      for (auto &node : source->nodes) {
        auto before = node.uuid;
        remap(node.kind == "FocusPoint" ? "SceneObject" : node.kind, node.uuid);
        if (before != node.uuid) {
          tinyxml2::XMLDocument xml;
          if (xml.Parse(node.xml.c_str()) == tinyxml2::XML_SUCCESS &&
              xml.RootElement()) {
            xml.RootElement()->SetAttribute("uuid", node.uuid.c_str());
            node.xml = source_structure::Xml(*xml.RootElement());
          }
        }
        remap("Layer", node.layerUuid);
        remap("Layer", node.parentUuid);
        remap("GroupObject", node.parentUuid);
        remap("Layer", node.projectedParentUuid);
        remap("GroupObject", node.projectedParentUuid);
      }
      IndexSourceStructure(*source);
      original = std::move(source);
    }
}
void RepairSourceIdentities(MvrScene &scene, Findings &out) {
  std::set<std::string> native;
  const auto collect = [&](const auto &values) {
    for (const auto &[id, value] : values)
      native.insert(id);
  };
  collect(scene.fixtures);
  collect(scene.trusses);
  collect(scene.supports);
  collect(scene.sceneObjects);
  collect(scene.groupObjects);
  collect(scene.layers);
  collect(scene.classes);
  collect(scene.positions);
  collect(scene.symdefs);
  MvrImportReferenceResolver resolver;
  for (const auto &id : native)
    resolver.ResolveStableUuid({"Native", {}, {}, {}, id, {}});
  size_t ordinal = 0;
  for (auto &original : scene.sourceStructures)
    if (original) {
      auto source = std::make_shared<MvrSourceStructure>(*original);
      std::map<std::string, std::vector<std::string>> aliases;
      for (auto &node : source->nodes)
        if (!node.modeled) {
          tinyxml2::XMLDocument xml;
          if (xml.Parse(node.xml.c_str()) != tinyxml2::XML_SUCCESS ||
              !xml.RootElement())
            continue;
          const auto raw = node.uuid;
          // An intact authored XML identity takes precedence over corrupt
          // indexing metadata. Allocation still reserves every editable/global
          // identity.
          auto authored = source_structure::Id(*xml.RootElement());
          const auto preferred =
              CanonicalizeUuid(raw).empty() ||
                      raw == "00000000-0000-0000-0000-000000000000"
                  ? authored
                  : raw;
          const auto repaired = resolver.ResolveStableUuid(
              {node.kind, std::to_string(ordinal++), {}, {}, preferred, {}});
          aliases[raw].push_back(repaired);
          if (repaired != raw) {
            Note(out, node.kind, repaired, "@uuid",
                 "Reconciled retained source occurrence identity", raw,
                 MvrExportDiagnosticImpact::IdentityChanged);
            node.uuid = repaired;
            xml.RootElement()->SetAttribute("uuid", repaired.c_str());
            node.xml = source_structure::Xml(*xml.RootElement());
          }
        }
      for (auto &node : source->nodes) {
        const auto remap = [&](std::string &id) {
          auto found = aliases.find(id);
          if (found != aliases.end() && found->second.size() == 1)
            id = found->second.front();
        };
        remap(node.parentUuid);
        remap(node.projectedParentUuid);
        remap(node.layerUuid);
      }
      IndexSourceStructure(*source);
      original = std::move(source);
    }
}
void RepairSourceHierarchy(MvrScene &scene, Findings &out) {
  std::map<std::string, MvrSourceNode> nodes;
  for (const auto &source : scene.sourceStructures)
    if (source)
      for (const auto &node : source->nodes)
        nodes[node.uuid] = node;
  for (auto &original : scene.sourceStructures)
    if (original) {
      auto source = std::make_shared<MvrSourceStructure>(*original);
      for (auto &node : source->nodes)
        if (!node.modeled) {
          bool broken = false;
          std::set<std::string> visited{node.uuid};
          for (auto parent = node.parentUuid; !parent.empty();) {
            if (!visited.insert(parent).second) {
              broken = true;
              break;
            }
            auto it = nodes.find(parent);
            if (it == nodes.end()) {
              broken = !source_structure::Current(scene, parent).exists;
              break;
            }
            parent = it->second.parentUuid;
          }
          Matrix parentWorld;
          bool rebase = broken;
          if (broken) {
            auto layer = scene.layers.find(node.layerUuid);
            if (layer == scene.layers.end()) {
              if (scene.layers.empty())
                continue;
              layer = scene.layers.find(
                  std::min_element(scene.layers.begin(), scene.layers.end(),
                                   [](const auto &a, const auto &b) {
                                     return a.first < b.first;
                                   })
                      ->first);
            }
            Note(out, node.kind, node.uuid, "ChildList/parent",
                 "Detached missing/cyclic source ownership into its surviving "
                 "layer, preserving authored world placement",
                 node.parentUuid, MvrExportDiagnosticImpact::DataSubstituted);
            node.parentUuid = node.layerUuid = layer->first;
            parentWorld = layer->second.transform;
          } else if (auto layer = scene.layers.find(node.parentUuid);
                     layer != scene.layers.end()) {
            auto before = nodes.find(node.parentUuid);
            if (before != nodes.end() &&
                !source_structure::Equal(before->second.authoredWorld,
                                         layer->second.transform)) {
              parentWorld = layer->second.transform;
              rebase = true;
            }
          }
          if (rebase) {
            Matrix inverse;
            if (!source_structure::Inverse(parentWorld, inverse))
              continue;
            const auto local =
                MatrixUtils::Multiply(inverse, node.authoredWorld);
            node.localSource =
                ParseCanonicalMatrix(MatrixUtils::FormatMatrix(local));
            tinyxml2::XMLDocument xml;
            if (xml.Parse(node.xml.c_str()) != tinyxml2::XML_SUCCESS ||
                !xml.RootElement())
              continue;
            auto *root = xml.RootElement();
            auto *matrix = root->FirstChildElement("Matrix");
            if (!matrix) {
              matrix = xml.NewElement("Matrix");
              root->InsertFirstChild(matrix);
            }
            matrix->SetText(MatrixUtils::FormatMatrix(local).c_str());
            node.xml = source_structure::Xml(*root);
          }
        }
      IndexSourceStructure(*source);
      original = std::move(source);
    }
}
} // namespace mvr::publication

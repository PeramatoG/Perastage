/* This file is part of Perastage. */
#include "mvr_publication_repair.h"
#include "mvr_publication_text.h"
#include "mvr_source_structure_internal.h"
#include "mvr_standard_preservation.h"
#include "mvr_standard_field_projection.h"
#include "mvr_symdef_preservation.h"
#include "mvr_matrix_precision.h"
#include "uuidutils.h"
#include "archive_entry_path.h"
#include "filesystem_path_utils.h"
#include "gdtf_archive_reader.h"
#include "gdtf_description_reader.h"
#include "mvr_binding_content.h"
#include <algorithm>
#include <map>
#include <set>
#include <string_view>
namespace mvr::publication {
using namespace source_structure;
namespace {
void Prune(tinyxml2::XMLElement &root, const MvrScene &scene,
           const std::set<std::string> &ids, const std::string &owner,
           Findings &out) {
  for (auto *child = root.FirstChildElement(); child;) {
    auto *next = child->NextSiblingElement();
    bool erase = false;
    std::string cause;
    const std::string name = child->Name();
    if (name == "Geometry3D") {
      const auto *file = child->Attribute("fileName");
      cause = file ? SymdefResourceProblem(scene, file) : "missing fileName";
      const auto *provenance = FindSourceNode(scene, owner);
      const bool trustedEdit = provenance && provenance->editedGeometry;
      const bool unsafe =
          file && !trustedEdit &&
          (perastage::archive::IsUnsafeNormalizedEntryPath(file) ||
           std::string(file).find('\\') != std::string::npos);
      if (file && FailedResourceReference(scene, owner, file))
        cause = "Source acquisition failed; a target namesake cannot replace "
                "the source resource.";
      erase = unsafe || !cause.empty() ||
              !ValidateType(Xml(*child), name, "Geometry3D");
      if (unsafe)
        cause = "unsafe source geometry reference";
    } else if (name == "Source" && child->Attribute("type") &&
               std::string(child->Attribute("type")) == "File") {
      const auto file = child->GetText() ? child->GetText() : "";
      auto path = PathUtils::PathFromUtf8(file);
      if (path.is_relative())
        path = PathUtils::PathFromUtf8(scene.basePath) / path;
      std::error_code error;
      erase = FailedResourceReference(scene, owner, file) ||
              perastage::archive::IsUnsafeNormalizedEntryPath(file) ||
              !std::filesystem::is_regular_file(path, error);
      cause = "unsafe or unavailable source media";
    } else if (name == "Symbol") {
      auto *target = child->Attribute("symdef");
      erase = !target || !scene.symdefs.contains(CanonicalizeUuid(target)) ||
              !ValidateType(Xml(*child), name, "Symbol");
      cause = "unrepresentable Symbol identity, matrix or definition";
    } else if (name == "Classing") {
      erase = !child->GetText() ||
              !scene.classes.contains(CanonicalizeUuid(child->GetText()));
      cause = "unresolved Class target";
    } else if (name == "Position") {
      erase = !child->GetText() ||
              !scene.positions.contains(CanonicalizeUuid(child->GetText()));
      cause = "unresolved Position target";
    }
    for (const auto *attribute : {"toObject", "linkedDef"})
      if (const auto *value = child->Attribute(attribute))
        if (!ids.contains(CanonicalizeUuid(value))) {
          erase = true;
          cause = std::string("unresolved ") + attribute + " target";
        }
    if (erase) {
      Note(out, name, owner, name,
           "Omitted only the affected source relationship/payload: " + cause,
           Xml(*child));
      root.DeleteChild(child);
    } else
      Prune(*child, scene, ids, owner, out);
    child = next;
  }
  if (const auto *target = root.Attribute("multipatch"))
    if (!ids.contains(CanonicalizeUuid(target))) {
      Note(out, root.Name(), owner, "@multipatch",
           "Omitted unresolved multipatch relationship", target);
      root.DeleteAttribute("multipatch");
    }
}
} // namespace
void RepairSource(MvrScene &scene, Findings &out) {
  std::set<std::string> ids;
  const auto collect = [&](const auto &values) {
    for (const auto &[id, value] : values)
      ids.insert(id);
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
  for (const auto &source : scene.sourceStructures)
    if (source)
      for (const auto &node : source->nodes)
        ids.insert(node.uuid);
  for (const auto &issue : scene.preservationIssues)
    if (issue.node == "MappingDefinition" &&
        CanProjectStandardSourceField(issue)) {
      tinyxml2::XMLDocument xml;
      xml.Parse(issue.originalInformation.c_str());
      if (xml.RootElement())
        ids.insert(Id(*xml.RootElement()));
    }
  std::set<std::string> omittedBindings;
  std::map<std::string, std::pair<std::string, std::string>> bindings;
  for (const auto &issue : scene.preservationIssues)
    if (issue.sourceLocation.find("/SceneObject/") != std::string::npos &&
        (issue.node == "GDTFSpec" || issue.node == "GDTFMode")) {
      auto &pair = bindings[issue.objectUuid];
      (issue.node == "GDTFSpec" ? pair.first : pair.second) =
          issue.originalInformation;
    }
  for (const auto &[owner, pair] : bindings) {
    tinyxml2::XMLDocument specXml, modeXml;
    specXml.Parse(pair.first.c_str());
    modeXml.Parse(pair.second.c_str());
    const auto *spec = specXml.RootElement();
    const auto *mode = modeXml.RootElement();
    const std::string reference =
        spec && spec->GetText() ? spec->GetText() : "";
    const std::string name = mode && mode->GetText() ? mode->GetText() : "";
    auto path = PathUtils::PathFromUtf8(reference);
    if (path.is_relative())
      path = PathUtils::PathFromUtf8(scene.basePath) / path;
    bool valid = false;
    if (!FailedResourceReference(scene, owner, reference) &&
        HasBindingContent(reference.c_str()) &&
        HasBindingContent(name.c_str()) &&
        !perastage::archive::IsUnsafeNormalizedEntryPath(reference)) {
      const auto archive = gdtf::ReadGdtfArchive(path);
      if (archive.Success()) {
        const auto description =
            gdtf::ReadGdtfDescription(archive.descriptionXml);
        valid = description.Success() &&
                std::count(description.dmxModeNames.begin(),
                           description.dmxModeNames.end(), name) == 1;
      }
    }
    if (!valid) {
      omittedBindings.insert(owner);
      if (HasBindingContent(reference.c_str()) ||
          HasBindingContent(name.c_str()))
        Note(out, "SceneObject", owner, "GDTFSpec/GDTFMode",
             "Omitted unusable source-only optional binding; editable geometry "
             "retained",
             pair.first + pair.second);
    }
  }
  // Cyclic definition edges are independently omittable; other definitions,
  // instance placements and geometry survive. Traverse in stable key order.
  std::map<std::string, int> colors;
  for (const auto &[id, value] : scene.symdefs)
    colors[id] = 0;
  const auto graph = [&](auto &&self, const std::string &id) -> void {
    colors[id] = 1;
    auto &definition = scene.symdefs.at(id);
    std::erase_if(definition.symbols, [&](const auto &symbol) {
      if (!scene.symdefs.contains(symbol.symdef) ||
          colors[symbol.symdef] == 1) {
        Note(out, "Symdef", id, "ChildList/Symbol",
             "Omitted cyclic or unresolved definition edge",
             symbol.uuid + " -> " + symbol.symdef);
        return true;
      }
      if (!colors[symbol.symdef])
        self(self, symbol.symdef);
      return false;
    });
    definition.sourceComplete = true;
    colors[id] = 2;
  };
  for (const auto &[id, color] : colors)
    if (!colors[id])
      graph(graph, id);
  std::set<std::string> sourceIds;
  for (auto &original : scene.sourceStructures)
    if (original) {
      auto source = std::make_shared<MvrSourceStructure>(*original);
      std::erase_if(source->nodes, [&](auto &node) {
        if (XmlText(node.xml) != node.xml) {
          Note(out, node.kind, node.uuid, "Source",
               "Repaired malformed source XML text encoding", node.xml,
               MvrExportDiagnosticImpact::DataSubstituted);
          node.xml = XmlText(node.xml);
        }
        tinyxml2::XMLDocument xml;
        const bool parsed =
            xml.Parse(node.xml.c_str()) == tinyxml2::XML_SUCCESS &&
            xml.RootElement();
        auto *root = xml.RootElement();
        if (!parsed) {
          Note(out, node.kind, node.uuid, "Source",
               "Isolated unparseable retained XML; editable object remains",
               node.xml);
          return true;
        }
        const auto current = Current(scene, node.uuid);
        if (node.modeled && !current.exists)
          return true; // Explicit deletion by the user.
        if (!node.modeled &&
            (CanonicalizeUuid(node.uuid).empty() ||
             node.uuid == "00000000-0000-0000-0000-000000000000" ||
             sourceIds.contains(node.uuid))) {
          const auto old = node.uuid;
          unsigned suffix = 0;
          do {
            node.uuid =
                DeriveDeterministicUuid("mvr:source:" + old + ":" + node.xml +
                                        ":" + std::to_string(suffix++));
          } while (ids.contains(node.uuid));
          ids.insert(node.uuid);
          root->SetAttribute("uuid", node.uuid.c_str());
          Note(out, node.kind, node.uuid, "@uuid",
               "Reassigned invalid/conflicting source-only occurrence identity",
               old, MvrExportDiagnosticImpact::IdentityChanged);
        }
        sourceIds.insert(node.uuid);
        if (omittedBindings.contains(node.uuid))
          for (const auto *name : {"GDTFSpec", "GDTFMode"})
            while (auto *field = root->FirstChildElement(name))
              root->DeleteChild(field);
        Prune(*root, scene, ids, node.uuid, out);
        if (node.modeled && node.kind == "Truss" &&
            !Equal(current.world, node.displayedWorld) &&
            (current.world.u != node.displayedWorld.u ||
             current.world.v != node.displayedWorld.v ||
             current.world.w != node.displayedWorld.w)) {
          Matrix inverse;
          if (!Inverse(node.displayedWorld, inverse)) {
            Note(out, node.kind, node.uuid, "Matrix",
                 "Used edited world placement because the singular display "
                 "baseline cannot be inverted",
                 FormatCanonicalMatrix(node.authoredWorld, node.localSource),
                 MvrExportDiagnosticImpact::DataSubstituted);
            node.authoredWorld = node.displayedWorld = current.world;
            node.localSource.reset();
          }
        }
        if (node.modeled) {
          if ((node.kind == "SceneObject" || node.kind == "FocusPoint") &&
              current.geometries.empty() && current.model.empty()) {
            if (auto *old = root->FirstChildElement("Geometries"))
              root->DeleteChild(old);
            root->InsertEndChild(xml.NewElement("Geometries"));
            node.geometryAuthority = true;
          }
          if (auto *geometry = root->FirstChildElement("Geometries")) {
            if (!ValidateType(Xml(*geometry), "Geometries", "Geometries")) {
              Note(out, node.kind, node.uuid, "Geometries",
                   "Omitted malformed retained geometry container; native "
                   "geometry is authoritative",
                   Xml(*geometry));
              root->DeleteChild(geometry);
              node.geometryAuthority = false;
            }
          } else if (node.geometryAuthority) {
            root->InsertEndChild(xml.NewElement("Geometries"));
          }
          node.valid = true;
          if (node.kind == "Layer") {
            node.localSource = scene.layers.at(node.uuid).matrixSource;
            node.authoredWorld = node.displayedWorld =
                scene.layers.at(node.uuid).transform;
          }
        } else {
          node.valid = ValidateType(Xml(*root), node.kind, node.kind);
          if (!node.valid) {
            Note(out, node.kind, node.uuid, "Source",
                 "Isolated unrepresentable source-only node; represented "
                 "editable objects are retained",
                 node.xml);
            ids.erase(node.uuid);
            return true;
          }
        }
        node.xml = Xml(*root);
        return false;
      });
      IndexSourceStructure(*source);
      original = std::move(source);
    }
  // Imported risk records are not an independent scene serializer. Keep every
  // projectable standard field; isolate only content the owning writer cannot
  // represent, with its complete original evidence in the disclosure.
  std::erase_if(scene.preservationIssues, [&](auto &issue) {
    if (issue.kind == MvrPreservationIssueKind::ResourceFailure)
      return false;
    if (issue.reason.starts_with("Ambiguous imported reference:")) {
      Note(out, issue.node, issue.objectUuid, issue.sourceLocation,
           "Omitted only the genuinely ambiguous imported relationship; all "
           "object occurrences retained",
           issue.originalInformation);
      return true;
    }
    if (omittedBindings.contains(issue.objectUuid) &&
        (issue.node == "GDTFSpec" || issue.node == "GDTFMode"))
      return true;
    if (CanRestoreSourceIssue(scene, issue))
      return false;
    if (CanProjectStandardSourceField(issue)) {
      tinyxml2::XMLDocument xml;
      xml.Parse(issue.originalInformation.c_str());
      if (xml.RootElement()) {
        Prune(*xml.RootElement(), scene, ids, issue.objectUuid, out);
        issue.originalInformation = Xml(*xml.RootElement());
      }
      return false;
    }
    Note(out, issue.node, issue.objectUuid, issue.sourceLocation,
         "Canonicalized the represented editable value; unrepresentable "
         "source-only content retained as inert recovery evidence: " +
             issue.reason,
         issue.originalInformation, MvrExportDiagnosticImpact::DataSubstituted);
    return true;
  });
  std::erase_if(scene.preservationIssues, [&](const auto &issue) {
    if (issue.kind != MvrPreservationIssueKind::ResourceFailure)
      return false;
    Note(out, issue.node, issue.objectUuid, issue.sourceLocation,
         "Omitted the failed acquired source reference during canonical "
         "projection; no target namesake substitution",
         issue.originalInformation);
    return true;
  });
  // Remove bindings/geometry provenance whose definitions are not represented;
  // the native geometry projection is still serialized independently.
  for (auto &[id, truss] : scene.trusses)
    if (truss.sourceRepresentation ==
            Truss::GeometryRepresentation::SymbolSymdef &&
        !scene.symdefs.contains(truss.sourceSymdefUuid)) {
      Note(out, "Truss", id, "Geometries/Symbol",
           "Retained available native geometry without unresolved Symbol "
           "definition",
           truss.sourceSymbolUuid + " -> " + truss.sourceSymdefUuid);
      truss.sourceRepresentation = Truss::GeometryRepresentation::Geometry3D;
      truss.sourceSymbolUuid.clear();
      truss.sourceSymdefUuid.clear();
    }
  const auto flattened = [&](auto &objects) {
    for (auto &[id, node] : objects)
      for (auto &geometry : node.geometries)
        if (!SourceOwnsGeometry(scene, id) ||
            !scene.symdefs.contains(geometry.sourceSymdefUuid)) {
          if (!geometry.sourceSymbolUuid.empty())
            Note(out, "Geometry3D", id, "Symbol",
                 "Retained expanded geometry without unrepresentable Symbol "
                 "provenance",
                 geometry.sourceSymbolUuid + " -> " +
                     geometry.sourceSymdefUuid);
          geometry.sourceSymbolUuid.clear();
          geometry.sourceSymdefUuid.clear();
        }
  };
  flattened(scene.sceneObjects);
  flattened(scene.supports);
}
} // namespace mvr::publication

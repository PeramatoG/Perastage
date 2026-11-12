/* This file is part of Perastage. */
#include "mvr_publication_repair.h"
#include "mvr_scene_resource_lookup.h"
#include "mvr_publication_text.h"
#include "mvr_symdef_preservation.h"
#include "mvr_source_structure_internal.h"
#include "mvr_matrix_precision.h"
#include "gdtf_archive_reader.h"
#include "gdtf_description_reader.h"
#include "fixture_network_reference.h"
#include "filesystem_path_utils.h"
#include "primitive_model_resources.h"
#include "archive_entry_path.h"
#include <algorithm>
#include <map>
#include <tinyxml2.h>
namespace mvr::publication {
bool FailedResourceReference(const MvrScene &scene, const std::string &owner,
                             const std::string &reference) {
  return std::any_of(scene.preservationIssues.begin(),
                     scene.preservationIssues.end(), [&](const auto &issue) {
                       return issue.kind ==
                                  MvrPreservationIssueKind::ResourceFailure &&
                              issue.objectUuid == owner &&
                              issue.originalInformation == reference;
                     });
}
namespace {
// Diagnostics keep full filesystem context with portable UTF-8 separators.
std::string GenericPathText(const std::filesystem::path &path) {
  const auto text = path.generic_u8string();
  return {text.begin(), text.end()};
}
std::filesystem::path Path(const MvrScene &scene, const std::string &name) {
  return PathUtils::ResolveSceneResourcePath(name, PathUtils::PathFromUtf8(scene.basePath));
}
std::vector<std::string> Modes(const std::filesystem::path &path) {
  const auto archive = gdtf::ReadGdtfArchive(path);
  if (!archive.Success() ||
      XmlText(archive.descriptionXml) != archive.descriptionXml)
    return {};
  tinyxml2::XMLDocument xml;
  if (xml.Parse(archive.descriptionXml.c_str()) != tinyxml2::XML_SUCCESS)
    return {};
  auto *root = xml.FirstChildElement("GDTF");
  auto *type = root ? root->FirstChildElement("FixtureType") : nullptr;
  if (!type || !type->FirstChildElement("AttributeDefinitions") ||
      !type->FirstChildElement("Geometries") ||
      !type->FirstChildElement("DMXModes"))
    return {};
  const auto description = gdtf::ReadGdtfDescription(archive.descriptionXml);
  if (!description.Success())
    return {};
  std::map<std::string, unsigned> counts;
  for (const auto &mode : description.dmxModeNames)
    ++counts[mode];
  std::vector<std::string> modes;
  for (const auto &[name, count] : counts)
    if (count == 1 && !name.empty())
      modes.push_back(name);
  return modes;
}
std::string NetworkText(const FixtureNetworkAssignment &n) {
  return "geometry=" + n.geometry + ", ipv4=" + n.ipv4.value_or("") +
         ", subnetmask=" + n.subnetmask.value_or("") +
         ", ipv6=" + n.ipv6.value_or("") + ", dhcp=" + n.dhcp.value_or("") +
         ", hostname=" + n.hostname.value_or("");
}
template <class Node>
void Networks(Node &node, const char *type, const MvrScene &scene,
              Findings &out) {
  if (!node.dmxAddresses)
    return;
  std::erase_if(node.dmxAddresses->networks, [&](const auto &network) {
    auto single = node;
    single.dmxAddresses->networks = {network};
    const auto check = fixture_network::InspectNodeReferences(
        single, Path(scene, node.gdtfSpec));
    if (check && !node.gdtfSpec.empty())
      return false;
    Note(out, type, node.uuid, "Addresses/Network",
         "Omitted only the invalid/unresolvable Network assignment: " +
             check.detail,
         NetworkText(network));
    return true;
  });
}
void EmptyGeometryEvidence(MvrScene &scene, const SceneObject &object) {
  auto source = std::make_shared<MvrSourceStructure>();
  MvrSourceNode node;
  node.uuid = object.uuid;
  node.kind =
      object.kind == SceneObjectKind::FocusPoint ? "FocusPoint" : "SceneObject";
  node.parentUuid = object.parentGroupUuid;
  node.projectedParentUuid = object.parentGroupUuid;
  node.layerUuid = object.layerUuid;
  node.modeled = node.valid = node.geometryAuthority = true;
  node.authoredWorld = node.displayedWorld = object.transform;
  node.localSource =
      ParseCanonicalMatrix(MatrixUtils::FormatMatrix(object.localTransform));
  node.xml = "<" + node.kind + " uuid='" + object.uuid + "'><Geometries/></" +
             node.kind + ">";
  source->nodes.push_back(std::move(node));
  IndexSourceStructure(*source);
  scene.sourceStructures.push_back(std::move(source));
}
} // namespace
void RepairResources(MvrScene &scene, const std::filesystem::path &fallback,
                     Findings &out) {
  std::map<std::filesystem::path, std::vector<std::string>> cache;
  const auto modesFor = [&](const std::filesystem::path &path)
      -> const std::vector<std::string> & {
    auto [it, inserted] = cache.try_emplace(path);
    if (inserted)
      it->second = Modes(path);
    return it->second;
  };
  const auto fallbackModes =
      fallback.empty() ? std::vector<std::string>{} : modesFor(fallback);
  for (auto &[id, fixture] : scene.fixtures) {
    auto spec = fixture.gdtfSpec.empty() ? fixture.originalMvrGdtfSpec
                                         : fixture.gdtfSpec;
    const auto exact = Path(scene, spec);
    if (!spec.empty() && !exact.empty() &&
        !FailedResourceReference(scene, id, spec) &&
        !FindSourceNode(scene, id)) {
      const auto resolved = ResolveExistingSceneResource(
          spec, PathUtils::PathFromUtf8(scene.basePath), false);
      std::error_code equivalentError;
      const bool sameFile =
          std::filesystem::equivalent(resolved, exact, equivalentError);
      if (!resolved.empty() && !sameFile &&
          resolved.lexically_normal() != exact.lexically_normal()) {
        Note(out, "Fixture", id, "GDTFSpec",
             "Resolved a unique legacy native filename alias to " +
                 GenericPathText(resolved),
             spec, MvrExportDiagnosticImpact::DataSubstituted);
        spec = PathUtils::PathToUtf8(resolved);
      }
    }
    auto modes = (spec.empty() || FailedResourceReference(scene, id, spec))
                     ? std::vector<std::string>{}
                     : modesFor(Path(scene, spec));
    const bool nativeModeDefault = fixture.gdtfMode.empty() && !FindSourceNode(scene, id);
    if (!spec.empty() && (modes.empty() || (!nativeModeDefault &&
        std::find(modes.begin(), modes.end(), fixture.gdtfMode) == modes.end()))) {
      out.push_back({modes.empty() ? MvrExportDiagnosticCode::ResourceMissing
                                 : MvrExportDiagnosticCode::StandardDataPreservationBlocked,
          MvrExportDiagnosticSeverity::Error, MvrExportDiagnosticImpact::ExportFailed,
          true, "Fixture", fixture.instanceName, id, spec,
          "Cannot preserve the assigned GDTF and mode '" + fixture.gdtfMode +
          "'. Restore the selected resource or explicitly choose another definition/mode; "
          "the authoritative binding was retained: " + spec});
      continue;
    }
    if (modes.empty() && !fallbackModes.empty()) {
      Note(out, "Fixture", id, "GDTFSpec/GDTFMode",
           "Substituted the installed generic fallback definition and its "
           "actual declared mode; source definition unavailable/unusable",
           spec + " / " + fixture.gdtfMode,
           MvrExportDiagnosticImpact::DataSubstituted);
      out.back().code = MvrExportDiagnosticCode::GdtfFallbackUsed;
      out.back().objectName = fixture.instanceName;
      fixture.gdtfSpec = PathUtils::PathToUtf8(fallback);
      fixture.originalMvrGdtfSpec.clear();
      fixture.gdtfMode = fallbackModes.front();
    } else if (!modes.empty()) {
      if (std::find(modes.begin(), modes.end(), fixture.gdtfMode) ==
          modes.end()) {
        Note(out, "Fixture", id, "GDTFMode",
             "Selected an existing declared mode '" + modes.front() +
                 "' from the same definition",
             fixture.gdtfMode, MvrExportDiagnosticImpact::DataSubstituted);
        fixture.gdtfMode = modes.front();
      }
      fixture.gdtfSpec = spec;
    }
    Networks(fixture, "Fixture", scene, out);
  }
  const auto optional = [&](auto &objects, const char *type) {
    for (auto &[id, node] : objects) {
      if (!node.gdtfSpec.empty()) {
        const auto modes = FailedResourceReference(scene, id, node.gdtfSpec)
                               ? std::vector<std::string>{}
                               : modesFor(Path(scene, node.gdtfSpec));
        if (modes.empty() || std::find(modes.begin(), modes.end(),
                                       node.gdtfMode) == modes.end()) {
          Note(out, type, id, "GDTFSpec/GDTFMode",
               "Omitted unavailable/invalid optional standard binding; other "
               "object data retained",
               node.gdtfSpec + " / " + node.gdtfMode);
          node.gdtfSpec.clear();
          node.gdtfMode.clear();
          if constexpr (requires { node.boundGdtfMetadata; })
            if (!node.dmxAddresses)
              node.dmxAddresses.emplace();
        }
      }
      if constexpr (requires { node.boundGdtfMetadata; }) {
        if (!node.perastageAuxGdtfArchivePath.empty() &&
            modesFor(Path(scene, node.perastageAuxGdtfArchivePath)).empty()) {
          Note(out, type, id, "AuxGdtf",
               "Omitted unavailable auxiliary binding",
               node.perastageAuxGdtfArchivePath);
          node.perastageAuxGdtfArchivePath.clear();
        }
        if (node.boundGdtfMetadata &&
            *node.boundGdtfMetadata != node.CaptureGdtfSourceMetadata()) {
          // Keep authored resource bytes. Explicit project metadata is already
          // written by TrussInfo; the binding is not a losslessness
          // certificate.
          Note(out, type, id, "GDTFSpec/TypeMetadata",
               "Retained source GDTF bytes and current project type metadata "
               "separately",
               PathUtils::PathFromUtf8(node.gdtfSpec)
                   .filename()
                   .generic_string(),
               MvrExportDiagnosticImpact::DataSubstituted);
          // Preserve the baseline so the writer can mark explicit native
          // overrides.
        }
      }
      Networks(node, type, scene, out);
    }
  };
  optional(scene.trusses, "Truss");
  optional(scene.supports, "Support");
  const auto unavailable = [&](const std::string &file, const char *type,
                               const std::string &id) {
    std::string primitive;
    if (ResolvePrimitiveTokenFromModelRef(file, primitive))
      return false;
    auto problem = FailedResourceReference(scene, id, file)
                       ? "Source resource acquisition failed; target "
                         "namesakes are not replacements."
                       : SymdefResourceProblem(scene, file);
    if (problem.empty())
      return false;
    const auto portable = GenericPathText(PathUtils::PathFromUtf8(file));
    // Replace this exact resource spelling, not arbitrary authored backslashes.
    if (!file.empty()) {
      size_t at = 0;
      while ((at = problem.find(file, at)) != std::string::npos) {
        problem.replace(at, file.size(), portable);
        at += portable.size();
      }
    }
    Note(out, type, id, "Geometries/Geometry3D",
         "Omitted unavailable/unpackageable geometry reference: " + problem,
         portable);
    return true;
  };
  for (auto &[id, definition] : scene.symdefs)
    std::erase_if(definition.geometries, [&](const auto &geometry) {
      return unavailable(geometry.file, "Symdef", id);
    });
  const auto geometry = [&](auto &objects, const char *type) {
    for (auto &[id, object] : objects) {
      std::erase_if(object.geometries, [&](const auto &item) {
        return unavailable(item.modelFile, type,
                           FailedResourceReference(scene, item.sourceSymdefUuid,
                                                   item.modelFile)
                               ? item.sourceSymdefUuid
                               : id);
      });
      if (!object.modelFile.empty() && unavailable(object.modelFile, type, id))
        object.modelFile.clear();
    }
  };
  geometry(scene.sceneObjects, "SceneObject");
  geometry(scene.supports, "Support");
  for (auto &[id, truss] : scene.trusses)
    for (auto *file : {&truss.modelFile, &truss.symbolFile})
      if (!file->empty() &&
          PathUtils::PathFromUtf8(*file).extension() != ".gdtf" &&
          unavailable(
              *file, "Truss",
              FailedResourceReference(scene, truss.sourceSymdefUuid, *file)
                  ? truss.sourceSymdefUuid
                  : id))
        file->clear();
  // A valid empty Geometries container retains the object, not a fictitious
  // cube.
  for (const auto &[id, object] : scene.sceneObjects)
    if (object.geometries.empty() && object.modelFile.empty() &&
        !FindSourceNode(scene, id))
      EmptyGeometryEvidence(scene, object);
}
} // namespace mvr::publication

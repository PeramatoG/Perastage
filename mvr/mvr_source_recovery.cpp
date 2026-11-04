#include "mvr_source_recovery.h"
#include "mvr_source_structure_internal.h"
#include "mvr_import_reference_resolver.h"
#include "mvr_binding_content.h"
#include "mvr_truss_address_preservation.h"
#include "uuidutils.h"
#include <map>
#include <set>
namespace mvr {
using namespace source_structure;
namespace {
bool EmptyPair(Element &owner) {
  auto *spec = owner.FirstChildElement("GDTFSpec");
  auto *mode = owner.FirstChildElement("GDTFMode");
  const auto empty = [](const Element *field) {
    return !field || (!field->FirstAttribute() && !field->FirstChildElement() &&
                      !field->NextSiblingElement(field->Name()) &&
                      !HasBindingContent(field->GetText()));
  };
  return (spec || mode) && empty(spec) && empty(mode);
}
} // namespace
std::vector<MvrExportDiagnostic> RecoverRetainedSource(MvrScene &scene) {
  std::vector<MvrExportDiagnostic> result;
  std::map<std::string, size_t> counts;
  for (const auto &source : scene.sourceStructures)
    if (source)
      for (const auto &node : source->nodes)
        ++counts[node.uuid];
  std::set<std::string> recoveredOwners, used;
  MvrImportReferenceResolver replay;
  size_t occurrence = 0;
  for (auto &original : scene.sourceStructures)
    if (original) {
      auto source = std::make_shared<MvrSourceStructure>(*original);
      std::map<std::string, std::string> lastOwner;
      for (auto &node : source->nodes) {
        ++occurrence;
        tinyxml2::XMLDocument xml;
        if (xml.Parse(node.xml.c_str()) != tinyxml2::XML_SUCCESS)
          continue;
        auto *root = xml.RootElement();
        const auto raw = node.uuid;
        if (node.modeled && node.kind != "Layer") {
          const auto layer = scene.layers.find(node.layerUuid);
          const auto predicted = replay.ResolveStableUuid(
              {node.kind,
               layer == scene.layers.end() ? "" : layer->second.name,
               root->Attribute("name") ? root->Attribute("name") : "",
               MatrixUtils::FormatMatrix(node.authoredWorld),
               raw,
               {}});
          if (counts[raw] > 1 && node.kind == "SceneObject") {
            std::string target;
            // Reuse an actual resolver result, never create a second identity.
            if (scene.sceneObjects.contains(predicted) &&
                !used.contains(predicted))
              target = predicted;
            else if (node.localSource) {
              for (const auto &[id, object] : scene.sceneObjects)
                if (!used.contains(id) && object.layerUuid == node.layerUuid &&
                    object.matrixSource &&
                    object.matrixSource->values == node.localSource->values) {
                  if (!target.empty()) {
                    target.clear();
                    break;
                  }
                  target = id;
                }
            }
            if (!target.empty()) {
              used.insert(target);
              node.uuid = target;
              node.displayedWorld = node.authoredWorld;
              root->SetAttribute("uuid", target.c_str());
              if (target != raw)
                result.push_back(
                    {MvrExportDiagnosticCode::IdentityReassigned,
                     MvrExportDiagnosticSeverity::Warning,
                     MvrExportDiagnosticImpact::IdentityChanged,
                     true,
                     node.kind,
                     {},
                     target,
                     "Source/occurrence[" + std::to_string(occurrence) +
                         "]/@uuid",
                     "Reconciled retained source owner " + raw + " -> " +
                         target +
                         " using the imported editable occurrence; geometry "
                         "resource unchanged."});
              auto *link = root->FirstChildElement("Classing");
              if (link && !link->FirstAttribute() &&
                  !link->FirstChildElement() &&
                  !link->NextSiblingElement("Classing") && link->GetText() &&
                  scene.classes.contains(CanonicalizeUuid(link->GetText())))
                recoveredOwners.insert(raw);
            }
          }
        }
        if (lastOwner.contains(node.parentUuid))
          node.parentUuid = lastOwner[node.parentUuid];
        lastOwner[raw] = node.uuid;
        if ((node.kind == "Truss" || node.kind == "Support" ||
             node.kind == "SceneObject") &&
            EmptyPair(*root)) {
          if (auto *spec = root->FirstChildElement("GDTFSpec"))
            root->DeleteChild(spec);
          if (auto *mode = root->FirstChildElement("GDTFMode"))
            root->DeleteChild(mode);
        }
        node.xml = Xml(*root);
      }
      IndexSourceStructure(*source);
      original = std::move(source);
    }
  std::erase_if(scene.preservationIssues, [&](const auto &issue) {
    if (issue.node == "Classing" &&
        recoveredOwners.contains(CanonicalizeUuid(issue.objectUuid)) &&
        issue.reason == "Classing has an ambiguous owner, unsupported content, "
                        "or no unique Class target.") {
      tinyxml2::XMLDocument xml;
      xml.Parse(issue.originalInformation.c_str());
      auto *link = xml.RootElement();
      return link && !link->FirstAttribute() && !link->FirstChildElement() &&
             link->GetText() &&
             scene.classes.contains(CanonicalizeUuid(link->GetText()));
    }
    if (issue.reason ==
        "MVR 1.6 Table 28 requires an authored mode with GDTFSpec.") {
      tinyxml2::XMLDocument xml;
      xml.Parse(issue.originalInformation.c_str());
      return xml.RootElement() && EmptyPair(*xml.RootElement());
    }
    return false;
  });
  const auto normalizeBindings = [&](auto &objects, const char *kind) {
    for (auto &[id, object] : objects) {
      if constexpr (requires { object.boundGdtfMetadata; })
        if (!PreserveTrussAddressBinding(object))
          continue;
      const bool spec = HasBindingContent(object.gdtfSpec.c_str());
      const bool mode = HasBindingContent(object.gdtfMode.c_str());
      if (spec && mode)
        continue;
      if (spec || mode) {
        const auto before = "GDTFSpec='" + object.gdtfSpec + "', GDTFMode='" +
                            object.gdtfMode + "'";
        result.push_back({MvrExportDiagnosticCode::ReferenceCleared,
                          MvrExportDiagnosticSeverity::Warning,
                          MvrExportDiagnosticImpact::DataOmitted,
                          true,
                          kind,
                          {},
                          id,
                          std::string(kind) + "/GDTFSpec,GDTFMode",
                          "Omitted incomplete optional binding: " + before +
                              ". No mode or replacement definition was "
                              "invented; other object data is retained."});
        if (object.dmxAddresses) {
          for (const auto &network : object.dmxAddresses->networks)
            result.push_back(
                {MvrExportDiagnosticCode::ReferenceCleared,
                 MvrExportDiagnosticSeverity::Warning,
                 MvrExportDiagnosticImpact::DataOmitted,
                 true,
                 kind,
                 {},
                 id,
                 std::string(kind) + "/Addresses/Network",
                 "Omitted Network dependent on the incomplete binding: "
                 "geometry='" +
                     network.geometry + "', ipv4='" +
                     network.ipv4.value_or("") + "', subnetmask='" +
                     network.subnetmask.value_or("") + "', ipv6='" +
                     network.ipv6.value_or("") + "', dhcp='" +
                     network.dhcp.value_or("") + "', hostname='" +
                     network.hostname.value_or("") +
                     "'. DMX Address entries are retained."});
          object.dmxAddresses->networks.clear();
        }
      }
      object.gdtfSpec.clear();
      object.gdtfMode.clear();
      // An imported Truss without a standard binding must remain unbound,
      // rather than enter the application-owned generated-GDTF path.
      if constexpr (requires { object.boundGdtfMetadata; })
        if (!object.dmxAddresses)
          object.dmxAddresses.emplace();
      std::erase_if(scene.preservationIssues, [&](const auto &issue) {
        return CanonicalizeUuid(issue.objectUuid) == id &&
               issue.reason ==
                   "MVR 1.6 Table 28 requires an authored mode with GDTFSpec.";
      });
    }
  };
  normalizeBindings(scene.trusses, "Truss");
  normalizeBindings(scene.supports, "Support");
  return result;
}
} // namespace mvr

/* This file is part of Perastage. */
#include "mvr_publication_repair.h"
#include "mvr_publication_text.h"
#include "mvr_import_reference_resolver.h"
#include "filesystem_path_utils.h"
#include "mvr_source_structure_internal.h"
#include "mvr_matrix_precision.h"
#include "fixture_patch_service.h"
#include "uuidutils.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <map>
#include <charconv>
#include <limits>
#include <sstream>
#include <locale>
#include <tinyxml2.h>
namespace mvr::publication {
std::string PlanningNumber(float value) {
  char text[64];
  const auto result = std::to_chars(text, text + sizeof(text), value,
                                    std::chars_format::general,
                                    std::numeric_limits<float>::max_digits10);
  return result.ec == std::errc{} ? std::string(text, result.ptr)
                                  : std::string{};
}
void Note(Findings &out, const std::string &type, const std::string &id,
          const std::string &field, const std::string &action,
          const std::string &before, MvrExportDiagnosticImpact impact) {
  out.push_back({MvrExportDiagnosticCode::InternalRecovery,
                 MvrExportDiagnosticSeverity::Warning,
                 impact,
                 true,
                 XmlText(type),
                 {},
                 XmlText(id),
                 XmlText(field),
                 XmlText(type) + " uuid=" + XmlText(id) +
                     " field=" + XmlText(field) + ": " + XmlText(action) +
                     "; original=" + DiagnosticText(before)});
}
namespace {
bool Finite(const Matrix &m) {
  for (const auto &axis : {m.u, m.v, m.w, m.o})
    for (float value : axis)
      if (!std::isfinite(value))
        return false;
  return true;
}
// Finite world placement is authoritative; invalid components have no legal
// numeric representation and are individually disclosed before substitution.
void RepairMatrix(Matrix &m, const std::string &type, const std::string &id,
                  Findings &out) {
  if (Finite(m))
    return;
  const auto before = MatrixUtils::FormatMatrix(m);
  const Matrix identity;
  for (auto pair : {std::pair{&m.u, &identity.u}, std::pair{&m.v, &identity.v},
                    std::pair{&m.w, &identity.w}, std::pair{&m.o, &identity.o}})
    for (size_t i = 0; i < 3; ++i)
      if (!std::isfinite((*pair.first)[i]))
        (*pair.first)[i] = (*pair.second)[i];
  Note(out, type, id, "Matrix",
       "Replaced only nonfinite components with identity components", before,
       MvrExportDiagnosticImpact::DataSubstituted);
}
template <class Node>
void Addresses(Node &node, const char *type, Findings &out) {
  if (!node.dmxAddresses) {
    if constexpr (requires { node.address; })
      if (!node.address.empty() &&
          !perastage::patch::ParseFixturePatchAddress(node.address)) {
        Note(out, type, node.uuid, "Addresses/primary",
             "Omitted invalid legacy primary patch", node.address);
        node.address.clear();
      }
    return;
  }
  // Keep each independently valid Address, including its lexical spelling.
  // Duplicate normalized breaks retain the first authored occurrence.
  std::set<std::string> breaks;
  std::erase_if(node.dmxAddresses->entries, [&](const auto &entry) {
    const auto key = entry.breakId
                         ? perastage::patch::DmxInteger(*entry.breakId, true)
                         : std::optional<std::string>{"0"};
    auto candidate = node;
    candidate.dmxAddresses->entries = {entry};
    candidate.dmxAddresses->networks.clear();
    const auto validation =
        perastage::patch::InspectFixtureAddresses(candidate);
    if (key && validation && breaks.insert(*key).second)
      return false;
    Note(out, type, node.uuid, "Addresses/Address",
         "Omitted invalid or repeated DMX break",
         "break=" + entry.breakId.value_or("[absent]") +
             ", value=" + entry.value);
    return true;
  });
  if constexpr (requires { node.address; }) {
    auto withoutNetworks = node;
    withoutNetworks.dmxAddresses->networks.clear();
    const auto standard =
        perastage::patch::InspectFixtureAddresses(withoutNetworks);
    if (standard && !fixture_network::Validate(node) && node.address.empty())
      node.address = standard.primary;
    withoutNetworks.address = node.address;
    const auto reconciliation =
        perastage::patch::ResolveFixturePatch(withoutNetworks);
    if (!reconciliation && standard) {
      const auto requested =
          perastage::patch::ParseFixturePatchAddress(node.address);
      if (requested) {
        auto &entries = node.dmxAddresses->entries;
        auto primary =
            std::find_if(entries.begin(), entries.end(), [](const auto &entry) {
              return perastage::patch::DmxInteger(entry.breakId.value_or("0"),
                                                  true) ==
                     std::optional<std::string>{"0"};
            });
        const auto before =
            primary == entries.end() ? "[absent]" : primary->value;
        const auto absolute =
            (static_cast<int64_t>(requested->universe) - 1) * 512 +
            requested->channel;
        if (primary == entries.end())
          entries.push_back({std::string("0"), std::to_string(absolute)});
        else
          primary->value = std::to_string(absolute);
        Note(out, type, node.uuid, "Addresses/Address[break=0]",
             "Applied explicit primary edit while retaining all independent "
             "breaks; requested=" +
                 node.address,
             before, MvrExportDiagnosticImpact::DataSubstituted);
      } else {
        Note(out, type, node.uuid, "Addresses/primary",
             "Retained standard assignments; omitted unrepresentable legacy "
             "primary edit",
             node.address);
        node.address = standard.primary;
      }
    }
  }
}
} // namespace
void RepairNative(MvrScene &scene, Findings &out) {
  std::set<std::string> occupied;
  const auto reserve = [&](const auto &values) {
    for (const auto &[id, value] : values)
      occupied.insert(id);
  };
  reserve(scene.fixtures);
  reserve(scene.trusses);
  reserve(scene.supports);
  reserve(scene.sceneObjects);
  reserve(scene.groupObjects);
  reserve(scene.layers);
  reserve(scene.positions);
  reserve(scene.symdefs);
  std::map<std::string, MvrClass> classes(scene.classes.begin(),
                                          scene.classes.end());
  scene.classes.clear();
  std::map<std::string, std::string> classIds;
  for (const auto &[id, definition] : classes) {
    auto target = CanonicalizeUuid(id);
    if (target.empty() || target == "00000000-0000-0000-0000-000000000000" ||
        occupied.contains(target)) {
      unsigned index = 0;
      do {
        target = DeriveDeterministicUuid("mvr:class:" + id + ":" +
                                         std::to_string(index++));
      } while (occupied.contains(target));
      Note(out, "Class", id, "uuid",
           "Reassigned conflicting Class identity to " + target, id,
           MvrExportDiagnosticImpact::IdentityChanged);
    }
    occupied.insert(target);
    scene.classes[target] = definition;
    if (auto &name = scene.classes[target].name;
        name && XmlText(*name) != *name) {
      Note(out, "Class", target, "@name",
           "Repaired malformed class name encoding", *name,
           MvrExportDiagnosticImpact::DataSubstituted);
      name = XmlText(*name);
    }
    classIds[id] = target;
  }
  MvrImportReferenceResolver positionIds;
  const auto reservePositions = [&](const auto &values) {
    for (const auto &[id, value] : values)
      positionIds.ResolveStableUuid({"Native", {}, {}, {}, id, {}});
  };
  reservePositions(scene.fixtures);
  reservePositions(scene.trusses);
  reservePositions(scene.supports);
  reservePositions(scene.sceneObjects);
  reservePositions(scene.groupObjects);
  reservePositions(scene.layers);
  reservePositions(scene.classes);
  reservePositions(scene.symdefs);
  std::map<std::string, std::string> positions(scene.positions.begin(),
                                               scene.positions.end()),
      positionAliases;
  scene.positions.clear();
  for (const auto &[raw, name] : positions) {
    const auto id =
        positionIds.ResolveStableUuid({"Position", {}, name, {}, raw, {}});
    scene.positions[id] = name;
    positionAliases[raw] = id;
    if (id != raw)
      Note(out, "Position", id, "@uuid",
           "Repaired conflicting position identity", raw,
           MvrExportDiagnosticImpact::IdentityChanged);
  }
  const auto positionsFor = [&](auto &objects, const char *kind) {
    for (auto &[id, node] : objects)
      if (!node.position.empty()) {
        if (auto it = positionAliases.find(node.position);
            it != positionAliases.end())
          node.position = it->second;
        else if (!CanonicalizeUuid(node.position).empty() &&
                 !scene.positions.contains(CanonicalizeUuid(node.position))) {
          Note(out, kind, id, "Position",
               "Omitted unresolved Position relationship without inventing a "
               "target",
               node.position);
          node.position.clear();
          node.positionName.clear();
        }
      }
  };
  positionsFor(scene.fixtures, "Fixture");
  positionsFor(scene.trusses, "Truss");
  positionsFor(scene.supports, "Support");
  for (auto &original : scene.sourceStructures)
    if (original) {
      auto source = std::make_shared<MvrSourceStructure>(*original);
      for (auto &node : source->nodes) {
        tinyxml2::XMLDocument xml;
        if (xml.Parse(node.xml.c_str()) != tinyxml2::XML_SUCCESS ||
            !xml.RootElement())
          continue;
        auto *link = xml.RootElement()->FirstChildElement("Classing");
        if (link && link->GetText() && classIds.contains(link->GetText()))
          link->SetText(classIds.at(link->GetText()).c_str());
        auto *position = xml.RootElement()->FirstChildElement("Position");
        if (position && position->GetText() &&
            positionAliases.contains(position->GetText()))
          position->SetText(positionAliases.at(position->GetText()).c_str());
        node.xml = source_structure::Xml(*xml.RootElement());
      }
      original = std::move(source);
    }
  for (auto &[id, name] : scene.positions)
    if (XmlText(name) != name) {
      Note(out, "Position", id, "@name",
           "Repaired malformed position name encoding", name,
           MvrExportDiagnosticImpact::DataSubstituted);
      name = XmlText(name);
    }
  std::erase_if(scene.opaqueUserDataBlocks, [&](const auto &block) {
    tinyxml2::XMLDocument xml;
    if (XmlText(block.xml) == block.xml &&
        xml.Parse(block.xml.c_str()) == tinyxml2::XML_SUCCESS &&
        xml.RootElement())
      return false;
    Note(out, "UserData", {}, block.provider,
         "Isolated malformed provider XML as inert recovery evidence",
         block.xml);
    out.back().code = MvrExportDiagnosticCode::ForeignMetadataDiscarded;
    return true;
  });
  const auto objects = [&](auto &values, const char *type) {
    for (auto &[id, node] : values) {
      RepairMatrix(node.transform, type, id, out);
      const auto text = [&](std::string &value, const char *field) {
        auto repaired = XmlText(value);
        if (repaired != value) {
          Note(out, type, id, field, "Repaired malformed UTF-8/XML text", value,
               MvrExportDiagnosticImpact::DataSubstituted);
          value = std::move(repaired);
        }
      };
      if constexpr (requires { node.name; })
        text(node.name, "@name");
      if constexpr (requires { node.instanceName; })
        text(node.instanceName, "@name");
      if constexpr (requires { node.fixtureIdText; })
        text(node.fixtureIdText, "FixtureID");
      if constexpr (requires { node.function; })
        text(node.function, "Function");
      if constexpr (requires { node.positionName; })
        text(node.positionName, "Position/name");
      if constexpr (requires { node.manufacturer; })
        text(node.manufacturer, "Manufacturer");
      if constexpr (requires { node.model; })
        text(node.model, "Model");
      if constexpr (requires { node.gdtfDescription; })
        text(node.gdtfDescription, "Description");
      if constexpr (requires { node.typeName; })
        text(node.typeName, "Type/name");
      const auto number = [&](float &value, const char *field) {
        if (!std::isfinite(value)) {
          Note(out, type, id, field,
               "Replaced nonfinite planning value with zero",
               PlanningNumber(value),
               MvrExportDiagnosticImpact::DataSubstituted);
          value = 0;
        }
      };
      if constexpr (requires { node.weightKg; })
        number(node.weightKg, "Weight");
      if constexpr (requires { node.powerConsumptionW; })
        number(node.powerConsumptionW, "Power");
      if constexpr (requires { node.lengthMm; }) {
        number(node.lengthMm, "Length");
        number(node.widthMm, "Width");
        number(node.heightMm, "Height");
      }
      if constexpr (requires { node.chainLength; })
        number(node.chainLength, "ChainLength");
      if constexpr (requires { node.capacityKg; }) {
        number(node.capacityKg, "Capacity");
        number(node.loadKg, "Load");
      }
      if constexpr (requires { node.planningPowerOverrideW; }) {
        if (node.planningPowerOverrideW)
          number(*node.planningPowerOverrideW, "PlanningPowerOverride");
        if (node.sourcePowerConsumptionW)
          number(*node.sourcePowerConsumptionW, "SourcePower");
      }
      if constexpr (requires { node.mvrFixtureColorCie; }) {
        tinyxml2::XMLDocument color;
        auto *element = color.NewElement("Color");
        color.InsertEndChild(element);
        element->SetText(node.mvrFixtureColorCie.c_str());
        if (!node.mvrFixtureColorCie.empty() &&
            !source_structure::ValidateType(source_structure::Xml(*element),
                                            "Color", "ciecolortype")) {
          const auto original = node.mvrFixtureColorCie;
          auto text = original;
          std::replace(text.begin(), text.end(), ',', ' ');
          std::istringstream input(text);
          input.imbue(std::locale::classic());
          double values[3];
          bool numeric = std::count(original.begin(), original.end(), ',') == 2;
          for (auto &value : values)
            numeric = numeric && bool(input >> value) && std::isfinite(value);
          input >> std::ws;
          numeric = numeric && input.eof();
          if (numeric) {
            std::string canonical;
            for (double value : values) {
              char buffer[768];
              const auto result =
                  std::to_chars(buffer, buffer + sizeof(buffer), value,
                                std::chars_format::fixed);
              if (!canonical.empty())
                canonical += ',';
              canonical.append(buffer, result.ptr);
            }
            node.mvrFixtureColorCie = canonical;
          } else {
            Note(out, type, id, "Color",
                 "Omitted malformed source Color; independent project visual "
                 "color is retained",
                 original);
            node.mvrFixtureColorCie.clear();
            if (node.mvrFixtureColorHex == node.mvrFixtureColorAtImport) {
              node.mvrFixtureColorHex.clear();
              node.mvrFixtureColorAtImport.clear();
            }
          }
        }
      }
      if constexpr (requires { node.sourceGeometryMatrix; })
        RepairMatrix(node.sourceGeometryMatrix, type, id, out);
      if constexpr (requires { node.sourceSymbolMatrix; })
        RepairMatrix(node.sourceSymbolMatrix, type, id, out);
      if (!node.classing.empty()) {
        if (classIds.contains(node.classing))
          node.classing = classIds.at(node.classing);
        else if (!scene.classes.contains(CanonicalizeUuid(node.classing))) {
          Note(out, type, id, "Classing", "Omitted unresolved Class target",
               node.classing);
          node.classing.clear();
        } else
          node.classing = CanonicalizeUuid(node.classing);
      }
      if constexpr (requires { node.dmxAddresses; })
        Addresses(node, type, out);
    }
  };
  objects(scene.fixtures, "Fixture");
  objects(scene.trusses, "Truss");
  objects(scene.supports, "Support");
  objects(scene.sceneObjects, "SceneObject");
  objects(scene.groupObjects, "GroupObject");
  for (auto &[id, layer] : scene.layers) {
    if (XmlText(layer.name) != layer.name) {
      Note(out, "Layer", id, "@name", "Repaired malformed layer name encoding",
           layer.name, MvrExportDiagnosticImpact::DataSubstituted);
      layer.name = XmlText(layer.name);
    }
    RepairMatrix(layer.transform, "Layer", id, out);
    if (!CanonicalLayerMatrixIsElevation(layer.transform, layer.matrixSource)) {
      auto before = FormatCanonicalMatrix(layer.transform, layer.matrixSource);
      const auto z = layer.transform.o[2];
      layer.transform = {};
      layer.transform.o[2] = z;
      layer.matrixSource.reset();
      Note(out, "Layer", id, "Matrix",
           "Canonicalized Layer to elevation; editable world placements "
           "retained",
           before, MvrExportDiagnosticImpact::DataSubstituted);
    }
  }
  const auto detach = [&](auto &values, const char *type) {
    for (auto &[id, node] : values) {
      std::set<std::string> ancestors{id};
      auto parent = node.parentGroupUuid;
      bool invalid = false;
      while (!parent.empty()) {
        Matrix inverse;
        if (!scene.groupObjects.contains(parent) ||
            !ancestors.insert(parent).second ||
            !source_structure::Inverse(scene.groupObjects.at(parent).transform,
                                       inverse)) {
          invalid = true;
          break;
        }
        parent = scene.groupObjects.at(parent).parentGroupUuid;
      }
      if (invalid) {
        Note(out, type, id, "parentGroupUuid",
             "Detached cyclic, missing or singular parent; world placement "
             "retained",
             node.parentGroupUuid);
        node.parentGroupUuid.clear();
      }
      if (invalid || !Finite(node.localTransform)) {
        node.localTransform = node.transform;
        if constexpr (requires { node.hasLocalTransform; })
          node.hasLocalTransform = false;
      }
    }
  };
  detach(scene.groupObjects, "GroupObject");
  detach(scene.fixtures, "Fixture");
  detach(scene.trusses, "Truss");
  detach(scene.supports, "Support");
  detach(scene.sceneObjects, "SceneObject");
  std::map<std::string, std::map<std::string, MvrNodeType>> memberships;
  const auto children = [&](const auto &values, MvrNodeType type) {
    for (const auto &[id, node] : values)
      if (!node.parentGroupUuid.empty())
        memberships[node.parentGroupUuid][id] = type;
  };
  children(scene.groupObjects, MvrNodeType::GroupObject);
  children(scene.fixtures, MvrNodeType::Fixture);
  children(scene.trusses, MvrNodeType::Truss);
  children(scene.supports, MvrNodeType::Support);
  children(scene.sceneObjects, MvrNodeType::SceneObject);
  for (auto &[id, group] : scene.groupObjects) {
    auto remaining = memberships[id];
    std::erase_if(group.children, [&](auto &child) {
      auto it = remaining.find(child.uuid);
      if (it == remaining.end())
        return true;
      child.type = it->second;
      remaining.erase(it);
      return false;
    });
    for (const auto &[uuid, type] : remaining)
      group.children.push_back({type, uuid});
  }
  for (auto &[id, fixture] : scene.fixtures)
    if (!fixture.focus.empty()) {
      auto it = scene.sceneObjects.find(fixture.focus);
      if (it == scene.sceneObjects.end() ||
          it->second.kind != SceneObjectKind::FocusPoint) {
        Note(out, "Fixture", id, "Focus",
             "Omitted unresolved or wrong-type Focus target", fixture.focus);
        fixture.focus.clear();
      }
    }
  if (scene.versionMajor != 1 || scene.versionMinor > 6) {
    Note(out, "GeneralSceneDescription", {}, "version",
         "Projected represented scene to supported MVR 1.6",
         std::to_string(scene.versionMajor) + "." +
             std::to_string(scene.versionMinor),
         MvrExportDiagnosticImpact::DataSubstituted);
    scene.versionMajor = 1;
    scene.versionMinor = 6;
  }
}
namespace {
std::string PortableDisclosure(const MvrScene &scene, std::string text) {
  if (scene.basePath.empty())
    return text;
  const auto base = PathUtils::PathFromUtf8(scene.basePath).lexically_normal();
  if (base == base.root_path())
    return text;
  std::vector<std::string> prefixes{scene.basePath};
  const auto addPrefixes = [&](const std::filesystem::path &path) {
    prefixes.push_back(PathUtils::PathToUtf8(path));
    const auto generic = path.generic_u8string();
    prefixes.emplace_back(generic.begin(), generic.end());
  };
  addPrefixes(base);
  // Physical aliases include Windows short paths and macOS /var symlinks.
  std::error_code error;
  const auto physical = std::filesystem::weakly_canonical(base, error);
  if (!error && physical != physical.root_path())
    addPrefixes(physical);
  for (auto prefix : prefixes) {
    while (!prefix.empty() && (prefix.back() == '/' || prefix.back() == '\\'))
      prefix.pop_back();
    if (prefix.empty())
      continue;
    for (const auto suffix : {"/", "\\"}) {
      const auto absolute = prefix + suffix;
      size_t at = 0;
      while ((at = text.find(absolute, at)) != std::string::npos)
        text.erase(at, absolute.size());
    }
  }
  return text;
}
} // namespace
void RetainDisclosure(MvrScene &scene, const Findings &out) {
  tinyxml2::XMLDocument doc;
  auto *data = doc.NewElement("Data");
  doc.InsertEndChild(data);
  data->SetAttribute("provider", "org.perastage.canonical-recovery");
  data->SetAttribute("ver", "1");
  for (const auto &finding : out)
    if (finding.userVisible &&
        finding.impact != MvrExportDiagnosticImpact::None) {
      auto *entry = doc.NewElement("Finding");
      entry->SetAttribute("type", XmlText(finding.objectType).c_str());
      entry->SetAttribute("uuid", XmlText(finding.objectIdentity).c_str());
      entry->SetAttribute(
          "field",
          XmlText(PortableDisclosure(scene, finding.resourceName)).c_str());
      entry->SetAttribute("impact", static_cast<int>(finding.impact));
      entry->SetText(
          DiagnosticText(PortableDisclosure(scene, finding.technicalDetail))
              .c_str());
      data->InsertEndChild(entry);
    }
  if (!data->FirstChildElement())
    return;
  const auto xml = source_structure::Xml(*data);
  MvrOpaqueUserDataBlock block{"org.perastage.canonical-recovery", "1", xml};
  if (std::find(scene.opaqueUserDataBlocks.begin(),
                scene.opaqueUserDataBlocks.end(),
                block) == scene.opaqueUserDataBlocks.end())
    scene.opaqueUserDataBlocks.push_back(std::move(block));
}
void AppendDisclosure(tinyxml2::XMLDocument &doc, const Findings &findings,
                      const std::string &sourceBasePath) {
  auto *root = doc.RootElement();
  if (!root)
    return;
  auto *user = root->FirstChildElement("UserData");
  std::set<std::string> existing;
  const auto key = [](const tinyxml2::XMLElement &f) {
    return std::string(f.Attribute("uuid") ? f.Attribute("uuid") : "") + "/" +
           (f.Attribute("field") ? f.Attribute("field") : "") + "/" +
           (f.GetText() ? f.GetText() : "");
  };
  for (auto *data = user ? user->FirstChildElement("Data") : nullptr; data;
       data = data->NextSiblingElement("Data"))
    if (data->Attribute("provider") &&
        std::string(data->Attribute("provider")) ==
            "org.perastage.canonical-recovery")
      for (auto *entry = data->FirstChildElement("Finding"); entry;
           entry = entry->NextSiblingElement("Finding"))
        existing.insert(key(*entry));
  MvrScene metadata;
  metadata.basePath = sourceBasePath;
  RetainDisclosure(metadata, findings);
  for (const auto &block : metadata.opaqueUserDataBlocks) {
    tinyxml2::XMLDocument xml;
    xml.Parse(block.xml.c_str());
    auto *data = xml.RootElement();
    for (auto *entry = data->FirstChildElement("Finding"); entry;) {
      auto *next = entry->NextSiblingElement("Finding");
      if (!existing.insert(key(*entry)).second)
        data->DeleteChild(entry);
      entry = next;
    }
    if (!data->FirstChildElement())
      continue;
    if (!user) {
      user = doc.NewElement("UserData");
      root->InsertFirstChild(user);
    }
    user->InsertEndChild(data->DeepClone(&doc));
  }
}

} // namespace mvr::publication

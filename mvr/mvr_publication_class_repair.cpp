/* This file is part of Perastage. */
#include "mvr_publication_repair.h"
#include "mvr_import_reference_resolver.h"
#include "mvr_source_structure_internal.h"
#include "uuidutils.h"
#include <map>
#include <set>
namespace mvr::publication {
void RepairSourceClasses(MvrScene &scene, Findings &out) {
  const auto originalClasses = scene.classes;
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
  reserve(scene.symdefs);
  reserve(scene.positions);
  reserve(scene.classes);
  for (const auto &source : scene.sourceStructures)
    if (source)
      for (const auto &node : source->nodes)
        occupied.insert(node.uuid);
  MvrImportReferenceResolver identities;
  for (const auto &id : occupied)
    identities.ResolveStableUuid({"Native", {}, {}, {}, id, {}});
  std::map<std::string, std::vector<std::string>> aliases;
  const auto key = [](const std::string &raw) {
    auto canonical = CanonicalizeUuid(raw);
    return canonical.empty() ? raw : canonical;
  };
  size_t occurrence = 0;
  for (const auto &issue : scene.preservationIssues)
    if (issue.node == "Class") {
      tinyxml2::XMLDocument xml;
      if (xml.Parse(issue.originalInformation.c_str()) != tinyxml2::XML_SUCCESS)
        continue;
      auto *definition = xml.RootElement();
      if (!definition || std::string(definition->Name()) != "Class")
        continue;
      const std::string raw =
          definition->Attribute("uuid") ? definition->Attribute("uuid") : "";
      if (originalClasses.contains(key(raw)))
        continue;
      const std::string name =
          definition->Attribute("name") ? definition->Attribute("name") : "";
      auto id = identities.ResolveStableUuid(
          {"Class", std::to_string(occurrence++), name, {}, raw, {}});
      MvrClass value;
      if (definition->Attribute("name"))
        value.name = name;
      scene.classes.emplace(id, std::move(value));
      aliases[key(raw)].push_back(id);
      Note(out, "Class", id, "AUXData/Class/@uuid",
           "Recovered represented Class name and occurrence identity from "
           "source evidence",
           issue.originalInformation,
           MvrExportDiagnosticImpact::IdentityChanged);
    }
  const auto target =
      [&](const std::string &raw) -> std::optional<std::string> {
    auto it = aliases.find(key(raw));
    if (it == aliases.end())
      return {};
    return it->second.size() == 1 ? it->second.front() : std::string{};
  };
  const auto apply = [&](auto &values, const std::string &owner,
                         const std::string &raw,
                         const std::string &replacement) {
    auto it = values.find(owner);
    if (it == values.end())
      return;
    if (it->second.classing.empty() || key(it->second.classing) == key(raw))
      it->second.classing = replacement;
  };
  std::erase_if(scene.preservationIssues, [&](const auto &issue) {
    if (issue.node != "Classing")
      return false;
    tinyxml2::XMLDocument xml;
    if (xml.Parse(issue.originalInformation.c_str()) != tinyxml2::XML_SUCCESS)
      return false;
    auto *link = xml.RootElement();
    if (!link || !link->GetText() || link->FirstAttribute() ||
        link->FirstChildElement())
      return false;
    auto replacement = target(link->GetText());
    if (!replacement)
      return false;
    auto owner = CanonicalizeUuid(issue.objectUuid);
    const std::string raw = link->GetText();
    apply(scene.fixtures, owner, raw, *replacement);
    apply(scene.trusses, owner, raw, *replacement);
    apply(scene.supports, owner, raw, *replacement);
    apply(scene.sceneObjects, owner, raw, *replacement);
    apply(scene.groupObjects, owner, raw, *replacement);
    Note(out, "Classing", owner, "Classing",
         replacement->empty()
             ? "Omitted genuinely ambiguous duplicate-Class relationship; all "
               "definitions retained"
             : "Remapped uniquely identified repaired Class target to " +
                   *replacement,
         raw,
         replacement->empty() ? MvrExportDiagnosticImpact::DataOmitted
                              : MvrExportDiagnosticImpact::IdentityChanged);
    return true;
  });
  for (auto &original : scene.sourceStructures)
    if (original) {
      auto source = std::make_shared<MvrSourceStructure>(*original);
      for (auto &node : source->nodes) {
        tinyxml2::XMLDocument xml;
        if (xml.Parse(node.xml.c_str()) != tinyxml2::XML_SUCCESS ||
            !xml.RootElement())
          continue;
        auto *link = xml.RootElement()->FirstChildElement("Classing");
        if (!link || !link->GetText())
          continue;
        auto replacement = target(link->GetText());
        if (!replacement)
          continue;
        if (replacement->empty())
          xml.RootElement()->DeleteChild(link);
        else
          link->SetText(replacement->c_str());
        node.xml = source_structure::Xml(*xml.RootElement());
      }
      IndexSourceStructure(*source);
      original = std::move(source);
    }
}
} // namespace mvr::publication

/* This file is part of Perastage. */
#include "mvr_import_reference_repair.h"
#include "mvr_source_structure_internal.h"
#include <map>
#include <algorithm>
namespace mvr {
void IsolateRetainedAmbiguousReferences(
    MvrScene &scene, const std::vector<std::string> &rawIds) {
  std::map<std::string, size_t> counts;
  for (const auto &id : rawIds)
    ++counts[id];
  if (std::none_of(counts.begin(), counts.end(),
                   [](const auto &item) { return item.second > 1; }))
    return;
  // Replay the existing occurrence decisions, never another UUID allocator.
  // This temporary wrapper is only a relationship audit, not a scene
  // serializer.
  tinyxml2::XMLDocument audit;
  auto *root = audit.NewElement("GeneralSceneDescription");
  audit.InsertEndChild(root);
  std::unordered_map<tinyxml2::XMLElement *, std::string> occurrences;
  struct Owner {
    MvrSourceNode *node;
    tinyxml2::XMLElement *xml;
  };
  std::vector<Owner> owners;
  std::vector<std::shared_ptr<MvrSourceStructure>> copies;
  size_t ordinal = 0;
  for (const auto &source : scene.sourceStructures)
    if (source) {
      auto copy = std::make_shared<MvrSourceStructure>(*source);
      for (auto &node : copy->nodes) {
        const auto raw = ordinal < rawIds.size() ? rawIds[ordinal] : node.uuid;
        ++ordinal;
        tinyxml2::XMLDocument xml;
        if (xml.Parse(node.xml.c_str()) != tinyxml2::XML_SUCCESS ||
            !xml.RootElement())
          continue;
        auto *part = xml.RootElement()->DeepClone(&audit)->ToElement();
        part->SetAttribute("uuid", raw.c_str());
        root->InsertEndChild(part);
        occurrences[part] = node.uuid;
        owners.push_back({&node, part});
      }
      copies.push_back(std::move(copy));
    }
  const auto issueCount = scene.preservationIssues.size();
  std::vector<std::pair<size_t, tinyxml2::XMLElement *>> fields;
  for (size_t i = 0; i < issueCount; ++i) {
    const auto &issue = scene.preservationIssues[i];
    if (issue.kind != MvrPreservationIssueKind::StandardData)
      continue;
    tinyxml2::XMLDocument xml;
    if (xml.Parse(issue.originalInformation.c_str()) != tinyxml2::XML_SUCCESS ||
        !xml.RootElement())
      continue;
    auto *wrapper = audit.NewElement("Evidence");
    root->InsertEndChild(wrapper);
    occurrences[wrapper] = issue.objectUuid;
    wrapper->InsertEndChild(xml.RootElement()->DeepClone(&audit));
    fields.emplace_back(i, wrapper);
  }
  IsolateAmbiguousImportedReferences(*root, scene, occurrences);
  for (auto &owner : owners) {
    owner.xml->SetAttribute("uuid", owner.node->uuid.c_str());
    owner.node->xml = source_structure::Xml(*owner.xml);
  }
  for (const auto &[i, wrapper] : fields) {
    auto *part = wrapper->FirstChildElement();
    scene.preservationIssues[i].originalInformation =
        part ? source_structure::Xml(*part) : std::string{};
  }
  size_t index = 0;
  for (auto &source : scene.sourceStructures)
    if (source) {
      IndexSourceStructure(*copies[index]);
      source = std::move(copies[index++]);
    }
}
} // namespace mvr

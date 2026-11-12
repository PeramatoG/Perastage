/* This file is part of Perastage. */
#include "mvr_import_reference_repair.h"
#include "mvr_source_structure_internal.h"
#include "uuidutils.h"
#include <map>
#include <set>
namespace mvr {
void IsolateAmbiguousImportedReferences(
    tinyxml2::XMLElement &root, MvrScene &scene,
    const std::unordered_map<tinyxml2::XMLElement *, std::string>
        &occurrences) {
  std::map<std::string, std::set<std::string>> aliases;
  for (const auto &[node, id] : occurrences)
    if (const auto *raw = node->Attribute("uuid"))
      aliases[CanonicalizeUuid(raw).empty() ? std::string(raw)
                                            : CanonicalizeUuid(raw)]
          .insert(id);
  const auto ambiguous = [&](const char *raw) {
    if (!raw || !*raw)
      return false;
    const auto canonical = CanonicalizeUuid(raw);
    auto it = aliases.find(canonical.empty() ? std::string(raw) : canonical);
    return it != aliases.end() && it->second.size() > 1;
  };
  const auto record = [&](const tinyxml2::XMLElement &node,
                          const std::string &owner, const std::string &path) {
    scene.preservationIssues.push_back(
        {node.Name(), owner, path, source_structure::Xml(node),
         "Ambiguous imported reference: the raw UUID identifies multiple "
         "object occurrences.",
         "The affected relationship is omitted; no arbitrary target is "
         "selected."});
  };
  const auto visit = [&](auto &&self, tinyxml2::XMLElement &node,
                         std::string owner, const std::string &path) -> void {
    if (std::string(node.Name()) == "UserData")
      return;
    if (auto it = occurrences.find(&node); it != occurrences.end())
      owner = it->second;
    if (ambiguous(node.Attribute("multipatch"))) {
      record(node, owner, path + "/@multipatch");
      node.DeleteAttribute("multipatch");
    }
    for (auto *child = node.FirstChildElement(); child;) {
      auto *next = child->NextSiblingElement();
      bool remove = false;
      const std::string kind = child->Name();
      if (kind == "Connection")
        remove = ambiguous(child->Attribute("toObject"));
      if (kind == "Focus" && ambiguous(child->GetText())) {
        remove = true;
        if (auto it = scene.fixtures.find(owner); it != scene.fixtures.end())
          it->second.focus.clear();
      }
      if (remove) {
        record(*child, owner, path + "/" + kind);
        node.DeleteChild(child);
      } else
        self(self, *child, owner, path + "/" + kind);
      child = next;
    }
  };
  visit(visit, root, {}, "/GeneralSceneDescription");
}
} // namespace mvr

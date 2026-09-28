#include "inspector_models.h"

#include <algorithm>
#include <functional>
#include <map>
#include <tuple>

namespace gui::inspection {
namespace {

// Returns the final slash-delimited component without normalizing its spelling.
std::string BaseName(const std::string &path) {
  const auto end = path.empty() ? 0 : path.find_last_not_of('/');
  if (end == std::string::npos)
    return path;
  const auto slash = path.rfind('/', end);
  return path.substr(slash == std::string::npos ? 0 : slash + 1,
                     end - (slash == std::string::npos ? 0 : slash + 1) + 1);
}

// Splits only paths already declared safe by Inspection Core.
std::vector<std::string> Components(const std::string &path) {
  std::vector<std::string> result;
  std::size_t begin = 0;
  while (begin < path.size()) {
    const auto end = path.find('/', begin);
    if (end > begin)
      result.push_back(path.substr(begin, end - begin));
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  return result;
}

// Orders folders before entries and then compares stable technical identity.
bool NodeLess(const PackageTreeNode &left, const PackageTreeNode &right) {
  if (left.entryType != right.entryType)
    return left.entryType == perastage::inspection::PackageEntryType::Directory;
  return std::tie(left.name, left.archivePath, left.syntheticFolder) <
         std::tie(right.name, right.archivePath, right.syntheticFolder);
}

// Sorts every package subtree deterministically.
void SortPackageTree(std::vector<PackageTreeNode> &nodes) {
  std::stable_sort(nodes.begin(), nodes.end(), NodeLess);
  for (auto &node : nodes)
    SortPackageTree(node.children);
}

// Converts a Core descriptor into one actual package node.
PackageTreeNode ActualNode(
    const perastage::inspection::ResourceDescriptor &resource) {
  return {BaseName(resource.displayPath), resource.displayPath,
          resource.entryType, resource.kind, resource.size,
          resource.sizeKnown, resource.pathSafe, false, {}};
}

// Returns all scene descriptor collections in stable presentation order.
std::vector<const std::vector<perastage::inspection::MvrSceneNodeDescriptor> *>
Collections(const perastage::inspection::MvrInspectionSnapshot &snapshot) {
  return {&snapshot.layers,       &snapshot.groupObjects, &snapshot.fixtures,
          &snapshot.trusses,     &snapshot.supports,     &snapshot.sceneObjects,
          &snapshot.focusPoints, &snapshot.videoScreens, &snapshot.projectors,
          &snapshot.positions};
}

} // namespace

// Reports whether the unchanged archive path can be copied.
bool PackageTreeNode::CanCopyArchivePath() const {
  return !syntheticFolder && !archivePath.empty();
}

// Builds a safe hierarchy solely from Inspection Core resource descriptors.
std::vector<PackageTreeNode> BuildPackageTree(
    const std::vector<perastage::inspection::ResourceDescriptor> &resources) {
  std::vector<PackageTreeNode> roots;
  for (const auto &resource : resources) {
    if (!resource.pathSafe) {
      roots.push_back(ActualNode(resource));
      continue;
    }
    const auto components = Components(resource.displayPath);
    if (components.empty()) {
      roots.push_back(ActualNode(resource));
      continue;
    }
    auto *siblings = &roots;
    std::string prefix;
    for (std::size_t index = 0; index + 1 < components.size(); ++index) {
      if (!prefix.empty())
        prefix += '/';
      prefix += components[index];
      auto folder = std::find_if(siblings->begin(), siblings->end(),
                                 [&](const PackageTreeNode &candidate) {
                                   return candidate.name == components[index] &&
                                          candidate.entryType == perastage::inspection::PackageEntryType::Directory;
                                 });
      if (folder == siblings->end()) {
        siblings->push_back({components[index], prefix,
                             perastage::inspection::PackageEntryType::Directory,
                             perastage::inspection::ResourceKind::Binary, 0,
                             false, true, true, {}});
        folder = std::prev(siblings->end());
      }
      siblings = &folder->children;
    }
    auto node = ActualNode(resource);
    if (resource.entryType == perastage::inspection::PackageEntryType::Directory) {
      auto existing = std::find_if(siblings->begin(), siblings->end(),
                                   [&](const PackageTreeNode &candidate) {
                                     return candidate.name == components.back() &&
                                            candidate.entryType == resource.entryType;
                                   });
      if (existing != siblings->end()) {
        const auto children = std::move(existing->children);
        *existing = std::move(node);
        existing->children = children;
        continue;
      }
    }
    siblings->push_back(std::move(node));
  }
  SortPackageTree(roots);
  return roots;
}

// Projects all neutral scene descriptors without consulting the live scene.
std::vector<SceneTreeNode>
BuildSceneTree(const perastage::inspection::MvrInspectionSnapshot &snapshot) {
  std::map<std::string, SceneTreeNode> nodes;
  std::map<std::string, std::string> parents;
  for (const auto *collection : Collections(snapshot)) {
    for (const auto &descriptor : *collection) {
      nodes.try_emplace(descriptor.uuid,
                        SceneTreeNode{descriptor.kind, descriptor.uuid,
                                      descriptor.name, false, {}});
      parents[descriptor.uuid] = !descriptor.parentGroupUuid.empty()
                                     ? descriptor.parentGroupUuid
                                     : descriptor.layerUuid;
    }
  }
  std::vector<SceneTreeNode> roots;
  for (const auto &[uuid, node] : nodes) {
    const auto parent = parents.find(uuid);
    if (parent == parents.end() || parent->second.empty()) {
      roots.push_back(node);
      continue;
    }
    const auto found = nodes.find(parent->second);
    if (found == nodes.end()) {
      auto orphan = node;
      orphan.unresolved = true;
      roots.push_back(std::move(orphan));
    }
  }
  std::function<SceneTreeNode(const std::string &, std::vector<std::string>)> build =
      [&](const std::string &uuid, std::vector<std::string> ancestry) {
        auto result = nodes.at(uuid);
        ancestry.push_back(uuid);
        for (const auto &[childUuid, parentUuid] : parents) {
          if (parentUuid == uuid &&
              std::find(ancestry.begin(), ancestry.end(), childUuid) == ancestry.end())
            result.children.push_back(build(childUuid, ancestry));
        }
        return result;
      };
  std::vector<std::string> emitted;
  std::function<void(const SceneTreeNode &)> remember = [&](const SceneTreeNode &node) {
    if (!node.uuid.empty())
      emitted.push_back(node.uuid);
    for (const auto &child : node.children)
      remember(child);
  };
  for (auto &root : roots) {
    const bool unresolved = root.unresolved;
    root = build(root.uuid, {});
    root.unresolved = unresolved;
    remember(root);
  }
  for (const auto &[uuid, node] : nodes) {
    if (std::find(emitted.begin(), emitted.end(), uuid) == emitted.end()) {
      auto unresolved = build(uuid, {});
      unresolved.unresolved = true;
      roots.push_back(std::move(unresolved));
    }
  }
  for (const auto &symdef : snapshot.symdefs)
    roots.push_back({"SymDef", symdef.uuid, {}, false, {}});
  for (const auto &foreign : snapshot.foreignUserData)
    roots.push_back({"ForeignData", {}, foreign.provider, false, {}});
  std::stable_sort(roots.begin(), roots.end(), [](const auto &left, const auto &right) {
    return std::tie(left.kind, left.name, left.uuid) <
           std::tie(right.kind, right.name, right.uuid);
  });
  return roots;
}

// Groups diagnostics by stable structured severity, classification, and code.
std::vector<IssueGroup> BuildIssueGroups(
    const std::vector<perastage::inspection::Diagnostic> &diagnostics) {
  std::map<std::tuple<perastage::inspection::DiagnosticSeverity,
                      perastage::inspection::DiagnosticClassification,
                      std::string>, std::size_t> counts;
  for (const auto &diagnostic : diagnostics)
    ++counts[{diagnostic.severity, diagnostic.classification, diagnostic.code}];
  std::vector<IssueGroup> result;
  for (const auto &[key, count] : counts)
    result.push_back({std::get<0>(key), std::get<1>(key), std::get<2>(key), count});
  return result;
}

// Finds a literal query from a character selection with optional wrap-around.
std::optional<std::size_t> FindText(const std::string &text,
                                    const std::string &query,
                                    std::size_t selectionStart,
                                    std::size_t selectionEnd, bool forward) {
  if (query.empty())
    return std::nullopt;
  std::size_t found = std::string::npos;
  if (forward) {
    found = text.find(query, std::min(selectionEnd, text.size()));
    if (found == std::string::npos)
      found = text.find(query);
  } else {
    const auto start = selectionStart == 0 ? std::string::npos : selectionStart - 1;
    found = text.rfind(query, start);
    if (found == std::string::npos)
      found = text.rfind(query);
  }
  return found == std::string::npos ? std::nullopt
                                    : std::optional<std::size_t>(found);
}

} // namespace gui::inspection

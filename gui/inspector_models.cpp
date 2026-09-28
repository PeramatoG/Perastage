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
  struct NodeRecord {
    const perastage::inspection::MvrSceneNodeDescriptor *descriptor = nullptr;
    std::optional<std::size_t> parent;
    bool unresolved = false;
  };

  std::vector<NodeRecord> records;
  for (const auto *collection : Collections(snapshot)) {
    for (const auto &descriptor : *collection)
      records.push_back({&descriptor, std::nullopt, false});
  }

  std::map<std::string, std::vector<std::size_t>> uuidIndices;
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (!records[index].descriptor->uuid.empty())
      uuidIndices[records[index].descriptor->uuid].push_back(index);
  }

  const auto uniqueIndex = [&uuidIndices](const std::string &uuid)
      -> std::optional<std::size_t> {
    const auto found = uuidIndices.find(uuid);
    if (found == uuidIndices.end() || found->second.size() != 1)
      return std::nullopt;
    return found->second.front();
  };

  // Authored ChildList edges take precedence over reduced parent/layer facts.
  for (std::size_t parent = 0; parent < records.size(); ++parent) {
    for (const auto &childUuid : records[parent].descriptor->childUuids) {
      const auto child = uniqueIndex(childUuid);
      if (child && *child != parent && !records[*child].parent)
        records[*child].parent = parent;
    }
  }

  // Parent-group and layer references fill only topology absent from ChildList.
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (records[index].parent)
      continue;
    const auto &descriptor = *records[index].descriptor;
    const std::string &fallback = !descriptor.parentGroupUuid.empty()
                                      ? descriptor.parentGroupUuid
                                      : descriptor.layerUuid;
    if (fallback.empty())
      continue;
    const auto parent = uniqueIndex(fallback);
    if (parent && *parent != index)
      records[index].parent = parent;
    else
      records[index].unresolved = true;
  }

  // Break malformed cycles at their deterministic lowest-index member.
  for (std::size_t start = 0; start < records.size(); ++start) {
    std::vector<std::size_t> path;
    std::optional<std::size_t> current = start;
    while (current) {
      const auto repeated = std::find(path.begin(), path.end(), *current);
      if (repeated != path.end()) {
        const auto breakAt = *std::min_element(repeated, path.end());
        records[breakAt].parent.reset();
        records[breakAt].unresolved = true;
        break;
      }
      path.push_back(*current);
      current = records[*current].parent;
    }
  }

  std::vector<std::vector<std::size_t>> children(records.size());
  std::vector<std::size_t> roots;
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (records[index].parent)
      children[*records[index].parent].push_back(index);
    else
      roots.push_back(index);
  }
  const auto indexLess = [&records](std::size_t left, std::size_t right) {
    const auto &lhs = *records[left].descriptor;
    const auto &rhs = *records[right].descriptor;
    return std::tie(lhs.kind, lhs.name, lhs.uuid, left) <
           std::tie(rhs.kind, rhs.name, rhs.uuid, right);
  };
  std::stable_sort(roots.begin(), roots.end(), indexLess);
  for (auto &siblings : children)
    std::stable_sort(siblings.begin(), siblings.end(), indexLess);

  std::function<SceneTreeNode(std::size_t)> build = [&](std::size_t index) {
    const auto &descriptor = *records[index].descriptor;
    SceneTreeNode result{descriptor.kind, descriptor.uuid, descriptor.name,
                         records[index].unresolved, {}};
    for (const auto child : children[index])
      result.children.push_back(build(child));
    return result;
  };

  std::vector<SceneTreeNode> result;
  for (const auto root : roots)
    result.push_back(build(root));
  for (const auto &symdef : snapshot.symdefs)
    result.push_back({"SymDef", symdef.uuid, {}, false, {}});
  for (const auto &foreign : snapshot.foreignUserData)
    result.push_back({"ForeignData", {}, foreign.provider, false, {}});
  std::stable_sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
    return std::tie(left.kind, left.name, left.uuid) <
           std::tie(right.kind, right.name, right.uuid);
  });
  return result;
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
  std::stable_sort(result.begin(), result.end(), [](const auto &left,
                                                    const auto &right) {
    return std::tie(left.severity, left.classification, left.code) >
           std::tie(right.severity, right.classification, right.code);
  });
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

#include "query/scene_query.h"

#include "mvrscene.h"

#include <algorithm>
#include <charconv>
#include <map>
#include <set>

namespace perastage::query {
namespace {

// Orders typed references by their stable kind token and UUID.
bool ReferenceLess(const scene_identity::ObjectReference &left,
                   const scene_identity::ObjectReference &right) {
  const std::string leftKind = scene_identity::KindToken(left.kind);
  const std::string rightKind = scene_identity::KindToken(right.kind);
  return leftKind != rightKind ? leftKind < rightKind : left.uuid < right.uuid;
}

// Maps an MVR node category to the shared scene identity category.
scene_identity::ObjectKind ToObjectKind(MvrNodeType type) {
  switch (type) {
  case MvrNodeType::Fixture:
    return scene_identity::ObjectKind::Fixture;
  case MvrNodeType::Truss:
    return scene_identity::ObjectKind::Truss;
  case MvrNodeType::Support:
    return scene_identity::ObjectKind::Support;
  case MvrNodeType::SceneObject:
    return scene_identity::ObjectKind::SceneObject;
  case MvrNodeType::GroupObject:
    return scene_identity::ObjectKind::Group;
  }
  return scene_identity::ObjectKind::SceneObject;
}

// Resolves a UUID to a typed public scene reference.
std::optional<scene_identity::ObjectReference>
FindReference(const MvrScene &scene, const std::string &uuid) {
  if (scene.fixtures.contains(uuid))
    return {{scene_identity::ObjectKind::Fixture, uuid}};
  if (scene.trusses.contains(uuid))
    return {{scene_identity::ObjectKind::Truss, uuid}};
  if (scene.supports.contains(uuid))
    return {{scene_identity::ObjectKind::Support, uuid}};
  if (scene.sceneObjects.contains(uuid))
    return {{scene_identity::ObjectKind::SceneObject, uuid}};
  if (scene.groupObjects.contains(uuid))
    return {{scene_identity::ObjectKind::Group, uuid}};
  return std::nullopt;
}

// Parses a complete positive universe.channel address.
std::optional<std::pair<int, int>> ParseAddress(const std::string &text) {
  const std::size_t dot = text.find('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 == text.size() ||
      text.find('.', dot + 1) != std::string::npos)
    return std::nullopt;
  int universe = 0;
  int channel = 0;
  const auto parse = [](std::string_view value, int &result) {
    const auto converted =
        std::from_chars(value.data(), value.data() + value.size(), result);
    return converted.ec == std::errc{} &&
           converted.ptr == value.data() + value.size();
  };
  if (!parse(std::string_view(text).substr(0, dot), universe) ||
      !parse(std::string_view(text).substr(dot + 1), channel) || universe < 1 ||
      channel < 1 || channel > 512)
    return std::nullopt;
  return {{universe, channel}};
}

// Appends all UUIDs in one selection bucket as typed references.
void AppendSelection(std::vector<scene_identity::ObjectReference> &result,
                     scene_identity::ObjectKind kind,
                     const std::vector<std::string> &uuids) {
  for (const std::string &uuid : uuids)
    result.push_back({kind, uuid});
}

} // namespace

// Returns stable aggregate information about the active scene.
Summary GetSummary(const MvrScene &scene) {
  return {scene.versionMajor,        scene.versionMinor,
          scene.fixtures.size(),     scene.trusses.size(),
          scene.supports.size(),     scene.sceneObjects.size(),
          scene.groupObjects.size(), scene.layers.size()};
}

// Returns the semantic selection as sorted typed references.
std::vector<scene_identity::ObjectReference>
GetSelection(const scene_grouping::ObjectSelection &selection) {
  std::vector<scene_identity::ObjectReference> result;
  AppendSelection(result, scene_identity::ObjectKind::Fixture,
                  selection.fixtures);
  AppendSelection(result, scene_identity::ObjectKind::Truss, selection.trusses);
  AppendSelection(result, scene_identity::ObjectKind::Support,
                  selection.supports);
  AppendSelection(result, scene_identity::ObjectKind::SceneObject,
                  selection.sceneObjects);
  std::sort(result.begin(), result.end(), ReferenceLess);
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

// Returns all public scene descriptors ordered by kind token and UUID.
std::vector<ObjectDescriptor> ListObjects(const MvrScene &scene) {
  std::vector<ObjectDescriptor> result;
  for (const auto &[uuid, value] : scene.fixtures)
    result.push_back({scene_identity::ObjectKind::Fixture, uuid,
                      value.instanceName, value.layer, value.parentGroupUuid,
                      value.typeName, value.gdtfSpec});
  for (const auto &[uuid, value] : scene.trusses)
    result.push_back({scene_identity::ObjectKind::Truss, uuid, value.name,
                      value.layer, value.parentGroupUuid, value.model,
                      value.gdtfSpec});
  for (const auto &[uuid, value] : scene.supports)
    result.push_back({scene_identity::ObjectKind::Support, uuid, value.name,
                      value.layer, value.parentGroupUuid, value.hoistFunction,
                      value.gdtfSpec});
  for (const auto &[uuid, value] : scene.sceneObjects)
    result.push_back({scene_identity::ObjectKind::SceneObject,
                      uuid,
                      value.name,
                      value.layer,
                      value.parentGroupUuid,
                      {},
                      value.GetPrimaryModel()});
  for (const auto &[uuid, value] : scene.groupObjects)
    result.push_back({scene_identity::ObjectKind::Group,
                      uuid,
                      value.name,
                      value.layer,
                      value.parentGroupUuid,
                      {},
                      {}});
  std::sort(
      result.begin(), result.end(), [](const auto &left, const auto &right) {
        return ReferenceLess({left.kind, left.uuid}, {right.kind, right.uuid});
      });
  return result;
}

// Finds one object only when both its kind and UUID match.
std::optional<ObjectDescriptor>
GetObject(const MvrScene &scene, const scene_identity::ObjectReference &object,
          std::vector<Diagnostic> &diagnostics) {
  const auto objects = ListObjects(scene);
  const auto found =
      std::find_if(objects.begin(), objects.end(), [&](const auto &candidate) {
        return candidate.kind == object.kind && candidate.uuid == object.uuid;
      });
  if (found != objects.end())
    return *found;
  diagnostics.push_back({"scene.object.not_found",
                         "No scene object matches the requested kind and UUID.",
                         object});
  return std::nullopt;
}

// Returns layers and typed membership ordered deterministically.
std::vector<LayerDescriptor> ListLayers(const MvrScene &scene) {
  std::vector<LayerDescriptor> result;
  for (const auto &[uuid, layer] : scene.layers) {
    LayerDescriptor descriptor{uuid, layer.name, layer.color, {}};
    for (const std::string &child : layer.childUUIDs)
      if (const auto reference = FindReference(scene, child))
        descriptor.members.push_back(*reference);
    std::sort(descriptor.members.begin(), descriptor.members.end(),
              ReferenceLess);
    descriptor.members.erase(
        std::unique(descriptor.members.begin(), descriptor.members.end()),
        descriptor.members.end());
    result.push_back(std::move(descriptor));
  }
  std::sort(result.begin(), result.end(),
            [](const auto &a, const auto &b) { return a.uuid < b.uuid; });
  return result;
}

// Returns group hierarchy and typed children ordered deterministically.
std::vector<GroupDescriptor> ListGroups(const MvrScene &scene) {
  std::vector<GroupDescriptor> result;
  for (const auto &[uuid, group] : scene.groupObjects) {
    GroupDescriptor descriptor{
        uuid, group.name, group.layer, group.parentGroupUuid, {}};
    for (const GroupObjectChildRef &child : group.children)
      descriptor.children.push_back({ToObjectKind(child.type), child.uuid});
    std::sort(descriptor.children.begin(), descriptor.children.end(),
              ReferenceLess);
    result.push_back(std::move(descriptor));
  }
  std::sort(result.begin(), result.end(),
            [](const auto &a, const auto &b) { return a.uuid < b.uuid; });
  return result;
}

// Analyzes fixture patch ranges using a caller-owned footprint source.
PatchStatus GetPatchStatus(const MvrScene &scene,
                           const FootprintResolver &resolveFootprint) {
  PatchStatus result;
  std::vector<const Fixture *> fixtures;
  for (const auto &[uuid, fixture] : scene.fixtures)
    fixtures.push_back(&fixture);
  std::sort(fixtures.begin(), fixtures.end(),
            [](const auto *a, const auto *b) { return a->uuid < b->uuid; });
  for (const Fixture *fixture : fixtures) {
    PatchFixture status;
    status.uuid = fixture->uuid;
    status.rawAddress = fixture->address;
    const auto address = ParseAddress(fixture->address);
    if (!address) {
      result.diagnostics.push_back(
          {"scene.patch.invalid_address",
           "The fixture patch address is not a valid universe.channel value.",
           scene_identity::ObjectReference{scene_identity::ObjectKind::Fixture,
                                           fixture->uuid}});
    } else {
      status.universe = address->first;
      status.startChannel = address->second;
    }
    status.footprint =
        resolveFootprint ? resolveFootprint(*fixture) : std::nullopt;
    if (!status.footprint || *status.footprint < 1) {
      status.footprint.reset();
      result.diagnostics.push_back(
          {"scene.patch.unresolved_footprint",
           "The fixture DMX footprint could not be resolved.",
           scene_identity::ObjectReference{scene_identity::ObjectKind::Fixture,
                                           fixture->uuid}});
    } else if (status.startChannel) {
      const int end = *status.startChannel + *status.footprint - 1;
      if (end > 512) {
        result.diagnostics.push_back(
            {"scene.patch.range_exceeds_universe",
             "The fixture patch range exceeds its DMX universe.",
             scene_identity::ObjectReference{
                 scene_identity::ObjectKind::Fixture, fixture->uuid}});
      } else
        status.endChannel = end;
    }
    result.fixtures.push_back(std::move(status));
  }
  for (std::size_t i = 0; i < result.fixtures.size(); ++i) {
    const auto &first = result.fixtures[i];
    if (!first.universe || !first.startChannel || !first.endChannel)
      continue;
    for (std::size_t j = i + 1; j < result.fixtures.size(); ++j) {
      const auto &second = result.fixtures[j];
      if (first.universe != second.universe || !second.startChannel ||
          !second.endChannel)
        continue;
      const int begin = std::max(*first.startChannel, *second.startChannel);
      const int end = std::min(*first.endChannel, *second.endChannel);
      if (begin <= end)
        result.conflicts.push_back(
            {*first.universe, begin, end, {first.uuid, second.uuid}});
    }
  }
  return result;
}

} // namespace perastage::query

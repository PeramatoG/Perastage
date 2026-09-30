#include "capability/capability_catalog.h"
#include "capability/capability_json_serializer.h"
#include "command/command_scene_tools.h"
#include "command/command_selection.h"
#include "command/command_transform.h"
#include "json.hpp"

#include <algorithm>
#include <cassert>
#include <set>

using namespace perastage;

namespace {

// Verifies that one real request conforms to the catalog's structural contract.
void Check(const command::Request &request) {
  assert(capability::ValidateRequestShape(request).empty());
}

} // namespace

// Verifies inventory, request parity, classifications, exposure, and JSON
// stability.
int main() {
  const auto catalog = capability::Catalog();
  assert(catalog.size() == 15);
  assert(std::is_sorted(catalog.begin(), catalog.end(),
                        [](const auto &left, const auto &right) {
                          return left.operationId < right.operationId;
                        }));
  std::set<std::string_view> ids;
  for (const auto &descriptor : catalog)
    assert(ids.insert(descriptor.operationId).second);
  assert(capability::Find("scene.summary") != nullptr);
  assert(capability::Find("scene.future") == nullptr);

  command::transform::Command position{command::transform::Kind::Position,
                                       {{0, {10.0}, false, false}}};
  command::transform::Command rotation{command::transform::Kind::Rotation,
                                       {{2, {15.0}, true, true}},
                                       std::array<double, 3>{1, 2, 3}};
  Check(command::transform::BuildRequest(position));
  Check(command::transform::BuildRequest(rotation));
  command::selection::Command selection;
  Check(command::selection::BuildRequest(selection));
  Check({command::selection::kClearCommandId, {}});
  command::scene_tools::GroupCommand group;
  Check(command::scene_tools::BuildGroupRequest(group, false));
  Check(command::scene_tools::BuildGroupRequest(group, true));
  Check(command::scene_tools::BuildFixtureToSupportRequest({{"fixture"}}));
  Check(command::scene_tools::BuildSceneObjectsToTrussesRequest({"object"}));

  for (const auto &descriptor : catalog) {
    if (descriptor.kind == capability::OperationKind::Query)
      assert(descriptor.effect == capability::Effect::ReadOnly);
    for (const auto &frontend : descriptor.frontends)
      assert(frontend.frontendId == "embedded_console" ||
             frontend.frontendId == "desktop_gui");
  }
  assert(capability::Find(command::selection::kUpdateCommandId)
             ->frontends.front()
             .state == capability::ExposureState::Partial);
  assert(capability::Find(command::scene_tools::kFixtureToSupportCommandId)
             ->effect == capability::Effect::Destructive);
  assert(capability::Find(command::transform::kPositionCommandId)->effect ==
         capability::Effect::Mutating);

  const std::string first = capability::SerializeCatalogJson();
  assert(first == capability::SerializeCatalogJson());
  const auto json = nlohmann::json::parse(first);
  assert(json.at("schema_version") == 1);
  assert(json.at("operations").size() == catalog.size());
  assert(json.at("operations").front().at("operation_id") ==
         catalog.front().operationId);
  return 0;
}

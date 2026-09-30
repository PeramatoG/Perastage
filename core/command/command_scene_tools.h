#pragma once

#include "command/command_execution.h"

#include <string>
#include <vector>

namespace perastage::command::scene_tools {

inline constexpr const char *kGroupCreateCommandId = "scene.group.create";
inline constexpr const char *kGroupUngroupCommandId = "scene.group.ungroup";
inline constexpr const char *kFixtureToSupportCommandId =
    "scene.convert.fixture_to_support";
inline constexpr const char *kSceneObjectsToTrussesCommandId =
    "scene.convert.scene_objects_to_trusses";

struct GroupCommand {
  scene_grouping::ObjectSelection objects;
};

struct FixtureToSupportCommand {
  std::vector<std::string> fixtureUuids;
};

struct SceneObjectsToTrussesCommand {
  std::string sourceSceneObjectUuid;
};

Request BuildGroupRequest(const GroupCommand &command, bool ungroup);
Request BuildFixtureToSupportRequest(const FixtureToSupportCommand &command);
Request
BuildSceneObjectsToTrussesRequest(const SceneObjectsToTrussesCommand &command);

Result ExecuteGroup(const GroupCommand &command, ExecutionContext &context);
Result ExecuteUngroup(const GroupCommand &command, ExecutionContext &context);
Result ExecuteFixtureToSupport(const FixtureToSupportCommand &command,
                               ExecutionContext &context);
Result ExecuteSceneObjectsToTrusses(const SceneObjectsToTrussesCommand &command,
                                    ExecutionContext &context);

} // namespace perastage::command::scene_tools

#pragma once

#include "command/command_execution.h"
#include "command/command_operation_ids.h"

#include <string>
#include <vector>

namespace perastage::command::scene_tools {

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

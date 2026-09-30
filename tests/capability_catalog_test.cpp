#include "capability/capability_catalog.h"
#include "capability/capability_json_serializer.h"
#include "command/command_scene_tools.h"
#include "command/command_selection.h"
#include "command/command_transform.h"
#include "json.hpp"
#include "query/query_contract.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <set>

using namespace perastage;

namespace {

// Returns a required descriptor and fails the test when lookup is incomplete.
const capability::Descriptor &Require(std::string_view operationId) {
  const auto *descriptor = capability::Find(operationId);
  assert(descriptor != nullptr);
  return *descriptor;
}

// Verifies that one real request conforms to the catalog's structural contract.
void CheckRequest(const command::Request &request) {
  assert(capability::ValidateRequestShape(request).empty());
}

// Returns a required argument descriptor by its stable ID.
const capability::ArgumentDescriptor &
RequireArgument(const capability::Descriptor &descriptor, std::string_view id) {
  const auto found =
      std::find_if(descriptor.arguments.begin(), descriptor.arguments.end(),
                   [id](const auto &argument) { return argument.id == id; });
  assert(found != descriptor.arguments.end());
  return *found;
}

// Verifies an argument's exact machine type and required state.
void CheckArgument(const capability::Descriptor &descriptor,
                   std::string_view id, capability::ArgumentType type,
                   bool required) {
  const auto &argument = RequireArgument(descriptor, id);
  assert(argument.type == type);
  assert(argument.required == required);
}

// Verifies that validation produced exactly one expected issue.
void CheckSingleIssue(const command::Request &request,
                      capability::RequestShapeIssueKind kind,
                      std::string_view argumentId = {}) {
  const auto issues = capability::ValidateRequestShape(request);
  assert(issues.size() == 1);
  assert(issues.front().kind == kind);
  assert(issues.front().argumentId == argumentId);
}

// Verifies one operation's exact frontend exposure.
void CheckExposure(std::string_view operationId, std::string_view frontendId,
                   capability::ExposureState state) {
  const auto &frontends = Require(operationId).frontends;
  assert(frontends.size() == 1);
  assert(frontends.front().frontendId == frontendId);
  assert(frontends.front().state == state);
}

// Verifies the exact current Command and Query inventory.
void CheckInventory() {
  const std::set<std::string_view> expected = {
      command::transform::kPositionCommandId,
      command::transform::kRotationCommandId,
      command::selection::kUpdateCommandId,
      command::selection::kClearCommandId,
      command::scene_tools::kGroupCreateCommandId,
      command::scene_tools::kGroupUngroupCommandId,
      command::scene_tools::kFixtureToSupportCommandId,
      command::scene_tools::kSceneObjectsToTrussesCommandId,
      query::kSummaryQueryId,
      query::kSelectionQueryId,
      query::kObjectsQueryId,
      query::kObjectQueryId,
      query::kLayersQueryId,
      query::kGroupsQueryId,
      query::kPatchQueryId,
  };
  const auto catalog = capability::Catalog();
  assert(std::is_sorted(catalog.begin(), catalog.end(),
                        [](const auto &left, const auto &right) {
                          return left.operationId < right.operationId;
                        }));
  std::set<std::string_view> actual;
  for (const auto &descriptor : catalog)
    assert(actual.insert(descriptor.operationId).second);
  assert(actual == expected);
  for (std::string_view operationId : expected)
    assert(capability::Find(operationId) != nullptr);
  assert(capability::Find("scene.future") == nullptr);
}

// Verifies the explicit mapping for every Command argument value alternative.
void CheckArgumentValueTypes() {
  using capability::ArgumentType;
  assert(capability::TypeOfArgumentValue(false) == ArgumentType::Boolean);
  assert(capability::TypeOfArgumentValue(std::int64_t{1}) ==
         ArgumentType::Int64);
  assert(capability::TypeOfArgumentValue(1.0) == ArgumentType::Float64);
  assert(capability::TypeOfArgumentValue(std::string{"value"}) ==
         ArgumentType::String);
  assert(capability::TypeOfArgumentValue(std::vector<std::int64_t>{1}) ==
         ArgumentType::Int64List);
  assert(capability::TypeOfArgumentValue(std::vector<double>{1.0}) ==
         ArgumentType::Float64List);
  assert(capability::TypeOfArgumentValue(std::vector<std::string>{"value"}) ==
         ArgumentType::StringList);
}

// Verifies complete transform and scene-tool request metadata parity.
void CheckCommandParity() {
  using capability::ArgumentType;
  command::transform::Command position{command::transform::Kind::Position,
      {{0, {10.0}, false, false},
       {1, {20.0}, true, false},
       {2, {30.0, 40.0}, false, true}},
      std::nullopt};
  command::transform::Command rotation{command::transform::Kind::Rotation,
                                       {{0, {10.0}, false, false},
                                        {1, {20.0}, true, false},
                                        {2, {30.0}, false, true}},
                                       std::array<double, 3>{1, 2, 3}};
  CheckRequest(command::transform::BuildRequest(position));
  CheckRequest(command::transform::BuildRequest(rotation));

  const auto &positionDescriptor =
      Require(command::transform::kPositionCommandId);
  const auto &rotationDescriptor =
      Require(command::transform::kRotationCommandId);
  for (std::string_view axis : {"x", "y", "z"}) {
    const std::string prefix(axis);
    CheckArgument(positionDescriptor, prefix + "_millimeters",
                  ArgumentType::Float64List, false);
    CheckArgument(rotationDescriptor, prefix + "_degrees",
                  ArgumentType::Float64List, false);
    for (std::string_view suffix : {"_relative", "_group"}) {
      CheckArgument(positionDescriptor, prefix + std::string(suffix),
                    ArgumentType::Boolean, false);
      CheckArgument(rotationDescriptor, prefix + std::string(suffix),
                    ArgumentType::Boolean, false);
    }
    CheckArgument(positionDescriptor, prefix + "_space", ArgumentType::String,
                  false);
    CheckArgument(rotationDescriptor, prefix + "_space", ArgumentType::String,
                  false);
  }
  assert(positionDescriptor.arguments.size() == 12);
  assert(rotationDescriptor.arguments.size() == 13);
  CheckArgument(rotationDescriptor, "pivot_mm", ArgumentType::Float64List,
                false);

  command::selection::Command selection;
  CheckRequest(command::selection::BuildRequest(selection));
  CheckRequest({command::selection::kClearCommandId, {}});
  command::scene_tools::GroupCommand group;
  CheckRequest(command::scene_tools::BuildGroupRequest(group, false));
  CheckRequest(command::scene_tools::BuildGroupRequest(group, true));
  CheckRequest(
      command::scene_tools::BuildFixtureToSupportRequest({{"fixture"}}));
  CheckRequest(
      command::scene_tools::BuildSceneObjectsToTrussesRequest({"object"}));
}

// Verifies every generic request-shape failure classification.
void CheckRequestShapeFailures() {
  using capability::RequestShapeIssueKind;
  CheckSingleIssue({"scene.unknown", {}},
                   RequestShapeIssueKind::UnknownOperation);
  CheckSingleIssue({query::kSummaryQueryId, {}},
                   RequestShapeIssueKind::UnknownOperation);
  CheckSingleIssue({command::selection::kClearCommandId,
                    {{"unexpected", std::string{"value"}}}},
                   RequestShapeIssueKind::UnknownArgument, "unexpected");
  CheckSingleIssue({command::transform::kPositionCommandId,
                    {{"x_relative", false}, {"x_relative", true}}},
                   RequestShapeIssueKind::DuplicateArgument, "x_relative");
  CheckSingleIssue({command::transform::kPositionCommandId,
                    {{"x_relative", std::string{"false"}}}},
                   RequestShapeIssueKind::WrongType, "x_relative");
  CheckSingleIssue({command::scene_tools::kFixtureToSupportCommandId, {}},
                   RequestShapeIssueKind::MissingArgument, "fixture_uuids");
  const auto repeatedUnknown = capability::ValidateRequestShape(
      {command::selection::kClearCommandId,
       {{"unexpected", false}, {"unexpected", true}}});
  assert(repeatedUnknown.size() == 2);
  assert(repeatedUnknown[0].kind == RequestShapeIssueKind::UnknownArgument);
  assert(repeatedUnknown[1].kind == RequestShapeIssueKind::DuplicateArgument);
}

// Verifies all Query descriptors and their caller-facing arguments.
void CheckQueries() {
  const std::array<std::string_view, 7> queryIds = {
      query::kSummaryQueryId, query::kSelectionQueryId, query::kObjectsQueryId,
      query::kObjectQueryId,  query::kLayersQueryId,    query::kGroupsQueryId,
      query::kPatchQueryId};
  for (std::string_view id : queryIds) {
    const auto &descriptor = Require(id);
    assert(descriptor.kind == capability::OperationKind::Query);
    assert(descriptor.effect == capability::Effect::ReadOnly);
    assert(descriptor.frontends.empty());
  }
  for (std::string_view id : {query::kSummaryQueryId, query::kSelectionQueryId,
                              query::kObjectsQueryId, query::kLayersQueryId,
                              query::kGroupsQueryId, query::kPatchQueryId})
    assert(Require(id).arguments.empty());
  const auto &object = Require(query::kObjectQueryId);
  assert(object.arguments.size() == 2);
  CheckArgument(object, "object_kind", capability::ArgumentType::String, true);
  CheckArgument(object, "object_uuid", capability::ArgumentType::String, true);
}

// Verifies the exact current operation-to-frontend exposure mapping.
void CheckFrontendExposure() {
  CheckExposure(command::transform::kPositionCommandId, "embedded_console",
                capability::ExposureState::Full);
  CheckExposure(command::transform::kRotationCommandId, "embedded_console",
                capability::ExposureState::Full);
  CheckExposure(command::selection::kClearCommandId, "embedded_console",
                capability::ExposureState::Full);
  CheckExposure(command::selection::kUpdateCommandId, "embedded_console",
                capability::ExposureState::Partial);
  for (std::string_view id :
       {command::scene_tools::kGroupCreateCommandId,
        command::scene_tools::kGroupUngroupCommandId,
        command::scene_tools::kFixtureToSupportCommandId,
        command::scene_tools::kSceneObjectsToTrussesCommandId})
    CheckExposure(id, "desktop_gui", capability::ExposureState::Full);
  for (const auto &descriptor : capability::Catalog())
    for (const auto &frontend : descriptor.frontends) {
      assert(frontend.frontendId != "development_cli");
      assert(frontend.frontendId != "ipc");
      assert(frontend.frontendId != "osc");
      assert(frontend.frontendId != "mcp");
      assert(frontend.frontendId != "remote");
      assert(frontend.frontendId != "ai");
      assert(frontend.frontendId != "voice");
    }
}

// Verifies deterministic JSON ordering and representative schema structure.
void CheckSerialization() {
  const std::string first = capability::SerializeCatalogJson();
  assert(first == capability::SerializeCatalogJson());
  const auto json = nlohmann::json::parse(first);
  assert(json.at("schema_version") == 1);
  const auto &operations = json.at("operations");
  assert(operations.size() == capability::Catalog().size());
  for (std::size_t index = 0; index < operations.size(); ++index)
    assert(operations[index].at("operation_id") ==
           capability::Catalog()[index].operationId);
  const auto &operation = operations.front();
  for (const char *field : {"operation_id", "kind", "effect", "summary",
                            "arguments", "frontend_exposure"})
    assert(operation.contains(field));
  const auto &argument = operation.at("arguments").front();
  for (const char *field : {"id", "type", "required", "description"})
    assert(argument.contains(field));
  const auto &frontend = operation.at("frontend_exposure").front();
  assert(frontend.contains("id"));
  assert(frontend.contains("state"));
}

} // namespace

// Verifies the complete capability metadata and discovery contract.
int main() {
  CheckInventory();
  CheckArgumentValueTypes();
  CheckCommandParity();
  CheckRequestShapeFailures();
  CheckQueries();
  CheckFrontendExposure();
  CheckSerialization();
  return 0;
}

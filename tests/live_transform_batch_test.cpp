#include "command/command_json_serializer.h"
#include "command/command_transform_batch.h"
#include "json.hpp"
#include "live/live_request_executor.h"
#include "matrixutils.h"
#include "mvrscene.h"
#include "query/scene_query.h"

#include <cassert>
#include <stdexcept>
#include <string_view>

namespace {
using Json = nlohmann::json;
namespace command = perastage::command;

class RecordingHost final : public command::ProjectMutationHost {
public:
  command::MutationPublication
  CommitMutation(const MvrScene &, const scene_grouping::ObjectSelection &,
                 const std::string &) override {
    if (failPublication)
      throw std::runtime_error("publication failed");
    ++publications;
    return {true, true};
  }
  bool failPublication = false;
  int publications = 0;
};

MvrScene BuildScene() {
  MvrScene scene;
  for (const std::string uuid : {"fixture-a", "fixture-b"}) {
    Fixture fixture;
    fixture.uuid = uuid;
    fixture.transform = MatrixUtils::Identity();
    scene.fixtures.emplace(uuid, fixture);
  }
  Truss truss;
  truss.uuid = "truss-a";
  truss.transform = MatrixUtils::Identity();
  scene.trusses.emplace(truss.uuid, truss);
  Support support;
  support.uuid = "support-a";
  support.transform = MatrixUtils::Identity();
  scene.supports.emplace(support.uuid, support);
  SceneObject object;
  object.uuid = "object-a";
  object.transform = MatrixUtils::Identity();
  scene.sceneObjects.emplace(object.uuid, object);
  return scene;
}

Json Wire(const Json &arguments) {
  return {{"schema_version", 1}, {"request_id", "batch-test"},
          {"operation", "execute"}, {"value", "scene.transform.batch"},
          {"arguments", arguments}};
}

command::Request TypedRequest(const Json &arguments) {
  command::Request request{"scene.transform.batch", {}};
  for (const char *id : {"target_kinds", "target_uuids", "component_kinds",
                        "axes", "values", "modes", "spaces"}) {
    if (std::string_view(id) == "values")
      request.arguments.push_back({id, arguments[id].get<std::vector<double>>()});
    else
      request.arguments.push_back(
          {id, arguments[id].get<std::vector<std::string>>()});
  }
  return request;
}

void AssertSelection(const scene_grouping::ObjectSelection &actual,
                     const scene_grouping::ObjectSelection &expected) {
  assert(actual.fixtures == expected.fixtures && actual.trusses == expected.trusses);
  assert(actual.supports == expected.supports &&
         actual.sceneObjects == expected.sceneObjects);
}

void AssertScene(const MvrScene &actual, const MvrScene &expected) {
  auto check = [](const auto &actualObjects, const auto &expectedObjects) {
    assert(actualObjects.size() == expectedObjects.size());
    for (const auto &[uuid, object] : expectedObjects) {
      const auto &matrix = actualObjects.at(uuid).transform;
      assert(matrix.u == object.transform.u && matrix.v == object.transform.v &&
             matrix.w == object.transform.w && matrix.o == object.transform.o);
    }
  };
  check(actual.fixtures, expected.fixtures);
  check(actual.trusses, expected.trusses);
  check(actual.supports, expected.supports);
  check(actual.sceneObjects, expected.sceneObjects);
  assert(perastage::query::ListObjects(actual) == perastage::query::ListObjects(expected));
}

// Compares the wire record with the semantic result, including all diagnostics.
Json CheckSemantic(const Json &arguments, command::ExecutionContext &context,
                   RecordingHost &host) {
  MvrScene expectedScene = context.scene;
  auto expectedSelection = context.selection;
  const auto beforeSelection = context.selection;
  RecordingHost expectedHost;
  expectedHost.failPublication = host.failPublication;
  command::ExecutionContext expectedContext{expectedScene, expectedSelection, expectedHost};
  const auto expected = command::transform::ExecuteBatch(TypedRequest(arguments), expectedContext);
  const int beforePublications = host.publications;
  const auto execution = perastage::live::ExecuteRequest(Wire(arguments).dump(), context, {});
  const Json response = Json::parse(execution.response);
  assert(response["schema_version"] == 1 && response["request_id"] == "batch-test");
  assert(response["ok"] == true && response["result"]["success"] == expected.Success());
  assert(response["result"]["parse_diagnostics"].empty());
  assert(response["result"]["records"] ==
         Json::array({Json::parse(command::serialization::SerializeResultToJson(expected))}));
  assert(execution.mutation.sceneChanged == expected.mutation.sceneChanged);
  assert(!execution.mutation.selectionChanged && !execution.mutation.projectMetadataChanged);
  assert(execution.mutation.undoEntryRecorded == expected.mutation.undoEntryRecorded);
  assert(execution.mutation.projectDirty == expected.mutation.projectDirty);
  assert(host.publications - beforePublications == expectedHost.publications);
  AssertSelection(context.selection, beforeSelection);
  AssertScene(context.scene, expectedScene);
  return response["result"]["records"][0];
}
} // namespace

int main() {
  MvrScene scene = BuildScene();
  scene_grouping::ObjectSelection selected{{"fixture-b", "fixture-a"}, {"truss-a"},
                                           {"support-a"}, {"object-a"}};
  RecordingHost host;
  command::ExecutionContext context{scene, selected, host};
  const Json valid = {
      {"target_kinds", {"fixture", "fixture", "fixture", "fixture", "truss", "support", "scene_object"}},
      {"target_uuids", {"fixture-a", "fixture-b", "fixture-a", "fixture-a", "truss-a", "support-a", "object-a"}},
      {"component_kinds", {"position", "position", "rotation", "position", "position", "rotation", "position"}},
      {"axes", {"x", "x", "z", "x", "y", "y", "z"}},
      {"values", {1000, -2000, 90, 500, 100, 45, 750}},
      {"modes", {"absolute", "absolute", "relative", "relative", "absolute", "absolute", "absolute"}},
      {"spaces", {"world", "world", "world", "local", "world", "world", "world"}}};
  assert(CheckSemantic(valid, context, host)["outcome"] == "success");
  assert(host.publications == 1);
  assert(scene.fixtures.at("fixture-b").transform.o[0] == -2000.0f);
  const auto beforeRejected = scene;
  const auto beforeSelection = selected;
  for (const auto &[field, value] : std::vector<std::pair<std::string, Json>>{
           {"target_uuids", {"fixture-a", "missing", "fixture-a", "fixture-a", "truss-a", "support-a", "object-a"}},
           {"target_kinds", {"fixture", "truss", "fixture", "fixture", "truss", "support", "scene_object"}},
           {"values", Json::array({1000})},
           {"axes", {"x", "bad", "z", "x", "y", "y", "z"}},
           {"modes", {"absolute", "unknown", "relative", "relative", "absolute", "absolute", "absolute"}},
           {"spaces", {"world", "unknown", "world", "local", "world", "world", "world"}}}) {
    Json rejected = valid;
    rejected[field] = value;
    const auto record = CheckSemantic(rejected, context, host);
    assert(record["outcome"] == "validation_error" && !record["diagnostics"].empty());
    AssertScene(scene, beforeRejected);
  }
  Json empty = valid;
  for (auto &values : empty)
    values = Json::array();
  assert(CheckSemantic(empty, context, host)["outcome"] == "validation_error");
  Json noop = {{"target_kinds", {"fixture"}}, {"target_uuids", {"fixture-b"}},
               {"component_kinds", {"position"}}, {"axes", {"x"}}, {"values", {-2000}},
               {"modes", {"absolute"}}, {"spaces", {"world"}}};
  const auto noChange = CheckSemantic(noop, context, host);
  assert(noChange["outcome"] == "success");
  assert(noChange["diagnostics"][0]["code"] == "scene.transform.batch.noop");
  assert(host.publications == 1);

  host.failPublication = true;
  assert(CheckSemantic(valid, context, host)["outcome"] == "execution_error");
  AssertScene(scene, beforeRejected);
  host.failPublication = false;

  std::vector<Json> malformed;
  for (const auto &field : {"target_kinds", "target_uuids", "component_kinds", "axes", "values", "modes", "spaces"}) {
    Json missing = valid;
    missing.erase(field);
    malformed.push_back(std::move(missing));
  }
  for (const auto &[field, value] : std::vector<std::pair<std::string, Json>>{
           {"values", Json::array({"1000"})}, {"values", Json::array({nullptr})},
           {"target_uuids", Json::array({42})}, {"axes", "x"}, {"text", "pos x 1"}}) {
    Json wrong = valid;
    wrong[field] = value;
    malformed.push_back(std::move(wrong));
  }
  for (const auto &arguments : malformed) {
    const auto response = Json::parse(perastage::live::ExecuteRequest(Wire(arguments).dump(), context, {}).response);
    assert(response["ok"] == false && response["error"]["code"] == "invalid_arguments");
    AssertScene(scene, beforeRejected);
    AssertSelection(selected, beforeSelection);
  }
  Json unsupported = Wire(valid);
  unsupported["value"] = "scene.transform.rotation";
  const auto response = Json::parse(perastage::live::ExecuteRequest(unsupported.dump(), context, {}).response);
  assert(response["ok"] == false && response["error"]["code"] == "unsupported_operation");
  assert(host.publications == 1);
  AssertScene(scene, beforeRejected);
  AssertSelection(selected, beforeSelection);
  return 0;
}

#include "json.hpp"
#include "live/live_request_executor.h"
#include "matrixutils.h"
#include "mvrscene.h"
#include "query/scene_query.h"

#include <cassert>

namespace {
using Json = nlohmann::json;

class RecordingHost final : public perastage::command::ProjectMutationHost {
public:
  perastage::command::MutationPublication
  CommitMutation(const MvrScene &, const scene_grouping::ObjectSelection &,
                 const std::string &) override {
    ++publications;
    return {true, true};
  }
  int publications = 0;
};

// Seeds every selectable category with two independently identified objects.
MvrScene BuildScene() {
  MvrScene scene;
  for (const std::string suffix : {"-a", "-b"}) {
    Fixture fixture;
    fixture.uuid = "fixture" + suffix;
    fixture.fixtureId = 1;
    fixture.transform = MatrixUtils::Identity();
    scene.fixtures.emplace(fixture.uuid, fixture);
    Truss truss;
    truss.uuid = "truss" + suffix;
    truss.transform = MatrixUtils::Identity();
    scene.trusses.emplace(truss.uuid, truss);
    Support support;
    support.uuid = "support" + suffix;
    support.transform = MatrixUtils::Identity();
    scene.supports.emplace(support.uuid, support);
    SceneObject object;
    object.uuid = "scene_object" + suffix;
    object.transform = MatrixUtils::Identity();
    scene.sceneObjects.emplace(object.uuid, object);
  }
  return scene;
}

// Returns the test's corresponding semantic selection bucket.
std::vector<std::string> &Bucket(scene_grouping::ObjectSelection &selection,
                                 const std::string &kind) {
  if (kind == "fixture")
    return selection.fixtures;
  if (kind == "truss")
    return selection.trusses;
  if (kind == "support")
    return selection.supports;
  assert(kind == "scene_object");
  return selection.sceneObjects;
}

// Builds explicit parallel arrays used by the generic Command contract.
Json Arguments(const std::string &kind, bool preserve,
               const std::vector<std::string> &operations,
               const std::vector<std::string> &uuids) {
  return {{"target_kind", kind}, {"preserve_existing", preserve},
          {"operation_kinds", operations},
          {"object_kinds", std::vector<std::string>(uuids.size(), kind)},
          {"object_uuids", uuids}};
}

Json Wire(const Json &arguments) {
  return {{"schema_version", 1}, {"request_id", "selection-test"},
          {"operation", "execute"}, {"value", "scene.selection.update"},
          {"arguments", arguments}};
}

// Checks both semantic summaries and versioned serialization for every request.
Json Execute(const Json &wire, perastage::command::ExecutionContext &context,
             bool changed) {
  const auto result = perastage::live::ExecuteRequest(wire.dump(), context, {});
  assert(result.mutation.selectionChanged == changed);
  assert(!result.mutation.sceneChanged && !result.mutation.projectDirty &&
         !result.mutation.undoEntryRecorded &&
         !result.mutation.projectMetadataChanged);
  const auto response = Json::parse(result.response);
  assert(response["schema_version"] == 1);
  if (response["ok"] == true) {
    assert(response["result"]["records"].size() == 1);
    const auto &mutation = response["result"]["records"][0]["mutation"];
    assert((mutation == Json{{"scene_changed", false},
                            {"selection_changed", changed},
                            {"project_metadata_changed", false},
                            {"undo_entry_recorded", false},
                            {"project_dirty", false}}));
  }
  return response;
}

// Confirms the current live selection uses Core's typed, sorted references.
void CheckPublishedSelection(perastage::command::ExecutionContext &context) {
  const auto response = Json::parse(perastage::live::ExecuteRequest(
      Json{{"schema_version", 1}, {"request_id", "selection-query"},
           {"operation", "query"}, {"value", "scene.selection.get"}}
          .dump(),
      context, {}).response);
  Json expected = Json::array();
  for (const auto &object : perastage::query::GetSelection(context.selection))
    expected.push_back({{"kind", perastage::scene_identity::KindToken(object.kind)},
                        {"uuid", object.uuid}});
  assert(response["ok"] == true && response["result"]["objects"] == expected);
}
} // namespace

// Covers all semantic kinds, ordered update modes and atomic validation failure.
int main() {
  MvrScene scene = BuildScene();
  scene_grouping::ObjectSelection selected{{"fixture-a"}, {"truss-a"},
                                           {"support-a"}, {"scene_object-a"}};
  RecordingHost host;
  perastage::command::ExecutionContext context{scene, selected, host};
  const auto objectsBefore = perastage::query::ListObjects(scene);
  const auto summaryBefore = perastage::query::GetSummary(scene);
  const std::vector<std::string> kinds = {"fixture", "truss", "support", "scene_object"};
  for (const std::string &kind : kinds) {
    const std::string a = kind + "-a";
    const std::string b = kind + "-b";
    const auto before = selected;
    auto update = [&](bool preserve, std::vector<std::string> operations,
                      std::vector<std::string> uuids, bool changed) {
      const auto arguments = Arguments(kind, preserve, operations, uuids);
      const auto response = Execute(Wire(arguments), context, changed);
      assert(response["ok"] == true && response["result"]["success"] == true);
      const auto &record = response["result"]["records"][0];
      assert(record["outcome"] == "success");
      assert(record["request"]["command_id"] == "scene.selection.update");
      Json serializedArguments = Json::object();
      for (const auto &argument : record["request"]["arguments"])
        serializedArguments[argument["id"].get<std::string>()] = argument["value"];
      assert(serializedArguments == arguments);
      CheckPublishedSelection(context);
      assert(host.publications == 0);
    };
    update(false, {"add"}, {b}, true);
    assert(Bucket(selected, kind) == std::vector<std::string>{b});
    update(true, {"add"}, {a}, true);
    assert(Bucket(selected, kind) == std::vector<std::string>({b, a}));
    update(true, {"add"}, {a}, false);
    update(true, {"remove"}, {b}, true);
    assert(Bucket(selected, kind) == std::vector<std::string>{a});
    update(true, {"remove"}, {b}, false);
    update(true, {"add", "remove", "add"}, {b, a, a}, true);
    assert(Bucket(selected, kind) == std::vector<std::string>({b, a}));
    update(false, {}, {}, true);
    assert(Bucket(selected, kind).empty());
    for (const auto &otherKind : kinds)
      if (otherKind != kind) {
        auto original = before;
        assert(Bucket(selected, otherKind) == Bucket(original, otherKind));
      }
    update(false, {"add"}, {a}, true);
  }

  for (const std::string &kind : kinds) {
    const auto selectionBefore = perastage::query::GetSelection(selected);
    for (const std::string &invalidUuid :
         std::vector<std::string>{"missing", "", kind == "fixture" ? "truss-a" : "fixture-a"}) {
      for (const std::string &operation : {"add", "remove"}) {
        const auto response = Execute(Wire(Arguments(
            kind, false, {"add", operation}, {kind + "-b", invalidUuid})), context, false);
        assert(response["ok"] == true && response["result"]["success"] == false);
        const auto &record = response["result"]["records"][0];
        assert(record["outcome"] == "validation_error");
        assert(record["diagnostics"][0]["code"] == "scene.selection.invalid_object");
        assert(perastage::query::GetSelection(selected) == selectionBefore);
        CheckPublishedSelection(context);
      }
    }
  }

  Json wrongKind = Arguments("fixture", false, {"add", "add"},
                             {"fixture-b", "truss-a"});
  wrongKind["object_kinds"][1] = "truss";
  const auto beforeWrongKind = perastage::query::GetSelection(selected);
  const auto wrongKindResult = Execute(Wire(wrongKind), context, false);
  assert(wrongKindResult["ok"] == true &&
         wrongKindResult["result"]["success"] == false);
  assert(perastage::query::GetSelection(selected) == beforeWrongKind);

  const Json valid = Arguments("fixture", false, {"add"}, {"fixture-b"});
  std::vector<Json> malformed;
  for (const auto &field : {"target_kind", "preserve_existing", "operation_kinds", "object_kinds", "object_uuids"}) {
    Json missing = valid;
    missing.erase(field);
    malformed.push_back(std::move(missing));
  }
  for (const auto &[field, value] : std::vector<std::pair<std::string, Json>>{
           {"target_kind", "group"}, {"preserve_existing", "false"},
           {"operation_kinds", Json::array({"replace"})},
           {"operation_kinds", Json::array()},
           {"object_kinds", Json::array({"fixture", "fixture"})},
           {"object_kinds", Json::array({"unknown"})},
           {"object_uuids", Json::array({42})}, {"objects", Json::array()}}) {
    Json arguments = valid;
    arguments[field] = value;
    malformed.push_back(std::move(arguments));
  }
  const auto selectionBefore = perastage::query::GetSelection(selected);
  for (const auto &arguments : malformed) {
    const auto response = Execute(Wire(arguments), context, false);
    assert(response["ok"] == false && response["error"]["code"] == "invalid_arguments");
    assert(perastage::query::GetSelection(selected) == selectionBefore);
  }
  Json wrongVersion = Wire(valid);
  wrongVersion["schema_version"] = 2;
  assert(Execute(wrongVersion, context, false)["error"]["code"] == "unsupported_version");
  Json wrongArguments = Wire(valid);
  wrongArguments["arguments"] = Json::array();
  assert(Execute(wrongArguments, context, false)["ok"] == false);
  Json unsupported = Wire(valid);
  unsupported["value"] = "scene.transform.position";
  assert(Execute(unsupported, context, false)["error"]["code"] == "unsupported_operation");
  assert(perastage::query::GetSelection(selected) == selectionBefore);
  assert(perastage::query::ListObjects(scene) == objectsBefore);
  assert(perastage::query::GetSummary(scene) == summaryBefore);
  assert(scene.fixtures.at("fixture-a").transform.o == MatrixUtils::Identity().o);
  assert(scene.trusses.at("truss-a").transform.o == MatrixUtils::Identity().o);
  assert(scene.supports.at("support-a").transform.o == MatrixUtils::Identity().o);
  assert(scene.sceneObjects.at("scene_object-a").transform.o == MatrixUtils::Identity().o);
  assert(host.publications == 0);
  return 0;
}

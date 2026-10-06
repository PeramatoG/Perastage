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

// Creates equivalent scene data in different insertion orders.
MvrScene BuildScene(bool reverse) {
  MvrScene scene;
  Fixture fixture;
  fixture.uuid = "fixture-a";
  fixture.instanceName = "Blinder \"Front\"";
  fixture.typeName = "LED Blinder";
  fixture.gdtfSpec = "Vendor@LED_Blinder.gdtf";
  fixture.gdtfMode = "8 channel";
  fixture.fixtureIdText = "FOH-A";
  fixture.fixtureId = 12;
  fixture.fixtureIdNumeric = 24;
  fixture.unitNumber = 36;
  fixture.customId = 48;
  fixture.customIdType = 60;
  fixture.address = "2.101";
  fixture.layer = "layer-a";
  fixture.parentGroupUuid = "group-a";
  fixture.transform = MatrixUtils::Identity();
  Fixture second = fixture;
  second.uuid = "fixture-z";
  second.instanceName = "Second";
  for (const Fixture &item : reverse ? std::vector<Fixture>{second, fixture}
                                    : std::vector<Fixture>{fixture, second})
    scene.fixtures.emplace(item.uuid, item);
  Truss truss;
  truss.uuid = "truss-a";
  truss.name = "Front truss";
  truss.layer = fixture.layer;
  truss.unitNumber = 7;
  truss.transform = MatrixUtils::Identity();
  scene.trusses.emplace(truss.uuid, truss);
  Support support;
  support.uuid = "support-a";
  support.name = "Front motor";
  support.layer = fixture.layer;
  support.transform = MatrixUtils::Identity();
  scene.supports.emplace(support.uuid, support);
  SceneObject object;
  object.uuid = "object-a";
  object.name = "Riser";
  object.layer = fixture.layer;
  object.modelFile = "riser.glb";
  object.fixtureIdText = "Scenic-A";
  object.fixtureIdNumeric = 17;
  object.transform = MatrixUtils::Identity();
  scene.sceneObjects.emplace(object.uuid, object);
  GroupObject group;
  group.uuid = "group-a";
  group.name = "Blinders";
  group.layer = fixture.layer;
  group.children = {{MvrNodeType::Fixture, second.uuid},
                    {MvrNodeType::Fixture, fixture.uuid}};
  scene.groupObjects.emplace(group.uuid, group);
  Layer layer;
  layer.uuid = fixture.layer;
  layer.name = "Front";
  layer.color = "#123456";
  layer.childUUIDs = {truss.uuid, fixture.uuid, support.uuid, object.uuid,
                     group.uuid, "unknown"};
  scene.layers.emplace(layer.uuid, layer);
  return scene;
}

// Executes the same versioned contract used by local-live transports.
Json Query(const std::string &id, perastage::command::ExecutionContext &context,
           const Json &arguments = Json::object()) {
  const auto result = perastage::live::ExecuteRequest(
      Json{{"schema_version", 1}, {"request_id", "query-test"},
           {"operation", "query"}, {"value", id}, {"arguments", arguments}}
          .dump(),
      context, {});
  assert(!result.mutation.sceneChanged && !result.mutation.selectionChanged &&
         !result.mutation.projectDirty && !result.mutation.undoEntryRecorded);
  return Json::parse(result.response);
}
} // namespace

// Checks real discovery descriptors, identity lookups and read-only behavior.
int main() {
  MvrScene scene = BuildScene(false);
  MvrScene reordered = BuildScene(true);
  scene_grouping::ObjectSelection selected{{"fixture-z", "fixture-a"},
                                           {"truss-a"}, {"support-a"},
                                           {"object-a"}};
  scene_grouping::ObjectSelection reorderedSelection = selected;
  RecordingHost host;
  perastage::command::ExecutionContext context{scene, selected, host};
  perastage::command::ExecutionContext other{reordered, reorderedSelection, host};
  const auto beforeObjects = perastage::query::ListObjects(scene);
  const auto beforeLayers = perastage::query::ListLayers(scene);
  const auto beforeGroups = perastage::query::ListGroups(scene);
  const auto beforeSelection = perastage::query::GetSelection(selected);

  const auto listed = Query("scene.objects.list", context);
  assert(listed["ok"] == true);
  assert(listed == Query("scene.objects.list", other));
  const auto &objects = listed["result"]["objects"];
  assert(objects.size() == 6);
  const auto &fixture = objects[0];
  assert(fixture["kind"] == "fixture" && fixture["uuid"] == "fixture-a");
  assert(fixture["name"] == "Blinder \"Front\"" &&
         fixture["type_name"] == "LED Blinder");
  assert(fixture["resource"] == "Vendor@LED_Blinder.gdtf");
  assert(fixture["layer"] == "layer-a" && fixture["parent_group"] == "group-a");
  assert(fixture["fixture_id_text"] == "FOH-A" && fixture["fixture_id"] == 12);
  assert(fixture["fixture_id_numeric"] == 24 && fixture["unit_number"] == 36);
  assert(fixture["custom_id"] == 48 && fixture["custom_id_type"] == 60);
  assert(fixture["patch_address"] == "2.101" && fixture["gdtf_mode"] == "8 channel");
  const auto found = Query("scene.object.get", context,
                           {{"kind", "fixture"}, {"uuid", "fixture-a"}});
  assert(found["ok"] == true && found["result"]["object"] == fixture);
  assert(found["result"]["diagnostics"].empty());
  for (const auto &object : objects) {
    const auto lookup = Query("scene.object.get", context,
                              {{"kind", object["kind"]}, {"uuid", object["uuid"]}});
    assert(lookup["ok"] == true && lookup["result"]["object"] == object);
  }
  for (const auto &arguments :
       std::vector<Json>{{{"kind", "truss"}, {"uuid", "fixture-a"}},
                         {{"kind", "fixture"}, {"uuid", "missing"}}}) {
    const auto missing = Query("scene.object.get", context, arguments);
    assert(missing["ok"] == false &&
           missing["error"]["code"] == "scene.object.not_found");
  }
  for (const auto &arguments :
       std::vector<Json>{{{"kind", "fixture"}},
                         {{"kind", "fixture"}, {"uuid", ""}},
                         {{"kind", "unknown"}, {"uuid", "fixture-a"}},
                         {{"kind", "fixture"}, {"uuid", 12}}})
    assert(Query("scene.object.get", context, arguments)["ok"] == false);

  const auto layers = Query("scene.layers.list", context);
  assert(layers == Query("scene.layers.list", other));
  assert(layers["result"]["layers"].size() == 1);
  assert(layers["result"]["layers"][0]["name"] == "Front");
  assert(layers["result"]["layers"][0]["members"].size() == 5);
  const auto groups = Query("scene.groups.list", context);
  assert(groups == Query("scene.groups.list", other));
  assert(groups["result"]["groups"][0]["children"][0]["uuid"] == "fixture-a");
  assert(Query("scene.summary", context)["result"]["fixtures"] == 2);
  assert(Query("scene.selection.get", context)["result"]["objects"].size() == 5);
  assert(Query("scene.objects.list", context, {{"find_blinders", true}})["ok"] == false);
  assert(perastage::query::ListObjects(scene) == beforeObjects);
  assert(perastage::query::ListLayers(scene) == beforeLayers);
  assert(perastage::query::ListGroups(scene) == beforeGroups);
  assert(perastage::query::GetSelection(selected) == beforeSelection);
  assert(host.publications == 0);
  return 0;
}

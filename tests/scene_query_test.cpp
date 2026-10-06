#include "mvrscene.h"
#include "query/scene_query.h"

#include <cassert>

using perastage::scene_identity::ObjectKind;

// Builds equivalent scenes with deliberately different insertion orders.
MvrScene BuildScene(bool reverse) {
  MvrScene scene;
  scene.versionMajor = 1;
  scene.versionMinor = 6;
  Fixture a;
  a.uuid = "fixture-a";
  a.instanceName = "A";
  a.typeName = "Wash";
  a.gdtfSpec = "wash.gdtf";
  a.layer = "layer";
  a.address = "1.10";
  a.gdtfMode = "10";
  a.fixtureIdText = "FOH-101";
  a.fixtureId = 101;
  a.fixtureIdNumeric = 201;
  a.unitNumber = 301;
  a.customId = 401;
  a.customIdType = 501;
  Fixture b;
  b.uuid = "fixture-b";
  b.instanceName = "B";
  b.typeName = "Spot";
  b.gdtfSpec = "spot.gdtf";
  b.layer = "layer";
  b.parentGroupUuid = "group-child";
  b.address = "1.15";
  b.gdtfMode = "8";
  if (reverse) {
    scene.fixtures.emplace(b.uuid, b);
    scene.fixtures.emplace(a.uuid, a);
  } else {
    scene.fixtures.emplace(a.uuid, a);
    scene.fixtures.emplace(b.uuid, b);
  }
  Truss truss;
  truss.uuid = "truss";
  truss.name = "Truss";
  truss.layer = "layer";
  truss.model = "H30V";
  truss.gdtfSpec = "truss.gdtf";
  truss.gdtfMode = "Default";
  truss.unitNumber = 11;
  truss.customId = 12;
  truss.customIdType = 13;
  Support support;
  support.uuid = "support";
  support.name = "Hoist";
  support.layer = "layer";
  support.gdtfSpec = "motor.gdtf";
  support.gdtfMode = "Motor";
  SceneObject object;
  object.uuid = "object";
  object.name = "Podium";
  object.layer = "layer";
  object.modelFile = "podium.3ds";
  object.fixtureIdText = "PODIUM-1";
  object.fixtureIdNumeric = 601;
  scene.trusses.emplace(truss.uuid, truss);
  scene.supports.emplace(support.uuid, support);
  scene.sceneObjects.emplace(object.uuid, object);
  GroupObject child;
  child.uuid = "group-child";
  child.name = "Child";
  child.layer = "layer";
  child.parentGroupUuid = "group-root";
  child.children = {{MvrNodeType::Fixture, "fixture-b"}};
  GroupObject root;
  root.uuid = "group-root";
  root.name = "Root";
  root.layer = "layer";
  root.children = {{MvrNodeType::GroupObject, "group-child"},
                   {MvrNodeType::Fixture, "fixture-a"}};
  scene.groupObjects.emplace(child.uuid, child);
  scene.groupObjects.emplace(root.uuid, root);
  Layer layer;
  layer.uuid = "layer";
  layer.name = "Main";
  layer.color = "#ffffff";
  layer.childUUIDs = {"truss", "fixture-b", "fixture-a", "group-root",
                      "unknown"};
  scene.layers.emplace(layer.uuid, layer);
  return scene;
}

// Verifies deterministic scene, selection, hierarchy, lookup, and patch
// queries.
int main() {
  assert(std::string(perastage::query::kSummaryQueryId) == "scene.summary");
  assert(std::string(perastage::query::kSelectionQueryId) ==
         "scene.selection.get");
  assert(std::string(perastage::query::kObjectsQueryId) ==
         "scene.objects.list");
  assert(std::string(perastage::query::kObjectQueryId) == "scene.object.get");
  assert(std::string(perastage::query::kLayersQueryId) == "scene.layers.list");
  assert(std::string(perastage::query::kGroupsQueryId) == "scene.groups.list");
  assert(std::string(perastage::query::kPatchQueryId) == "scene.patch.status");

  MvrScene scene = BuildScene(false);
  MvrScene reordered = BuildScene(true);
  const auto summary = perastage::query::GetSummary(scene);
  assert(summary.fixtures == 2 && summary.trusses == 1 &&
         summary.supports == 1);
  assert(summary.sceneObjects == 1 && summary.groups == 2 &&
         summary.layers == 1);
  assert(perastage::query::ListObjects(scene) ==
         perastage::query::ListObjects(reordered));
  assert(perastage::query::ListGroups(scene) ==
         perastage::query::ListGroups(reordered));
  assert(perastage::query::ListLayers(scene) ==
         perastage::query::ListLayers(reordered));
  assert(perastage::query::ListObjects(scene) ==
         perastage::query::ListObjects(scene));
  const auto objects = perastage::query::ListObjects(scene);
  assert(objects.size() == 7);
  const auto &fixture = objects.front();
  assert(fixture.kind == ObjectKind::Fixture && fixture.uuid == "fixture-a" &&
         fixture.name == "A" && fixture.layerUuid == "layer" &&
         fixture.parentGroupUuid.empty() && fixture.typeName == "Wash" &&
         fixture.resource == "wash.gdtf");
  assert(fixture.fixtureIdText == "FOH-101" && fixture.fixtureId == 101 &&
         fixture.fixtureIdNumeric == 201 && fixture.unitNumber == 301 &&
         fixture.customId == 401 && fixture.customIdType == 501 &&
         fixture.rawAddress == "1.10" && fixture.gdtfMode == "10");
  const auto &unassignedFixture = objects[1];
  assert(unassignedFixture.fixtureIdText == "" &&
         unassignedFixture.fixtureId == 0 &&
         unassignedFixture.fixtureIdNumeric == 0 &&
         unassignedFixture.unitNumber == 0 && unassignedFixture.customId == 0 &&
         unassignedFixture.customIdType == 0);
  const auto &group = objects[2];
  assert(group.kind == ObjectKind::Group && !group.fixtureIdText &&
         !group.fixtureId && !group.fixtureIdNumeric && !group.unitNumber &&
         !group.customId && !group.customIdType && !group.rawAddress &&
         !group.gdtfMode);
  const auto &sceneObject = objects[4];
  assert(sceneObject.kind == ObjectKind::SceneObject &&
         sceneObject.resource == "podium.3ds" &&
         sceneObject.fixtureIdText == "PODIUM-1" &&
         sceneObject.fixtureIdNumeric == 601 && !sceneObject.fixtureId &&
         !sceneObject.unitNumber && !sceneObject.customId &&
         !sceneObject.customIdType && !sceneObject.rawAddress &&
         !sceneObject.gdtfMode);
  const auto &hoist = objects[5];
  assert(hoist.kind == ObjectKind::Support && hoist.resource == "motor.gdtf" &&
         hoist.gdtfMode == "Motor" && !hoist.fixtureIdText && !hoist.fixtureId &&
         !hoist.fixtureIdNumeric && !hoist.unitNumber && !hoist.customId &&
         !hoist.customIdType && !hoist.rawAddress);
  const auto &truss = objects.back();
  assert(truss.kind == ObjectKind::Truss && truss.typeName == "H30V" &&
         truss.resource == "truss.gdtf" && truss.gdtfMode == "Default" &&
         truss.unitNumber == 11 && truss.customId == 12 &&
         truss.customIdType == 13 && !truss.fixtureIdText && !truss.fixtureId &&
         !truss.fixtureIdNumeric && !truss.rawAddress);

  scene_grouping::ObjectSelection selection{
      {"fixture-b", "fixture-a"}, {"truss"}, {"support"}, {"object"}};
  const auto before = selection;
  const auto selected = perastage::query::GetSelection(selection);
  assert(selected.size() == 5 && selected.front().kind == ObjectKind::Fixture);
  assert(selection.fixtures == before.fixtures &&
         selection.trusses == before.trusses &&
         selection.supports == before.supports &&
         selection.sceneObjects == before.sceneObjects);

  std::vector<perastage::query::Diagnostic> diagnostics;
  const auto found = perastage::query::GetObject(
      scene, {ObjectKind::Fixture, "fixture-a"}, diagnostics);
  assert(found && *found == fixture && diagnostics.empty());
  for (const auto &descriptor : objects) {
    const auto lookup = perastage::query::GetObject(
        scene, {descriptor.kind, descriptor.uuid}, diagnostics);
    assert(lookup && *lookup == descriptor && diagnostics.empty());
  }
  assert(!perastage::query::GetObject(scene, {ObjectKind::Truss, "fixture-a"},
                                      diagnostics));
  assert(diagnostics.size() == 1 &&
         diagnostics.front().code == "scene.object.not_found");
  assert(!perastage::query::GetObject(scene, {ObjectKind::Fixture, ""},
                                      diagnostics));
  assert(!perastage::query::GetObject(scene, {ObjectKind::Support, "missing"},
                                      diagnostics));
  assert(diagnostics.size() == 3 &&
         perastage::query::ListObjects(scene) == objects);

  const auto layers = perastage::query::ListLayers(scene);
  assert(layers.size() == 1 && layers.front().members.size() == 4);
  const auto groups = perastage::query::ListGroups(scene);
  assert(groups.size() == 2 && groups.front().parentGroupUuid == "group-root");
  assert(groups.back().children.size() == 2);

  const auto footprint = [](const Fixture &fixture) -> std::optional<int> {
    return fixture.gdtfMode == "10" ? 10 : fixture.gdtfMode == "8" ? 8 : 0;
  };
  const auto patch = perastage::query::GetPatchStatus(scene, footprint);
  assert(patch.fixtures.size() == 2 && patch.conflicts.size() == 1);
  assert(patch.conflicts.front().firstChannel == 15 &&
         patch.conflicts.front().lastChannel == 19);
  assert(patch == perastage::query::GetPatchStatus(reordered, footprint));

  const auto summaryBeforeQueries = perastage::query::GetSummary(scene);
  const auto objectsBeforeQueries = perastage::query::ListObjects(scene);
  const auto layersBeforeQueries = perastage::query::ListLayers(scene);
  const auto groupsBeforeQueries = perastage::query::ListGroups(scene);
  const auto patchBeforeQueries =
      perastage::query::GetPatchStatus(scene, footprint);
  std::vector<perastage::query::Diagnostic> repeatedDiagnostics;
  (void)perastage::query::GetSummary(scene);
  (void)perastage::query::GetSelection(selection);
  (void)perastage::query::ListObjects(scene);
  (void)perastage::query::GetObject(scene, {ObjectKind::Fixture, "fixture-a"},
                                    repeatedDiagnostics);
  (void)perastage::query::ListLayers(scene);
  (void)perastage::query::ListGroups(scene);
  (void)perastage::query::GetPatchStatus(scene, footprint);
  assert(perastage::query::GetSummary(scene) == summaryBeforeQueries);
  assert(perastage::query::ListObjects(scene) == objectsBeforeQueries);
  assert(perastage::query::ListLayers(scene) == layersBeforeQueries);
  assert(perastage::query::ListGroups(scene) == groupsBeforeQueries);
  assert(perastage::query::GetPatchStatus(scene, footprint) ==
         patchBeforeQueries);
  assert(scene.fixtures.size() == 2 &&
         scene.fixtures.at("fixture-a").address == "1.10" &&
         scene.fixtures.at("fixture-b").parentGroupUuid == "group-child");
  assert(scene.trusses.size() == 1 && scene.supports.size() == 1 &&
         scene.sceneObjects.size() == 1 && scene.groupObjects.size() == 2 &&
         scene.layers.size() == 1);
  assert(scene.groupObjects.at("group-root").children.size() == 2 &&
         scene.layers.at("layer").childUUIDs.size() == 5);
  assert(selection.fixtures == before.fixtures &&
         selection.trusses == before.trusses &&
         selection.supports == before.supports &&
         selection.sceneObjects == before.sceneObjects);

  scene.fixtures.at("fixture-b").address = "2.1";
  assert(perastage::query::GetPatchStatus(scene, footprint).conflicts.empty());
  scene.fixtures.at("fixture-b").address = "invalid";
  scene.fixtures.at("fixture-b").gdtfMode.clear();
  const auto invalid = perastage::query::GetPatchStatus(scene, footprint);
  assert(invalid.diagnostics.size() == 2);
  assert(!invalid.fixtures.back().universe &&
         !invalid.fixtures.back().footprint);

  MvrScene ranges;
  auto addRange = [&ranges](const std::string &uuid, const std::string &address,
                            int channels) {
    Fixture fixture;
    fixture.uuid = uuid;
    fixture.address = address;
    fixture.gdtfMode = std::to_string(channels);
    ranges.fixtures.emplace(uuid, fixture);
  };
  addRange("a", "1.1", 10);
  addRange("b", "1.5", 10);
  addRange("c", "1.8", 2);
  addRange("different-universe", "2.5", 10);
  addRange("ends-at-512", "3.512", 1);
  addRange("exceeds-512", "4.512", 2);
  addRange("invalid-whitespace", " 5.1", 1);
  addRange("unresolved", "6.1", -1);
  addRange("zero", "7.1", 0);
  const auto numericFootprint =
      [](const Fixture &fixture) -> std::optional<int> {
    if (fixture.gdtfMode == "-1")
      return std::nullopt;
    return std::stoi(fixture.gdtfMode);
  };
  const auto rangeStatus =
      perastage::query::GetPatchStatus(ranges, numericFootprint);
  assert(rangeStatus.conflicts.size() == 3);
  assert(rangeStatus.conflicts[0].fixtureUuids ==
         std::vector<std::string>({"a", "b"}));
  assert(rangeStatus.conflicts[1].fixtureUuids ==
         std::vector<std::string>({"a", "c"}));
  assert(rangeStatus.conflicts[2].fixtureUuids ==
         std::vector<std::string>({"b", "c"}));
  assert(rangeStatus.fixtures[4].uuid == "ends-at-512" &&
         rangeStatus.fixtures[4].endChannel == 512);
  assert(rangeStatus.fixtures[5].uuid == "exceeds-512" &&
         !rangeStatus.fixtures[5].endChannel);
  assert(rangeStatus.fixtures[6].uuid == "invalid-whitespace" &&
         !rangeStatus.fixtures[6].universe);
  assert(!rangeStatus.fixtures[7].footprint);
  assert(!rangeStatus.fixtures[8].footprint);

  MvrScene reversedRanges;
  const std::vector<std::string> reverseOrder = {
      "zero",        "unresolved",  "invalid-whitespace",
      "exceeds-512", "ends-at-512", "different-universe",
      "c",           "b",           "a"};
  for (const std::string &uuid : reverseOrder)
    reversedRanges.fixtures.emplace(uuid, ranges.fixtures.at(uuid));
  assert(rangeStatus ==
         perastage::query::GetPatchStatus(reversedRanges, numericFootprint));
  return 0;
}

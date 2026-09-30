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
  a.layer = "layer";
  a.address = "1.10";
  a.gdtfMode = "10";
  Fixture b;
  b.uuid = "fixture-b";
  b.instanceName = "B";
  b.typeName = "Spot";
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
  Support support;
  support.uuid = "support";
  support.name = "Hoist";
  support.layer = "layer";
  SceneObject object;
  object.uuid = "object";
  object.name = "Podium";
  object.layer = "layer";
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
  assert(found && found->name == "A" && diagnostics.empty());
  assert(!perastage::query::GetObject(scene, {ObjectKind::Truss, "fixture-a"},
                                      diagnostics));
  assert(diagnostics.size() == 1 &&
         diagnostics.front().code == "scene.object.not_found");

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

  scene.fixtures.at("fixture-b").address = "2.1";
  assert(perastage::query::GetPatchStatus(scene, footprint).conflicts.empty());
  scene.fixtures.at("fixture-b").address = "invalid";
  scene.fixtures.at("fixture-b").gdtfMode.clear();
  const auto invalid = perastage::query::GetPatchStatus(scene, footprint);
  assert(invalid.diagnostics.size() == 2);
  assert(!invalid.fixtures.back().universe &&
         !invalid.fixtures.back().footprint);
  return 0;
}

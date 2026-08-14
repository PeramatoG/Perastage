#include "scene_clipboard.h"
#include "uuidutils.h"

#include <cassert>
#include <set>

// Verifies value preservation, UUID remapping, metadata, and hierarchy behavior.
int main() {
  MvrScene scene;
  Fixture fixture;
  fixture.uuid = "fixture-source";
  fixture.fixtureId = 42;
  fixture.fixtureIdText = "LX-42";
  fixture.address = "2.101";
  fixture.focus = "shared-focus";
  fixture.transform.o = {1000.0f, 2000.0f, 3000.0f};
  scene.fixtures.emplace(fixture.uuid, fixture);
  Support support;
  support.uuid = "support-source";
  support.name = "Motor A";
  support.motorFixtureUuid = fixture.uuid;
  support.capacityKg = 1000.0f;
  scene.supports.emplace(support.uuid, support);
  Truss truss;
  truss.uuid = "truss-source";
  truss.name = "T1";
  truss.sourceSymdefUuid = "shared-symdef";
  truss.lengthMm = 3000.0f;
  scene.trusses.emplace(truss.uuid, truss);
  SceneObject object;
  object.uuid = "object-source";
  object.name = "Deck";
  object.fixtureIdText = "OBJ-7";
  scene.sceneObjects.emplace(object.uuid, object);
  SelectionState selection;
  selection.SetSelectedFixtures({fixture.uuid});
  selection.SetSelectedSupports({support.uuid});
  selection.SetSelectedTrusses({truss.uuid});
  selection.SetSelectedSceneObjects({object.uuid});
  std::unordered_map<std::string, std::string> labels{{fixture.uuid, "{\"x\":4}"}};
  scene_clipboard::Service clipboard;
  assert(clipboard.Capture(scene, selection, 8, labels));
  const auto pasted = clipboard.Paste(scene, 8, &labels);
  assert(pasted.changed && pasted.nodes.size() == 4);
  std::set<std::string> generated;
  for (const auto &[oldUuid, newUuid] : pasted.uuidRemap) {
    assert(oldUuid != newUuid && IsValidUuid(newUuid));
    generated.insert(newUuid);
  }
  assert(generated.size() == 4);
  const auto &newFixture = scene.fixtures.at(pasted.uuidRemap.at(fixture.uuid));
  assert(newFixture.fixtureId == 42 && newFixture.fixtureIdText == "LX-42");
  assert(newFixture.address == "2.101" && newFixture.focus == "shared-focus");
  const auto &newSupport = scene.supports.at(pasted.uuidRemap.at(support.uuid));
  assert(newSupport.motorFixtureUuid == newFixture.uuid && newSupport.capacityKg == 1000.0f);
  const auto &newTruss = scene.trusses.at(pasted.uuidRemap.at(truss.uuid));
  assert(newTruss.sourceSymdefUuid == "shared-symdef" && newTruss.lengthMm == 3000.0f);
  assert(scene.sceneObjects.at(pasted.uuidRemap.at(object.uuid)).fixtureIdText == "OBJ-7");
  assert(labels.at(newFixture.uuid) == "{\"x\":4}" && !clipboard.CanPaste(9));
  const auto repeatedPaste = clipboard.Paste(scene, 8, &labels);
  assert(repeatedPaste.changed && repeatedPaste.nodes.size() == 4);
  for (const auto &[sourceUuid, repeatedUuid] : repeatedPaste.uuidRemap) {
    assert(IsValidUuid(repeatedUuid));
    assert(repeatedUuid != pasted.uuidRemap.at(sourceUuid));
    assert(!generated.contains(repeatedUuid));
  }
  MvrScene grouped;
  grouped.groupObjects["parent"].uuid = "parent";
  Fixture groupedFixture = fixture;
  groupedFixture.parentGroupUuid = "parent";
  grouped.fixtures[groupedFixture.uuid] = groupedFixture;
  grouped.groupObjects["parent"].children.push_back({MvrNodeType::Fixture, groupedFixture.uuid});
  selection.Clear();
  selection.SetSelectedFixtures({groupedFixture.uuid});
  assert(clipboard.Capture(grouped, selection, 10));
  const auto groupedPaste = clipboard.Paste(grouped, 10);
  const std::string groupedClone = groupedPaste.uuidRemap.at(groupedFixture.uuid);
  assert(grouped.fixtures.at(groupedClone).parentGroupUuid == "parent");
  assert(grouped.groupObjects.at("parent").children.size() == 2);
  grouped.groupObjects.erase("parent");
  const auto rootedPaste = clipboard.Paste(grouped, 10);
  const auto &rooted = grouped.fixtures.at(rootedPaste.uuidRemap.at(groupedFixture.uuid));
  assert(rooted.parentGroupUuid.empty() && !rooted.hasLocalTransform);
  selection.SetSelectedFixtures({groupedFixture.uuid});
  const auto cut = clipboard.Cut(grouped, selection, 10);
  assert(cut.changed && !grouped.fixtures.contains(groupedFixture.uuid));
  assert(selection.GetSelectedFixtures().empty());
  return 0;
}

#include "mvr_identity_recovery.h"
#include "uuidutils.h"

#include <cassert>
#include <string>

// Verifies deterministic canonical recovery and hierarchy reference rewriting.
int main() {
  MvrScene scene;
  Layer malformedLayer;
  malformedLayer.uuid = "layer1";
  malformedLayer.name = "Rig";
  scene.layers[malformedLayer.uuid] = malformedLayer;

  GroupObject group;
  group.uuid = " {AAAAAAAA-BBBB-4CCC-8DDD-EEEEEEEEEEEE} ";
  group.name = "Group";
  group.layer = "Rig";
  scene.groupObjects["legacy-group-key"] = group;

  Truss truss;
  truss.uuid = "broken-truss";
  truss.name = "Truss";
  truss.layer = "Rig";
  truss.parentGroupUuid = "legacy-group-key";
  scene.trusses[truss.uuid] = truss;
  scene.groupObjects["legacy-group-key"].children.push_back(
      {MvrNodeType::Truss, "broken-truss"});

  const auto first =
      mvridentity::RecoverSceneIdentities(scene, "identity-recovery-test");
  assert(first.diagnostics.size() >= 3);
  assert(scene.layers.size() == 2);
  assert(scene.groupObjects.size() == 1);
  assert(scene.trusses.size() == 1);
  const auto &recoveredGroup = scene.groupObjects.begin()->second;
  const auto &recoveredTruss = scene.trusses.begin()->second;
  assert(IsValidUuid(recoveredGroup.uuid));
  assert(IsValidUuid(recoveredTruss.uuid));
  assert(recoveredGroup.uuid == "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");
  assert(recoveredTruss.parentGroupUuid == recoveredGroup.uuid);
  assert(recoveredGroup.children.at(0).uuid == recoveredTruss.uuid);
  assert(scene.layers.contains(scene.layers.begin()->second.uuid));

  const std::string stableTrussUuid = recoveredTruss.uuid;
  const auto second =
      mvridentity::RecoverSceneIdentities(scene, "identity-recovery-test");
  assert(second.diagnostics.empty());
  assert(scene.trusses.contains(stableTrussUuid));
  return 0;
}

#include "mvr_merge_analyzer.h"
#include "mvr_merge_applier.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

// Builds a fixture with the requested UUID and position reference.
static Fixture MakeFixture(const std::string &uuid,
                           const std::string &position) {
  Fixture fixture;
  fixture.uuid = uuid;
  fixture.position = position;
  return fixture;
}

// Builds a layer with the requested UUID, name, color, and child list.
static Layer MakeLayer(const std::string &uuid, const std::string &name,
                       const std::string &color,
                       std::vector<std::string> childUuids = {}) {
  Layer layer;
  layer.uuid = uuid;
  layer.name = name;
  layer.color = color;
  layer.childUUIDs = std::move(childUuids);
  return layer;
}

// Builds a fixture with a user-visible type name and GDTF identity.
static Fixture MakeTypedFixture(const std::string &uuid,
                                const std::string &typeName,
                                const std::string &gdtfSpec,
                                const std::string &gdtfMode) {
  Fixture fixture = MakeFixture(uuid, "");
  fixture.typeName = typeName;
  fixture.gdtfSpec = gdtfSpec;
  fixture.gdtfMode = gdtfMode;
  return fixture;
}

// Writes a small file for SHA-256 identity and resource-copy tests.
static void WriteGdtfFile(const std::filesystem::path &path,
                          const std::string &content) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << content;
}

// Returns true when a scene-relative resource path exists.
static bool ResourceExists(const std::filesystem::path &basePath,
                           const std::string &resourcePath) {
  return std::filesystem::is_regular_file(basePath / resourcePath);
}

// Builds a support with requested references.
static Support MakeSupport(const std::string &uuid, const std::string &position,
                           const std::string &motorFixtureUuid) {
  Support support;
  support.uuid = uuid;
  support.position = position;
  support.motorFixtureUuid = motorFixtureUuid;
  return support;
}

// Builds a group object that references one child UUID.
static GroupObject MakeGroup(const std::string &uuid, MvrNodeType childType,
                             const std::string &childUuid) {
  GroupObject group;
  group.uuid = uuid;
  group.children.push_back(GroupObjectChildRef{childType, childUuid});
  return group;
}

// Verifies fixture UUID collisions use deterministic replacement UUIDs.
static void VerifyFixtureUuidCollisionUsesStableReplacement() {
  MvrScene target;
  target.fixtures["fixture-a"] = MakeFixture("fixture-a", "position-a");

  MvrScene imported;
  imported.fixtures["fixture-a"] = MakeFixture("fixture-a", "");

  const mvr::MvrMergeAnalysis firstAnalysis =
      mvr::AnalyzeImportedSceneMerge(target, imported);
  const mvr::MvrMergeAnalysis secondAnalysis =
      mvr::AnalyzeImportedSceneMerge(target, imported);
  assert(firstAnalysis.uuidCollisionsDetected == 1);
  assert(firstAnalysis.uuidCollisionsResolved == 1);
  assert(firstAnalysis.uuidMap.at("fixture-a") != "fixture-a");
  assert(firstAnalysis.uuidMap.at("fixture-a") ==
         secondAnalysis.uuidMap.at("fixture-a"));
  assert(firstAnalysis.fixtureUuidRemap.at("fixture-a") ==
         firstAnalysis.uuidMap.at("fixture-a"));

  const mvr::MvrSceneMergeResult result =
      mvr::ApplyImportedSceneMerge(target, imported, firstAnalysis);
  assert(result.uuidCollisionsResolved == 1);
  assert(result.fixturesAdded == 1);
  assert(result.fixtureUuidRemap.at("fixture-a") ==
         firstAnalysis.uuidMap.at("fixture-a"));
  assert(target.fixtures.count("fixture-a") == 1);
  assert(target.fixtures.count(firstAnalysis.uuidMap.at("fixture-a")) == 1);
}

// Verifies group child references follow UUID remaps.
static void VerifyGroupChildRemapping() {
  MvrScene target;
  target.fixtures["fixture-a"] = MakeFixture("fixture-a", "");

  MvrScene imported;
  imported.fixtures["fixture-a"] = MakeFixture("fixture-a", "");
  imported.groupObjects["group-a"] =
      MakeGroup("group-a", MvrNodeType::Fixture, "fixture-a");

  const mvr::MvrMergeAnalysis analysis =
      mvr::AnalyzeImportedSceneMerge(target, imported);
  const std::string remappedFixtureUuid = analysis.uuidMap.at("fixture-a");
  mvr::ApplyImportedSceneMerge(target, imported, analysis);

  assert(target.groupObjects.count("group-a") == 1);
  assert(target.groupObjects.at("group-a").children.size() == 1);
  assert(target.groupObjects.at("group-a").children.front().uuid ==
         remappedFixtureUuid);
}

// Verifies support references follow remapped UUIDs.
static void VerifySupportPositionReferenceRemapping() {
  MvrScene target;
  target.positions["position-a"] = "Existing position";
  target.fixtures["fixture-a"] = MakeFixture("fixture-a", "position-a");

  MvrScene imported;
  imported.positions["position-a"] = "Imported position";
  imported.fixtures["fixture-a"] = MakeFixture("fixture-a", "position-a");
  imported.supports["support-a"] =
      MakeSupport("support-a", "position-a", "fixture-a");

  const mvr::MvrMergeAnalysis analysis =
      mvr::AnalyzeImportedSceneMerge(target, imported);
  const std::string remappedPositionUuid = analysis.uuidMap.at("position-a");
  const std::string remappedFixtureUuid = analysis.uuidMap.at("fixture-a");
  mvr::ApplyImportedSceneMerge(target, imported, analysis);

  assert(target.positions.count(remappedPositionUuid) == 1);
  assert(target.supports.count("support-a") == 1);
  assert(target.supports.at("support-a").position == remappedPositionUuid);
  assert(target.supports.at("support-a").motorFixtureUuid ==
         remappedFixtureUuid);
}

// Verifies fixture type conflicts block automatic merge.
static void VerifyFixtureTypeGdtfConflictBlocksAutomaticMerge() {
  const std::filesystem::path tempDir = std::filesystem::temp_directory_path() /
                                        "perastage_mvr_merge_identity_test";
  std::filesystem::remove_all(tempDir);
  WriteGdtfFile(tempDir / "current" / "spot.gdtf", "current gdtf");
  WriteGdtfFile(tempDir / "incoming" / "spot.gdtf", "incoming gdtf");

  MvrScene target;
  target.basePath = (tempDir / "current").string();
  target.fixtures["current-fixture"] =
      MakeTypedFixture("current-fixture", "Spot", "spot.gdtf", "Mode A");

  MvrScene imported;
  imported.basePath = (tempDir / "incoming").string();
  imported.fixtures["incoming-fixture"] =
      MakeTypedFixture("incoming-fixture", "spot", "spot.gdtf", "Mode A");

  const mvr::MvrMergeAnalysis analysis =
      mvr::AnalyzeImportedSceneMerge(target, imported);
  assert(analysis.currentFixtureTypes.at("spot").typeName == "Spot");
  assert(analysis.incomingFixtureTypes.at("spot").typeName == "spot");
  assert(!analysis.currentFixtureTypes.at("spot").gdtfSha256.empty());
  assert(!analysis.incomingFixtureTypes.at("spot").gdtfSha256.empty());
  assert(analysis.currentFixtureTypes.at("spot").gdtfSha256 !=
         analysis.incomingFixtureTypes.at("spot").gdtfSha256);
  assert(analysis.fixtureTypeConflicts.size() == 1);
  assert(mvr::HasBlockingFixtureTypeConflicts(analysis));

  const mvr::MvrSceneMergeResult result =
      mvr::ApplyImportedSceneMerge(target, imported, analysis);
  assert(result.fixtureTypeConflictsBlocked == 1);
  assert(target.fixtures.count("incoming-fixture") == 0);
}

// Verifies current-definition decisions update incoming fixtures.
static void VerifyUseCurrentDefinitionDecisionAppliesToIncomingFixtures() {
  MvrScene target;
  target.fixtures["current-fixture"] =
      MakeTypedFixture("current-fixture", "Spot", "current.gdtf", "Mode A");

  MvrScene imported;
  imported.fixtures["incoming-a"] =
      MakeTypedFixture("incoming-a", "spot", "incoming.gdtf", "Mode B");
  imported.fixtures["incoming-b"] =
      MakeTypedFixture("incoming-b", "SPOT", "incoming.gdtf", "Mode B");

  mvr::MvrMergeOptions options;
  options.fixtureTypeDecisions["spot"] =
      mvr::MvrMergeFixtureTypeDecision::UseCurrentDefinition;
  const mvr::MvrMergeAnalysis analysis =
      mvr::AnalyzeImportedSceneMerge(target, imported, options);
  assert(!mvr::HasBlockingFixtureTypeConflicts(analysis));
  mvr::ApplyImportedSceneMerge(target, imported, analysis);

  assert(target.fixtures.at("incoming-a").typeName == "Spot");
  assert(target.fixtures.at("incoming-a").gdtfSpec == "current.gdtf");
  assert(target.fixtures.at("incoming-a").gdtfMode == "Mode A");
  assert(target.fixtures.at("incoming-b").typeName == "Spot");
  assert(target.fixtures.at("incoming-b").gdtfSpec == "current.gdtf");
}

// Verifies rename decisions preserve imported definitions.
static void VerifyRenameIncomingDefinitionDecisionAppliesToIncomingFixtures() {
  MvrScene target;
  target.fixtures["current-fixture"] =
      MakeTypedFixture("current-fixture", "Spot", "current.gdtf", "Mode A");

  MvrScene imported;
  imported.fixtures["incoming-a"] =
      MakeTypedFixture("incoming-a", "Spot", "incoming.gdtf", "Mode B");
  imported.fixtures["incoming-b"] =
      MakeTypedFixture("incoming-b", "spot", "incoming.gdtf", "Mode B");

  mvr::MvrMergeOptions options;
  options.fixtureTypeDecisions["spot"] =
      mvr::MvrMergeFixtureTypeDecision::RenameIncomingType;
  const mvr::MvrMergeAnalysis analysis =
      mvr::AnalyzeImportedSceneMerge(target, imported, options);
  assert(!mvr::HasBlockingFixtureTypeConflicts(analysis));
  assert(analysis.incomingFixtureTypeRenames.at("spot") == "Spot (Imported)");
  mvr::ApplyImportedSceneMerge(target, imported, analysis);

  assert(target.fixtures.at("incoming-a").typeName == "Spot (Imported)");
  assert(target.fixtures.at("incoming-a").gdtfSpec == "incoming.gdtf");
  assert(target.fixtures.at("incoming-a").gdtfMode == "Mode B");
  assert(target.fixtures.at("incoming-b").typeName == "Spot (Imported)");
}

// Verifies imported layers without UUIDs receive generated merge UUIDs.
static void VerifyEmptyLayerUuidReceivesGeneratedUuid() {
  MvrScene target;

  MvrScene imported;
  imported.layers[""] = MakeLayer("", "Incoming empty UUID", "#123456");

  const mvr::MvrMergeAnalysis analysis =
      mvr::AnalyzeImportedSceneMerge(target, imported);
  assert(analysis.layerUuidMap.contains(""));
  assert(!analysis.layerUuidMap.at("").empty());
  const mvr::MvrSceneMergeResult result =
      mvr::ApplyImportedSceneMerge(target, imported, analysis);

  const std::string generatedUuid = analysis.layerUuidMap.at("");
  assert(result.layersAdded == 1);
  assert(target.layers.count("") == 0);
  assert(target.layers.count(generatedUuid) == 1);
  assert(target.layers.at(generatedUuid).uuid == generatedUuid);
  assert(target.layers.at(generatedUuid).name == "Incoming empty UUID");
}

// Verifies same-name layers with different UUIDs keep current layer state names.
static void VerifySameLayerNameWithDifferentUuidRenamesIncomingLayer() {
  MvrScene target;
  target.layers["current-layer"] =
      MakeLayer("current-layer", "Rig", "#0000FF");

  MvrScene imported;
  imported.layers["incoming-layer"] =
      MakeLayer("incoming-layer", "Rig", "#FF0000");
  imported.fixtures["incoming-fixture"] = MakeFixture("incoming-fixture", "");
  imported.fixtures["incoming-fixture"].layer = "Rig";

  const mvr::MvrMergeAnalysis analysis =
      mvr::AnalyzeImportedSceneMerge(target, imported);
  assert(analysis.incomingLayerNameRenames.at("Rig") == "Rig (Imported)");
  const mvr::MvrSceneMergeResult result =
      mvr::ApplyImportedSceneMerge(target, imported, analysis);

  assert(result.layersAdded == 1);
  assert(target.layers.at("current-layer").name == "Rig");
  assert(target.layers.at("incoming-layer").name == "Rig (Imported)");
  assert(target.fixtures.at("incoming-fixture").layer == "Rig (Imported)");
}

// Verifies Symdef UUID collisions remap Symbol-derived object references.
static void VerifySymdefUuidCollisionWithDifferentGeometryFilesRemapsSymbol() {
  MvrScene target;
  target.symdefFiles["symdef-a"] = "symbols/current.3ds";
  target.symdefTypes["symdef-a"] = "Mesh";
  target.symdefGeometries["symdef-a"] = {
      SymdefGeometry{"symbols/current.3ds", "Mesh", Matrix{}}};

  MvrScene imported;
  imported.symdefFiles["symdef-a"] = "symbols/incoming.3ds";
  imported.symdefTypes["symdef-a"] = "Mesh";
  imported.symdefGeometries["symdef-a"] = {
      SymdefGeometry{"symbols/incoming.3ds", "Mesh", Matrix{}}};
  Truss truss;
  truss.uuid = "incoming-truss";
  truss.sourceSymdefUuid = "symdef-a";
  imported.trusses[truss.uuid] = truss;

  const mvr::MvrMergeAnalysis analysis =
      mvr::AnalyzeImportedSceneMerge(target, imported);
  const std::string remappedSymdefUuid = analysis.uuidMap.at("symdef-a");
  assert(remappedSymdefUuid != "symdef-a");
  const mvr::MvrSceneMergeResult result =
      mvr::ApplyImportedSceneMerge(target, imported, analysis);

  assert(result.nonObjectLookupConflictsResolved == 1);
  assert(target.symdefFiles.at("symdef-a") == "symbols/current.3ds");
  assert(target.symdefFiles.at(remappedSymdefUuid) == "symbols/incoming.3ds");
  assert(target.trusses.at("incoming-truss").sourceSymdefUuid ==
         remappedSymdefUuid);
}

// Verifies imported resource copying preserves target basePath.
static void VerifyImportedResourcesAreRewrittenIntoTargetBasePath() {
  const std::filesystem::path tempDir = std::filesystem::temp_directory_path() /
                                        "perastage_mvr_merge_resource_test";
  std::filesystem::remove_all(tempDir);
  const std::filesystem::path targetBase = tempDir / "current_project";
  const std::filesystem::path importedBase = tempDir / "incoming_mvr";

  WriteGdtfFile(targetBase / "shared" / "fixture.gdtf", "current fixture");
  WriteGdtfFile(importedBase / "shared" / "fixture.gdtf", "incoming fixture");
  WriteGdtfFile(importedBase / "truss" / "type.gdtf", "incoming truss gdtf");
  WriteGdtfFile(importedBase / "truss" / "symbol.3ds", "incoming symbol");
  WriteGdtfFile(importedBase / "truss" / "model.gtruss",
                "incoming truss model");
  WriteGdtfFile(importedBase / "truss" / "aux.gdtf", "incoming truss aux");
  WriteGdtfFile(importedBase / "objects" / "object.3ds", "incoming object");
  WriteGdtfFile(importedBase / "objects" / "geometry.3ds", "incoming geometry");
  WriteGdtfFile(importedBase / "symbols" / "symdef.3ds", "incoming symdef");
  WriteGdtfFile(importedBase / "symbols" / "symdef_child.3ds",
                "incoming symdef child");

  MvrScene target;
  target.basePath = targetBase.string();
  target.fixtures["current-fixture"] = MakeTypedFixture(
      "current-fixture", "Current", "shared/fixture.gdtf", "Mode A");

  MvrScene imported;
  imported.basePath = importedBase.string();
  imported.fixtures["incoming-fixture"] = MakeTypedFixture(
      "incoming-fixture", "Incoming", "shared/fixture.gdtf", "Mode B");

  Truss truss;
  truss.uuid = "incoming-truss";
  truss.gdtfSpec = "truss/type.gdtf";
  truss.symbolFile = "truss/symbol.3ds";
  truss.modelFile = "truss/model.gtruss";
  truss.perastageAuxGdtfArchivePath = "truss/aux.gdtf";
  imported.trusses[truss.uuid] = truss;

  SceneObject object;
  object.uuid = "incoming-object";
  object.modelFile = "objects/object.3ds";
  object.geometries.push_back(
      GeometryInstance{"objects/geometry.3ds", Matrix{}});
  imported.sceneObjects[object.uuid] = object;

  imported.symdefFiles["incoming-symdef"] = "symbols/symdef.3ds";
  imported.symdefGeometries["incoming-symdef"] = {
      SymdefGeometry{"symbols/symdef_child.3ds", "Mesh", Matrix{}}};

  const mvr::MvrMergeAnalysis analysis =
      mvr::AnalyzeImportedSceneMerge(target, imported);
  const mvr::MvrSceneMergeResult result =
      mvr::ApplyImportedSceneMerge(target, imported, analysis);
  assert(result.fixturesAdded == 1);
  assert(target.basePath == targetBase.string());

  const Fixture &fixture = target.fixtures.at("incoming-fixture");
  assert(fixture.gdtfSpec != "shared/fixture.gdtf");
  assert(ResourceExists(targetBase, fixture.gdtfSpec));
  assert(ResourceExists(targetBase,
                        target.fixtures.at("current-fixture").gdtfSpec));

  const Truss &mergedTruss = target.trusses.at("incoming-truss");
  assert(ResourceExists(targetBase, mergedTruss.gdtfSpec));
  assert(ResourceExists(targetBase, mergedTruss.symbolFile));
  assert(ResourceExists(targetBase, mergedTruss.modelFile));
  assert(ResourceExists(targetBase, mergedTruss.perastageAuxGdtfArchivePath));

  const SceneObject &mergedObject = target.sceneObjects.at("incoming-object");
  assert(ResourceExists(targetBase, mergedObject.modelFile));
  assert(ResourceExists(targetBase, mergedObject.geometries.front().modelFile));
  const std::string mergedSymdefUuid =
      mvr::RemapImportedUuidReference("incoming-symdef", analysis);
  assert(ResourceExists(targetBase, target.symdefFiles.at(mergedSymdefUuid)));
  assert(ResourceExists(
      targetBase, target.symdefGeometries.at(mergedSymdefUuid).front().file));

  const std::filesystem::path reloadedBase = tempDir / "reloaded_project";
  std::filesystem::copy(targetBase, reloadedBase,
                        std::filesystem::copy_options::recursive);
  assert(ResourceExists(reloadedBase,
                        target.fixtures.at("current-fixture").gdtfSpec));
  assert(ResourceExists(reloadedBase, fixture.gdtfSpec));
  assert(ResourceExists(reloadedBase, mergedTruss.symbolFile));
  assert(
      ResourceExists(reloadedBase, mergedObject.geometries.front().modelFile));
}

// Verifies merge analysis resolves collisions before applying imported data.
int main() {
  VerifyFixtureUuidCollisionUsesStableReplacement();
  VerifyGroupChildRemapping();
  VerifySupportPositionReferenceRemapping();
  VerifyFixtureTypeGdtfConflictBlocksAutomaticMerge();
  VerifyUseCurrentDefinitionDecisionAppliesToIncomingFixtures();
  VerifyRenameIncomingDefinitionDecisionAppliesToIncomingFixtures();
  VerifyEmptyLayerUuidReceivesGeneratedUuid();
  VerifySameLayerNameWithDifferentUuidRenamesIncomingLayer();
  VerifySymdefUuidCollisionWithDifferentGeometryFilesRemapsSymbol();
  VerifyImportedResourcesAreRewrittenIntoTargetBasePath();
  return 0;
}

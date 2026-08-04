#include "project_fixture_gdtf_consolidator.h"

#include "gdtf_test_fixture_builder.h"
#include "mvrscene.h"
#include "symbol_cache_manifest.h"

#include <cassert>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// Creates a fixture instance with one deterministic project GDTF reference.
static Fixture BuildFixture(const std::string &uuid, const std::string &spec) {
  Fixture fixture;
  fixture.uuid = uuid;
  fixture.gdtfSpec = spec;
  fixture.gdtfMode = "Shapes";
  fixture.typeName = "Aleda B-EYE K10";
  return fixture;
}

// Verifies derived symbol differences consolidate while authoritative edits remain distinct.
static void VerifyFingerprintAndConsolidation() {
  const fs::path root = fs::temp_directory_path() /
                        "perastage-project-gdtf-consolidator-test";
  fs::remove_all(root);
  fs::create_directories(root);
  const std::string baseName = "Aleda@Perastage.gdtf";
  const std::string duplicateName = "Aleda@Perastage_2.gdtf";
  const std::string editedName = "Aleda@Perastage_3.gdtf";

  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("Aleda B-EYE K10", "Clay Paky",
                           "BF9967F2-4FC4-4BC7-9C1B-2CA2A15EF507")
      .WithDmxMode("Shapes", "Root")
      .WithModelResource("main")
      .WithArchiveEntry("models/main.3ds", "authoritative-model")
      .WriteArchive(root / baseName);
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("Aleda B-EYE K10", "Clay Paky",
                           "BF9967F2-4FC4-4BC7-9C1B-2CA2A15EF507")
      .WithDmxMode("Shapes", "Root")
      .WithModelResource("main")
      .WithPerastageGeneratedSymbols()
      .WithArchiveEntry("models/main.3ds", "authoritative-model")
      .WriteArchive(root / duplicateName);
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("Aleda B-EYE K10", "Clay Paky",
                           "BF9967F2-4FC4-4BC7-9C1B-2CA2A15EF507")
      .WithDmxMode("Shapes", "Root")
      .WithModelResource("main")
      .WithArchiveEntry("models/main.3ds", "user-edited-model")
      .WriteArchive(root / editedName);

  std::string error;
  const std::string baseFingerprint =
      project_gdtf::ComputeBaseGdtfFingerprint((root / baseName).string(), error);
  const std::string duplicateFingerprint =
      project_gdtf::ComputeBaseGdtfFingerprint((root / duplicateName).string(), error);
  const std::string editedFingerprint =
      project_gdtf::ComputeBaseGdtfFingerprint((root / editedName).string(), error);
  assert(!baseFingerprint.empty());
  assert(baseFingerprint == duplicateFingerprint);
  assert(baseFingerprint != editedFingerprint);
  const std::string strictBase =
      symbol_cache::ComputeGdtfSemanticFingerprint((root / baseName).string(),
                                                   error);
  const std::string strictDuplicate =
      symbol_cache::ComputeGdtfSemanticFingerprint(
          (root / duplicateName).string(), error);
  assert(!strictBase.empty());
  assert(strictBase != strictDuplicate);

  MvrScene scene;
  scene.basePath = root.string();
  for (int index = 0; index < 16; ++index) {
    const std::string uuid = "base-" + std::to_string(index);
    scene.fixtures.emplace(uuid, BuildFixture(uuid, baseName));
  }
  for (int index = 0; index < 4; ++index) {
    const std::string uuid = "duplicate-" + std::to_string(index);
    scene.fixtures.emplace(uuid, BuildFixture(uuid, duplicateName));
  }
  symbol_cache::SymbolCacheManifest manifest;
  const project_gdtf::ConsolidationPlan plan =
      project_gdtf::BuildConsolidationPlan(scene, manifest);
  assert(plan.groups.size() == 1);
  assert(plan.groups.front().survivorGdtfSpec == baseName);
  assert(plan.groups.front().rebindings.size() == 4);

  std::string applyError;
  MvrScene staleScene = scene;
  staleScene.fixtures.at("duplicate-0").gdtfSpec = editedName;
  assert(!project_gdtf::ApplyConsolidationPlan(staleScene, plan, applyError));
  assert(staleScene.fixtures.at("duplicate-1").gdtfSpec == duplicateName);
  assert(project_gdtf::ApplyConsolidationPlan(scene, plan, applyError));
  for (const auto &[uuid, fixture] : scene.fixtures) {
    (void)uuid;
    assert(fixture.gdtfSpec == baseName);
  }

  MvrScene differentModes;
  differentModes.basePath = root.string();
  differentModes.fixtures.emplace("shapes",
      BuildFixture("shapes", baseName));
  Fixture standard = BuildFixture("standard", duplicateName);
  standard.gdtfMode = "Standard";
  differentModes.fixtures.emplace("standard", standard);
  assert(project_gdtf::BuildConsolidationPlan(differentModes, manifest)
             .groups.empty());
  fs::remove_all(root);
}

// Runs GUI-independent project GDTF consolidation coverage.
int main() {
  VerifyFingerprintAndConsolidation();
  return 0;
}

#include "project_fixture_gdtf_consolidator.h"

#include "gdtf_test_fixture_builder.h"
#include "mvrscene.h"
#include "symbols/fixture_symbol_resource_contract.h"

#include <array>
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
      .WithPerastageGeneratedSymbols()
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
  const project_gdtf::ConsolidationPlan plan =
      project_gdtf::BuildConsolidationPlan(scene);
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
  assert(project_gdtf::BuildConsolidationPlan(differentModes)
             .groups.empty());
  fs::remove_all(root);
}

// Keeps authored standard SVGs authoritative, regardless of conventional names.
static void VerifyResourceOwnershipFingerprinting() {
  const fs::path root = fs::temp_directory_path() /
                        "perastage-project-gdtf-resource-ownership-test";
  fs::remove_all(root);
  fs::create_directories(root);
  const std::string authoredSvg =
      "<svg viewBox=\"0 0 10 10\"><polygon points=\"0,0 10,0 10,10\"/></svg>";
  const std::string changedSvg =
      "<svg viewBox=\"0 0 10 10\"><polygon points=\"0,0 8,0 8,10\"/></svg>";
  const std::string generatedSvg =
      "<svg data-perastage-symbol-version=\"1\" viewBox=\"0 0 10 10\">"
      "<polygon points=\"0,0 10,0 10,10\"/></svg>";
  const std::string changedGeneratedSvg =
      "<svg data-perastage-symbol-version=\"1\" viewBox=\"0 0 10 10\">"
      "<polygon points=\"0,0 8,0 8,10\"/></svg>";
  const std::array<std::string, 3> standardPaths = {
      "models/svg/base.svg", "models/svg_side/base.svg",
      "models/svg_front/base.svg"};
  const std::array<std::string, 4> internalPaths = {
      "perastage/symbols/base/top.svg", "perastage/symbols/base/side.svg",
      "perastage/symbols/base/front.svg", "perastage/symbols/base/bottom.svg"};
  const auto writeAuthored = [&](const fs::path &path, int changedView,
                                 bool legacyBottom, int internalVariant = 0) {
    auto builder = tests::gdtf::BuildMinimalValidFixture();
    builder.WithModelResource("base");
    for (std::size_t index = 0; index < standardPaths.size(); ++index)
      builder.WithArchiveEntry(standardPaths[index],
                               static_cast<int>(index) == changedView
                                   ? changedSvg : authoredSvg);
    if (legacyBottom)
      builder.WithArchiveEntry("models/svg/base_bottom.svg", authoredSvg);
    if (internalVariant) {
      for (const std::string &internalPath : internalPaths)
        builder.WithArchiveEntry(internalPath, internalVariant == 1
                                                  ? generatedSvg
                                                  : changedGeneratedSvg);
    }
    builder.WriteArchive(path);
  };
  const fs::path authored = root / "Authored@Perastage.gdtf";
  const fs::path authoredBottom = root / "AuthoredBottom@Perastage.gdtf";
  const fs::path authoredWithInternal = root / "AuthoredInternal.gdtf";
  const fs::path authoredChangedInternal = root / "AuthoredChangedInternal.gdtf";
  writeAuthored(authored, -1, false);
  writeAuthored(authoredBottom, -1, true);
  writeAuthored(authoredWithInternal, -1, false, 1);
  writeAuthored(authoredChangedInternal, -1, false, 2);
  std::string error;
  const std::string authoredFingerprint =
      project_gdtf::ComputeBaseGdtfFingerprint(authored.string(), error);
  assert(!authoredFingerprint.empty());
  assert(authoredFingerprint == project_gdtf::ComputeBaseGdtfFingerprint(
                                    authoredBottom.string(), error));
  assert(authoredFingerprint == project_gdtf::ComputeBaseGdtfFingerprint(
                                    authoredWithInternal.string(), error));
  assert(authoredFingerprint == project_gdtf::ComputeBaseGdtfFingerprint(
                                    authoredChangedInternal.string(), error));
  MvrScene authoredVariants;
  authoredVariants.basePath = root.string();
  authoredVariants.fixtures.emplace(
      "original", BuildFixture("original", authored.filename().string()));
  for (std::size_t index = 0; index < standardPaths.size(); ++index) {
    const fs::path variant = root / ("Changed@Perastage_" +
                                    std::to_string(index) + ".gdtf");
    writeAuthored(variant, static_cast<int>(index), false, 1);
    assert(authoredFingerprint != project_gdtf::ComputeBaseGdtfFingerprint(
                                      variant.string(), error));
    const std::string uuid = "variant-" + std::to_string(index);
    authoredVariants.fixtures.emplace(
        uuid, BuildFixture(uuid, variant.filename().string()));
  }
  assert(project_gdtf::BuildConsolidationPlan(authoredVariants).groups.empty());

  const fs::path noSymbols = root / "NoSymbols.gdtf";
  const fs::path generated = root / "Generated.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithModelResource("base")
      .WriteArchive(noSymbols);
  auto generatedBuilder = tests::gdtf::BuildMinimalValidFixture();
  generatedBuilder.WithModelResource("base");
  for (const std::string &path : internalPaths)
    generatedBuilder.WithArchiveEntry(path, generatedSvg);
  generatedBuilder.WriteArchive(generated);
  assert(project_gdtf::ComputeBaseGdtfFingerprint(noSymbols.string(), error) ==
         project_gdtf::ComputeBaseGdtfFingerprint(generated.string(), error));

  // New internal resources must not hide legacy alternatives from the digest.
  const fs::path legacy = root / "Legacy.gdtf";
  const fs::path legacyAndInternal = root / "LegacyAndInternal.gdtf";
  auto legacyBuilder = tests::gdtf::BuildMinimalValidFixture();
  legacyBuilder.WithModelResource("base");
  for (const std::string &path : standardPaths)
    legacyBuilder.WithArchiveEntry(path, generatedSvg);
  legacyBuilder.WithArchiveEntry("models/svg/base_bottom.svg", generatedSvg);
  legacyBuilder.WriteArchive(legacy);
  for (const std::string &path : internalPaths)
    legacyBuilder.WithArchiveEntry(path, changedGeneratedSvg);
  legacyBuilder.WithArchiveEntry("models/svg_bottom/base.svg", generatedSvg);
  legacyBuilder.WriteArchive(legacyAndInternal);
  FixtureSymbolResourceInspection legacyResources;
  assert(InspectFixtureSymbolResources(legacyAndInternal.string(), legacyResources));
  assert(legacyResources.perastageViewsUsable);
  assert(!legacyResources.standardViewsUsable);
  assert(legacyResources.perastageResources.size() == 9);
  assert(project_gdtf::ComputeBaseGdtfFingerprint(legacy.string(), error) ==
         project_gdtf::ComputeBaseGdtfFingerprint(legacyAndInternal.string(), error));

  // Generated standard resources remain significant despite their provenance.
  const fs::path generatedStandard = root / "GeneratedStandard.gdtf";
  const fs::path changedGeneratedStandard = root / "ChangedGeneratedStandard.gdtf";
  const auto writeGeneratedStandard = [&](const fs::path &path, bool changed) {
    auto builder = tests::gdtf::BuildMinimalValidFixture();
    builder.WithModelResource("base");
    for (std::size_t index = 0; index < standardPaths.size(); ++index) {
      std::string svg = changed && index == 0 ? changedGeneratedSvg : generatedSvg;
      svg.insert(4, " data-perastage-resource-set=\"standard-gdtf\"");
      builder.WithArchiveEntry(standardPaths[index], svg);
    }
    for (const std::string &internalPath : internalPaths)
      builder.WithArchiveEntry(internalPath, generatedSvg);
    builder.WriteArchive(path);
  };
  writeGeneratedStandard(generatedStandard, false);
  writeGeneratedStandard(changedGeneratedStandard, true);
  const auto standardFingerprint = project_gdtf::ComputeBaseGdtfFingerprint(
      generatedStandard.string(), error);
  assert(!standardFingerprint.empty());
  assert(standardFingerprint != project_gdtf::ComputeBaseGdtfFingerprint(
                                    noSymbols.string(), error));
  assert(standardFingerprint != project_gdtf::ComputeBaseGdtfFingerprint(
                                    changedGeneratedStandard.string(), error));
  fs::remove_all(root);
}

// Runs GUI-independent project GDTF consolidation coverage.
int main() {
  VerifyFingerprintAndConsolidation();
  VerifyResourceOwnershipFingerprinting();
  return 0;
}

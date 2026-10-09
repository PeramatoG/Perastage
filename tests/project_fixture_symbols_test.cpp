#include "configservices.h"
#include "symbols/project_fixture_symbols.h"
#include "symbols/project_fixture_symbol_migration.h"
#include "file_import_utils.h"
#include "support/gdtf_test_fixture_builder.h"
#include "wx_path_utils.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

namespace {
constexpr char kFirst[] = "11223344-5566-7788-99aa-bbccddeeff00";
constexpr char kSecond[] = "11223344-5566-7788-99aa-bbccddeeff01";

Fixture MakeFixture(const std::string &uuid) {
  Fixture fixture;
  fixture.uuid = uuid;
  fixture.gdtfSpec = "/nonexistent/original.gdtf";
  return fixture;
}

symbols::ProjectFixtureSymbolBundle Bundle(symbols::ProjectFixtureSymbolKind kind) {
  symbols::ProjectFixtureSymbolBundle bundle;
  bundle.generatorVersion = "fixture-vectors-test-v7";
  bundle.kind = kind;
  bundle.sourceFixtureTypeId = "22334455-6677-8899-aabb-ccddeeff0011";
  bundle.sourceFingerprint = "semantic-test-source-v3";
  for (size_t i = 0; i < bundle.views.size(); ++i) {
    bundle.views[i].svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 12 8\">"
                          "<path fill-rule=\"evenodd\" d=\"M0 0L12 0L12 8Z M2 2L3 2L3 3Z\"/>"
                          "<!-- exact view " + std::to_string(i) + " -->\n</svg>";
    bundle.views[i].offsetXmm = -3.25 - i;
    bundle.views[i].offsetYmm = 4.125 + i;
  }
  return bundle;
}

std::map<std::string, std::vector<std::uint8_t>> ArchiveEntries(
    const std::filesystem::path &path) {
  wxFileInputStream file(WxPathUtils::WxStringFromFilesystemPath(path));
  wxZipInputStream archive(file);
  std::map<std::string, std::vector<std::uint8_t>> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(archive.GetNextEntry())), entry) {
    std::vector<std::uint8_t> bytes;
    char buffer[4096];
    while (true) {
      archive.Read(buffer, sizeof(buffer));
      const size_t count = archive.LastRead();
      if (count == 0)
        break;
      bytes.insert(bytes.end(), buffer, buffer + count);
    }
    assert(entries.emplace(entry->GetName(wxPATH_UNIX).ToStdString(), std::move(bytes)).second);
  }
  return entries;
}
} // namespace

int main() {
  using symbols::ProjectFixtureSymbolKind;
  using symbols::ProjectFixtureSymbolStore;
  const auto directory = std::filesystem::temp_directory_path() /
                         "perastage_project_fixture_symbols_test";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  std::filesystem::create_directories(directory);
  const std::vector<std::uint8_t> sceneBytes = {'P', 'K', 's', 'c', 'e', 'n', 'e'};
  const auto saveConfig = [](std::vector<std::uint8_t> &bytes) {
    bytes = {'{', '}'};
    return true;
  };
  const auto saveScene = [&sceneBytes](std::vector<std::uint8_t> &bytes) {
    bytes = sceneBytes;
    return true;
  };

  for (auto kind : {ProjectFixtureSymbolKind::GeneratedFallback,
                    ProjectFixtureSymbolKind::UserOverride}) {
    ProjectSession session;
    auto &scene = session.GetScene();
    scene.fixtures[kFirst] = MakeFixture(kFirst);
    scene.fixtures[kSecond] = MakeFixture(kSecond);
    assert(scene.basePath.empty());
    std::string error;
    const auto bundle = Bundle(kind);
    auto &store = session.GetFixtureSymbols();
    assert(store.ApplyToFixtures(scene, {kFirst}, bundle, error));
    assert(store.ApplyToFixtures(scene, {kSecond}, bundle, error));
    assert(store.BundleCount() == 1);
    assert(store.FindForFixture(scene.fixtures.at(kFirst)));
    const auto contentId = store.ContentIdForDefinition(
        scene.fixtures.at(kFirst).projectSymbolDefinitionId);
    assert(contentId.size() == 71);

    std::vector<ProjectArchiveResource> resources;
    assert(store.CollectArchiveResources(scene, resources, error));
    assert(resources.size() == 6); // One index, one shared manifest, four SVGs.
    for (const auto &resource : resources)
      assert(resource.required);
    std::vector<ProjectArchiveResource> repeat;
    assert(store.CollectArchiveResources(scene, repeat, error));
    assert(repeat.size() == resources.size());
    for (size_t i = 0; i < repeat.size(); ++i) {
      assert(repeat[i].entryName == resources[i].entryName);
      assert(repeat[i].bytes == resources[i].bytes);
    }

    const auto path = directory / (kind == ProjectFixtureSymbolKind::UserOverride
                                      ? "override.pstg" : "generated.pstg");
    const std::vector<std::uint8_t> layoutBytes = {5, 8, 13};
    assert(session.SaveProject(path.string(), saveConfig, saveScene,
        ProjectSession::CollectArchiveResourcesFn([&layoutBytes] {
          return std::vector<ProjectArchiveResource>{
              {"resources/layout_images/retained.rgba", layoutBytes}};
        })));
    const auto archived = ArchiveEntries(path);
    assert(archived.size() == 9);
    assert(archived.at("scene.mvr") == sceneBytes);
    assert(archived.contains("resources/fixture_symbols/index.json"));
    for (const auto &[name, bytes] : archived) {
      (void)bytes;
      assert(!name.starts_with("perastage/"));
    }

    ProjectSession reopened;
    std::string extractedResourceRoot;
    assert(reopened.LoadProject(
        path.string(), [&extractedResourceRoot](const ProjectSession::ProjectConfigPayload &payload) {
          extractedResourceRoot = payload.resourceRoot;
          return true;
        },
        [&reopened, &sceneBytes](const ProjectSession::ProjectScenePayload &payload) {
          assert(payload.bytes == sceneBytes);
          reopened.GetScene().fixtures[kFirst] = MakeFixture(kFirst);
          reopened.GetScene().fixtures[kSecond] = MakeFixture(kSecond);
          return true;
        }));
    assert(reopened.GetFixtureSymbols().BundleCount() == 1);
    const auto retainedResource = std::filesystem::path(extractedResourceRoot) /
                                  "resources/layout_images/retained.rgba";
    assert(std::filesystem::is_regular_file(retainedResource));
    const auto previousValidation = reopened.GetLoadedCacheValidationContext();
    const auto previousResourceCount = reopened.GetLoadedArchiveResources().size();
    std::vector<project_cache::NamedPayloadFingerprint> saveFingerprints = {{
        "resources/layout_images/retained.rgba",
        project_cache::FingerprintBytes(layoutBytes.data(), layoutBytes.size())}};
    assert(store.AppendReferencedResourceFingerprints(scene, saveFingerprints, error));
    assert(saveFingerprints.size() == resources.size() + 1);
    assert(previousValidation.HasCompletePackageCoverage());
    assert(previousValidation.packagedLayoutResourceFingerprint ==
           project_cache::AggregateNamedPayloadFingerprints(saveFingerprints));
    for (const auto &[uuid, fixture] : reopened.GetScene().fixtures) {
      (void)uuid;
      const auto *restored = reopened.GetFixtureSymbols().FindForFixture(fixture);
      assert(restored && *restored == bundle);
    }

    // Orphan bytes in an old/imported package are not rendering dependencies.
    const auto orphanPath = directory / "with_orphan.pstg";
    assert(session.SaveProject(orphanPath.string(), saveConfig, saveScene,
        ProjectSession::CollectArchiveResourcesFn([&layoutBytes] {
          return std::vector<ProjectArchiveResource>{
              {"resources/layout_images/retained.rgba", layoutBytes},
              {"resources/fixture_symbols/sha256-" + std::string(64, '0') + "/top.svg",
               {'u', 'n', 'u', 's', 'e', 'd'}}};
        })));
    ProjectSession orphanReopened;
    assert(orphanReopened.LoadProject(orphanPath.string(),
        [](const ProjectSession::ProjectConfigPayload &) { return true; },
        [&orphanReopened](const ProjectSession::ProjectScenePayload &) {
          orphanReopened.GetScene().fixtures[kFirst] = MakeFixture(kFirst);
          orphanReopened.GetScene().fixtures[kSecond] = MakeFixture(kSecond);
          return true;
        }));
    assert(orphanReopened.GetLoadedCacheValidationContext()
               .packagedLayoutResourceFingerprint ==
           previousValidation.packagedLayoutResourceFingerprint);

    // Corrupt authoritative data must fail before scene/config callbacks, retaining
    // the previous project symbol instead of treating it as a disposable cache.
    auto corrupt = resources;
    corrupt.back().bytes.push_back('!');
    ProjectSession corruptWriter;
    const auto corruptPath = directory / "corrupt.pstg";
    assert(corruptWriter.SaveProject(
        corruptPath.string(), saveConfig, saveScene,
        ProjectSession::CollectArchiveResourcesFn([corrupt] { return corrupt; })));
    bool callbackCalled = false;
    assert(!reopened.LoadProject(
        corruptPath.string(),
        [&callbackCalled](const ProjectSession::ProjectConfigPayload &) {
          callbackCalled = true; return true;
        },
        [&callbackCalled](const ProjectSession::ProjectScenePayload &) {
          callbackCalled = true; return true;
        }));
    assert(!callbackCalled);
    assert(std::filesystem::is_regular_file(retainedResource));
    {
      std::ifstream file(retainedResource, std::ios::binary);
      const std::vector<std::uint8_t> retained{
          std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
      assert(retained == layoutBytes);
    }
    assert(reopened.GetLoadedArchiveResources().size() == previousResourceCount);
    assert(reopened.GetLoadedCacheValidationContext().scenePackageFingerprint ==
           previousValidation.scenePackageFingerprint);
    assert(*reopened.GetFixtureSymbols().FindForFixture(
               reopened.GetScene().fixtures.at(kFirst)) == bundle);

    // Even a callback failure must preserve the previous scene and its assets.
    assert(!reopened.LoadProject(path.string(),
        [](const ProjectSession::ProjectConfigPayload &) { return true; },
        [&reopened](const ProjectSession::ProjectScenePayload &) {
          reopened.GetScene().fixtures.clear();
          return false;
        }));
    assert(std::filesystem::is_regular_file(retainedResource));
    assert(*reopened.GetFixtureSymbols().FindForFixture(
               reopened.GetScene().fixtures.at(kFirst)) == bundle);

    ProjectFixtureSymbolStore validator;
    assert(!validator.LoadArchiveResources(corrupt, error));
    auto missing = resources;
    missing.pop_back();
    assert(!validator.LoadArchiveResources(missing, error));
    auto duplicate = resources;
    duplicate.push_back(resources.front());
    assert(!validator.LoadArchiveResources(duplicate, error));

    // Replacement/deletion cannot accumulate unused payloads; undo restores the
    // earlier immutable bundle as well as the explicit scene definition binding.
    HistoryManager history;
    SelectionState selection;
    history.PushUndoState(scene, selection, "Replace symbol", {}, nullptr, {}, &store);
    auto replacement = bundle;
    replacement.views.front().offsetXmm += 2.0;
    assert(store.ApplyToFixtures(scene, {kFirst, kSecond}, replacement, error));
    assert(store.BundleCount() == 1);
    const auto changedPath = directory / "changed_symbol.pstg";
    assert(session.SaveProject(changedPath.string(), saveConfig, saveScene,
        ProjectSession::CollectArchiveResourcesFn([&layoutBytes] {
          return std::vector<ProjectArchiveResource>{
              {"resources/layout_images/retained.rgba", layoutBytes}};
        })));
    ProjectSession changedReopened;
    assert(changedReopened.LoadProject(changedPath.string(),
        [](const ProjectSession::ProjectConfigPayload &) { return true; },
        [&changedReopened](const ProjectSession::ProjectScenePayload &) {
          changedReopened.GetScene().fixtures[kFirst] = MakeFixture(kFirst);
          changedReopened.GetScene().fixtures[kSecond] = MakeFixture(kSecond);
          return true;
        }));
    const auto &changedValidation = changedReopened.GetLoadedCacheValidationContext();
    assert(changedValidation.scenePackageFingerprint ==
           previousValidation.scenePackageFingerprint);
    assert(changedValidation.packagedLayoutResourceFingerprint !=
           previousValidation.packagedLayoutResourceFingerprint);
    saveFingerprints = {{"resources/layout_images/retained.rgba",
        project_cache::FingerprintBytes(layoutBytes.data(), layoutBytes.size())}};
    assert(store.AppendReferencedResourceFingerprints(scene, saveFingerprints, error));
    assert(changedValidation.packagedLayoutResourceFingerprint ==
           project_cache::AggregateNamedPayloadFingerprints(saveFingerprints));
    assert(history.Undo(scene, selection, nullptr, nullptr, nullptr, &store) ==
           "Replace symbol");
    assert(*store.FindForFixture(scene.fixtures.at(kFirst)) == bundle);
    assert(history.Redo(scene, selection, nullptr, nullptr, nullptr, &store) ==
           "Replace symbol");
    assert(*store.FindForFixture(scene.fixtures.at(kFirst)) == replacement);
    scene.fixtures.clear();
    std::vector<ProjectArchiveResource> unreferenced;
    assert(store.CollectArchiveResources(scene, unreferenced, error));
    assert(unreferenced.empty());
    store.PruneUnreferenced(scene);
    assert(store.BundleCount() == 0);
  }

  // Positively identified legacy inputs can preserve exactly the available views.
  MvrScene scene;
  scene.fixtures[kFirst] = MakeFixture(kFirst);
  ProjectFixtureSymbolStore store;
  auto partial = Bundle(ProjectFixtureSymbolKind::GeneratedFallback);
  partial.generatorVersion = "legacy-read-v1";
  for (size_t i = 1; i < partial.views.size(); ++i) {
    partial.views[i].svg.clear();
    partial.views[i].offsetXmm = 0.0;
    partial.views[i].offsetYmm = 0.0;
  }
  std::string error;
  assert(store.ApplyToFixtures(scene, {kFirst}, partial, error));
  std::vector<ProjectArchiveResource> partialResources;
  assert(store.CollectArchiveResources(scene, partialResources, error));
  assert(partialResources.size() == 3);
  ProjectFixtureSymbolStore restored;
  assert(restored.LoadArchiveResources(partialResources, error));
  assert(restored.RestoreFixtureBindings(scene, error));
  assert(*restored.FindForFixture(scene.fixtures.at(kFirst)) == partial);
  assert(!partial.FindView(SymbolViewKind::Bottom));
  auto invalid = partial;
  invalid.views.front().offsetXmm = std::numeric_limits<double>::quiet_NaN();
  assert(!restored.ApplyToFixtures(scene, {kFirst}, invalid, error));
  assert(*restored.FindForFixture(scene.fixtures.at(kFirst)) == partial);

  Fixture copied = scene.fixtures.at(kFirst);
  copied.uuid = kSecond;
  scene.fixtures[kSecond] = copied;
  const auto copiedDefinition = copied.projectSymbolDefinitionId;
  auto changedPartial = partial;
  changedPartial.views.front().svg += "\n";
  assert(restored.ApplyToFixtures(scene, {kFirst}, changedPartial, error));
  assert(scene.fixtures.at(kSecond).projectSymbolDefinitionId == copiedDefinition);
  assert(*restored.FindForFixture(scene.fixtures.at(kSecond)) == partial);
  assert(*restored.FindForFixture(scene.fixtures.at(kFirst)) == changedPartial);
  assert(restored.BundleCount() == 2);

  const auto legacyPath = directory / "Legacy.gdtf";
  const std::string legacySvg =
      "<svg viewBox=\"0 0 12 8\" data-perastage-offset-x-mm=\"7.125\" "
      "data-perastage-offset-y-mm=\"-2.5\"><polygon points=\"0,0 12,0 12,8\"/></svg>";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("main")
      .WithArchiveEntry("perastage/symbols/main/top.svg", legacySvg)
      .WriteArchive(legacyPath);
  const auto originalDigest = FileImportUtils::ComputeFileSha256(legacyPath);
  MvrScene legacyScene;
  legacyScene.fixtures[kFirst] = MakeFixture(kFirst);
  legacyScene.fixtures.at(kFirst).gdtfSpec = legacyPath.string();
  ProjectFixtureSymbolStore legacyStore;
  assert(symbols::MaterializeLegacyFixtureSymbols(legacyScene, legacyStore, "", error));
  const auto *migrated = legacyStore.FindForFixture(legacyScene.fixtures.at(kFirst));
  assert(migrated && migrated->FindView(SymbolViewKind::Top)->svg == legacySvg);
  assert(migrated->FindView(SymbolViewKind::Top)->offsetXmm == 7.125);
  assert(migrated->FindView(SymbolViewKind::Top)->offsetYmm == -2.5);
  assert(!migrated->FindView(SymbolViewKind::Bottom));
  assert(FileImportUtils::ComputeFileSha256(legacyPath) == originalDigest);

  const auto authoredPath = directory / "Authored.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("main")
      .WithArchiveEntry("models/svg/main.svg",
                        "<svg viewBox=\"0 0 12 8\"><polygon points=\"0,0 12,0 12,8\"/></svg>")
      .WriteArchive(authoredPath);
  const auto authoredDigest = FileImportUtils::ComputeFileSha256(authoredPath);
  legacyScene.fixtures.at(kFirst).gdtfSpec = authoredPath.string();
  legacyScene.fixtures.at(kFirst).projectSymbolDefinitionId.clear();
  legacyStore.Clear();
  assert(symbols::MaterializeLegacyFixtureSymbols(legacyScene, legacyStore, "", error));
  assert(legacyStore.BundleCount() == 0);
  assert(FileImportUtils::ComputeFileSha256(authoredPath) == authoredDigest);
  std::filesystem::remove_all(directory, ec);
}

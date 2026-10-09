/*
 * This file is part of Perastage.
 */
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <wx/init.h>
#include <wx/mstream.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "support/archive_entry_test_utils.h"
#include "support/gdtf_test_fixture_builder.h"
#include "../core/configmanager.h"
#include "../core/gdtfdictionary.h"
#include "../core/symbols/fixture_symbol_application.h"
#include "../core/symbols/fixture_symbol_availability.h"
#include "../core/symbols/fixture_symbol_resource_contract.h"
#include "../core/symbols/project_fixture_symbols.h"
#include "../core/symbols/standard_gdtf_completion.h"
#include "../core/wx_path_utils.h"
#include "../gui/windows/symbol_fixture_applier.h"

namespace fs = std::filesystem;
namespace {
constexpr const char *kSelected = "10000000-0000-4000-8000-000000000001";
constexpr const char *kShared = "10000000-0000-4000-8000-000000000002";
constexpr const char *kOtherMode = "10000000-0000-4000-8000-000000000003";
constexpr const char *kGenerated = "10000000-0000-4000-8000-000000000004";
constexpr const char *kPlaceholder = "10000000-0000-4000-8000-000000000005";

struct ScopedFixtureDictionary {
  std::string previous = GdtfDictionary::GetActiveDictionaryFilePath();
  bool changed = false;
  ~ScopedFixtureDictionary() {
    if (changed) {
      std::string error;
      assert(GdtfDictionary::SetActiveDictionaryFilePath(previous, &error));
    }
  }
  void Activate(const fs::path &path) {
    std::string error;
    assert(GdtfDictionary::CreateEmptyDictionaryFile(path.string(), &error));
    assert(GdtfDictionary::SetActiveDictionaryFilePath(path.string(), &error));
    changed = true;
  }
};

struct TemporaryProject {
  fs::path path = fs::temp_directory_path() /
      ("perastage-project-symbol-apply-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
  TemporaryProject() { fs::create_directories(path); }
  ~TemporaryProject() { std::error_code error; fs::remove_all(path, error); }
};

std::string ReadFile(const fs::path &path) {
  std::ifstream stream(path, std::ios::binary);
  assert(stream.is_open());
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::map<std::string, std::string> ReadArchive(wxInputStream &input) {
  std::map<std::string, std::string> entries;
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    const auto normalized = tests::archive::NormalizePresentedArchivePath(
        entry->GetName().ToStdString());
    assert(normalized.ok);
    std::string bytes;
    char buffer[4096];
    do {
      zip.Read(buffer, sizeof(buffer));
      bytes.append(buffer, zip.LastRead());
    } while (zip.LastRead() > 0);
    assert(entries.emplace(normalized.path, std::move(bytes)).second);
  }
  return entries;
}

std::map<std::string, std::string> ReadArchiveFile(const fs::path &path) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(path));
  assert(input.IsOk());
  return ReadArchive(input);
}

std::map<std::string, std::string> ReadArchiveBytes(const std::string &bytes) {
  wxMemoryInputStream input(bytes.data(), bytes.size());
  return ReadArchive(input);
}

Fixture MakeFixture(const char *uuid, const std::string &spec,
                    const std::string &mode = "Default") {
  Fixture fixture;
  fixture.uuid = uuid;
  fixture.typeName = "SymbolApplicationFixture";
  fixture.gdtfSpec = spec;
  fixture.gdtfMode = mode;
  return fixture;
}

std::vector<symbols::Symbol2D> BuildSymbols() {
  std::vector<symbols::Symbol2D> result;
  for (const auto view : {symbols::SymbolView::Top, symbols::SymbolView::Front,
                         symbols::SymbolView::Left, symbols::SymbolView::Bottom}) {
    symbols::Symbol2D symbol;
    symbol.view = view;
    symbol.bounds = {{3.0f, 4.0f}, {103.0f, 54.0f}, true};
    symbols::PolygonWithHoles2D region;
    region.outer = {{3, 4}, {103, 4}, {103, 54}, {3, 54}};
    region.holes.push_back({{20, 20}, {30, 20}, {30, 30}, {20, 30}});
    symbol.fill.push_back(std::move(region));
    symbol.strokes = {{{3, 4}, {103, 54}}};
    result.push_back(std::move(symbol));
  }
  return result;
}

void CheckManualApplyAndIntegratedPersistence(ConfigManager &cfg,
                                             const fs::path &directory) {
  cfg.Reset();
  auto &scene = cfg.GetScene();
  scene.basePath = directory.string();
  const fs::path source = directory / "AuthoredFixture.gdtf";
  const fs::path generatedSource = directory / "GeneratedFixture.gdtf";
  const std::string authoredSvg =
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10 10\">"
      "<polygon points=\"0,0 10,0 10,10 0,10\"/></svg>";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("authored")
      .WithArchiveEntry("models/svg/authored.svg", authoredSvg)
      .WithArchiveEntry("models/svg_side/authored.svg", authoredSvg)
      .WithArchiveEntry("models/svg_front/authored.svg", authoredSvg)
      .WithArchiveEntry("custom/manufacturer.txt", "retained authored resource")
      .WriteArchive(source);
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("generated")
      .WriteArchive(generatedSource);
  const auto sourceBytes = ReadFile(source);
  const auto generatedSourceBytes = ReadFile(generatedSource);
  scene.fixtures[kSelected] = MakeFixture(kSelected, source.filename().string());
  scene.fixtures[kShared] = MakeFixture(kShared, source.filename().string());
  scene.fixtures[kOtherMode] = MakeFixture(kOtherMode, source.filename().string(), "Other mode");
  scene.fixtures[kGenerated] = MakeFixture(kGenerated, generatedSource.filename().string());

  symbol_preview::FixtureSymbolInspectionResult inspected;
  std::string error;
  assert(symbol_preview::InspectFixtureSymbolState(scene.fixtures.at(kSelected),
                                                   scene, inspected, error));
  assert(inspected.hasValidSvgSymbolSet);
  assert(ReadFile(source) == sourceBytes);
  const auto symbols = BuildSymbols();
  const auto applied = symbol_preview::ApplySymbolsToFixtureProjectWithResult(symbols, kSelected);
  assert(applied.success && applied.projectSymbolsUpdated && !applied.unsavedProject);
  assert(cfg.IsDirty());
  assert(scene.fixtures.at(kSelected).gdtfSpec == source.filename().string());
  assert(scene.fixtures.at(kSelected).projectSymbolDefinitionId ==
         scene.fixtures.at(kShared).projectSymbolDefinitionId);
  assert(scene.fixtures.at(kOtherMode).projectSymbolDefinitionId.empty());
  assert(ReadFile(source) == sourceBytes);
  auto &store = cfg.GetProjectFixtureSymbols();
  const auto expectedOverride = *store.FindForFixture(scene.fixtures.at(kSelected));
  assert(expectedOverride.kind == symbols::ProjectFixtureSymbolKind::UserOverride);
  assert(expectedOverride.sourceFixtureTypeId == tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId);
  assert(!expectedOverride.sourceFingerprint.empty());
  for (const auto &view : expectedOverride.views) {
    assert(view.offsetXmm == -3.0 && view.offsetYmm == -4.0);
    assert(view.svg.find("fill-rule=\"evenodd\"") != std::string::npos);
  }
  assert(store.BundleCount() == 1);

  const auto repeated = symbol_preview::ApplySymbolsToFixtureProjectWithResult(symbols, kSelected);
  assert(repeated.success && store.BundleCount() == 1);
  assert(*store.FindForFixture(scene.fixtures.at(kSelected)) == expectedOverride);
  auto changed = symbols;
  changed.front().strokes.front().back().x -= 5;
  const auto changedApply = symbol_preview::ApplySymbolsToFixtureProjectWithResult(changed, kSelected);
  assert(changedApply.success && store.BundleCount() == 1);
  assert(*store.FindForFixture(scene.fixtures.at(kSelected)) != expectedOverride);
  cfg.Undo();
  assert(*store.FindForFixture(scene.fixtures.at(kSelected)) == expectedOverride);
  cfg.Redo();
  assert(*store.FindForFixture(scene.fixtures.at(kSelected)) != expectedOverride);
  assert(symbol_preview::ApplySymbolsToFixtureProjectWithResult(symbols, kSelected).success);

  const auto automatic = symbols::PrepareFixtureSymbols(scene, store, kGenerated,
      generatedSource.string(), symbols, gdtf::MutationPolicy::PreserveImported);
  assert(automatic.success && automatic.projectSymbolsUpdated);
  assert(!automatic.standardGdtfUpdated && !automatic.fixtureReferencesUpdated);
  const auto expectedGenerated = *store.FindForFixture(scene.fixtures.at(kGenerated));
  assert(expectedGenerated.kind == symbols::ProjectFixtureSymbolKind::GeneratedFallback);
  assert(ReadFile(generatedSource) == generatedSourceBytes);
  assert(!symbols::NeedsAutomaticFixtureSymbolPreparation(scene.fixtures.at(kGenerated),
      store, generatedSource.string(), gdtf::MutationPolicy::PreserveImported));
  const auto replacementAttempt = symbols::PrepareFixtureSymbols(scene, store, kSelected,
      source.string(), changed, gdtf::MutationPolicy::PreserveImported);
  assert(replacementAttempt.success && !replacementAttempt.projectSymbolsUpdated);
  assert(*store.FindForFixture(scene.fixtures.at(kSelected)) == expectedOverride);

  const fs::path project = directory / "Symbols.pstg";
  assert(cfg.SaveProject(project.string()));
  const auto projectEntries = ReadArchiveFile(project);
  assert(projectEntries.contains("scene.mvr"));
  size_t manifests = 0;
  for (const auto &[name, bytes] : projectEntries) {
    (void)bytes;
    if (name.starts_with("resources/fixture_symbols/") && name.ends_with("manifest.json"))
      ++manifests;
  }
  assert(manifests == 2);
  const auto sceneEntries = ReadArchiveBytes(projectEntries.at("scene.mvr"));
  for (const auto &[name, bytes] : sceneEntries) {
    assert(!name.starts_with("resources/fixture_symbols/"));
    assert(!name.starts_with("perastage/"));
    if (fs::path(name).extension() == ".gdtf") {
      const auto definitionEntries = ReadArchiveBytes(bytes);
      for (const auto &[resource, content] : definitionEntries) {
        (void)content;
        assert(!resource.starts_with("perastage/symbols/"));
        assert(!resource.starts_with("resources/fixture_symbols/"));
      }
    }
  }
  assert(ReadFile(source) == sourceBytes);
  assert(ReadFile(generatedSource) == generatedSourceBytes);
  fs::remove(source);
  fs::remove(generatedSource);
  cfg.Reset();
  assert(cfg.LoadProject(project.string()));
  const auto &loadedScene = cfg.GetScene();
  assert(*store.FindForFixture(loadedScene.fixtures.at(kSelected)) == expectedOverride);
  assert(*store.FindForFixture(loadedScene.fixtures.at(kGenerated)) == expectedGenerated);
  assert(store.FindForFixture(loadedScene.fixtures.at(kShared)) ==
         store.FindForFixture(loadedScene.fixtures.at(kSelected)));
}

void CheckUnsavedProjectAndFailures(ConfigManager &cfg, const fs::path &directory) {
  cfg.Reset();
  auto &scene = cfg.GetScene();
  assert(scene.basePath.empty());
  scene.fixtures[kSelected] = MakeFixture(kSelected, (directory / "Unavailable.gdtf").string());
  const auto symbols = BuildSymbols();
  const auto applied = symbol_preview::ApplySymbolsToFixtureProjectWithResult(symbols, kSelected);
  assert(applied.success && applied.projectSymbolsUpdated && applied.unsavedProject);
  assert(scene.basePath.empty());
  assert(!scene.fixtures.at(kSelected).projectSymbolDefinitionId.empty());
  const auto retained = *cfg.GetProjectFixtureSymbols().FindForFixture(scene.fixtures.at(kSelected));
  assert(retained.kind == symbols::ProjectFixtureSymbolKind::UserOverride);
  symbol_preview::FixtureSymbolInspectionResult inspection;
  std::string error;
  assert(symbol_preview::InspectFixtureSymbolState(scene.fixtures.at(kSelected), scene,
                                                   inspection, error));
  assert(inspection.hasValidSvgSymbolSet && !inspection.requiresSymbolGeneration);
  auto incomplete = symbols;
  incomplete.pop_back();
  assert(!symbol_preview::ApplySymbolsToFixtureProjectWithResult(incomplete, kSelected).success);
  assert(*cfg.GetProjectFixtureSymbols().FindForFixture(scene.fixtures.at(kSelected)) == retained);
  assert(!symbol_preview::ApplySymbolsToFixtureProjectWithResult(symbols, "missing-fixture").success);
  assert(scene.basePath.empty());

  // Without a source identity, manual intent is scoped to the selected UUID.
  scene.fixtures[kGenerated] = MakeFixture(kGenerated, "");
  scene.fixtures[kOtherMode] = MakeFixture(kOtherMode, "");
  assert(symbol_preview::ApplySymbolsToFixtureProjectWithResult(symbols, kGenerated).success);
  assert(scene.fixtures.at(kOtherMode).projectSymbolDefinitionId.empty());
}

void CheckAutomaticSafeDerivativeOwnership(ConfigManager &cfg, const fs::path &directory) {
  cfg.Reset();
  auto &scene = cfg.GetScene();
  scene.basePath = directory.string();
  const fs::path source = directory / "MissingStandard.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("exact-model-file")
      .WriteArchive(source);
  const auto before = ReadFile(source);
  scene.fixtures[kSelected] = MakeFixture(kSelected, source.filename().string());
  scene.fixtures[kShared] = MakeFixture(kShared, source.filename().string());
  const auto symbols = BuildSymbols();
  const auto completed = symbols::PrepareFixtureSymbols(scene,
      cfg.GetProjectFixtureSymbols(), kSelected, source.string(), symbols,
      gdtf::MutationPolicy::CompleteAndImprove);
  assert(completed.success && completed.projectSymbolsUpdated);
  assert(completed.standardGdtfUpdated && completed.fixtureReferencesUpdated);
  assert(scene.fixtures.at(kSelected).gdtfSpec == scene.fixtures.at(kShared).gdtfSpec);
  assert(scene.fixtures.at(kSelected).gdtfSpec.starts_with("fixtures/"));
  assert(!fs::equivalent(source, completed.publishedGdtfPath));
  assert(ReadFile(source) == before);
  const auto exact = symbols::InspectStandardGdtfViews(completed.publishedGdtfPath);
  assert(exact.success);
  for (const auto &view : exact.views)
    assert(view.state == symbols::StandardGdtfViewState::ExistingUsable);
  const auto publicationBeforeRepeat = ReadFile(completed.publishedGdtfPath);
  const auto repeated = symbols::PrepareFixtureSymbols(scene,
      cfg.GetProjectFixtureSymbols(), kSelected, completed.publishedGdtfPath,
      symbols, gdtf::MutationPolicy::CompleteAndImprove);
  assert(repeated.success && !repeated.standardGdtfUpdated);
  assert(!repeated.projectSymbolsUpdated && !repeated.fixtureReferencesUpdated);
  assert(ReadFile(completed.publishedGdtfPath) == publicationBeforeRepeat);

  cfg.Reset();
  assert(scene.basePath.empty());
  ScopedFixtureDictionary dictionary;
  dictionary.Activate(directory / "completion-dictionary.json");
  const fs::path assets = directory / "completion-dictionary_assets";
  fs::create_directories(assets);
  const fs::path collision = assets /
      GdtfDictionary::BuildPerastageCanonicalGdtfFileName(source.string());
  fs::copy_file(source, collision, fs::copy_options::overwrite_existing);
  const auto collisionBefore = ReadFile(collision);
  scene.fixtures[kSelected] = MakeFixture(kSelected, collision.string());
  scene.fixtures[kShared] = MakeFixture(kShared, collision.string());
  scene.fixtures[kShared].typeName = "Another exact-definition alias";
  const auto unsaved = symbols::PrepareFixtureSymbols(scene,
      cfg.GetProjectFixtureSymbols(), kSelected, collision.string(), symbols,
      gdtf::MutationPolicy::CompleteAndImprove);
  assert(unsaved.success && unsaved.standardGdtfUpdated);
  assert(unsaved.fixtureReferencesUpdated && scene.basePath.empty());
  assert(!fs::equivalent(collision, unsaved.publishedGdtfPath));
  assert(ReadFile(collision) == collisionBefore);
  assert(scene.fixtures.at(kSelected).gdtfSpec == unsaved.publishedGdtfPath);
  assert(scene.fixtures.at(kShared).gdtfSpec == unsaved.publishedGdtfPath);
  const auto library = GdtfDictionary::Get(scene.fixtures.at(kSelected).typeName);
  assert(library && library->path == unsaved.publishedGdtfPath);
  for (const auto &[name, bytes] : ReadArchiveFile(unsaved.publishedGdtfPath)) {
    (void)bytes;
    assert(!name.starts_with("perastage/symbols/"));
  }
  const auto unsavedBytes = ReadFile(unsaved.publishedGdtfPath);
  const auto repeatUnsaved = symbols::PrepareFixtureSymbols(scene,
      cfg.GetProjectFixtureSymbols(), kSelected, unsaved.publishedGdtfPath,
      symbols, gdtf::MutationPolicy::CompleteAndImprove);
  assert(repeatUnsaved.success && !repeatUnsaved.standardGdtfUpdated);
  assert(ReadFile(unsaved.publishedGdtfPath) == unsavedBytes);
  assert(ReadFile(collision) == collisionBefore);

  // A different source with the same canonical name cannot replace either an
  // unrelated destination or the already published derivative in the library.
  const fs::path distinctSource = directory / "AnotherImportedSource.gdtf";
  fs::copy_file(source, distinctSource, fs::copy_options::overwrite_existing);
  const auto distinctBytes = ReadFile(distinctSource);
  cfg.Reset();
  scene.fixtures[kSelected] = MakeFixture(kSelected, distinctSource.string());
  const auto separate = symbols::PrepareFixtureSymbols(scene,
      cfg.GetProjectFixtureSymbols(), kSelected, distinctSource.string(), symbols,
      gdtf::MutationPolicy::CompleteAndImprove);
  assert(separate.success && separate.standardGdtfUpdated);
  assert(separate.publishedGdtfPath != collision.string());
  assert(separate.publishedGdtfPath != unsaved.publishedGdtfPath);
  assert(ReadFile(collision) == collisionBefore);
  assert(ReadFile(unsaved.publishedGdtfPath) == unsavedBytes);
  assert(ReadFile(distinctSource) == distinctBytes);
}

void CheckLegacyReadAndRuntimePlaceholder(ConfigManager &cfg, const fs::path &directory) {
  cfg.Reset();
  auto &scene = cfg.GetScene();
  const fs::path legacy = directory / "LegacyInternal.gdtf";
  const std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" "
      "viewBox=\"0 0 100 50\" data-perastage-symbol-version=\"1\" "
      "data-perastage-offset-x-mm=\"7\" data-perastage-offset-y-mm=\"8\">"
      "<polygon points=\"0,0 100,0 100,50 0,50\"/></svg>";
  auto builder = tests::gdtf::BuildMinimalValidFixture().WithModelResource("legacy");
  for (const auto view : {SymbolViewKind::Top, SymbolViewKind::Front,
                         SymbolViewKind::Left, SymbolViewKind::Bottom})
    builder.WithArchiveEntry(BuildPerastageFixtureSymbolPath("legacy", view), svg);
  builder.WriteArchive(legacy);
  const auto before = ReadFile(legacy);
  scene.fixtures[kSelected] = MakeFixture(kSelected, legacy.string());
  FixtureSymbolResourceInspection resources;
  assert(InspectFixtureSymbolResources(legacy.string(), resources));
  assert(resources.perastageViewsUsable);
  assert(!resources.standardViewsUsable);
  const auto loaded = symbol_cache::LoadUsableFixtureSymbol(legacy.string(), SymbolViewKind::Top);
  assert(loaded && loaded->offsetXmm == 7.0 && loaded->offsetYmm == 8.0);
  assert(ReadFile(legacy) == before);

  // Completing standards cleans the derivative only after the exact legacy
  // project representation has been retained outside the exchange archive.
  scene.basePath = directory.string();
  const auto completed = symbols::PrepareFixtureSymbols(scene,
      cfg.GetProjectFixtureSymbols(), kSelected, legacy.string(), BuildSymbols(),
      gdtf::MutationPolicy::CompleteAndImprove);
  assert(completed.success && completed.projectSymbolsUpdated);
  assert(completed.standardGdtfUpdated && completed.fixtureReferencesUpdated);
  const auto *legacyProject = cfg.GetProjectFixtureSymbols().FindForFixture(scene.fixtures.at(kSelected));
  assert(legacyProject && legacyProject->generatorVersion == "legacy-perastage-import-1");
  for (const auto &view : legacyProject->views) {
    assert(view.svg == svg);
    assert(view.offsetXmm == 7.0 && view.offsetYmm == 8.0);
  }
  for (const auto &[name, bytes] : ReadArchiveFile(completed.publishedGdtfPath)) {
    (void)bytes;
    assert(!name.starts_with("perastage/symbols/"));
  }
  assert(ReadFile(legacy) == before);

  const fs::path noGeometryLegacy = directory / "LegacyNoGeometry.gdtf";
  fs::copy_file(legacy, noGeometryLegacy, fs::copy_options::overwrite_existing);
  scene.fixtures[kGenerated] = MakeFixture(kGenerated, noGeometryLegacy.string());
  const auto noGeometry = symbols::ApplyRuntimeFixtureSymbolPlaceholder(scene,
      cfg.GetProjectFixtureSymbols(), kGenerated, noGeometryLegacy.string());
  assert(noGeometry.success && noGeometry.projectSymbolsUpdated);
  const auto *retainedLegacy = cfg.GetProjectFixtureSymbols().FindForFixture(scene.fixtures.at(kGenerated));
  assert(retainedLegacy && retainedLegacy->generatorVersion == "legacy-perastage-import-1");
  assert(retainedLegacy->FindView(SymbolViewKind::Top)->svg == svg);
  assert(ReadFile(noGeometryLegacy) == before);

  const fs::path authoredTop = directory / "UsableAuthoredTop.gdtf";
  const std::string authored = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10 10\">"
      "<polygon points=\"0,0 10,0 10,10\"/></svg>";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("authored")
      .WithArchiveEntry("models/svg/authored.svg", authored).WriteArchive(authoredTop);
  scene.fixtures[kOtherMode] = MakeFixture(kOtherMode, authoredTop.string());
  const auto authoredBytes = ReadFile(authoredTop);
  const auto keepAuthored = symbols::ApplyRuntimeFixtureSymbolPlaceholder(scene,
      cfg.GetProjectFixtureSymbols(), kOtherMode, authoredTop.string());
  assert(keepAuthored.success && !keepAuthored.projectSymbolsUpdated);
  assert(!cfg.GetProjectFixtureSymbols().FindForFixture(scene.fixtures.at(kOtherMode)));
  assert(ReadFile(authoredTop) == authoredBytes);

  scene.fixtures[kPlaceholder] = MakeFixture(kPlaceholder, (directory / "PlaceholderMissing.gdtf").string());
  const auto placeholder = symbols::ApplyRuntimeFixtureSymbolPlaceholder(scene,
      cfg.GetProjectFixtureSymbols(), kPlaceholder, "");
  assert(placeholder.success && placeholder.projectSymbolsUpdated);
  assert(!placeholder.standardGdtfUpdated && !placeholder.fixtureReferencesUpdated);
  const auto expected = *cfg.GetProjectFixtureSymbols().FindForFixture(scene.fixtures.at(kPlaceholder));
  assert(expected.generatorVersion == "runtime-fixture-placeholder-1");
  assert(expected.kind == symbols::ProjectFixtureSymbolKind::GeneratedFallback);
  for (const auto &view : expected.views) {
    assert(view.svg.find("viewBox=\"0 0 200 200\"") != std::string::npos);
    assert(view.offsetXmm == 100.0 && view.offsetYmm == 100.0);
  }
  const auto repeated = symbols::ApplyRuntimeFixtureSymbolPlaceholder(scene,
      cfg.GetProjectFixtureSymbols(), kPlaceholder, "");
  assert(repeated.success && !repeated.projectSymbolsUpdated);
  assert(ReadFile(legacy) == before);
}

void CheckNewInstanceInheritanceAndConflict(const fs::path &directory) {
  MvrScene scene;
  symbols::ProjectFixtureSymbolStore store;
  scene.fixtures[kSelected] = MakeFixture(kSelected, "definition.gdtf");
  scene.fixtures[kShared] = MakeFixture(kShared, "definition.gdtf");
  scene.fixtures[kOtherMode] = MakeFixture(kOtherMode, "definition.gdtf", "Other mode");
  symbols::ProjectFixtureSymbolBundle bundle;
  std::string error;
  assert(symbols::BuildProjectFixtureSymbolBundle(BuildSymbols(),
      symbols::ProjectFixtureSymbolKind::UserOverride, "", bundle, error));
  assert(store.ApplyToFixtures(scene, {kSelected, kShared}, bundle, error));
  const auto originalBinding = scene.fixtures.at(kSelected).projectSymbolDefinitionId;
  const auto originalContent = store.ContentIdForDefinition(originalBinding);
  scene.fixtures[kGenerated] = MakeFixture(kGenerated, "definition.gdtf");
  const auto inherited = symbols::InheritFixtureProjectSymbols(scene, store, kGenerated);
  assert(inherited.success && inherited.projectSymbolsUpdated);
  assert(!inherited.symbolBindingConflict);
  assert(scene.fixtures.at(kSelected).projectSymbolDefinitionId == originalBinding);
  assert(scene.fixtures.at(kShared).projectSymbolDefinitionId == originalBinding);
  assert(scene.fixtures.at(kOtherMode).projectSymbolDefinitionId.empty());
  assert(store.ContentIdForDefinition(scene.fixtures.at(kGenerated).projectSymbolDefinitionId) == originalContent);
  assert(*store.FindForFixture(scene.fixtures.at(kGenerated)) == bundle);
  assert(store.BundleCount() == 1);

  auto conflictingBundle = bundle;
  conflictingBundle.views.front().offsetXmm += 1.0;
  scene.fixtures[kOtherMode].gdtfMode = "Default";
  assert(store.ApplyToFixtures(scene, {kOtherMode}, conflictingBundle, error));
  const auto conflictBinding = scene.fixtures.at(kOtherMode).projectSymbolDefinitionId;
  scene.fixtures[kPlaceholder] = MakeFixture(kPlaceholder, "definition.gdtf");
  const auto conflict = symbols::InheritFixtureProjectSymbols(scene, store, kPlaceholder);
  assert(conflict.success && conflict.symbolBindingConflict);
  assert(!conflict.projectSymbolsUpdated && !conflict.warnings.empty());
  assert(scene.fixtures.at(kPlaceholder).projectSymbolDefinitionId.empty());
  assert(scene.fixtures.at(kOtherMode).projectSymbolDefinitionId == conflictBinding);
  assert(scene.fixtures.at(kSelected).projectSymbolDefinitionId == originalBinding);
  assert(*store.FindForFixture(scene.fixtures.at(kSelected)) == bundle);
  assert(*store.FindForFixture(scene.fixtures.at(kOtherMode)) == conflictingBundle);
  const auto rejectedAutomatic = symbols::PrepareFixtureSymbols(scene, store, kPlaceholder,
      "", BuildSymbols(), gdtf::MutationPolicy::PreserveImported);
  assert(rejectedAutomatic.success && rejectedAutomatic.symbolBindingConflict);
  assert(!rejectedAutomatic.projectSymbolsUpdated);
  assert(scene.fixtures.at(kPlaceholder).projectSymbolDefinitionId.empty());

  scene.basePath = directory.string();
  const fs::path legacySource = directory / "definition.gdtf";
  const std::string legacySvg = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10 10\" "
      "data-perastage-symbol-version=\"1\"><polygon points=\"0,0 10,0 10,10\"/></svg>";
  auto builder = tests::gdtf::BuildMinimalValidFixture().WithModelResource("conflict");
  for (const auto view : {SymbolViewKind::Top, SymbolViewKind::Front,
                         SymbolViewKind::Left, SymbolViewKind::Bottom})
    builder.WithArchiveEntry(BuildPerastageFixtureSymbolPath("conflict", view), legacySvg);
  builder.WriteArchive(legacySource);
  const auto sourceBefore = ReadFile(legacySource);
  const auto retainLegacy = symbols::PrepareFixtureSymbols(scene, store, kPlaceholder,
      legacySource.string(), BuildSymbols(), gdtf::MutationPolicy::CompleteAndImprove);
  assert(retainLegacy.success && retainLegacy.symbolBindingConflict);
  assert(!retainLegacy.projectSymbolsUpdated && !retainLegacy.standardGdtfUpdated);
  assert(!retainLegacy.fixtureReferencesUpdated && !retainLegacy.warnings.empty());
  assert(scene.fixtures.at(kPlaceholder).gdtfSpec == "definition.gdtf");
  assert(scene.fixtures.at(kPlaceholder).projectSymbolDefinitionId.empty());
  assert(ReadFile(legacySource) == sourceBefore);
}
} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());
  TemporaryProject project;
  ConfigManager &cfg = ConfigManager::Get();
  CheckManualApplyAndIntegratedPersistence(cfg, project.path);
  CheckUnsavedProjectAndFailures(cfg, project.path);
  CheckAutomaticSafeDerivativeOwnership(cfg, project.path);
  CheckLegacyReadAndRuntimePlaceholder(cfg, project.path);
  CheckNewInstanceInheritanceAndConflict(project.path);
  cfg.Reset();
  return 0;
}

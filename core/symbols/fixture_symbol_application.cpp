#include "symbols/fixture_symbol_application.h"

#include "gdtf/editor/gdtf_document.h"
#include "symbols/Symbol2DSvg.h"
#include "symbols/fixture_symbol_availability.h"
#include "symbols/fixture_symbol_svg_cache.h"
#include "symbols/fixture_symbol_resource_revision.h"
#include "symbols/project_fixture_symbol_migration.h"

#include <algorithm>
#include <filesystem>
#include <utility>

#include "fixture.h"
#include "mvrscene.h"

namespace symbols {
namespace {

SymbolView SymbolViewForKind(SymbolViewKind kind) {
  switch (kind) {
  case SymbolViewKind::Top: return SymbolView::Top;
  case SymbolViewKind::Front: return SymbolView::Front;
  case SymbolViewKind::Left: return SymbolView::Left;
  default: return SymbolView::Bottom;
  }
}

std::vector<std::string> DefinitionInstances(
    const MvrScene &scene, const Fixture &selected,
    const ProjectFixtureSymbolStore &store, ProjectFixtureSymbolKind kind) {
  std::vector<std::string> instances;
  for (const auto &[uuid, fixture] : scene.fixtures) {
    const bool sameBinding = !selected.projectSymbolDefinitionId.empty() &&
        fixture.projectSymbolDefinitionId == selected.projectSymbolDefinitionId;
    const bool sameSource = !selected.gdtfSpec.empty() &&
        fixture.gdtfSpec == selected.gdtfSpec &&
        fixture.gdtfMode == selected.gdtfMode;
    if ((uuid == selected.uuid || sameBinding || sameSource) &&
        (kind == ProjectFixtureSymbolKind::UserOverride ||
         !store.FindForFixture(fixture)))
      instances.push_back(uuid);
  }
  std::sort(instances.begin(), instances.end());
  return instances;
}

} // namespace

bool BuildProjectFixtureSymbolBundle(
    const std::vector<Symbol2D> &symbols, ProjectFixtureSymbolKind kind,
    const std::string &sourceGdtfPath, ProjectFixtureSymbolBundle &bundle,
    std::string &errorMessage) {
  bundle = {};
  bundle.kind = kind;
  bundle.generatorVersion = "fixture-vectorization-1";
  if (!sourceGdtfPath.empty()) {
    const auto source = gdtf::LoadGdtfDocument(sourceGdtfPath);
    bundle.sourceFixtureTypeId = source.Description().fixtureTypeId;
    std::string fingerprintError;
    bundle.sourceFingerprint = symbol_cache::ComputeGdtfSemanticFingerprint(
        sourceGdtfPath, fingerprintError);
  }
  for (auto &view : bundle.views) {
    const auto expectedView = SymbolViewForKind(view.viewKind);
    const auto candidate = std::find_if(symbols.begin(), symbols.end(),
        [&](const auto &symbol) { return symbol.view == expectedView; });
    if (candidate == symbols.end() || !candidate->bounds.valid) {
      errorMessage = "A project fixture symbol requires Top, Front, Side and Bottom views.";
      return false;
    }
    if (!SerializeSymbolToSvg(*candidate, view.svg, errorMessage))
      return false;
    view.offsetXmm = -candidate->bounds.min.x;
    view.offsetYmm = -candidate->bounds.min.y;
  }
  return true;
}

FixtureSymbolApplicationResult ApplyFixtureProjectSymbols(
    MvrScene &scene, ProjectFixtureSymbolStore &store,
    const std::string &fixtureUuid, const std::string &sourceGdtfPath,
    const std::vector<Symbol2D> &symbols, ProjectFixtureSymbolKind kind) {
  FixtureSymbolApplicationResult result;
  const auto selected = scene.fixtures.find(fixtureUuid);
  if (selected == scene.fixtures.end()) {
    result.diagnostic = "Could not resolve the selected fixture in the scene.";
    return result;
  }
  if (kind == ProjectFixtureSymbolKind::GeneratedFallback) {
    result = InheritFixtureProjectSymbols(scene, store, fixtureUuid);
    if (!result.success || result.projectSymbolsUpdated || result.symbolBindingConflict)
      return result;
  }
  const auto instances = DefinitionInstances(scene, selected->second, store, kind);
  if (instances.empty()) {
    result.success = true;
    return result;
  }
  ProjectFixtureSymbolBundle bundle;
  if (!BuildProjectFixtureSymbolBundle(symbols, kind, sourceGdtfPath, bundle,
                                      result.diagnostic) ||
      !store.ApplyToFixtures(scene, instances, bundle, result.diagnostic))
    return result;
  result.success = true;
  result.projectSymbolsUpdated = true;
  return result;
}

FixtureSymbolApplicationResult InheritFixtureProjectSymbols(
    MvrScene &scene, ProjectFixtureSymbolStore &store,
    const std::string &fixtureUuid) {
  FixtureSymbolApplicationResult result;
  const auto selected = scene.fixtures.find(fixtureUuid);
  if (selected == scene.fixtures.end()) {
    result.diagnostic = "Could not resolve the fixture for project symbol inheritance.";
    return result;
  }
  result.success = true;
  if (selected->second.gdtfSpec.empty())
    return result;
  std::vector<std::string> unbound;
  const ProjectFixtureSymbolBundle *consensus = nullptr;
  std::string consensusContentId;
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (fixture.gdtfSpec != selected->second.gdtfSpec ||
        fixture.gdtfMode != selected->second.gdtfMode)
      continue;
    if (fixture.projectSymbolDefinitionId.empty()) {
      unbound.push_back(uuid);
      continue;
    }
    const auto *bundle = store.FindForFixture(fixture);
    const auto contentId = store.ContentIdForDefinition(fixture.projectSymbolDefinitionId);
    if (!bundle || contentId.empty() ||
        (!consensusContentId.empty() && contentId != consensusContentId)) {
      result.symbolBindingConflict = true;
      result.warnings.push_back(
          "Project symbol inheritance requires matching authoritative content for every exact source and mode binding.");
      return result;
    }
    consensus = bundle;
    consensusContentId = contentId;
  }
  if (!consensus || unbound.empty())
    return result;
  const auto retained = *consensus;
  if (!store.ApplyToFixtures(scene, unbound, retained, result.diagnostic)) {
    result.success = false;
    return result;
  }
  result.projectSymbolsUpdated = true;
  return result;
}

bool NeedsAutomaticFixtureSymbolPreparation(
    const Fixture &fixture, const ProjectFixtureSymbolStore &store,
    const std::string &sourceGdtfPath, gdtf::MutationPolicy policy) {
  (void)policy;
  if (store.FindForFixture(fixture))
    return false;
  const auto availability = symbol_cache::InspectFixtureSymbolAvailability(sourceGdtfPath);
  return !availability.storedSvgUsable;
}

FixtureSymbolApplicationResult PrepareFixtureSymbols(
    MvrScene &scene, ProjectFixtureSymbolStore &store,
    const std::string &fixtureUuid, const std::string &sourceGdtfPath,
    const std::vector<Symbol2D> &symbols, gdtf::MutationPolicy policy) {
  (void)policy;
  const auto availability = symbol_cache::InspectFixtureSymbolAvailability(sourceGdtfPath);
  FixtureSymbolApplicationResult result = InheritFixtureProjectSymbols(scene, store, fixtureUuid);
  if (!result.success)
    return result;
  const auto selectedBefore = scene.fixtures.find(fixtureUuid);
  if (selectedBefore == scene.fixtures.end()) {
    result.success = false;
    result.diagnostic = "Could not resolve the fixture for symbol preparation.";
    return result;
  }
  const bool hasRecognizedLegacy = std::any_of(
      availability.resources.perastageViews.begin(),
      availability.resources.perastageViews.end(),
      [](const auto &view) { return view.usable && view.PerastageOwned(); });
  if (!result.symbolBindingConflict && !store.FindForFixture(selectedBefore->second) && hasRecognizedLegacy) {
    if (!MaterializeLegacyFixtureSymbols(scene, store,
            std::filesystem::path(sourceGdtfPath).parent_path().string(),
            result.diagnostic)) {
      result.success = false;
      return result;
    }
    result.projectSymbolsUpdated =
        store.FindForFixture(scene.fixtures.at(fixtureUuid)) != nullptr;
  }
  if (!result.symbolBindingConflict && !availability.storedSvgUsable) {
    const auto generated = ApplyFixtureProjectSymbols(scene, store, fixtureUuid,
        sourceGdtfPath, symbols, ProjectFixtureSymbolKind::GeneratedFallback);
    result.success = generated.success;
    result.projectSymbolsUpdated = result.projectSymbolsUpdated || generated.projectSymbolsUpdated;
    result.diagnostic = generated.diagnostic;
  }

  return result;
}

FixtureSymbolApplicationResult ApplyRuntimeFixtureSymbolPlaceholder(
    MvrScene &scene, ProjectFixtureSymbolStore &store,
    const std::string &fixtureUuid, const std::string &sourceGdtfPath) {
  FixtureSymbolApplicationResult result;
  const auto selected = scene.fixtures.find(fixtureUuid);
  if (selected == scene.fixtures.end()) {
    result.diagnostic = "Could not resolve the fixture for a project placeholder.";
    return result;
  }
  result = InheritFixtureProjectSymbols(scene, store, fixtureUuid);
  if (!result.success || result.projectSymbolsUpdated || result.symbolBindingConflict)
    return result;
  if (store.FindForFixture(selected->second))
    return result;
  if (!sourceGdtfPath.empty()) {
    const auto availability = symbol_cache::InspectFixtureSymbolAvailability(sourceGdtfPath);
    const bool hasLegacy = std::any_of(availability.resources.perastageViews.begin(),
        availability.resources.perastageViews.end(),
        [](const auto &view) { return view.usable && view.PerastageOwned(); });
    if (hasLegacy) {
      if (!MaterializeLegacyFixtureSymbols(scene, store,
              std::filesystem::path(sourceGdtfPath).parent_path().string(),
              result.diagnostic)) {
        result.success = false;
        return result;
      }
      result.projectSymbolsUpdated =
          store.FindForFixture(scene.fixtures.at(fixtureUuid)) != nullptr;
      return result;
    }
    if (std::any_of(availability.internalViews.begin(), availability.internalViews.end(),
                    [](const auto &view) { return view.usable; }))
      return result;
  }
  const auto instances = DefinitionInstances(scene, selected->second, store,
                                             ProjectFixtureSymbolKind::GeneratedFallback);
  result.success = true;
  if (instances.empty())
    return result;
  std::vector<Symbol2D> symbols;
  for (const auto view : {SymbolView::Top, SymbolView::Front, SymbolView::Left,
                         SymbolView::Bottom}) {
    Symbol2D symbol;
    symbol.view = view;
    symbol.bounds = {{-100.0f, -100.0f}, {100.0f, 100.0f}, true};
    const Polyline2D square = {{-100.0f, -100.0f}, {100.0f, -100.0f},
                               {100.0f, 100.0f}, {-100.0f, 100.0f},
                               {-100.0f, -100.0f}};
    symbol.fill.push_back({square, {}});
    symbol.strokes.push_back(square);
    symbols.push_back(std::move(symbol));
  }
  ProjectFixtureSymbolBundle bundle;
  if (!BuildProjectFixtureSymbolBundle(symbols, ProjectFixtureSymbolKind::GeneratedFallback,
                                      sourceGdtfPath, bundle, result.diagnostic)) {
    result.success = false;
    return result;
  }
  bundle.generatorVersion = "runtime-fixture-placeholder-1";
  if (!store.ApplyToFixtures(scene, instances, bundle, result.diagnostic)) {
    result.success = false;
    return result;
  }
  result.projectSymbolsUpdated = true;
  return result;
}

} // namespace symbols

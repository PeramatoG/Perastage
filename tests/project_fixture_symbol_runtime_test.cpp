#include "symbols/project_fixture_symbol_runtime.h"
#include "symbols/fixture_symbol_availability.h"
#include "symbols/fixture_symbol_preview_model.h"
#include "symbols/fixture_symbol_lookup_source.h"
#include "pdf/pdf_fixture_symbol_source.h"
#include "mvrscene.h"

#include <cassert>
#include <string>

namespace {
using View = SymbolViewKind;
using Purpose = FixtureSymbolResolutionPurpose;
const std::string kSvg =
    "<svg viewBox=\"0 0 10 10\" data-perastage-offset-x-mm=\"999\">"
    "<polygon points=\"0,0 10,0 10,10\"/></svg>";

void CheckPersistedOwner(symbols::ProjectFixtureSymbolKind kind) {
  MvrScene scene;
  scene.fixtures["fixture"].uuid = "12345678-1234-5678-90ab-1234567890ab";
  scene.fixtures["fixture"].gdtfSpec = "unavailable-external-source.gdtf";
  symbols::ProjectFixtureSymbolStore owner;
  symbols::ProjectFixtureSymbolBundle bundle;
  bundle.generatorVersion = "old-generator";
  bundle.kind = kind;
  for (auto &view : bundle.views) {
    view.svg = kSvg;
    view.offsetXmm = 3.25;
    view.offsetYmm = -7.5;
  }
  std::string error;
  assert(owner.ApplyToFixtures(scene, {"fixture"}, bundle, error));
  symbols::InstallProjectFixtureSymbolProvider([&] { return &owner; });
  const auto source = symbols::BuildProjectFixtureSymbolSource(scene.fixtures.at("fixture"));
  assert(symbols::IsProjectFixtureSymbolSource(source));
  auto first = symbol_cache::LoadUsableFixtureSymbol(source, View::Bottom, &error);
  assert(first && error.empty());
  assert(first->offsetXmm == 3.25 && first->offsetYmm == -7.5);
  assert(first->resourceSet == FixtureSymbolResourceSet::InternalRendering);
  assert(first->provenance == (kind == symbols::ProjectFixtureSymbolKind::UserOverride
      ? FixtureSymbolProvenance::ProjectUserOverride : FixtureSymbolProvenance::GeneratedPerastage));
  assert(!symbol_cache::LoadUsableFixtureSymbol(source, View::Top, &error, Purpose::StandardGdtf));
  assert(!error.empty());

  std::vector<ProjectArchiveResource> resources;
  assert(owner.CollectArchiveResources(scene, resources, error));
  symbols::ProjectFixtureSymbolStore reopened;
  assert(reopened.LoadArchiveResources(resources, error));
  assert(reopened.RestoreFixtureBindings(scene, error));
  owner.Clear();
  symbols::InstallProjectFixtureSymbolProvider([&] { return &reopened; });
  const auto reopenedSource = symbols::BuildProjectFixtureSymbolSource(scene.fixtures.at("fixture"));
  assert(reopenedSource == source);
  auto restored = symbol_cache::LoadUsableFixtureSymbol(reopenedSource, View::Bottom, &error);
  assert(restored && restored.get() != first.get());
  assert(restored->viewBoxWidth == first->viewBoxWidth && restored->fills.size() == first->fills.size());
  assert(restored->offsetXmm == first->offsetXmm && restored->offsetYmm == first->offsetYmm);
  const auto availability = symbol_cache::InspectFixtureSymbolAvailability(reopenedSource);
  assert(availability.resources.perastageViewsUsable && !availability.resources.standardViewsUsable);
  assert(BuildFixtureSymbolPreviewModel(availability.resources,
      FixtureSymbolResourceSet::InternalRendering, View::Bottom).available);
  const auto side = symbol_cache::LoadUsableFixtureSymbol(reopenedSource, View::Right);
  assert(side && side->viewKind == View::Right && !side->usedViewFallback);
  const auto back = symbol_cache::LoadUsableFixtureSymbol(reopenedSource, View::Back);
  assert(back && back->viewKind == View::Top && back->usedViewFallback);
  assert(viewer2d::pdf::ShouldLoadLegendSvgFromKey(reopenedSource));
  const std::string external = "other.gdtf";
  assert(viewer2d::pdf::ResolveLegendFixtureSymbolSource(reopenedSource, external) == reopenedSource);

  // Replacement produces a new content key; old captured sources cannot return
  // stale cached bytes or silently switch to an unrelated representation.
  bundle.views[3].svg = "<svg viewBox=\"0 0 20 10\"><polygon points=\"0,0 20,0 20,10\"/></svg>";
  assert(reopened.ApplyToFixtures(scene, {"fixture"}, bundle, error));
  const auto replacement = symbols::BuildProjectFixtureSymbolSource(scene.fixtures.at("fixture"));
  assert(replacement != reopenedSource);
  assert(!symbol_cache::LoadUsableFixtureSymbol(reopenedSource, View::Bottom));
  const auto replaced = symbol_cache::LoadUsableFixtureSymbol(replacement, View::Bottom);
  assert(replaced && replaced->viewBoxWidth == 20.0);
  symbols::ClearProjectFixtureSymbolRuntimeCache();
  assert(symbol_cache::LoadUsableFixtureSymbol(replacement, View::Bottom)->viewBoxWidth == 20.0);
  symbols::InstallProjectFixtureSymbolProvider({});
}

void CheckNeutralCompoundSvg() {
  PerastageSvgSymbolData data;
  const std::string svg = "<svg viewBox=\"0 0 10 10\"><path fill-rule=\"evenodd\" "
      "d=\"M 0 0 L 10 0 L 10 10 L 0 10 Z M 2 2 L 2 8 L 8 8 L 8 2 Z\"/></svg>";
  assert(ParseFixtureSymbolSvg(svg, data));
  assert(data.fills.size() == 1 && data.fills[0].holes.size() == 1);
}

void CheckCapturedSourceWithMixedModes() {
  MvrScene scene;
  auto &override = scene.fixtures["first"];
  override.typeName = "Same fixture type";
  override.gdtfSpec = "fixture.gdtf";
  override.gdtfMode = "First mode";
  override.projectSymbolDefinitionId = "explicit-definition";
  auto &authored = scene.fixtures["second"];
  authored.typeName = override.typeName;
  authored.gdtfSpec = override.gdtfSpec;
  authored.gdtfMode = "Second mode";
  int calls = 0;
  const auto legacy = [&](const Fixture &) {
    ++calls;
    return std::string("resolved/fixture.gdtf");
  };
  const std::string captured = "/project/fixture.gdtf";
  assert(symbols::ResolveFixtureSymbolLookupSource(captured, authored.typeName, scene, legacy) == captured);
  assert(calls == 0);
  const std::string project = "pstg-symbol:explicit-definition:content-id";
  assert(symbols::ResolveFixtureSymbolLookupSource(project, override.typeName, scene, legacy) == project);
  assert(calls == 0);
  assert(symbols::ResolveFixtureSymbolLookupSource("Old fixture name", override.typeName,
      scene, legacy) == "resolved/fixture.gdtf");
  assert(symbols::ResolveFixtureSymbolLookupSource("Old fixture name", override.typeName,
      scene, [&](const Fixture &) { return project; }) == "Old fixture name");
}
} // namespace

int main() {
  CheckPersistedOwner(symbols::ProjectFixtureSymbolKind::GeneratedFallback);
  CheckPersistedOwner(symbols::ProjectFixtureSymbolKind::UserOverride);
  CheckNeutralCompoundSvg();
  CheckCapturedSourceWithMixedModes();
}

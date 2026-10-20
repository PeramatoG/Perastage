#include "configmanager.h"
#include "fixture.h"
#include "fixture_label_overrides.h"
#include "fixture_label_composition.h"
#include "LayoutManager.h"

#include <cassert>

int main() {
  ConfigManager &cfg = ConfigManager::Get();
  cfg.Reset();

  Fixture validFixture;
  validFixture.uuid = "fixture-valid";
  cfg.GetScene().fixtures[validFixture.uuid] = validFixture;

  cfg.SetValue("label_fixture_overrides",
               R"({
                 "fixture-valid": {
                   "showLabelName": [true, null, null]
                 },
                 "fixture-missing": {
                   "showLabelName": [false, null, null]
                 }
               })");

  viewer2d::ReconcileFixtureLabelOverridesWithScene(cfg);

  const auto overrides = viewer2d::LoadFixtureLabelOverrides(cfg);
  assert(overrides.size() == 1);
  assert(overrides.count("fixture-valid") == 1);
  assert(overrides.count("fixture-missing") == 0);
  assert(cfg.HasKey("label_fixture_overrides"));

  const auto before = cfg.GetValue("label_fixture_overrides");
  cfg.ClearHistory(); cfg.MarkSaved();
  auto draft = viewer2d::LoadFixtureLabelOverrides(cfg);
  const auto legacy = viewer2d::ResolveFieldStyle(cfg, draft, validFixture.uuid, 0, 0);
  assert(legacy.visible && legacy.size == viewer2d::ResolveLabelFontSizeName(cfg, &draft.at(validFixture.uuid)) / 25.f);
  assert(!cfg.IsDirty() && !cfg.CanUndo());
  auto style = legacy; style.size = .173f; style.bold = true; style.x = .27f; style.alignment = 2;
  draft[validFixture.uuid].fieldStyles[0][0] = style;
  // Cancelling a draft leaves storage/history untouched.
  assert(cfg.GetValue("label_fixture_overrides") == before && !cfg.CanUndo());
  assert(viewer2d::CommitFixtureLabelStyles(cfg, draft));
  const auto applied = cfg.GetValue("label_fixture_overrides");
  assert(cfg.IsDirty() && cfg.CanUndo());
  assert(!viewer2d::CommitFixtureLabelStyles(cfg, draft));
  cfg.Undo(); assert(cfg.GetValue("label_fixture_overrides") == before && !cfg.CanUndo());
  cfg.Redo(); assert(cfg.GetValue("label_fixture_overrides") == applied);
  assert(viewer2d::LoadFixtureLabelOverrides(cfg).at(validFixture.uuid).fieldStyles[0][0] == style);
  auto reset = draft; reset.erase(validFixture.uuid);
  assert(viewer2d::CommitFixtureLabelStyles(cfg, reset));
  assert(!viewer2d::HasFieldPresentation(viewer2d::LoadFixtureLabelOverrides(cfg), validFixture.uuid, 0, 0));
  cfg.Undo(); assert(cfg.GetValue("label_fixture_overrides") == applied);
  const typography::FontPair fonts(PERASTAGE_TEST_RESOURCE_DIR);
  validFixture.instanceName = "Éclairage Süd"; validFixture.fixtureId = 42; validFixture.address = "3.157";
  for (int f = 1; f < 3; ++f) draft[validFixture.uuid].fieldStyles[0][f] = style;
  const viewer2d::LabelBounds bounds{-.1,-.2,.1,.2};
  const auto compose = [&] { return viewer2d::ComposeFixtureLabels(cfg, draft, validFixture, 0, bounds, fonts); };
  auto a = compose(); std::string text;
  for (const auto &t : a.text) text += t.text;
  assert(text == "Éclairage SüdID: 423.157" && a.diagnostics.empty());
  validFixture.instanceName = "Changed";
  auto b = compose(); text.clear(); for (const auto &t : b.text) text += t.text;
  assert(text == "ChangedID: 423.157");
  validFixture.instanceName = "灯";
  assert(!compose().diagnostics.empty());
  // Global styles survive scene reconciliation; Bottom keeps the historical Side fallback.
  draft[viewer2d::kLabelDefaultsKey].fieldStyles[2][0] = style;
  viewer2d::SaveFixtureLabelOverrides(cfg, draft);
  viewer2d::ReconcileFixtureLabelOverridesWithScene(cfg);
  const auto reloaded = viewer2d::LoadFixtureLabelOverrides(cfg);
  assert(reloaded.contains(viewer2d::kLabelDefaultsKey));
  assert(viewer2d::ResolveFieldStyle(cfg, reloaded, "another-fixture", 3, 0) == style);
  const std::string remapped = "00000000-0000-4000-8000-000000000001";
  assert(viewer2d::RemapFixtureLabelOverrideKeys(cfg, {{validFixture.uuid, remapped}}) == 1);
  const auto remappedStyles = viewer2d::LoadFixtureLabelOverrides(cfg);
  assert(remappedStyles.contains(viewer2d::kLabelDefaultsKey));
  assert(remappedStyles.at(remapped).fieldStyles[0][0] == style);
  cfg.ClearHistory();
  const auto layoutBefore = cfg.GetValue("layouts_collection");
  layouts::LayoutDefinition page; page.name = "Label history isolation";
  assert(layouts::LayoutManager::Get().AddLayout(page));
  const auto layoutAfter = cfg.GetValue("layouts_collection");
  const auto labelsBefore = cfg.GetValue("label_fixture_overrides");
  auto changedStyles = remappedStyles;
  changedStyles.at(remapped).fieldStyles[0][0]->x += .5f;
  assert(viewer2d::CommitFixtureLabelStyles(cfg, changedStyles));
  const auto labelsAfter = cfg.GetValue("label_fixture_overrides");
  cfg.Undo(); assert(cfg.GetValue("layouts_collection") == layoutAfter);
  assert(cfg.GetValue("label_fixture_overrides") == labelsBefore);
  cfg.Undo(); assert(cfg.GetValue("layouts_collection") == layoutBefore);
  cfg.Redo(); assert(cfg.GetValue("label_fixture_overrides") == labelsBefore);
  cfg.Redo(); assert(cfg.GetValue("label_fixture_overrides") == labelsAfter);
  assert(cfg.GetValue("layouts_collection") == layoutAfter);
  cfg.Reset();
  return 0;
}

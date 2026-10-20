#include "configmanager.h"
#include "fixture.h"
#include "fixture_label_overrides.h"
#include "fixture_label_composition.h"
#include "LayoutManager.h"
#include "../gui/fixture_label_preview_geometry.h"
#include "fixture_label_style_serialization.h"

#include <cassert>

int main() {
  ConfigManager &cfg = ConfigManager::Get();
  cfg.Reset();
  const auto versionOne = nlohmann::json::parse(R"({"presentation":{"version":1,"fields":[[{"sizeMetres":0.12,"color":[0,0,0]}]]}})");
  viewer2d::LabelFieldStyles migrated;
  viewer2d::ReadFieldStyles(versionOne,migrated);
  assert(migrated[0][0] && migrated[0][0]->outlineEnabled && migrated[0][0]->outlineWidth>0);
  nlohmann::json versionTwo;
  viewer2d::WriteFieldStyles(versionTwo,migrated);
  assert(versionTwo["presentation"]["version"]==2);

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
  assert(legacy.outlineEnabled && legacy.outlineWidth > 0);
  style.outlineColor = {.2f,.3f,.4f}; style.outlineWidth = .013f;
  draft[validFixture.uuid].fieldStyles[0][0] = style;
  // Cancelling a draft leaves storage/history untouched.
  assert(cfg.GetValue("label_fixture_overrides") == before && !cfg.CanUndo());
  const auto renderRevision = cfg.GetRenderRevision();
  assert(viewer2d::CommitFixtureLabelStyles(cfg, draft));
  assert(cfg.GetRenderRevision() == renderRevision);
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
  assert(a.fields[0] && a.fields[1] && a.fields[2]);
  assert(a.text.front().style.outlineWidth == style.outlineWidth);
  assert(a.text.front().style.outlineColor.r == style.outlineColor[0]);
  draft[validFixture.uuid].fieldStyles[0][0]->outlineEnabled = false;
  assert(compose().text.front().style.outlineWidth == 0);
  draft[validFixture.uuid].fieldStyles[0][0] = style;
  for (const auto &t : a.text) text += t.text;
  assert(text == "Éclairage SüdID: 423.157" && a.diagnostics.empty());
  validFixture.instanceName = "Changed";
  auto b = compose(); text.clear(); for (const auto &t : b.text) text += t.text;
  assert(text == "ChangedID: 423.157");
  viewer2d::FixtureLabelCompositionCache compositionCache;
  const auto cachedText = [&] {
    std::string result;
    for (const auto &t : compositionCache.Resolve(cfg, draft, validFixture, 0, bounds, fonts).text)
      result += t.text;
    return result;
  };
  assert(cachedText() == "ChangedID: 423.157");
  const auto *cachedGlyph = compositionCache.Resolve(cfg, draft, validFixture, 0, bounds, fonts).text.data();
  assert(compositionCache.Resolve(cfg, draft, validFixture, 0, bounds, fonts).text.data() == cachedGlyph);
  validFixture.instanceName = "Renamed"; validFixture.fixtureId = 9; validFixture.address = "1.002";
  assert(cachedText() == "RenamedID: 91.002");
  label_preview::Transform preview{250,175,100};
  const viewer2d::LabelBounds hitBounds{1,-2,3,4};
  assert(preview.Hit(hitBounds,{400,75}));
  assert(!preview.Hit(hitBounds,{300,75}));
  label_preview::Drag drag;
  drag.Begin({400,75},style,preview.scale);
  assert(!drag.Move({400,75}) && !drag.Changed());
  const auto movedStyle = drag.Move({420,55});
  assert(movedStyle && std::abs(movedStyle->x-style.x-.2f)<1e-6f);
  assert(std::abs(movedStyle->y-style.y-.2f)<1e-6f);
  assert(movedStyle->anchor==style.anchor && movedStyle->alignment==style.alignment);
  preview.scale = 200; // A resize cannot change the scale captured by the gesture.
  assert(std::abs(drag.Move({420,55})->x-style.x-.2f)<1e-6f);
  assert(drag.Move({400,75}) && !drag.Changed());
  drag.Finish(); assert(!drag.Active() && !drag.Move({500,500}));
  const auto x = compositionCache.Resolve(cfg, draft, validFixture, 0, bounds, fonts).text.front().x;
  draft[validFixture.uuid].fieldStyles[0][0]->x += .5f;
  assert(std::abs(compositionCache.Resolve(cfg, draft, validFixture, 0, bounds, fonts).text.front().x - x - .5f) < 1e-6f);
  const viewer2d::LabelBounds moved{bounds.minX+1,bounds.minY,bounds.maxX+1,bounds.maxY};
  assert(std::abs(compositionCache.Resolve(cfg, draft, validFixture, 0, moved, fonts).text.front().x - x - 1.5f) < 1e-6f);
  draft[validFixture.uuid].fieldStyles[0][0] = style;
  compositionCache.Clear();
  assert(cachedText() == "RenamedID: 91.002");
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

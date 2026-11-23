#include "typography/composition.h"
#include "../viewer2d/interaction/viewer2d_navigation_policy.h"
#include "../viewer2d/render/fixture_label_screen_projection.h"
#include "../viewer2d/viewer2d_coordinate_math.h"
#include "performance_metrics.h"
#include "uuidutils.h"

#include "configmanager.h"
#include "fixture.h"
#include "fixture_label_overrides.h"
#include "fixture_label_composition.h"
#include "fixture_label_name.h"
#include <wx/tokenzr.h>
#include "LayoutManager.h"
#include "../gui/fixture_label_preview_geometry.h"
#include "fixture_label_style_serialization.h"

#include <cassert>
#include <chrono>
#include <iostream>

void RunLayoutFixtureLabelPresentationTests();
namespace {

void CheckCameraPersistence(ConfigManager &cfg) {
  using namespace viewer2d::interaction;
  cfg.MarkSaved();
  const auto originalRevision = cfg.GetRenderRevision();
  NavigationViewState camera{cfg.GetFloat("view2d_offset_x"),
                             cfg.GetFloat("view2d_offset_y"),
                             cfg.GetFloat("view2d_zoom")};
  camera = Viewer2DNavigationPolicy::ApplyDragPan(camera, 70, -27).view;
  camera = Viewer2DNavigationPolicy::ApplyWheelZoom(camera, 4).view;
  camera = Viewer2DNavigationPolicy::ApplyKeyboard(camera, NavigationKey::Left, false).view;
  assert(!cfg.IsDirty() && cfg.GetRenderRevision() == originalRevision);

  cfg.SetFloat("view2d_offset_x", camera.offsetX);
  cfg.SetFloat("view2d_offset_y", camera.offsetY);
  cfg.SetFloat("view2d_zoom", camera.zoom);
  cfg.SetFloat("view2d_view", static_cast<float>(Viewer2DView::Side));
  cfg.SetFloat("view2d_render_mode", static_cast<float>(Viewer2DRenderMode::White));
  assert(cfg.IsDirty() && cfg.GetRenderRevision() == originalRevision);

  const auto path = std::filesystem::temp_directory_path() /
                    ("perastage-view2d-camera-" + GenerateUuid() + ".json");
  assert(cfg.SaveToFile(path.string()));
  cfg.Reset();
  assert(cfg.LoadFromFile(path.string()));
  std::filesystem::remove(path);
  assert(std::abs(cfg.GetFloat("view2d_offset_x") - camera.offsetX) < 1e-5);
  assert(std::abs(cfg.GetFloat("view2d_offset_y") - camera.offsetY) < 1e-5);
  assert(std::abs(cfg.GetFloat("view2d_zoom") - camera.zoom) < 1e-5);
  assert(cfg.GetFloat("view2d_view") == static_cast<float>(Viewer2DView::Side));
  assert(cfg.GetFloat("view2d_render_mode") == static_cast<float>(Viewer2DRenderMode::White));
  cfg.Reset();
}

void CheckProjectionAndCulling(const viewer2d::FixtureLabelComposition::Field &field) {
  constexpr int width = 800, height = 600;
  for (const auto view : {Viewer2DView::Top, Viewer2DView::Bottom,
                          Viewer2DView::Front, Viewer2DView::Side}) {
    for (const float zoom : {0.1f, 1.0f, 4.0f}) {
      const viewer2d::CoordinateTransform transform{width, height, zoom, 82, -31, view};
      const auto origin = *viewer2d::WorldToFramebuffer({0, 0, 0}, transform);
      const double scale = viewer2d::kPixelsPerMeter * zoom;
      const double scaleX = view == Viewer2DView::Bottom ? -scale : scale;
      viewer2d::FixtureLabelScreenProjection projection{origin[0], origin[1], scaleX, -scale};
      const auto toWorld = [view](float x, float y) -> std::array<float, 3> {
        if (view == Viewer2DView::Front)
          return {x, 0, y};
        if (view == Viewer2DView::Side)
          return {0, -x, y};
        return {x, y, 0};
      };
      const auto expected = *viewer2d::WorldToFramebuffer(toWorld(3, -7), transform);
      const auto actual = projection.Project(3, -7);
      assert(std::abs(expected[0] - actual[0]) < 1e-4);
      assert(std::abs(expected[1] - actual[1]) < 1e-4);

      // Move a real composed stack to each clip edge, including where only
      // its halo can touch the framebuffer. Its fixture can be offscreen.
      constexpr double padding = 5.0;
      const auto &ink = field.inkBounds;
      const auto a = projection.Project(ink.minX, ink.minY);
      const auto b = projection.Project(ink.maxX, ink.maxY);
      projection.originY += height / 2.0 - (a[1] + b[1]) / 2.0;
      projection.originX -= std::max(a[0], b[0]) + padding - .5;
      assert(projection.IntersectsViewport(ink, padding, width, height));
      projection.originX -= 1;
      assert(!projection.IntersectsViewport(ink, padding, width, height));
      projection.originX += width + 1 + 2 * padding + std::abs(a[0] - b[0]) - 1;
      assert(projection.IntersectsViewport(ink, padding, width, height));
      projection.originX += 1;
      assert(!projection.IntersectsViewport(ink, padding, width, height));

      projection.originX += width / 2.0 - (projection.Project(ink.minX, 0)[0] +
                                            projection.Project(ink.maxX, 0)[0]) / 2.0;
      projection.originY -= std::max(projection.Project(0, ink.minY)[1],
                                     projection.Project(0, ink.maxY)[1]) + padding - .5;
      assert(projection.IntersectsViewport(ink, padding, width, height));
      projection.originY -= 1;
      assert(!projection.IntersectsViewport(ink, padding, width, height));
    }
  }
}

} // namespace

int main() {
  ConfigManager &cfg = ConfigManager::Get();
  cfg.Reset();
  CheckCameraPersistence(cfg);
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
  // Adopting only an ink property retains the exact legacy preview baseline.
  auto legacyDraft = viewer2d::LoadFixtureLabelOverrides(cfg);
  legacyDraft[validFixture.uuid].fieldStyles = {};
  for (int view = 0; view < 3; ++view) {
    const auto editing = viewer2d::ResolveEditableFieldStyles(cfg, legacyDraft, validFixture.uuid, view, fonts);
    auto adopted = editing[0]; adopted.outlineWidth = .04f;
    auto changed = legacyDraft;
    changed[validFixture.uuid].fieldStyles[view][0] = adopted;
    const auto resolved = viewer2d::ResolveEditableFieldStyles(cfg, changed, validFixture.uuid, view, fonts);
    assert(resolved[0].x == editing[0].x && resolved[0].y == editing[0].y);
    assert(resolved[0].anchor == editing[0].anchor && resolved[0].alignment == editing[0].alignment);
    assert(resolved[1] == editing[1] && resolved[2] == editing[2]);
  }
  // Compare the exact historical wx tokenization with the shared, backend-free rule.
  for (const std::string name : {"LITE BEYE 10R4", "  LITE   BEYE  10R4  ",
      "One", "One two three four five", "Éclairage Süd Grande Scène", "", "One\ntwo three"}) {
    wxStringTokenizer words(wxString::FromUTF8(name), " ");
    std::string expected; size_t count=0;
    while (words.HasMoreTokens()) {
      if (count) expected += count % 2 == 0 ? "\n" : " ";
      expected += words.GetNextToken().ToStdString(wxConvUTF8); ++count;
    }
    assert(viewer2d::WrapFixtureLabelName(name) == expected);
  }
  validFixture.instanceName = "LITE BEYE 10R4 Long Fixture Name";
  cfg.GetScene().fixtures[validFixture.uuid] = validFixture;
  for (int view=0; view<3; ++view) {
    auto adapted = viewer2d::ResolveEditableFieldStyles(cfg, legacyDraft, validFixture.uuid, view, fonts);
    auto changed = legacyDraft;
    changed[validFixture.uuid].fieldStyles[view][0] = adapted[0];
    const auto original = viewer2d::ComposeFixtureLabels(cfg, changed, validFixture, view, bounds, fonts);
    changed[validFixture.uuid].fieldStyles[view][0]->x += .01f;
    const auto shifted = viewer2d::ComposeFixtureLabels(cfg, changed, validFixture, view, bounds, fonts);
    const auto &wrapped = typography::Compose(viewer2d::WrapFixtureLabelName(validFixture.instanceName),
        typography::TextStyle{adapted[0].size}, fonts.regular);
    assert(wrapped.lines.size() == 3 && original.fields[0]->count == wrapped.glyphs.size());
    for (size_t i=0; i<original.fields[0]->count; ++i) {
      assert(original.text[i].text == wrapped.glyphs[i].text);
      assert(std::abs(original.text[i].y - original.text.front().y - wrapped.glyphs[i].baseline) < 1e-6);
      assert(original.text[i].y == shifted.text[i].y);
      assert(std::abs(shifted.text[i].x - original.text[i].x - .01f) < 1e-6);
    }
    assert(viewer2d::CommitFixtureLabelStyles(cfg, changed));
    assert(viewer2d::LoadFixtureLabelOverrides(cfg).at(validFixture.uuid).fieldStyles[view][0] ==
        changed.at(validFixture.uuid).fieldStyles[view][0]);
    cfg.Undo(); cfg.Redo();
    const auto roundtrip = viewer2d::ComposeFixtureLabels(cfg, viewer2d::LoadFixtureLabelOverrides(cfg),
        validFixture, view, bounds, fonts);
    assert(roundtrip.text.size() == shifted.text.size());
    for (size_t i=0; i<shifted.text.size(); ++i)
      assert(roundtrip.text[i].x == shifted.text[i].x && roundtrip.text[i].y == shifted.text[i].y);
  }
  validFixture.instanceName = "Éclairage Süd";
  auto a = compose(); std::string text;
  assert(a.fields[0] && a.fields[1] && a.fields[2]);
  CheckProjectionAndCulling(*a.fields[0]);
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
  {
    cfg.GetScene().fixtures[validFixture.uuid] = validFixture;
    performance::Viewer2DFrameScope profile(true);
    performance::Reset();
    const auto renderRevision = cfg.GetRenderRevision();
    cfg.SetFloat("view2d_offset_x", 64.0f);
    cfg.SetFloat("view2d_offset_y", -21.5f);
    cfg.SetFloat("view2d_zoom", 3.0f);
    assert(cfg.GetRenderRevision() == renderRevision);
    assert(compositionCache.Resolve(cfg, draft, validFixture, 0, bounds, fonts).text.data() == cachedGlyph);

    auto anotherFixture = validFixture;
    anotherFixture.uuid = "other-fixture";
    draft[anotherFixture.uuid] = draft.at(validFixture.uuid);
    cfg.GetScene().fixtures[anotherFixture.uuid] = anotherFixture;
    const auto *otherGlyphs = compositionCache.Resolve(cfg, draft, anotherFixture,
                                                       0, bounds, fonts).text.data();
    assert(otherGlyphs->style.glyphOutline == cachedGlyph->style.glyphOutline);
    const auto count = [](performance::Work work) {
      return performance::samples[static_cast<size_t>(work)].count;
    };
    assert(count(performance::Work::View2DCompositions) == 1);
    anotherFixture.instanceName += " Changed";
    cfg.GetScene().fixtures[anotherFixture.uuid] = anotherFixture;
    cfg.MarkDirty();
    compositionCache.PruneRemovedFixtures(cfg.GetScene());
    assert(compositionCache.Resolve(cfg, draft, validFixture, 0, bounds, fonts).text.data() == cachedGlyph);
    compositionCache.Resolve(cfg, draft, anotherFixture, 0, bounds, fonts);
    assert(count(performance::Work::View2DLabelContent) == 1);
    draft[anotherFixture.uuid].fieldStyles[0][0]->bold = false;
    compositionCache.Resolve(cfg, draft, anotherFixture, 0, bounds, fonts);
    assert(count(performance::Work::View2DLabelStyle) == 1);
    compositionCache.Resolve(cfg, draft, anotherFixture, 3, bounds, fonts);
    assert(count(performance::Work::View2DLabelOrientation) == 1);
    cfg.GetScene().fixtures.erase(anotherFixture.uuid);
    draft.erase(anotherFixture.uuid);
    compositionCache.PruneRemovedFixtures(cfg.GetScene());
    assert(count(performance::Work::View2DLabelRemoved) == 1);
    const typography::FontPair alternateRoot(fonts.resourceRoot / ".");
    compositionCache.Resolve(cfg, draft, validFixture, 0, bounds, alternateRoot);
    assert(count(performance::Work::View2DLabelFont) == 1);
  }
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
  draft[validFixture.uuid].fieldStyles[0][0]->fontFamily = "Source Sans 3";
  const auto &selected = compositionCache.Resolve(cfg, draft, validFixture, 0, bounds, fonts);
  const auto sourceSans = fonts.catalog->Resolve("Source Sans 3", style.bold).face;
  auto mix = draft;
  auto &mixedStyles = mix.at(validFixture.uuid).fieldStyles[0];
  mixedStyles[0]->bold = true;
  mixedStyles[1]->fontFamily = "Noto Serif";
  mixedStyles[1]->bold = false;
  mixedStyles[2]->fontFamily = "PerastageSans";
  const auto mixed = viewer2d::ComposeFixtureLabels(cfg, mix, validFixture, 0, bounds, fonts);
  for (size_t field = 0; field < mixed.fields.size(); ++field) {
    assert(mixed.fields[field]);
    const auto &geometry = *mixed.fields[field];
    const auto &settings = *mixedStyles[field];
    const auto &ink = mixed.text[geometry.first].style;
    const auto expected = fonts.catalog->Resolve(settings.fontFamily, settings.bold).face;
    assert(ink.fontResource == expected->fingerprint);
    const auto metrics = typography::MeasureFontLine(expected->metrics, settings.size);
    assert(std::abs(ink.ascent - metrics.ascent) < 1e-6);
    assert(std::abs(ink.descent - metrics.descent) < 1e-6);
    assert(geometry.bounds.maxY - geometry.bounds.minY >= ink.ascent + ink.descent - 1e-6);
  }
  std::vector<Fixture> repeated(1000, validFixture);
  viewer2d::FixtureLabelCompositionCache repeatedCache;
  std::vector<const TextCommand *> cachedTextPointers;
  const auto coldStart = std::chrono::steady_clock::now();
  for (size_t i = 0; i < repeated.size(); ++i) {
    repeated[i].uuid = "typography-repeated-" + std::to_string(i);
    const auto &entry = repeatedCache.Resolve(cfg, mix, repeated[i], 0, bounds, fonts);
    assert(!entry.text.empty());
    cachedTextPointers.push_back(entry.text.data());
  }
  const auto cachedStart = std::chrono::steady_clock::now();
  for (size_t i = 0; i < repeated.size(); ++i)
    assert(repeatedCache.Resolve(cfg, mix, repeated[i], 0, bounds, fonts).text.data() == cachedTextPointers[i]);
  const auto ms = [](auto duration) { return std::chrono::duration<double, std::milli>(duration).count(); };
  std::cout << "1000 repeated labels, cold " << ms(cachedStart - coldStart)
            << " ms, cached " << ms(std::chrono::steady_clock::now() - cachedStart) << " ms\n";
  const auto beforeFontEdit = cfg.GetValue("label_fixture_overrides");
  assert(!selected.text.empty() && selected.text.front().style.fontResource == sourceSans->fingerprint);
  assert(viewer2d::CommitFixtureLabelStyles(cfg, draft));
  assert(viewer2d::LoadFixtureLabelOverrides(cfg).at(validFixture.uuid).fieldStyles[0][0].value().fontFamily == "Source Sans 3");
  cfg.Undo();
  assert(cfg.GetValue("label_fixture_overrides") == beforeFontEdit);
  cfg.Redo();
  assert(viewer2d::LoadFixtureLabelOverrides(cfg).at(validFixture.uuid).fieldStyles[0][0].value().fontFamily == "Source Sans 3");
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
  RunLayoutFixtureLabelPresentationTests();
  return 0;
}

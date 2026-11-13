#include "configmanager.h"
#include "LayoutManager.h"
#include "LayoutTemplateSerializer.h"
#include "LayoutTemplatePackageService.h"
#include "layoutlegenditems.h"
#include "legendutils.h"
#include "viewer2dstate.h"
#include "LayoutImageResourceRegistry.h"
#include "layout_event_table_cache_policy.h"

#include <cassert>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <wx/init.h>

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());
  auto &cfg = ConfigManager::Get(); cfg.Reset();
  const auto editorState=viewer2d::CaptureState(nullptr,cfg);
  auto captureState=editorState;
  captureState.renderOptions.darkMode=!editorState.renderOptions.darkMode;
  captureState.renderOptions.showGrid=!editorState.renderOptions.showGrid;
  captureState.camera.zoom=3;
  cfg.ClearHistory(); cfg.MarkSaved();
  const auto beforeCaptureRevision=cfg.CaptureDirtyState();
  auto stateDocument=[&] {
    layouts::LayoutDefinition snapshot; snapshot.name="State snapshot";
    snapshot.view2dViews={viewer2d::ToLayoutDefinition(viewer2d::CaptureState(nullptr,cfg),{1,2,101,73})};
    return layouts::ToJson(snapshot);
  };
  const auto beforeCaptureState=stateDocument();
  {
    viewer2d::ScopedViewer2DState capture(nullptr,nullptr,cfg,captureState,nullptr,nullptr,false);
    assert(!cfg.IsDirty() && !cfg.CanUndo());
    assert(viewer2d::CaptureState(nullptr,cfg).renderOptions.darkMode==captureState.renderOptions.darkMode);
  }
  assert(!cfg.IsDirty() && !cfg.CanUndo());
  assert(stateDocument()==beforeCaptureState);
  assert(cfg.CaptureDirtyState().revision==beforeCaptureRevision.revision);
  assert(cfg.CaptureDirtyState().savedRevision==beforeCaptureRevision.savedRevision);
  {
    viewer2d::ScopedViewer2DState capture(nullptr,nullptr,cfg,captureState,nullptr,nullptr,false);
    cfg.SetValue("unrelated-edit-during-capture","retained");
  }
  assert(cfg.IsDirty() && cfg.GetValue("unrelated-edit-during-capture")=="retained");
  cfg.Reset();
  auto source = BuildSharedLayoutLegendItems();
  assert(source.size() == 1 && source[0].typeName == "No fixtures" && source[0].count == 0);
  Fixture first; first.uuid = "one"; first.typeName = "Spot";
  first.gdtfSpec = "profile.gdtf"; first.gdtfMode = "16";
  Fixture second = first; second.uuid = "two";
  cfg.GetScene().fixtures = {{first.uuid,first},{second.uuid,second}};
  source = BuildSharedLayoutLegendItems();
  assert(source.size() == 1 && source[0].count == 2 && source[0].channelCount == 16);
  cfg.GetScene().fixtures[second.uuid].gdtfMode = "32";
  source = BuildSharedLayoutLegendItems();
  assert(!source[0].channelCount); // Mixed DMX footprints do not claim one count.
  assert(source[0].persistentSymbolIdentity == BuildFixturePersistentSymbolIdentity(first));
  Fixture wash = first; wash.uuid = "three"; wash.typeName = "Wash"; wash.gdtfMode = "0";
  cfg.GetScene().fixtures[wash.uuid] = wash;
  source = BuildSharedLayoutLegendItems();
  assert(source.size() == 2 && source[1].typeName == "Wash" && source[1].channelCount == 0);
  cfg.GetScene().fixtures.clear();
  auto &manager = layouts::LayoutManager::Get();
  const auto dir = std::filesystem::temp_directory_path() /
      ("legend-history-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(dir);
  { std::ofstream image(dir / "image.png"); image << "synthetic image resource"; }
  layouts::LayoutDefinition page; page.name = "Legend history";
  layouts::LayoutLegendDefinition a, b; a.id = 1; b.id = 2;
  a.frame = b.frame = {10,20,300,100};
  a.itemSettings = {{"Wash", false, true, true, false, ""},
                    {"Spot", true, false, false, true, "Side A"}};
  b.itemSettings = {{"Spot", false, true, true, false, "Hidden B"},
                    {"Wash", true, false, true, false, "Front B"}};
  page.legendViews = {a,b};
  layouts::LayoutEventTableDefinition table; table.id = 3; table.fields[0] = "Venue";
  page.eventTables.push_back(table);
  auto tableB = table; tableB.id = 4; tableB.fields[0] = "Other venue";
  page.eventTables.push_back(tableB);
  layouts::Layout2DViewDefinition view; view.id=5; view.frame={17,29,101,73};
  layouts::LayoutTextDefinition text; text.id=6; text.frame={17,29,151,87}; text.text="Styled page";
  layouts::LayoutImageDefinition image; image.id=7; image.frame={17,29,83,41}; image.imagePath=(dir/"image.png").string();
  page.view2dViews.push_back(view); page.textViews.push_back(text); page.imageViews.push_back(image);
  assert(manager.AddLayout(page));
  cfg.ClearHistory(); cfg.MarkSaved();
  assert(manager.UpdateLayoutEventTable(page.name, table));
  assert(manager.UpdateLayoutEventTable(page.name, tableB));
  assert(!cfg.IsDirty() && !cfg.CanUndo());
  auto cancelledTable = table; cancelledTable.fields[0] = "Cancelled draft";
  assert(!cfg.IsDirty() && !cfg.CanUndo());
  const auto before = cfg.GetValue("layouts_collection");
  assert(manager.UpdateLayoutLegend(page.name, a));
  assert(!cfg.IsDirty() && !cfg.CanUndo());
  auto cancelled = a; cancelled.itemSettings[0].visible = true;
  assert(cfg.GetValue("layouts_collection") == before); // Unpublished draft.
  a.showChannelColumn = false; a.itemSettings[1].showSideSymbol = false;
  assert(manager.UpdateLayoutLegend(page.name, a));
  const auto after = cfg.GetValue("layouts_collection");
  assert(after != before && cfg.IsDirty() && cfg.CanUndo());
  cfg.Undo(); assert(cfg.GetValue("layouts_collection") == before);
  cfg.Redo(); assert(cfg.GetValue("layouts_collection") == after);
  b.itemSettings[0].visible = true;
  assert(manager.UpdateLayoutLegend(page.name, b));
  const auto tableAHash = gui::EventTableContentHash(table);
  const auto tableBHash = gui::EventTableContentHash(tableB);
  assert(manager.UpdateLayoutEventTable(page.name, table)); // No-op.
  const auto preTable = cfg.GetValue("layouts_collection");
  table.fields[0] = "Edited venue É";
  assert(gui::EventTableContentHash(table) != tableAHash);
  assert(gui::EventTableContentHash(tableB) == tableBHash);
  assert(manager.UpdateLayoutEventTable(page.name, table));
  const auto postA = cfg.GetValue("layouts_collection");
  cfg.Undo(); assert(cfg.GetValue("layouts_collection") == preTable);
  cfg.Redo(); assert(cfg.GetValue("layouts_collection") == postA);
  tableB.fields[1] = "Other location";
  assert(manager.UpdateLayoutEventTable(page.name, tableB));
  const auto postB = cfg.GetValue("layouts_collection");
  cfg.Undo(); assert(cfg.GetValue("layouts_collection") == postA);
  cfg.Redo(); assert(cfg.GetValue("layouts_collection") == postB);
  auto moved = table; moved.frame.x += 10; moved.zIndex += 1;
  assert(gui::EventTableContentHash(moved) == gui::EventTableContentHash(table));
  moved.frame.width += 1;
  assert(gui::EventTableContentHash(moved) != gui::EventTableContentHash(table));
  const auto both = cfg.GetValue("layouts_collection");
  cfg.Undo(); assert(cfg.GetValue("layouts_collection") == postA);
  cfg.Redo(); assert(cfg.GetValue("layouts_collection") == both);

  const auto beforePhysical=cfg.GetValue("layouts_collection");
  view.scaleMode=layouts::ViewportScaleMode::Fixed;
  view.physicalScale=layouts::PhysicalViewport{50,2,-3};
  layouts::SetViewportFrame(view,{17.123,29.456,101.234,73.345});
  assert(manager.UpdateLayout2DView(page.name,view));
  const auto afterPhysical=cfg.GetValue("layouts_collection");
  assert(manager.SetLayoutPageSize(page.name,print::PageSize::A3));
  cfg.Undo(); assert(cfg.GetValue("layouts_collection")==afterPhysical);
  cfg.Undo(); assert(cfg.GetValue("layouts_collection")==beforePhysical);
  cfg.Redo(); assert(cfg.GetValue("layouts_collection")==afterPhysical);
  cfg.ClearHistory(); cfg.MarkSaved();
  assert(manager.UpdateLayout2DView(page.name,view));
  assert(!cfg.IsDirty() && !cfg.CanUndo());
  auto invalidView=view; invalidView.physicalScale->denominator=0;
  assert(!manager.UpdateLayout2DView(page.name,invalidView));
  assert(cfg.GetValue("layouts_collection")==afterPhysical && !cfg.CanUndo() && !cfg.IsDirty());
  auto badDocument=nlohmann::json::parse(*afterPhysical);
  for (auto &layout : badDocument["layouts"]) if (layout["name"]==page.name)
    layout["view2dViews"][0]["viewport"]["physical"]["denominator"]=0;
  const auto invalidConfig=nlohmann::json{{"layouts_collection",badDocument.dump()},
      {"unrelated-invalid-load","must not publish"}}.dump();
  assert(!cfg.LoadFromBuffer(std::vector<std::uint8_t>(invalidConfig.begin(),invalidConfig.end())));
  assert(cfg.GetValue("layouts_collection")==afterPhysical);
  assert(!cfg.GetValue("unrelated-invalid-load").has_value());
  assert(!cfg.CanUndo() && !cfg.IsDirty());
  auto freeView=view; freeView.scaleMode=layouts::ViewportScaleMode::Free;
  assert(manager.UpdateLayout2DView(page.name,freeView));
  cfg.Undo(); assert(cfg.GetValue("layouts_collection")==afterPhysical);
  cfg.Redo(); assert(cfg.GetValue("layouts_collection")!=afterPhysical);
  cfg.Undo(); assert(cfg.GetValue("layouts_collection")==afterPhysical);

  const auto package = dir / "legend.pslayout";
  std::string error;
  assert(manager.ExportLayoutTemplate(page.name, package.string(), &error));
  layouts::LayoutTemplateImportResult imported;
  assert(layouts::LayoutTemplatePackageService::ImportFile(package.string(), imported, &error));
  assert(imported.layout.legendViews[0].itemSettings[1].customName == "Side A");
  assert(!imported.layout.legendViews[0].showChannelColumn);
  assert(imported.layout.legendViews[1].itemSettings[0].visible);
  assert(imported.layout.eventTables[0].fields[0] == "Edited venue É");
  assert(imported.layout.eventTables[1].fields[1] == "Other location");
  assert(imported.layout.view2dViews[0].scaleMode==layouts::ViewportScaleMode::Fixed);
  assert(imported.layout.view2dViews[0].physicalScale->denominator==50);
  assert(layouts::ResolveViewportFrame(imported.layout.view2dViews[0]).width==101.234);
  assert(imported.layout.view2dViews[0].frame.width==101 && imported.layout.view2dViews[0].frame.x==17);
  assert(imported.layout.textViews[0].frame.height==87 && imported.layout.textViews[0].text=="Styled page");
  assert(imported.layout.imageViews[0].frame.width==83 && imported.layout.imageViews[0].frame.height==41);
  const auto legacy = dir / "legend.json";
  { std::ofstream out(legacy); out << layouts::ToTemplateDocument({imported.layout}); }
  layouts::LayoutTemplateImportResult legacyImported;
  assert(layouts::LayoutTemplatePackageService::ImportFile(legacy.string(), legacyImported, &error));
  assert(layouts::ToJson(imported.layout) == layouts::ToJson(legacyImported.layout));

  // Exercise actual PSTG config packaging/reopening. Scene IO is explicitly a
  // test callback; this does not qualify MVR scene import or a full GUI reopen.
  ProjectSession archive;
  const auto pstg = dir / "legend.pstg";
  assert(archive.SaveProject(pstg.string(), [&](std::vector<std::uint8_t> &bytes) {
    const auto configPath=dir/"config.json";
    if (!cfg.SaveToFile(configPath.string())) return false;
    std::ifstream in(configPath,std::ios::binary);
    bytes.assign(std::istreambuf_iterator<char>(in),{}); return !in.bad();
  }, [](std::vector<std::uint8_t> &bytes) {
    const std::string scene="PKSCENE"; bytes.assign(scene.begin(),scene.end()); return true;
  }, [] {
    std::vector<ProjectSession::ArchiveResource> resources;
    for(const auto &entry : layouts::LayoutImageResourceRegistry::Get().UsedResources())
      resources.push_back({entry.archivePath,entry.bytes});
    return resources;
  }));
  cfg.Reset();
  ProjectSession reopened;
  assert(reopened.LoadProject(pstg.string(), [&](const std::string &path) {
    return cfg.LoadFromFile(path);
  }, [](const std::string &) { return true; }));
  auto expected=nlohmann::json::parse(*afterPhysical);
  auto actual=layouts::ToTemplateDocument(manager.GetLayouts().Items());
  // Materialized image paths belong to the reopened archive workspace. All
  // stored resource identities, frames and other document values remain exact.
  const auto &allLayouts=manager.GetLayouts().Items();
  const auto restored=std::find_if(allLayouts.begin(),allLayouts.end(),[&](const auto &v) {return v.name==page.name;});
  assert(restored!=allLayouts.end() && restored->imageViews.size()==1);
  const auto &restoredImage=restored->imageViews[0];
  std::vector<std::uint8_t> restoredBytes;
  assert(layouts::LayoutImageResourceRegistry::Get().GetResourceBytes(restoredImage.projectResourcePath,restoredBytes));
  assert(std::string(restoredBytes.begin(),restoredBytes.end())=="synthetic image resource");
  const auto layoutIndex=static_cast<std::size_t>(std::distance(allLayouts.begin(),restored));
  expected["layouts"][layoutIndex]["imageViews"][0]["path"]=actual["layouts"][layoutIndex]["imageViews"][0]["path"];
  assert(actual==expected);
  assert(nlohmann::json::parse(*cfg.GetValue("layouts_collection"))==actual);
  cfg.Reset();
  std::filesystem::remove_all(dir);
}

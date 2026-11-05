#include "configmanager.h"
#include "LayoutManager.h"
#include "LayoutTemplateSerializer.h"
#include "LayoutTemplatePackageService.h"
#include "layoutlegenditems.h"
#include "legendutils.h"
#include "layout_event_table_cache_policy.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <wx/init.h>

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());
  auto &cfg = ConfigManager::Get(); cfg.Reset();
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

  const auto dir = std::filesystem::temp_directory_path() /
      ("legend-history-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(dir);
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
  const auto legacy = dir / "legend.json";
  { std::ofstream out(legacy); out << layouts::ToTemplateDocument({imported.layout}); }
  layouts::LayoutTemplateImportResult legacyImported;
  assert(layouts::LayoutTemplatePackageService::ImportFile(legacy.string(), legacyImported, &error));
  assert(layouts::ToJson(imported.layout) == layouts::ToJson(legacyImported.layout));

  // Exercise actual PSTG config packaging/reopening. Scene IO is explicitly a
  // test callback; this does not qualify MVR scene import or a full GUI reopen.
  ProjectSession archive;
  const auto pstg = dir / "legend.pstg";
  assert(archive.SaveProject(pstg.string(), [&](const std::string &path) {
    return cfg.SaveToFile(path);
  }, [](const std::string &path) {
    std::ofstream out(path, std::ios::binary); out << "PKSCENE"; return out.good();
  }));
  cfg.Reset();
  ProjectSession reopened;
  assert(reopened.LoadProject(pstg.string(), [&](const std::string &path) {
    return cfg.LoadFromFile(path);
  }, [](const std::string &) { return true; }));
  assert(cfg.GetValue("layouts_collection") == both);
  assert(layouts::ToTemplateDocument(manager.GetLayouts().Items()).dump() == *both);
  cfg.Reset();
  std::filesystem::remove_all(dir);
}

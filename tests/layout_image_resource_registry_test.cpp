#include "wx_path_utils.h"
#include "LayoutImageResourceRegistry.h"
#include "LayoutManager.h"
#include "LayoutTemplateSerializer.h"
#include "configmanager.h"
#include "configservices.h"
#include "json.hpp"
#include "projectutils.h"
#include "support/zip_test_utils.h"
#include "support/archive_entry_test_utils.h"

#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

#include <cassert>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;

// Creates a small binary image fixture at the requested path.
void WriteImageFixture(const fs::path &path, const std::string &bytes) {
  std::ofstream out(path, std::ios::binary);
  out << bytes;
  assert(out.good());
}

// Asserts publication, exact restoration and one history entry per semantic edit.
void TestDocumentHistory(const fs::path &tempDir) {
  auto &cfg = ConfigManager::Get();
  auto &manager = layouts::LayoutManager::Get();
  manager.ResetToDefault(cfg);
  Fixture fixture;
  fixture.uuid = "scene-must-stay-unchanged";
  cfg.GetScene().fixtures[fixture.uuid] = fixture;
  const auto transition = [&](const auto &operation) {
    cfg.ClearHistory();
    cfg.MarkSaved();
    const auto before = cfg.GetValue("layouts_collection");
    assert(operation());
    const auto after = cfg.GetValue("layouts_collection");
    assert(before != after && cfg.IsDirty() && cfg.CanUndo());
    assert(!cfg.Undo().empty());
    assert(!cfg.CanUndo() && cfg.CanRedo());
    assert(cfg.GetValue("layouts_collection") == before);
    assert(!cfg.Redo().empty());
    assert(!cfg.CanRedo());
    assert(cfg.GetValue("layouts_collection") == after);
    assert(layouts::ToTemplateDocument(manager.GetLayouts().Items()).dump() == after);
    assert(cfg.GetScene().fixtures.size() == 1);
    assert(cfg.GetScene().fixtures.contains(fixture.uuid));
  };
  const auto unchanged = [&](const auto &operation) {
    cfg.ClearHistory();
    cfg.MarkSaved();
    const auto before = cfg.GetValue("layouts_collection");
    const auto dirty = cfg.CaptureDirtyState();
    operation();
    assert(cfg.GetValue("layouts_collection") == before);
    assert(!cfg.CanUndo() && !cfg.IsDirty());
    assert(cfg.CaptureDirtyState().revision == dirty.revision);
  };
  layouts::LayoutDefinition page;
  page.name = "History page";
  page.pageSetup.landscape = true;
  transition([&] { return manager.AddLayout(page); });
  unchanged([&] { assert(!manager.AddLayout(page)); });
  unchanged([&] { assert(manager.RenameLayout(page.name, page.name)); });
  unchanged([&] { assert(!manager.RenameLayout(page.name, "")); });
  transition([&] { return manager.RenameLayout(page.name, "Renamed"); });
  transition([&] { return manager.SetLayoutOrientation("Renamed", false); });
  unchanged([&] { assert(manager.SetLayoutOrientation("Renamed", false)); });
  unchanged([&] { assert(!manager.RemoveLayout("missing")); });
  const fs::path templatePath = tempDir / "history.pslayout";
  std::string error, importedName;
  assert(manager.ExportLayoutTemplate("Renamed", templatePath.string(), &error));
  transition([&] { return manager.ImportLayoutTemplate(templatePath.string(), "Renamed", &importedName, &error); });
  assert(importedName != "Renamed");
  unchanged([&] { assert(!manager.ImportLayoutTemplate((tempDir / "missing.pslayout").string(), {}, nullptr, &error)); });
  transition([&] { return manager.RemoveLayout(importedName); });
  transition([&] { return manager.RemoveLayout("Renamed"); });
  const auto first = manager.GetLayouts().Items().front().name;
  unchanged([&] { assert(!manager.RemoveLayout(first)); });

  // Every existing element type uses the same commit path, including frames.
  layouts::Layout2DViewDefinition view;
  layouts::LayoutLegendDefinition legend;
  layouts::LayoutEventTableDefinition table;
  layouts::LayoutTextDefinition text;
  layouts::LayoutImageDefinition image;
  view.id = legend.id = table.id = text.id = image.id = 1;
  view.frame = legend.frame = table.frame = text.frame = image.frame = {10, 20, 100, 80};
  image.imagePath = (tempDir / "used.png").string();
  const auto element = [&](auto &value, const auto &update, const auto &remove) {
    transition([&] { return update(value); });
    unchanged([&] { assert(update(value)); });
    value.frame = {35, 45, 120, 95};
    transition([&] { return update(value); });
    unchanged([&] { assert(update(value)); });
    transition(remove);
  };
  element(view, [&](const auto &v) { return manager.UpdateLayout2DView(first, v); }, [&] { return manager.RemoveLayout2DView(first, 1); });
  element(legend, [&](const auto &v) { return manager.UpdateLayoutLegend(first, v); }, [&] { return manager.RemoveLayoutLegend(first, 1); });
  element(table, [&](const auto &v) { return manager.UpdateLayoutEventTable(first, v); }, [&] { return manager.RemoveLayoutEventTable(first, 1); });
  element(text, [&](const auto &v) { return manager.UpdateLayoutText(first, v); }, [&] { return manager.RemoveLayoutText(first, 1); });
  // Images acquire owned metadata on first publication; compare the stored value.
  transition([&] { return manager.UpdateLayoutImage(first, image); });
  image = manager.GetLayouts().Items().front().imageViews.front();
  unchanged([&] { assert(manager.UpdateLayoutImage(first, image)); });
  image.frame = {35, 45, 120, 95};
  transition([&] { return manager.UpdateLayoutImage(first, image); });
  // A frontend can still hold old resource metadata after changing the source.
  const auto oldResource = image.projectResourcePath;
  image.imagePath = (tempDir / "unused.png").string();
  transition([&] { return manager.UpdateLayoutImage(first, image); });
  image.frame.x += 5;
  transition([&] { return manager.UpdateLayoutImage(first, image); });
  image = manager.GetLayouts().Items().front().imageViews.front();
  assert(image.projectResourcePath != oldResource);
  assert(image.originalImagePath == image.imagePath);
  std::vector<std::uint8_t> currentImageBytes;
  assert(layouts::LayoutImageResourceRegistry::Get().GetResourceBytes(
      image.projectResourcePath, currentImageBytes));
  assert(std::string(currentImageBytes.begin(), currentImageBytes.end()) == "unused-image-bytes");
  // Re-selecting the same file with changed bytes is a real resource edit.
  const auto beforeReloadResource = image.projectResourcePath;
  WriteImageFixture(tempDir / "unused.png", "replacement-at-same-path");
  transition([&] { return manager.UpdateLayoutImage(first, image); });
  image = manager.GetLayouts().Items().front().imageViews.front();
  assert(image.projectResourcePath != beforeReloadResource);
  WriteImageFixture(tempDir / "unused.png", "unused-image-bytes");
  auto invalidImage = image;
  invalidImage.imagePath = (tempDir / "does-not-exist.png").string();
  unchanged([&] { assert(!manager.UpdateLayoutImage(first, invalidImage)); });
  transition([&] { return manager.RemoveLayoutImage(first, 1); });

  assert(manager.UpdateLayout2DView(first, view));
  assert(manager.UpdateLayoutLegend(first, legend));
  transition([&] { return manager.MoveLayout2DView(first, 1, true); });
  unchanged([&] { assert(manager.MoveLayout2DView(first, 1, true)); });
  transition([&] { return manager.MoveLayout2DView(first, 1, false); });
  unchanged([&] { assert(manager.MoveLayout2DView(first, 1, false)); });
  // A no-op after Undo must leave the existing redo branch available.
  assert(manager.RenameLayout(first, "Temporary name"));
  cfg.Undo();
  assert(cfg.CanRedo());
  cfg.MarkSaved();
  assert(manager.RenameLayout(first, first));
  assert(cfg.CanRedo() && !cfg.IsDirty());
  assert(manager.RenameLayout(first, "New branch"));
  assert(!cfg.CanRedo());

  // Missing and explicitly empty metadata remain different across history.
  manager.ResetToDefault(cfg);
  cfg.RemoveKey("layouts_collection");
  transition([&] { return manager.AddLayout(page); });
  cfg.Undo();
  assert(!cfg.HasKey("layouts_collection"));
  assert(manager.GetLayouts().Count() == 1);
  cfg.SetValue("layouts_collection", "{\"schemaVersion\":1,\"layouts\":[]}");
  manager.RestoreFromHistory(cfg.GetValue("layouts_collection"));
  assert(manager.GetLayouts().Count() == 0);
  transition([&] { return manager.AddLayout(page); });
  cfg.Undo();
  assert(manager.GetLayouts().Count() == 0);
  manager.ResetToDefault(cfg);
  cfg.ClearHistory();
  cfg.GetScene().fixtures.clear();
}

} // namespace

// Verifies image resources are captured, counted, packaged, and pruned when unused.
int main() {
  const fs::path sourceResourceRoot =
      fs::path(__FILE__).parent_path().parent_path() / "resources";
  const fs::path representativeIcon = ProjectUtils::ResolveResourcePathFromRoot(
      sourceResourceRoot,
      fs::path("icons") / "outline" / "file.svg");
  assert(!representativeIcon.empty());
  assert(fs::is_regular_file(representativeIcon));
  assert(ProjectUtils::ResolveResourcePathFromRoot(
             sourceResourceRoot,
             fs::path("icons") / "outline" / "missing-test-icon.svg")
             .empty());
  const fs::path tempDir = fs::temp_directory_path() / "perastage_layout_image_resource_test";
  std::error_code ec;
  fs::remove_all(tempDir, ec);
  ec.clear();
  assert(fs::create_directories(tempDir, ec) && !ec);

  const fs::path usedImage = tempDir / "used.png";
  const fs::path unusedImage = tempDir / "unused.png";
  WriteImageFixture(usedImage, "used-image-bytes");
  WriteImageFixture(unusedImage, "unused-image-bytes");

  TestDocumentHistory(tempDir);

  layouts::LayoutImageResourceRegistry::Get().Clear();
  layouts::LayoutManager &manager = layouts::LayoutManager::Get();
  manager.ResetToDefault(ConfigManager::Get());
  const std::string layoutName = manager.GetLayouts().Items().front().name;

  layouts::LayoutImageDefinition firstImage;
  firstImage.id = 1;
  firstImage.imagePath = usedImage.string();
  assert(manager.UpdateLayoutImage(layoutName, firstImage));

  const auto storedFirstResourcePath =
      manager.GetLayouts().Items().front().imageViews.front().projectResourcePath;
  assert(!storedFirstResourcePath.empty());
  assert(layouts::LayoutImageResourceRegistry::Get().UsageCount(
             storedFirstResourcePath) == 1);

  layouts::LayoutImageDefinition secondImage;
  secondImage.id = 2;
  secondImage.imagePath = unusedImage.string();
  assert(manager.UpdateLayoutImage(layoutName, secondImage));
  const auto unusedResourcePath = manager.GetLayouts().Items().front().imageViews.back().projectResourcePath;
  assert(layouts::LayoutImageResourceRegistry::Get().UsageCount(unusedResourcePath) == 1);
  assert(manager.RemoveLayoutImage(layoutName, 2));
  assert(layouts::LayoutImageResourceRegistry::Get().UsageCount(unusedResourcePath) == 0);

  manager.PrepareImageResourcesForSave();
  manager.SaveToConfig(ConfigManager::Get());

  ProjectSession session;
  const fs::path projectPath = tempDir / "images.pstg";
  const bool saved = session.SaveProject(
      projectPath.string(),
      [tempDir](std::vector<std::uint8_t> &configBytes) {
        const fs::path configPath = tempDir / "config.json";
        if (!ConfigManager::Get().SaveToFile(configPath.string()))
          return false;
        std::ifstream in(configPath, std::ios::binary);
        if (!in.is_open())
          return false;
        configBytes.assign(std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>());
        return in.good() || in.eof();
      },
      [](std::vector<std::uint8_t> &sceneBytes) {
        const std::string scene = "PKSCENE";
        sceneBytes.assign(scene.begin(), scene.end());
        return true;
      },
      []() {
        std::vector<ProjectSession::ArchiveResource> resources;
        for (const auto &entry :
             layouts::LayoutImageResourceRegistry::Get().UsedResources()) {
          resources.push_back({entry.archivePath, entry.bytes});
        }
        return resources;
      });
  assert(saved);

  const auto entries = tests::zip::ReadEntries(projectPath);
  int storedResourceCount = 0;
  for (const auto &entry : entries) {
    if (entry.isDirectory)
      continue;
    assert(entry.name.find('\\') == std::string::npos);
    if (entry.name == storedFirstResourcePath) {
      ++storedResourceCount;
      assert(entry.payload == "used-image-bytes");
    }
    assert(entry.name != unusedResourcePath);
  }
  assert(storedResourceCount == 1);
  std::string centralDirectoryError;
  const auto storedNames = tests::archive::ReadRawCentralDirectoryEntryNames(
      projectPath.string(), centralDirectoryError);
  assert(centralDirectoryError.empty());
  assert(std::count(storedNames.begin(), storedNames.end(),
                    storedFirstResourcePath) == 1);
  for (const auto &storedName : storedNames)
    assert(storedName.find('\\') == std::string::npos);

  // Undo owns deleted image bytes even after the original file disappears.
  auto &cfg = ConfigManager::Get();
  cfg.ClearHistory();
  assert(fs::remove(usedImage));
  assert(manager.RemoveLayoutImage(layoutName, 1));
  assert(!layouts::LayoutImageResourceRegistry::Get().HasResourceBytes(storedFirstResourcePath));
  assert(!cfg.Undo().empty());
  assert(!cfg.CanUndo());
  std::vector<std::uint8_t> restoredBytes;
  assert(layouts::LayoutImageResourceRegistry::Get().GetResourceBytes(storedFirstResourcePath, restoredBytes));
  assert(std::string(restoredBytes.begin(), restoredBytes.end()) == "used-image-bytes");
  assert(!cfg.Redo().empty());
  assert(layouts::LayoutImageResourceRegistry::Get().UsedResources().empty());
  assert(!cfg.Undo().empty());
  assert(manager.ExportLayoutTemplate(layoutName, (tempDir / "restored.pslayout").string(), nullptr));

  layouts::LayoutImageResourceRegistry::Get().Clear();
  fs::remove_all(tempDir, ec);
  assert(!ec);
  return 0;
}

#include "wx_path_utils.h"
#include "LayoutImageResourceRegistry.h"
#include "LayoutManager.h"
#include "configmanager.h"
#include "configservices.h"
#include "json.hpp"
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

} // namespace

// Verifies image resources are captured, counted, packaged, and pruned when unused.
int main() {
  const fs::path tempDir = fs::temp_directory_path() / "perastage_layout_image_resource_test";
  std::error_code ec;
  fs::remove_all(tempDir, ec);
  ec.clear();
  assert(fs::create_directories(tempDir, ec) && !ec);

  const fs::path usedImage = tempDir / "used.png";
  const fs::path unusedImage = tempDir / "unused.png";
  WriteImageFixture(usedImage, "used-image-bytes");
  WriteImageFixture(unusedImage, "unused-image-bytes");

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

  layouts::LayoutImageResourceRegistry::Get().Clear();
  fs::remove_all(tempDir, ec);
  assert(!ec);
  return 0;
}

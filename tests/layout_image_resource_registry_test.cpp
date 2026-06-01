#include "LayoutImageResourceRegistry.h"
#include "LayoutManager.h"
#include "configmanager.h"
#include "configservices.h"
#include "json.hpp"

#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;

// Reads the current ZIP entry payload into a string for archive assertions.
std::string ReadCurrentZipEntry(wxZipInputStream &zip) {
  std::string data;
  char buffer[256];
  while (true) {
    zip.Read(buffer, sizeof(buffer));
    const size_t bytes = zip.LastRead();
    if (bytes == 0)
      break;
    data.append(buffer, bytes);
  }
  return data;
}

// Reads all file entries from a ZIP archive into a name-to-payload map.
std::map<std::string, std::string> ReadArchiveEntries(const fs::path &archivePath) {
  std::map<std::string, std::string> entries;
  wxFileInputStream input(archivePath.string());
  assert(input.IsOk());
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (!entry->IsDir())
      entries[entry->GetName().ToStdString()] = ReadCurrentZipEntry(zip);
  }
  return entries;
}

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
  fs::create_directories(tempDir, ec);

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

  const auto &storedFirstImage = manager.GetLayouts().Items().front().imageViews.front();
  assert(!storedFirstImage.projectResourcePath.empty());
  assert(layouts::LayoutImageResourceRegistry::Get().UsageCount(
             storedFirstImage.projectResourcePath) == 1);

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

  const auto entries = ReadArchiveEntries(projectPath);
  assert(entries.find(storedFirstImage.projectResourcePath) != entries.end());
  assert(entries.at(storedFirstImage.projectResourcePath) == "used-image-bytes");
  assert(entries.find(unusedResourcePath) == entries.end());

  layouts::LayoutImageResourceRegistry::Get().Clear();
  fs::remove_all(tempDir, ec);
  return 0;
}

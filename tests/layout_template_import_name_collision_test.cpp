#include "LayoutManager.h"

#include <cassert>
#include <filesystem>
#include <fstream>

namespace {

void RemoveAllLayoutsExceptOne(layouts::LayoutManager &manager) {
  bool removed = true;
  while (removed) {
    removed = false;
    const auto &items = manager.GetLayouts().Items();
    for (size_t i = 1; i < items.size(); ++i) {
      if (manager.RemoveLayout(items[i].name)) {
        removed = true;
        break;
      }
    }
  }
}

} // namespace

int main() {
  auto &manager = layouts::LayoutManager::Get();
  RemoveAllLayoutsExceptOne(manager);

  const std::string existingName = manager.GetLayouts().Items().front().name;

  const std::filesystem::path tempPath =
      std::filesystem::temp_directory_path() /
      "perastage_layout_template_name_collision_test.json";

  const std::string fileContent =
      "{\n"
      "  \"schemaVersion\": 1,\n"
      "  \"layouts\": [\n"
      "    {\n"
      "      \"name\": \"" +
      existingName +
      "\",\n"
      "      \"view2dViews\": [\n"
      "        {\n"
      "          \"id\": 1,\n"
      "          \"frame\": {\"x\": 0, \"y\": 0, \"width\": 100, \"height\": 100}\n"
      "        }\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}\n";

  {
    std::ofstream out(tempPath, std::ios::binary);
    assert(out.is_open());
    out << fileContent;
    assert(out.good());
  }

  std::string createdName;
  std::string error;
  const bool imported = manager.ImportLayoutTemplate(tempPath.string(), "", &createdName, &error);
  assert(imported);
  assert(error.empty());

  assert(createdName != existingName);
  assert(createdName.rfind(existingName + " (", 0) == 0);

  bool foundImportedLayout = false;
  for (const auto &layout : manager.GetLayouts().Items()) {
    if (layout.name == createdName) {
      foundImportedLayout = true;
      assert(layout.view2dViews.size() == 1);
      break;
    }
  }
  assert(foundImportedLayout);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  return 0;
}

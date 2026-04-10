#include "LayoutManager.h"
#include "configmanager.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {

constexpr const char *kLayoutsConfigKey = "layouts_collection";

void WriteTemplateFile(const std::filesystem::path &filePath) {
  std::ofstream out(filePath, std::ios::binary);
  assert(out.is_open());
  out << "{\n"
         "  \"schemaVersion\": 1,\n"
         "  \"layouts\": [\n"
         "    {\n"
         "      \"name\": \"Plan View\",\n"
         "      \"view2dViews\": [\n"
         "        {\n"
         "          \"id\": 1,\n"
         "          \"frame\": {\"x\": 0, \"y\": 0, \"width\": 100, \"height\": 100}\n"
         "        }\n"
         "      ]\n"
         "    }\n"
         "  ]\n"
         "}\n";
  assert(out.good());
}

} // namespace

int main() {
  auto &cfg = ConfigManager::Get();
  cfg.ClearValues();

  auto &manager = layouts::LayoutManager::Get();
  manager.ResetToDefault(cfg);
  cfg.RemoveKey(kLayoutsConfigKey);

  const std::filesystem::path tempRoot =
      std::filesystem::temp_directory_path() /
      "perastage_layout_defaults_from_empty_config_test";
  const std::filesystem::path defaultsDir = tempRoot / "default_layouts";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(defaultsDir, ec);
  assert(!ec);

  const std::filesystem::path templatePath = defaultsDir / "plan_view.json";
  WriteTemplateFile(templatePath);

  const std::string previousEnv =
      std::getenv("PERASTAGE_LIBRARY_PATH")
          ? std::getenv("PERASTAGE_LIBRARY_PATH")
          : "";
  setenv("PERASTAGE_LIBRARY_PATH", tempRoot.c_str(), 1);

  manager.LoadFromConfig(cfg);

  const auto &layouts = manager.GetLayouts().Items();
  assert(layouts.size() == 1);
  assert(layouts.front().name == "Plan View");
  assert(cfg.HasKey(kLayoutsConfigKey));

  if (previousEnv.empty())
    unsetenv("PERASTAGE_LIBRARY_PATH");
  else
    setenv("PERASTAGE_LIBRARY_PATH", previousEnv.c_str(), 1);

  std::filesystem::remove_all(tempRoot, ec);

  return 0;
}

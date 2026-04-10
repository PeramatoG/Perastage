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

void SetLibraryEnv(const std::string &value) {
#ifdef _WIN32
  const int result = _putenv_s("PERASTAGE_LIBRARY_PATH", value.c_str());
  assert(result == 0);
#else
  const int result = setenv("PERASTAGE_LIBRARY_PATH", value.c_str(), 1);
  assert(result == 0);
#endif
}

void RestoreLibraryEnv(const std::string &previousValue) {
#ifdef _WIN32
  const int result = _putenv_s("PERASTAGE_LIBRARY_PATH", previousValue.c_str());
  assert(result == 0);
#else
  if (previousValue.empty()) {
    const int result = unsetenv("PERASTAGE_LIBRARY_PATH");
    assert(result == 0);
    return;
  }
  const int result = setenv("PERASTAGE_LIBRARY_PATH", previousValue.c_str(), 1);
  assert(result == 0);
#endif
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
  SetLibraryEnv(tempRoot.string());

  manager.LoadFromConfig(cfg);

  auto layouts = manager.GetLayouts().Items();
  assert(layouts.size() == 1);
  assert(layouts.front().name == "Plan View");
  assert(cfg.HasKey(kLayoutsConfigKey));

  manager.ResetToDefault(cfg);
  cfg.SetValue(kLayoutsConfigKey, "");
  manager.LoadFromConfig(cfg);
  layouts = manager.GetLayouts().Items();
  assert(layouts.size() == 1);
  assert(layouts.front().name == "Plan View");

  RestoreLibraryEnv(previousEnv);

  std::filesystem::remove_all(tempRoot, ec);

  return 0;
}

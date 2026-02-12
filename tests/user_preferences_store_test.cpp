#include "configservices.h"

#include <cassert>
#include <filesystem>

int main() {
  UserPreferencesStore store;
  store.RegisterVariable("zoom", "float", 1.0f, 0.5f, 2.0f);
  store.SetValue("zoom", "4.0");
  assert(store.GetFloat("zoom") == 2.0f);

  const std::filesystem::path out = std::filesystem::temp_directory_path() /
                                    "perastage_user_preferences_store_test.json";
  assert(store.SaveToFile(out.string()));

  UserPreferencesStore loaded;
  loaded.RegisterVariable("zoom", "float", 1.0f, 0.5f, 2.0f);
  assert(loaded.LoadFromFile(out.string()));
  assert(loaded.GetFloat("zoom") == 2.0f);
  std::error_code ec;
  std::filesystem::remove(out, ec);
  return 0;
}

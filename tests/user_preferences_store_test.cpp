#include "configservices.h"
#include "mvr_preferences.h"
#include "gdtf_mutation_policy.h"
#include "model_detail_policy.h"

#include <cassert>
#include <filesystem>
#include <sstream>

int main() {
  UserPreferencesStore store;
  assert(model_detail::ReadPreferences(store) == model_detail::Preferences{});
  for (const auto value : {"", "true", "yes", "unknown", "2"}) {
    store.SetValue(model_detail::kMovingProxyConfigKey, value);
    store.SetValue(model_detail::kDetailConfigKey, "unknown");
    assert(model_detail::ReadPreferences(store) == model_detail::Preferences{});
  }
  model_detail::SavePreferences(store, {model_detail::Level::Low, true});
  store.RegisterVariable("zoom", "float", 1.0f, 0.5f, 2.0f);
  store.SetValue("zoom", "4.0");
  assert(store.GetFloat("zoom") == 2.0f);
  assert(gdtf::ReadMutationPolicy(store) ==
         gdtf::MutationPolicy::CompleteAndImprove);
  assert(gdtf::AllowsAutomaticCompletion(gdtf::ReadMutationPolicy(store)));
  gdtf::SaveMutationPolicy(store, gdtf::MutationPolicy::PreserveImported);
  assert(store.GetValue(gdtf::kMutationPolicyConfigKey) == "preserve_imported");

  const std::filesystem::path out = std::filesystem::temp_directory_path() /
                                    "perastage_user_preferences_store_test.json";
  assert(!mvr::preferences::AutoCloseGdtfDownloadWindow(store));
  for (const char *value : {"", "0", "true", "yes", "invalid"}) {
    store.SetValue(mvr::preferences::kAutoCloseGdtfDownloadWindowKey, value);
    assert(!mvr::preferences::AutoCloseGdtfDownloadWindow(store));
  }
  mvr::preferences::SaveAutoCloseGdtfDownloadWindow(store, true);
  assert(store.SaveToFile(out.string()));

  UserPreferencesStore loaded;
  loaded.RegisterVariable("zoom", "float", 1.0f, 0.5f, 2.0f);
  assert(loaded.LoadFromFile(out.string()));
  assert(mvr::preferences::AutoCloseGdtfDownloadWindow(loaded));
  mvr::preferences::SaveAutoCloseGdtfDownloadWindow(loaded, false);
  assert(loaded.GetFloat("zoom") == 2.0f);
  assert((model_detail::ReadPreferences(loaded) ==
          model_detail::Preferences{model_detail::Level::Low, true}));
  model_detail::SavePreferences(loaded, {model_detail::Level::High, false});
  assert(gdtf::ReadMutationPolicy(loaded) ==
         gdtf::MutationPolicy::PreserveImported);
  assert(!gdtf::AllowsAutomaticCompletion(gdtf::ReadMutationPolicy(loaded)));

  gdtf::SaveMutationPolicy(loaded, gdtf::MutationPolicy::CompleteAndImprove);
  std::ostringstream serialized;
  assert(loaded.SaveToStream(serialized));
  const std::string json = serialized.str();
  UserPreferencesStore completed;
  assert(completed.LoadFromBuffer({json.begin(), json.end()}));
  assert((model_detail::ReadPreferences(completed) ==
          model_detail::Preferences{model_detail::Level::High, false}));
  assert(!mvr::preferences::AutoCloseGdtfDownloadWindow(completed));
  assert(completed.GetValue(gdtf::kMutationPolicyConfigKey) ==
         "complete_and_improve");
  assert(gdtf::AllowsAutomaticCompletion(gdtf::ReadMutationPolicy(completed)));

  completed.SetValue(gdtf::kMutationPolicyConfigKey, "unsupported_mode");
  assert(gdtf::ReadMutationPolicy(completed) == gdtf::DefaultMutationPolicy());
  completed.SetValue(model_detail::kDetailConfigKey, "unsupported_mode");
  const auto snapshot = model_detail::CapturePreferences(completed);
  model_detail::SavePreferences(completed, {model_detail::Level::Low, true});
  snapshot.Restore(completed);
  assert(completed.GetValue(model_detail::kDetailConfigKey) == "unsupported_mode");
  assert(completed.GetValue(model_detail::kMovingProxyConfigKey) == "0");
  UserPreferencesStore empty;
  const auto missingSnapshot = model_detail::CapturePreferences(empty);
  missingSnapshot.Restore(completed);
  assert(!completed.GetValue(model_detail::kDetailConfigKey));
  assert(!completed.GetValue(model_detail::kMovingProxyConfigKey));
  // Persisted language codes are stable strings; a restart must preserve them.
  for (const char *code : {"en", "es", "fr", "de", "zh_CN"}) {
    completed.SetValue("ui_language", code);
    assert(completed.SaveToFile(out.string()));
    UserPreferencesStore restarted;
    assert(restarted.LoadFromFile(out.string()));
    assert(restarted.GetValue("ui_language") == code);
    assert(!mvr::preferences::AutoCloseGdtfDownloadWindow(restarted));
  }
  std::error_code ec;
  std::filesystem::remove(out, ec);
  return 0;
}

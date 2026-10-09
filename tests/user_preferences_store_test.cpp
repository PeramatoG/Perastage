#include "configservices.h"
#include "gdtf_mutation_policy.h"

#include <cassert>
#include <filesystem>
#include <sstream>

int main() {
  UserPreferencesStore store;
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
  assert(store.SaveToFile(out.string()));

  UserPreferencesStore loaded;
  loaded.RegisterVariable("zoom", "float", 1.0f, 0.5f, 2.0f);
  assert(loaded.LoadFromFile(out.string()));
  assert(loaded.GetFloat("zoom") == 2.0f);
  assert(gdtf::ReadMutationPolicy(loaded) ==
         gdtf::MutationPolicy::PreserveImported);
  assert(!gdtf::AllowsAutomaticCompletion(gdtf::ReadMutationPolicy(loaded)));

  gdtf::SaveMutationPolicy(loaded, gdtf::MutationPolicy::CompleteAndImprove);
  std::ostringstream serialized;
  assert(loaded.SaveToStream(serialized));
  const std::string json = serialized.str();
  UserPreferencesStore completed;
  assert(completed.LoadFromBuffer({json.begin(), json.end()}));
  assert(completed.GetValue(gdtf::kMutationPolicyConfigKey) ==
         "complete_and_improve");
  assert(gdtf::AllowsAutomaticCompletion(gdtf::ReadMutationPolicy(completed)));

  completed.SetValue(gdtf::kMutationPolicyConfigKey, "unsupported_mode");
  assert(gdtf::ReadMutationPolicy(completed) == gdtf::DefaultMutationPolicy());
  std::error_code ec;
  std::filesystem::remove(out, ec);
  return 0;
}

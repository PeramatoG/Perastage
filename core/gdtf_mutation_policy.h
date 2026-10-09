#pragma once

#include <optional>
#include <string>

namespace gdtf {

// Controls automatic definition completion, independently of scene or explicit edits.
enum class MutationPolicy { CompleteAndImprove, PreserveImported };

inline constexpr const char *kMutationPolicyConfigKey = "gdtf_mutation_policy";

constexpr MutationPolicy DefaultMutationPolicy() {
  return MutationPolicy::CompleteAndImprove;
}

MutationPolicy ParseMutationPolicy(const std::optional<std::string> &value);
const char *MutationPolicyConfigValue(MutationPolicy policy);

constexpr bool AllowsAutomaticCompletion(MutationPolicy policy) {
  return policy == MutationPolicy::CompleteAndImprove;
}

// Uses the existing Core and GUI preferences contracts without owning storage.
template <typename Preferences>
MutationPolicy ReadMutationPolicy(const Preferences &preferences) {
  return ParseMutationPolicy(preferences.GetValue(kMutationPolicyConfigKey));
}

template <typename Preferences>
void SaveMutationPolicy(Preferences &preferences, MutationPolicy policy) {
  preferences.SetValue(kMutationPolicyConfigKey, MutationPolicyConfigValue(policy));
}

} // namespace gdtf

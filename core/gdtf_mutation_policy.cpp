#include "gdtf_mutation_policy.h"

namespace gdtf {

MutationPolicy ParseMutationPolicy(const std::optional<std::string> &value) {
  if (value && *value == "preserve_imported")
    return MutationPolicy::PreserveImported;
  return DefaultMutationPolicy();
}

const char *MutationPolicyConfigValue(MutationPolicy policy) {
  return policy == MutationPolicy::PreserveImported ? "preserve_imported"
                                                   : "complete_and_improve";
}

} // namespace gdtf

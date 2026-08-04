#include "symbols/fixture_symbol_generation_identity.h"

#include <cassert>
#include <set>
#include <string>

namespace {

// Builds an identity and requires the input contract to be valid.
symbol_cache::FixtureSymbolGenerationIdentity Build(
    const std::string &reference, const std::string &mode,
    int formatVersion, const std::string &fingerprint,
    const std::string &label) {
  symbol_cache::FixtureSymbolGenerationIdentity identity;
  std::string error;
  assert(symbol_cache::BuildFixtureSymbolGenerationIdentity(
      reference, mode, formatVersion, fingerprint, label, identity, error));
  return identity;
}

// Verifies labels never participate while every generation input does.
void TestIdentityInputsAndCoalescing() {
  const auto first = Build("fixtures/vendor/model.gdtf", "Mode A", 1, "fp-a", "Old");
  const auto renamed =
      Build("fixtures/vendor/model.gdtf", "Mode A", 1, "fp-a", "New");
  assert(first == renamed);
  assert(first.key == renamed.key);

  const auto archive =
      Build("fixtures/other/model.gdtf", "Mode A", 1, "fp-a", "Old");
  const auto mode = Build("fixtures/vendor/model.gdtf", "Mode B", 1, "fp-a", "Old");
  const auto semantic =
      Build("fixtures/vendor/model.gdtf", "Mode A", 1, "fp-b", "Old");
  const auto format = Build("fixtures/vendor/model.gdtf", "Mode A", 2, "fp-a", "Old");
  assert(first.key != archive.key);
  assert(first.key != mode.key);
  assert(first.key != semantic.key);
  assert(first.key != format.key);

  const std::set<std::string> jobs = {first.key, renamed.key, archive.key, mode.key};
  assert(jobs.size() == 3);
}

// Verifies portable normalization is stable and host-specific inputs are rejected.
void TestPortableIdentityPolicy() {
  std::string normalized;
  std::string error;
  assert(symbol_cache::NormalizePortableGdtfIdentity(
      "fixtures/vendor/model.gdtf", normalized, error));
  assert(normalized == "fixtures/vendor/model.gdtf");
  for (const char *unsafe : {"/tmp/model.gdtf", "C:/model.gdtf",
                             "../model.gdtf", "fixtures\\model.gdtf",
                             "fixtures//model.gdtf"}) {
    assert(!symbol_cache::NormalizePortableGdtfIdentity(unsafe, normalized, error));
    assert(!error.empty());
  }
}

} // namespace

// Runs fixture-symbol generation identity regression coverage.
int main() {
  TestIdentityInputsAndCoalescing();
  TestPortableIdentityPolicy();
  return 0;
}

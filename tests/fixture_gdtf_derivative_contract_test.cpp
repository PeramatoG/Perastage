#include "fixture_gdtf_derivative_contract.h"

#include "gdtf_test_fixture_builder.h"

#include <cassert>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// Verifies canonical derivatives require all four stored fixture-symbol views.
int main() {
  const fs::path root = fs::temp_directory_path() /
                        "perastage-fixture-derivative-contract-test";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path incomplete = root / "Incomplete@Perastage.gdtf";
  const fs::path complete = root / "Complete@Perastage.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithModelResource("main")
      .WriteArchive(incomplete);
  tests::gdtf::BuildMinimalValidFixture()
      .WithModelResource("main")
      .WithPerastageGeneratedSymbols()
      .WriteArchive(complete);
  std::string error;
  assert(!fixture_gdtf::ValidatePublishedDerivative(incomplete.string(), error));
  assert(!error.empty());
  assert(fixture_gdtf::ValidatePublishedDerivative(complete.string(), error));
  assert(error.empty());
  fs::remove_all(root);
  return 0;
}

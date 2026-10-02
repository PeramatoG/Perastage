#include "gdtf_filename_policy.h"
#include "support/gdtf_test_fixture_builder.h"

#include <cassert>
#include <filesystem>

// Verifies canonical naming remains deterministic across explicit and source identities.
int main() {
  namespace fs = std::filesystem;
  assert(gdtf_filename_policy::BuildCanonicalFileName("Acme", "Model One") ==
         "Acme@Model_One@Perastage.gdtf");
  assert(gdtf_filename_policy::BuildCanonicalFileName("", "", "Fallback") ==
         "Unknown@Fallback@Perastage.gdtf");
  assert(gdtf_filename_policy::IsPerastageNamedFile(
      "Acme@Model@Perastage.gdtf"));
  assert(!gdtf_filename_policy::IsPerastageNamedFile("Acme@Model.gdtf"));

  const fs::path root =
      fs::temp_directory_path() / "perastage-gdtf-filename-policy-test";
  std::error_code error;
  fs::remove_all(root, error);
  fs::create_directories(root);
  const fs::path source = root / "fallback-stem.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("Fixture Model", "Manufacturer",
                           tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId)
      .WriteArchive(source);
  assert(gdtf_filename_policy::BuildCanonicalFileName(source) ==
         "Manufacturer@Fixture_Model@Perastage.gdtf");
  fs::remove_all(root, error);
  return 0;
}

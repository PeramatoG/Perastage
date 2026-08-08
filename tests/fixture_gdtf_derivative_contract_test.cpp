#include "fixture_gdtf_derivative_contract.h"
#include "fixture_gdtf_derivative_publication.h"

#include "gdtf_test_fixture_builder.h"

#include <cassert>
#include <filesystem>
#include <fstream>
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

  const fs::path project = root / "project";
  fs::create_directories(project / "fixtures");
  const fs::path published = project / "fixtures" / "Fixture@Perastage.gdtf";
  const std::string previousBytes = "previous-published-derivative";
  std::ofstream(published, std::ios::binary) << previousBytes;

  fixture_gdtf::PreparedDerivative failedPreparation;
  assert(fixture_gdtf::PrepareProjectDerivative(
      incomplete, project, published.filename(), failedPreparation, error));
  assert(fs::exists(failedPreparation.workingPath));
  assert(!fixture_gdtf::PublishPreparedDerivative(failedPreparation, error));
  assert(!error.empty());
  assert(!fs::exists(failedPreparation.workingPath));
  std::ifstream previousInput(published, std::ios::binary);
  assert(std::string(std::istreambuf_iterator<char>(previousInput),
                     std::istreambuf_iterator<char>()) == previousBytes);

  fixture_gdtf::PreparedDerivative successfulPreparation;
  assert(fixture_gdtf::PrepareProjectDerivative(
      complete, project, published.filename(), successfulPreparation, error));
  assert(successfulPreparation.publishedReference.find(".working") ==
         std::string::npos);
  assert(fixture_gdtf::PublishPreparedDerivative(successfulPreparation, error));
  assert(!fs::exists(successfulPreparation.workingPath));
  assert(fixture_gdtf::ValidatePublishedDerivative(published.string(), error));
  fs::remove_all(root);
  return 0;
}

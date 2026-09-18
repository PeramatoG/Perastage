#include "dictionary_json_contract.h"
#include "dictionary_snapshot_service.h"
#include "json.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

// Creates an isolated temporary directory for one test process.
std::filesystem::path MakeTemporaryDirectory() {
  const auto suffix =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto path = std::filesystem::temp_directory_path() /
                    ("perastage_dictionary_snapshot_" + std::to_string(suffix));
  std::filesystem::create_directories(path);
  return path;
}

// Writes text to a test fixture file.
void WriteText(const std::filesystem::path &path, const std::string &text) {
  std::ofstream output(path);
  output << text;
}

// Reads a complete test fixture file.
std::string ReadText(const std::filesystem::path &path) {
  std::ifstream input(path);
  std::ostringstream content;
  content << input.rdbuf();
  return content.str();
}

// Verifies deterministic reference-only fixture and truss serialization.
void TestDeterministicSerialization(const std::filesystem::path &root) {
  const auto fixturePath = root / "fixture.json";
  const auto secondFixturePath = root / "fixture_second.json";
  std::unordered_map<std::string, GdtfDictionary::Entry> fixtures;
  fixtures["Zulu"] = {
      "/outside/Zulu.gdtf", "Mode Z", "Wash", "#AABBCC", {}, {}};
  fixtures["Alpha"] = {
      "relative/Alpha.gdtf", "Mode A", "Spot", "#010203", {}, {}};

  assert(DictionarySnapshotService::WriteFixtureSnapshot(fixturePath, fixtures)
             .success);
  assert(DictionarySnapshotService::WriteFixtureSnapshot(secondFixturePath,
                                                         fixtures)
             .success);
  assert(ReadText(fixturePath) == ReadText(secondFixturePath));
  const auto fixtureJson = nlohmann::json::parse(ReadText(fixturePath));
  const auto &alpha = fixtureJson["entries"]["Alpha"];
  assert(alpha["file"] == "Alpha.gdtf");
  assert(alpha["mode"] == "Mode A");
  assert(alpha["category"] == "Spot");
  assert(alpha["visual_color"] == "#010203");
  assert(ReadText(fixturePath).find("/outside/") == std::string::npos);
  assert(ReadText(fixturePath).find("relative/") == std::string::npos);
  assert(ReadText(fixturePath).find("\"Alpha\"") <
         ReadText(fixturePath).find("\"Zulu\""));

  const auto trussPath = root / "truss.json";
  const auto secondTrussPath = root / "truss_second.json";
  const std::unordered_map<std::string, std::string> trusses{
      {"Zulu", "/outside/Zulu.gdtf"}, {"Alpha", "relative/Alpha.gtruss"}};
  assert(DictionarySnapshotService::WriteTrussSnapshot(trussPath, trusses)
             .success);
  assert(DictionarySnapshotService::WriteTrussSnapshot(secondTrussPath, trusses)
             .success);
  assert(ReadText(trussPath) == ReadText(secondTrussPath));
  const auto trussJson = nlohmann::json::parse(ReadText(trussPath));
  assert(trussJson["entries"]["Alpha"]["file"] == "Alpha.gtruss");
  assert(ReadText(trussPath).find("relative/") == std::string::npos);
  assert(ReadText(trussPath).find("\"Alpha\"") <
         ReadText(trussPath).find("\"Zulu\""));
}

// Verifies shared adjacent, compatibility, library, and missing path rules.
void TestPathValidationParity(const std::filesystem::path &root) {
  const auto library = root / "library";
  std::filesystem::create_directories(library);
  WriteText(root / "beside.asset", "beside");
  WriteText(library / "library.asset", "library");

  for (const std::string type : {"fixtures", "trusses"}) {
    const auto snapshot = root / (type + ".json");
    const auto compatibility = root / (type + "_assets");
    std::filesystem::create_directories(compatibility);
    WriteText(compatibility / "compat.asset", "compat");

    nlohmann::json entries = nlohmann::json::object();
    entries["Beside"] = {{"file", "beside.asset"}};
    entries["Compatibility"] = {{"file", "compat.asset"}};
    entries["Library"] = {{"file", "nested/library.asset"}};
    for (int index = 0; index < 7; ++index)
      entries["Missing" + std::to_string(index)] = {
          {"file", "missing" + std::to_string(index) + ".asset"}};
    WriteText(snapshot,
              DictionaryJsonContract::MakeRoot(type, entries).dump(4));

    const auto result =
        type == "fixtures"
            ? DictionarySnapshotService::ValidateFixtureImportPaths(snapshot,
                                                                    library)
            : DictionarySnapshotService::ValidateTrussImportPaths(snapshot,
                                                                  library);
    assert(result.validSnapshot);
    assert(result.checkedEntries == 10);
    assert(result.foundEntries == 3);
    assert(result.missingEntries == 7);
    assert(result.missingExamples.size() == 5);
  }
}

// Verifies malformed, unreadable, and wrong-contract inputs are rejected.
void TestInvalidSnapshots(const std::filesystem::path &root) {
  const auto missing = DictionarySnapshotService::ValidateFixtureImportPaths(
      root / "missing.json", root);
  assert(!missing.validSnapshot && !missing.error.empty());

  const auto malformedPath = root / "malformed.json";
  WriteText(malformedPath, "{not json");
  const auto malformed = DictionarySnapshotService::ValidateFixtureImportPaths(
      malformedPath, root);
  assert(!malformed.validSnapshot && !malformed.error.empty());

  const auto wrongTypePath = root / "wrong_type.json";
  WriteText(wrongTypePath, DictionaryJsonContract::MakeRoot(
                               "trusses", nlohmann::json::object())
                               .dump());
  const auto wrongType = DictionarySnapshotService::ValidateFixtureImportPaths(
      wrongTypePath, root);
  assert(!wrongType.validSnapshot && !wrongType.error.empty());

  const auto invalidShapePath = root / "invalid_shape.json";
  WriteText(invalidShapePath, R"({"kind":"perastage.dictionary"})");
  const auto invalidShape = DictionarySnapshotService::ValidateTrussImportPaths(
      invalidShapePath, root);
  assert(!invalidShape.validSnapshot && !invalidShape.error.empty());
}

// Runs the focused dictionary snapshot service regression coverage.
int RunTests() {
  const auto root = MakeTemporaryDirectory();
  TestDeterministicSerialization(root);
  TestPathValidationParity(root);
  TestInvalidSnapshots(root);
  std::filesystem::remove_all(root);
  return 0;
}

} // namespace

// Executes the standalone snapshot service test binary.
int main() { return RunTests(); }

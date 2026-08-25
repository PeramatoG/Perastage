#include "rider_fixture_resolution.h"

#include <cassert>
#include <filesystem>
#include <fstream>

namespace GdtfDictionary {

// Implements the production dictionary lookup contract for isolated testing.
std::optional<Entry> FindInLoadedDictionary(
    const std::unordered_map<std::string, Entry> &dictionary,
    const std::string &type, bool validateExistingPath) {
  const std::string key = NormalizeTypeKey(type);
  for (const auto &[alias, entry] : dictionary) {
    if (NormalizeTypeKey(alias) != key)
      continue;
    if (validateExistingPath && !entry.path.empty() &&
        !std::filesystem::exists(entry.path))
      return std::nullopt;
    return entry;
  }
  return std::nullopt;
}

} // namespace GdtfDictionary

// Verifies conservative matching, dictionary priority, and mode ambiguity.
int main() {
  using namespace mvr::gdtf_catalog_matcher;
  using namespace rider_fixture_resolution;

  const auto profilePath =
      std::filesystem::temp_directory_path() / "perastage-resolution-test.gdtf";
  std::ofstream(profilePath).put('\n');
  std::unordered_map<std::string, GdtfDictionary::Entry> dictionary{
      {"GLP JDC1", {profilePath.string(), "Mode 1"}}};
  std::vector<RiderImporter::FixtureTypeRequest> requests{
      {"GLP JDC1", "GLPJDC1", 12, {"LX1", "LX2"}},
      {"PIXEL STROBE 400 RGB", "PIXELSTROBE400RGB", 8, {"LX2"}},
      {"PIXEL STROBE 800 RGB", "PIXELSTROBE800RGB", 2, {"LX3"}}};
  std::vector<GdtfCatalogEntry> catalog{
      {"revision-400", "Prolight Spain", "PIXEL STROBE 400 RGB",
       {{"Standard", 24}}, 10, 5.0f}};

  Analysis analysis = Service::Analyze(requests, dictionary, catalog);
  assert(analysis.RequiresPreflight());
  assert(analysis.items[0].state == State::Dictionary);
  assert(analysis.items[1].state == State::Suggested);
  assert(analysis.items[2].state != State::Suggested);

  const Analysis allKnown = Service::Analyze({requests.front()}, dictionary, {});
  assert(!allKnown.RequiresPreflight());
  const Analysis offlineUnknown = Service::Analyze({requests[2]}, dictionary, {});
  assert(offlineUnknown.RequiresPreflight());
  assert(offlineUnknown.items.front().state == State::Unresolved);
  const Analysis emptyPath = Service::Analyze(
      {requests.front()}, {{"GLP JDC1", GdtfDictionary::Entry{}}}, {});
  assert(emptyPath.RequiresPreflight());

  Service::SelectCatalogEntry(analysis.items[1], *analysis.items[1].suggestedEntry);
  assert(analysis.items[1].selectedMode == "Standard");
  GdtfCatalogEntry multiMode{"multi", "Maker", "Model", {{"A", 8}, {"B", 16}},
                             1, 1.0f};
  Service::SelectCatalogEntry(analysis.items[1], multiMode);
  assert(analysis.items[1].RequiresModeSelection());
  Service::SelectGeneric(analysis.items[2]);
  assert(analysis.items[2].IsReady());

  std::filesystem::remove(profilePath);
  return 0;
}

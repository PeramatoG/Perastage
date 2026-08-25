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
       {{"Standard", 24}}, 10, 5.0f},
      {"revision-jdc1", "GLP", "JDC1", {{"Standard", 62}}, 11, 5.0f}};

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

  Analysis defaultFallback = Service::Analyze({requests[2]}, dictionary, {});
  Service::FinalizeDefaults(defaultFallback);
  assert(defaultFallback.items.front().state == State::Generic);
  Analysis ambiguousFallback = Service::Analyze({requests[1]}, dictionary, catalog);
  Service::SelectCatalogEntry(ambiguousFallback.items.front(), multiMode);
  Service::FinalizeDefaults(ambiguousFallback);
  assert(ambiguousFallback.items.front().state == State::Generic);
  Analysis preservedChoice = Service::Analyze({requests[1]}, dictionary, {});
  Service::SelectGeneric(preservedChoice.items.front());
  const Analysis backgroundMatch =
      Service::Analyze({requests[1]}, dictionary, catalog);
  Service::MergeCatalogSuggestions(preservedChoice, backgroundMatch);
  assert(preservedChoice.items.front().state == State::Generic);

  RiderImporter::FixtureTypeRequest editedRequest{
      "GLP JDC1 16CH", "GLPJDC116CH", 4, {"LX1"}};
  Analysis edited = Service::Analyze({editedRequest}, {}, catalog);
  assert(edited.items.front().originalFixtureType == "GLP JDC1 16CH");
  edited.items.front().effectiveFixtureType = "GLP JDC1";
  Service::ResolveItem(edited.items.front(), {}, catalog);
  assert(edited.items.front().originalFixtureType == "GLP JDC1 16CH");
  assert(edited.items.front().effectiveFixtureType == "GLP JDC1");
  assert(edited.items.front().selectedEntry);
  assert(edited.items.front().selectedEntry->rid == "revision-jdc1");
  assert(edited.items.front().origin == ResolutionOrigin::AutomaticMatch);
  Service::SetCreate(edited.items.front(), false);
  assert(edited.items.front().origin == ResolutionOrigin::Skipped);

  Service::FallbackAfterFailure(analysis.items[1], "Download failed");
  assert(analysis.items[1].origin == ResolutionOrigin::GenericFallback);
  assert(!analysis.items[1].selectedEntry);
  assert(analysis.items[1].details.find("Download failed") != std::string::npos);

  assert(Service::IsValidProgressTransition(
      {ProgressStage::LoadingCatalog}, {ProgressStage::ParsingCatalog}));
  assert(Service::IsValidProgressTransition(
      {ProgressStage::ParsingCatalog},
      {ProgressStage::MatchingFixtures, 1, 2, 1}));
  assert(Service::IsValidProgressTransition(
      {ProgressStage::MatchingFixtures, 1, 2, 1},
      {ProgressStage::MatchingFixtures, 2, 2, 1}));
  assert(Service::IsValidProgressTransition(
      {ProgressStage::MatchingFixtures, 2, 2, 1},
      {ProgressStage::Complete, 2, 2, 1}));
  assert(!Service::IsValidProgressTransition(
      {ProgressStage::MatchingFixtures, 2, 2, 1},
      {ProgressStage::MatchingFixtures, 3, 2, 1}));
  assert(Service::IsValidProgressTransition(
      {ProgressStage::LoadingCatalog}, {ProgressStage::Unavailable}));
  assert(StatusSemanticForOrigin(ResolutionOrigin::Dictionary) ==
         StatusSemantic::Neutral);
  assert(StatusSemanticForOrigin(ResolutionOrigin::DictionaryModified) ==
         StatusSemantic::Modified);
  assert(StatusSemanticForOrigin(ResolutionOrigin::AutomaticMatch) ==
         StatusSemantic::Success);
  assert(StatusSemanticForOrigin(ResolutionOrigin::UserSelection) ==
         StatusSemantic::Information);
  assert(StatusSemanticForOrigin(ResolutionOrigin::GenericFallback) ==
         StatusSemantic::Warning);
  assert(StatusSemanticForOrigin(ResolutionOrigin::Skipped) ==
         StatusSemantic::Muted);

  std::vector<Progress> reportedProgress;
  const Analysis progressAnalysis = Service::Analyze(
      {requests[1], requests[2]}, {}, catalog,
      [&](const Progress &progress) { reportedProgress.push_back(progress); });
  assert(progressAnalysis.items.size() == 2);
  assert(reportedProgress.size() == 3);
  assert(reportedProgress[0].stage == ProgressStage::MatchingFixtures);
  assert(reportedProgress[0].current == 1 && reportedProgress[0].total == 2);
  assert(reportedProgress[1].current == 2 && reportedProgress[1].total == 2);
  assert(reportedProgress[2].stage == ProgressStage::Complete);
  assert(reportedProgress[2].automaticMatches == 1);

  Analysis renamedDuringMatch = Service::Analyze({requests[1]}, {}, {});
  renamedDuringMatch.items.front().effectiveFixtureType = "Edited after start";
  Service::MergeCatalogSuggestions(renamedDuringMatch, backgroundMatch);
  assert(!renamedDuringMatch.items.front().selectedEntry);

  std::filesystem::remove(profilePath);
  return 0;
}

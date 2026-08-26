#pragma once

#include "gdtfdictionary.h"
#include "riderimporter.h"
#include "../mvr/gdtf_catalog_matcher.h"

#include <optional>
#include <functional>
#include <unordered_map>
#include <vector>

namespace rider_fixture_resolution {

enum class State { Dictionary, Suggested, Review, Unresolved, Generic };
enum class ResolutionOrigin {
  Dictionary,
  DictionaryModified,
  AutomaticMatch,
  UserSelection,
  GenericFallback,
  Skipped
};
enum class ProgressStage {
  LoadingCatalog,
  WaitingForCredentials,
  Authenticating,
  DownloadingCatalog,
  ParsingCatalog,
  MatchingFixtures,
  Complete,
  Unavailable
};
enum class StatusSemantic {
  Neutral,
  Modified,
  Success,
  Information,
  Warning,
  Muted
};
struct Progress {
  ProgressStage stage = ProgressStage::LoadingCatalog;
  size_t current = 0;
  size_t total = 0;
  size_t automaticMatches = 0;
};
using ProgressCallback = std::function<void(const Progress &)>;

struct Item {
  RiderImporter::FixtureTypeRequest request;
  std::string originalFixtureType;
  std::string effectiveFixtureType;
  bool create = true;
  State state = State::Unresolved;
  ResolutionOrigin origin = ResolutionOrigin::GenericFallback;
  std::optional<GdtfDictionary::Entry> dictionaryEntry;
  std::optional<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> suggestedEntry;
  std::optional<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> selectedEntry;
  std::string selectedMode;
  std::string originalDictionaryMode;
  std::string details;

  bool RequiresModeSelection() const;
  bool IsReady() const;
};

struct Analysis {
  std::vector<Item> items;
  bool RequiresPreflight() const;
};

struct Summary {
  size_t created = 0;
  size_t dictionary = 0;
  size_t automaticMatches = 0;
  size_t userSelections = 0;
  size_t genericFallbacks = 0;
  size_t skipped = 0;
};

class Service {
public:
  // Resolves dictionary entries first and matches each remaining alias once.
  static Analysis Analyze(
      const std::vector<RiderImporter::FixtureTypeRequest> &requests,
      const std::unordered_map<std::string, GdtfDictionary::Entry> &dictionary,
      const std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> &catalog,
      ProgressCallback progress = {});

  // Reports whether a progress transition is internally consistent.
  static bool IsValidProgressTransition(const Progress &previous,
                                        const Progress &next);

  // Counts the resolutions that the current import plan will actually use.
  static Summary Summarize(const Analysis &analysis);

  // Selects a real catalog entry while preserving multi-mode ambiguity.
  static void SelectCatalogEntry(
      Item &item,
      const mvr::gdtf_catalog_matcher::GdtfCatalogEntry &entry,
      ResolutionOrigin origin = ResolutionOrigin::UserSelection);

  // Re-resolves one edited row using dictionary first and the shared MVR matcher.
  static void ResolveItem(
      Item &item,
      const std::unordered_map<std::string, GdtfDictionary::Entry> &dictionary,
      const std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> &catalog);

  // Updates whether the original parsed fixture request should be created.
  static void SetCreate(Item &item, bool create);

  // Selects the existing generic import fallback without persisting a mapping.
  static void SelectGeneric(Item &item);

  // Records a recoverable enhancement failure and selects Generic for the row.
  static void FallbackAfterFailure(Item &item, const std::string &reason);

  // Converts incomplete real selections to the non-blocking generic fallback.
  static void FinalizeDefaults(Analysis &analysis);

  // Merges catalog results without replacing explicit user decisions.
  static void MergeCatalogSuggestions(Analysis &current,
                                      const Analysis &matched);
  // Merges one worker result without replacing a newer explicit decision.
  static void MergeCatalogSuggestion(Item &current, const Item &matched);
};

const char *StateName(State state);
const char *OriginName(ResolutionOrigin origin);
StatusSemantic StatusSemanticForOrigin(ResolutionOrigin origin);

} // namespace rider_fixture_resolution

#pragma once

#include "gdtfdictionary.h"
#include "riderimporter.h"
#include "../mvr/gdtf_catalog_matcher.h"

#include <optional>
#include <unordered_map>
#include <vector>

namespace rider_fixture_resolution {

enum class State { Dictionary, Suggested, Review, Unresolved, Generic };

struct Item {
  RiderImporter::FixtureTypeRequest request;
  State state = State::Unresolved;
  std::optional<GdtfDictionary::Entry> dictionaryEntry;
  std::optional<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> suggestedEntry;
  std::optional<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> selectedEntry;
  std::string selectedMode;
  std::string details;

  bool RequiresModeSelection() const;
  bool IsReady() const;
};

struct Analysis {
  std::vector<Item> items;
  bool RequiresPreflight() const;
};

class Service {
public:
  // Resolves dictionary entries first and matches each remaining alias once.
  static Analysis Analyze(
      const std::vector<RiderImporter::FixtureTypeRequest> &requests,
      const std::unordered_map<std::string, GdtfDictionary::Entry> &dictionary,
      const std::vector<mvr::gdtf_catalog_matcher::GdtfCatalogEntry> &catalog);

  // Selects a real catalog entry while preserving multi-mode ambiguity.
  static void SelectCatalogEntry(
      Item &item,
      const mvr::gdtf_catalog_matcher::GdtfCatalogEntry &entry);

  // Selects the existing generic import fallback without persisting a mapping.
  static void SelectGeneric(Item &item);
};

const char *StateName(State state);

} // namespace rider_fixture_resolution

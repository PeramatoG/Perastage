#include "rider_fixture_resolution.h"

#include <algorithm>

namespace rider_fixture_resolution {
namespace {

using namespace mvr::gdtf_catalog_matcher;

// Finds a catalog row by the stable revision selected by the shared matcher.
const GdtfCatalogEntry *FindRevision(const std::vector<GdtfCatalogEntry> &catalog,
                                    const std::string &rid) {
  const auto found = std::find_if(catalog.begin(), catalog.end(),
                                  [&](const GdtfCatalogEntry &entry) {
                                    return entry.rid == rid;
                                  });
  return found == catalog.end() ? nullptr : &*found;
}

} // namespace

// Reports whether a selected catalog profile still has an ambiguous mode.
bool Item::RequiresModeSelection() const {
  return selectedEntry && selectedEntry->modes.size() > 1 && selectedMode.empty();
}

// Reports whether the row can safely continue to the import stage.
bool Item::IsReady() const {
  return state == State::Dictionary || state == State::Generic ||
         (selectedEntry.has_value() && !RequiresModeSelection());
}

// Reports whether any alias needs explicit user resolution.
bool Analysis::RequiresPreflight() const {
  return std::any_of(items.begin(), items.end(), [](const Item &item) {
    return item.state != State::Dictionary;
  });
}

// Resolves fixture aliases without mutating the dictionary or scene.
Analysis Service::Analyze(
    const std::vector<RiderImporter::FixtureTypeRequest> &requests,
    const std::unordered_map<std::string, GdtfDictionary::Entry> &dictionary,
    const std::vector<GdtfCatalogEntry> &catalog) {
  Analysis result;
  result.items.reserve(requests.size());
  for (const auto &request : requests) {
    Item item;
    item.request = request;
    item.dictionaryEntry =
        GdtfDictionary::FindInLoadedDictionary(dictionary, request.typeName);
    if (item.dictionaryEntry) {
      item.state = State::Dictionary;
      item.selectedMode = item.dictionaryEntry->mode;
      item.details = "Active fixture dictionary";
      result.items.push_back(std::move(item));
      continue;
    }

    GdtfDownloadRequest matchRequest;
    matchRequest.authoritativeFixtureNames.push_back(request.typeName);
    const GdtfDownloadMatch match = SelectBestDownloadMatch(matchRequest, catalog);
    const GdtfCatalogEntry *entry = match.found
                                        ? FindRevision(catalog, match.rid)
                                        : nullptr;
    if (!entry) {
      item.state = State::Unresolved;
      item.details = "No reliable catalog candidate";
      result.items.push_back(std::move(item));
      continue;
    }

    item.suggestedEntry = *entry;
    const FixtureNameMatchTier tier =
        ComputeFixtureNameMatchTier(entry->fixtureName, request.typeName);
    const NumericTokenCompatibility numeric = ComputeNumericTokenCompatibility(
        BuildCanonicalFixtureModel(entry->fixtureName, entry->manufacturer),
        BuildCanonicalFixtureModel(request.typeName));
    if (numeric == NumericTokenCompatibility::Different) {
      item.state = State::Unresolved;
      item.details = "Conflicting numeric model identity";
    } else if (tier == FixtureNameMatchTier::ExactNormalized) {
      item.state = State::Suggested;
      item.details = "Exact normalized model match";
    } else {
      item.state = State::Review;
      item.details = "Plausible catalog match requires review";
    }
    result.items.push_back(std::move(item));
  }
  return result;
}

// Applies a user-confirmed catalog selection and auto-selects only one mode.
void Service::SelectCatalogEntry(Item &item, const GdtfCatalogEntry &entry) {
  item.selectedEntry = entry;
  item.selectedMode = entry.modes.size() == 1 ? entry.modes.front().name : "";
}

// Marks an alias for the established generic fallback without dictionary state.
void Service::SelectGeneric(Item &item) {
  item.state = State::Generic;
  item.selectedEntry.reset();
  item.selectedMode.clear();
  item.details = "Generic fallback selected for this import";
}

// Converts resolution state to stable diagnostic text.
const char *StateName(State state) {
  switch (state) {
  case State::Dictionary: return "Dictionary";
  case State::Suggested: return "Suggested";
  case State::Review: return "Review";
  case State::Unresolved: return "Unresolved";
  case State::Generic: return "Generic";
  }
  return "Unresolved";
}

} // namespace rider_fixture_resolution

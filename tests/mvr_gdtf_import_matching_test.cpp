#include "gdtf_import_matching.h"

#include <cassert>
#include <string>
#include <vector>

namespace matching = mvr::gdtf_import_matching;

struct DownloadRequestProbe {
  std::string requestedFixtureName;
  std::string resolvedFixtureTypeName;
};

// Verifies that original MVR fixture names drive download matching when embedded metadata is generic.
static void VerifyDistinctOriginalNamesBeatSharedPlaceholderMetadata() {
  const std::string placeholderType = "BLED Standard mode 12CH";
  const std::vector<DownloadRequestProbe> requests = {
      {matching::SelectRequestedFixtureName("Aleda K10 B-EYE",
                                            "Fixtures/BLED Standard mode 12CH.gdtf"),
       placeholderType},
      {matching::SelectRequestedFixtureName("Super Storm 1500",
                                            "Fixtures/BLED Standard mode 12CH.gdtf"),
       placeholderType},
  };

  assert(matching::SelectDownloadSearchFixtureName(requests[0].requestedFixtureName,
                                                   requests[0].resolvedFixtureTypeName) ==
         "Aleda K10 B-EYE");
  assert(matching::SelectDownloadSearchFixtureName(requests[1].requestedFixtureName,
                                                   requests[1].resolvedFixtureTypeName) ==
         "Super Storm 1500");
  assert(matching::SelectDownloadSearchFixtureName(requests[0].requestedFixtureName,
                                                   requests[0].resolvedFixtureTypeName) !=
         placeholderType);
  assert(matching::SelectDownloadSearchFixtureName(requests[1].requestedFixtureName,
                                                   requests[1].resolvedFixtureTypeName) !=
         placeholderType);
}

// Verifies that the GDTFSpec basename is used when the fixture node name is unavailable.
static void VerifySpecBasenameFallback() {
  assert(matching::SelectRequestedFixtureName("", "Folder/Super Storm 1500.gdtf") ==
         "Super Storm 1500");
  assert(matching::SelectRequestedFixtureName("\t\n", "Folder\\Aleda K10 B-EYE.gdtf") ==
         "Aleda K10 B-EYE");
}

// Verifies that resolved metadata remains the final fallback when no original MVR identity exists.
static void VerifyResolvedTypeFallback() {
  assert(matching::SelectDownloadSearchFixtureName("", "BLED Standard mode 12CH") ==
         "BLED Standard mode 12CH");
}

// Verifies name confidence outranks footprint matches.
static void VerifyExactNameWithoutFootprintBeatsPartialNameWithFootprint() {
  const auto exactTier = matching::ComputeFixtureNameMatchTier("My Beam 200",
                                                              "My Beam 200");
  const auto partialTier = matching::ComputeFixtureNameMatchTier("My Beam 200 Extended",
                                                                "My Beam 200");
  const matching::DownloadCandidateRank exactNoFootprint{
      exactTier, false, false, 100, 0.0f};
  const matching::DownloadCandidateRank partialWithFootprint{
      partialTier, true, false, 200, 5.0f};

  assert(exactTier == matching::FixtureNameMatchTier::ExactNormalized);
  assert(partialTier == matching::FixtureNameMatchTier::Partial);
  assert(matching::IsBetterDownloadCandidate(exactNoFootprint, partialWithFootprint));
  assert(!matching::IsBetterDownloadCandidate(partialWithFootprint, exactNoFootprint));
}

// Verifies manufacturer matches outrank recency ties.
static void VerifyManufacturerMatchBeatsRecencyInsideSameNameAndFootprintTier() {
  const auto tier = matching::ComputeFixtureNameMatchTier("My Beam 200", "My Beam 200");
  const matching::DownloadCandidateRank manufacturerMatch{
      tier, true, true, 100, 0.0f};
  const matching::DownloadCandidateRank newerWithoutManufacturer{
      tier, true, false, 200, 5.0f};

  assert(matching::IsBetterDownloadCandidate(manufacturerMatch,
                                             newerWithoutManufacturer));
  assert(!matching::IsBetterDownloadCandidate(newerWithoutManufacturer,
                                              manufacturerMatch));
}

struct CatalogModeProbe {
  std::string name;
  int footprint = 0;
};

struct DownloadCandidateProbe {
  std::string rid;
  std::string fixtureName;
  std::vector<CatalogModeProbe> modes;
  long long recency = 0;
  float rating = 0.0f;
};

struct SelectedDownloadProbe {
  std::string rid;
  std::string modeName;
};

// Selects the best probe download candidate using the same matching rank used by downloads.
static SelectedDownloadProbe SelectBestDownloadCandidateProbe(
    const std::string &requestedFixtureName,
    const std::string &requestedMode,
    int requestedFootprint,
    const std::vector<DownloadCandidateProbe> &candidates) {
  SelectedDownloadProbe selected;
  matching::DownloadCandidateRank bestRank;
  for (const auto &candidate : candidates) {
    const auto nameTier = matching::ComputeFixtureNameMatchTier(candidate.fixtureName,
                                                               requestedFixtureName);
    if (nameTier == matching::FixtureNameMatchTier::None)
      continue;

    const auto modeScore = matching::ComputeGdtfModeMatchScore(
        requestedMode, candidate.modes, requestedFootprint);
    matching::DownloadCandidateRank rank{nameTier, modeScore.footprintMatch, false,
                                         candidate.recency, candidate.rating};
    rank.modeTier = modeScore.tier;
    if (!matching::IsBetterDownloadCandidate(rank, bestRank))
      continue;

    bestRank = rank;
    selected = {candidate.rid, modeScore.modeName};
  }
  return selected;
}

// Verifies 12CH digit signatures keep Standard mode requests aligned with mode-aware fixtures.
static void VerifyStandardModeDigitSignatureBeatsNewerFootprintOnlyMatch() {
  const std::vector<DownloadCandidateProbe> candidates = {
      {"fixture-standard", "BLED", {{"Basic", 8}, {"Touring 12CH", 24}}, 100, 3.0f},
      {"fixture-newer", "BLED", {{"Economy", 12}}, 200, 5.0f},
  };

  const auto selected = SelectBestDownloadCandidateProbe(
      "BLED", "Standard mode 12CH", 12, candidates);

  assert(selected.rid == "fixture-standard");
  assert(selected.modeName == "Touring 12CH");
}

// Verifies simple named modes without digits outrank footprint-only alternatives.
static void VerifyBasicModeNameBeatsFootprintOnlyMatch() {
  const std::vector<DownloadCandidateProbe> candidates = {
      {"fixture-basic", "Wash 600", {{"Basic", 16}, {"Extended", 24}}, 100, 3.0f},
      {"fixture-footprint", "Wash 600", {{"Arc Layout", 16}}, 250, 5.0f},
  };

  const auto selected = SelectBestDownloadCandidateProbe(
      "Wash 600", "Basic", 16, candidates);

  assert(selected.rid == "fixture-basic");
  assert(selected.modeName == "Basic");
}

// Verifies fixture-specific named modes stay paired with the matching catalog mode.
static void VerifyFixtureSpecificNamedModeStaysAligned() {
  const std::vector<DownloadCandidateProbe> candidates = {
      {"fixture-shapes", "Aleda K10 B-EYE", {{"B-EYE Shapes", 35}, {"Standard", 21}}, 100, 3.0f},
      {"fixture-digits", "Aleda K10 B-EYE", {{"Pixel 35CH", 35}, {"Standard", 21}}, 300, 5.0f},
  };

  const auto selected = SelectBestDownloadCandidateProbe(
      "Aleda K10 B-EYE", "B-EYE Shapes", 35, candidates);

  assert(selected.rid == "fixture-shapes");
  assert(selected.modeName == "B-EYE Shapes");
}

// Runs focused coverage for MVR-requested GDTF import matching identity selection.
int main() {
  VerifyDistinctOriginalNamesBeatSharedPlaceholderMetadata();
  VerifySpecBasenameFallback();
  VerifyResolvedTypeFallback();
  VerifyExactNameWithoutFootprintBeatsPartialNameWithFootprint();
  VerifyManufacturerMatchBeatsRecencyInsideSameNameAndFootprintTier();
  VerifyStandardModeDigitSignatureBeatsNewerFootprintOnlyMatch();
  VerifyBasicModeNameBeatsFootprintOnlyMatch();
  VerifyFixtureSpecificNamedModeStaysAligned();
  return 0;
}

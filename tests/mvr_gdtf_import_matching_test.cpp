#include "gdtf_catalog_matcher.h"
#include "gdtf_import_matching.h"

#include <cassert>
#include <string>
#include <vector>

namespace catalog = mvr::gdtf_catalog_matcher;
namespace import_matching = mvr::gdtf_import_matching;

struct DownloadRequestProbe {
  std::string requestedFixtureName;
  std::string resolvedFixtureTypeName;
};

// Verifies that original MVR fixture names drive download matching when embedded metadata is generic.
static void VerifyDistinctOriginalNamesBeatSharedPlaceholderMetadata() {
  const std::string placeholderType = "BLED Standard mode 12CH";
  const std::vector<DownloadRequestProbe> requests = {
      {import_matching::SelectRequestedFixtureName("Aleda K10 B-EYE",
                                                   "Fixtures/BLED Standard mode 12CH.gdtf"),
       placeholderType},
      {import_matching::SelectRequestedFixtureName("Super Storm 1500",
                                                   "Fixtures/BLED Standard mode 12CH.gdtf"),
       placeholderType},
  };

  assert(catalog::SelectDownloadSearchFixtureName(requests[0].requestedFixtureName,
                                                  requests[0].resolvedFixtureTypeName) ==
         "Aleda K10 B-EYE");
  assert(catalog::SelectDownloadSearchFixtureName(requests[1].requestedFixtureName,
                                                  requests[1].resolvedFixtureTypeName) ==
         "Super Storm 1500");
  assert(catalog::SelectDownloadSearchFixtureName(requests[0].requestedFixtureName,
                                                  requests[0].resolvedFixtureTypeName) !=
         placeholderType);
  assert(catalog::SelectDownloadSearchFixtureName(requests[1].requestedFixtureName,
                                                  requests[1].resolvedFixtureTypeName) !=
         placeholderType);
}

// Verifies that the GDTFSpec basename is used when the fixture node name is unavailable.
static void VerifySpecBasenameFallback() {
  assert(import_matching::SelectRequestedFixtureName("", "Folder/Super Storm 1500.gdtf") ==
         "Super Storm 1500");
  assert(import_matching::SelectRequestedFixtureName("\t\n", "Folder\\Aleda K10 B-EYE.gdtf") ==
         "Aleda K10 B-EYE");
}

// Verifies that resolved metadata remains the final fallback when no original MVR identity exists.
static void VerifyResolvedTypeFallback() {
  assert(catalog::SelectDownloadSearchFixtureName("", "BLED Standard mode 12CH") ==
         "BLED Standard mode 12CH");
}


// Verifies missing GDTF files keep the declared spec identity available as a fixture type.
static void VerifyMissingGdtfTypeFallbackPrefersSpecBasename() {
  assert(import_matching::SelectFallbackFixtureTypeName(
             "Fixture 101", "Fixtures/Super Storm 1500.gdtf") ==
         "Super Storm 1500");
  assert(import_matching::SelectFallbackFixtureTypeName(
             "Aleda K10 B-EYE", "") ==
         "Aleda K10 B-EYE");
}

// Verifies name confidence outranks footprint matches.
static void VerifyExactNameWithoutFootprintBeatsPartialNameWithFootprint() {
  const auto exactTier = catalog::ComputeFixtureNameMatchTier("My Beam 200",
                                                             "My Beam 200");
  const auto partialTier = catalog::ComputeFixtureNameMatchTier("My Beam 200 Extended",
                                                               "My Beam 200");
  const catalog::DownloadCandidateRank exactNoFootprint{
      exactTier, false, false, 100, 0.0f};
  const catalog::DownloadCandidateRank partialWithFootprint{
      partialTier, true, false, 200, 5.0f};

  assert(exactTier == catalog::FixtureNameMatchTier::ExactNormalized);
  assert(partialTier == catalog::FixtureNameMatchTier::Partial);
  assert(catalog::IsBetterDownloadCandidate(exactNoFootprint, partialWithFootprint));
  assert(!catalog::IsBetterDownloadCandidate(partialWithFootprint, exactNoFootprint));
}

// Verifies manufacturer matches outrank recency ties.
static void VerifyManufacturerMatchBeatsRecencyInsideSameNameAndFootprintTier() {
  const auto tier = catalog::ComputeFixtureNameMatchTier("My Beam 200", "My Beam 200");
  const catalog::DownloadCandidateRank manufacturerMatch{
      tier, true, true, 100, 0.0f};
  const catalog::DownloadCandidateRank newerWithoutManufacturer{
      tier, true, false, 200, 5.0f};

  assert(catalog::IsBetterDownloadCandidate(manufacturerMatch,
                                            newerWithoutManufacturer));
  assert(!catalog::IsBetterDownloadCandidate(newerWithoutManufacturer,
                                             manufacturerMatch));
}

// Verifies 12CH digit signatures keep Standard mode requests aligned with mode-aware fixtures.
static void VerifyStandardModeDigitSignatureBeatsNewerFootprintOnlyMatch() {
  const std::vector<catalog::GdtfCatalogEntry> candidates = {
      {"fixture-standard", "", "BLED", {{"Basic", 8}, {"Touring 12CH", 24}}, 100, 3.0f},
      {"fixture-newer", "", "BLED", {{"Economy", 12}}, 200, 5.0f},
  };

  const auto selected = catalog::SelectBestDownloadMatch(
      "BLED", "", "Standard mode 12CH", "", 12, candidates);

  assert(selected.rid == "fixture-standard");
  assert(selected.modeName == "Touring 12CH");
  assert(selected.selectionReason == "name+mode-digits");
}

// Verifies simple named modes without digits outrank footprint-only alternatives.
static void VerifyBasicModeNameBeatsFootprintOnlyMatch() {
  const std::vector<catalog::GdtfCatalogEntry> candidates = {
      {"fixture-basic", "", "Wash 600", {{"Basic", 16}, {"Extended", 24}}, 100, 3.0f},
      {"fixture-footprint", "", "Wash 600", {{"Arc Layout", 16}}, 250, 5.0f},
  };

  const auto selected = catalog::SelectBestDownloadMatch(
      "Wash 600", "", "Basic", "", 16, candidates);

  assert(selected.rid == "fixture-basic");
  assert(selected.modeName == "Basic");
  assert(selected.selectionReason == "name+mode");
}

// Verifies fixture-specific named modes stay paired with the matching catalog mode.
static void VerifyFixtureSpecificNamedModeStaysAligned() {
  const std::vector<catalog::GdtfCatalogEntry> candidates = {
      {"fixture-shapes", "", "Aleda K10 B-EYE", {{"B-EYE Shapes", 35}, {"Standard", 21}}, 100, 3.0f},
      {"fixture-digits", "", "Aleda K10 B-EYE", {{"Pixel 35CH", 35}, {"Standard", 21}}, 300, 5.0f},
  };

  const auto selected = catalog::SelectBestDownloadMatch(
      "Aleda K10 B-EYE", "", "B-EYE Shapes", "", 35, candidates);

  assert(selected.rid == "fixture-shapes");
  assert(selected.modeName == "B-EYE Shapes");
  assert(selected.selectionReason == "name+mode");
}

// Verifies manufacturer evidence is applied during final catalog selection.
static void VerifyFinalSelectionUsesManufacturerTieBreaker() {
  const std::vector<catalog::GdtfCatalogEntry> candidates = {
      {"wrong-brand", "Other Lighting", "My Beam 200", {{"Basic", 16}}, 300, 5.0f},
      {"right-brand", "Acme GmbH", "My Beam 200", {{"Basic", 16}}, 100, 3.0f},
  };

  const auto selected = catalog::SelectBestDownloadMatch(
      "My Beam 200", "", "Basic", "Acme", 16, candidates);

  assert(selected.rid == "right-brand");
  assert(selected.modeName == "Basic");
  assert(selected.selectionReason == "name+mode");
}

// Runs focused coverage for MVR-requested GDTF import and catalog matching selection.
int main() {
  VerifyDistinctOriginalNamesBeatSharedPlaceholderMetadata();
  VerifySpecBasenameFallback();
  VerifyResolvedTypeFallback();
  VerifyMissingGdtfTypeFallbackPrefersSpecBasename();
  VerifyExactNameWithoutFootprintBeatsPartialNameWithFootprint();
  VerifyManufacturerMatchBeatsRecencyInsideSameNameAndFootprintTier();
  VerifyStandardModeDigitSignatureBeatsNewerFootprintOnlyMatch();
  VerifyBasicModeNameBeatsFootprintOnlyMatch();
  VerifyFixtureSpecificNamedModeStaysAligned();
  VerifyFinalSelectionUsesManufacturerTieBreaker();
  return 0;
}

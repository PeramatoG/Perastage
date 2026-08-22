#include "gdtf_catalog_matcher.h"
#include "gdtf_catalog_parser.h"
#include "gdtf_import_matching.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

namespace catalog = mvr::gdtf_catalog_matcher;
namespace parser = mvr::gdtf_catalog_parser;
namespace import_matching = mvr::gdtf_import_matching;

// Verifies authoritative GDTF identity wins over an unrelated instance alias.
static void VerifyAuthoritativeIdentityWins() {
  const std::vector<catalog::GdtfCatalogEntry> entries = {
      {"x", "Stage 4", "X-STROBE", {}, 500, 5.0f},
      {"pixel", "Prolight Spain", "PIXEL STROBE 400 RGB", {}, 100, 3.0f}};
  const auto match = catalog::SelectBestDownloadMatch(
      "X-STROBE", "PIXEL STROBE 400 RGB", "", "Prolight Spain", 0, entries);
  assert(match.rid == "pixel");
  assert(match.selectionReason.find("authoritative") != std::string::npos);
}

// Verifies exact model identities remain selectable despite differing object names.
static void VerifyExactMacModelsWin() {
  const std::vector<catalog::GdtfCatalogEntry> entries = {
      {"variant", "Martin", "Martin Mac Quantum Profile", {}, 500, 5.0f},
      {"quantum", "Martin", "MAC Quantum Profile", {}, 100, 3.0f},
      {"axiom", "Martin", "MAC Axiom Hybrid", {}, 100, 3.0f}};
  assert(catalog::SelectBestDownloadMatch("Stage Left", "MAC Quantum Profile", "",
                                         "Martin", 0, entries).rid == "quantum");
  assert(catalog::SelectBestDownloadMatch("Stage Right", "MAC Axiom Hybrid", "",
                                         "Martin", 0, entries).rid == "axiom");
}

// Verifies generic referenced identities can still be rescued by a useful alias.
static void VerifyPlaceholderAliasRescue() {
  const std::vector<catalog::GdtfCatalogEntry> entries = {
      {"placeholder", "", "BLED Standard mode 12CH", {}, 500, 5.0f},
      {"real", "Clay Paky", "Aleda K10 B-EYE", {}, 100, 3.0f}};
  const auto match = catalog::SelectBestDownloadMatch(
      "Aleda K10 B-EYE", "BLED Standard mode 12CH", "", "Clay Paky", 0, entries);
  assert(match.rid == "real");
}

// Verifies numeric model tokens are retained and disagreements are not hard vetoes.
static void VerifyNumericModelEvidence() {
  assert(catalog::BuildCoreFixtureNameKey("Beam 200") !=
         catalog::BuildCoreFixtureNameKey("Beam 2000"));
  assert(catalog::BuildCoreFixtureNameKey("K10") == "k10");
  assert(catalog::BuildCoreFixtureNameKey("Storm 1500") == "storm1500");
  assert(catalog::ComputeFixtureNameMatchTier("PIXEL STROBE 400 RGB",
                                             "PIXEL STROBE 400 RGB") ==
         catalog::FixtureNameMatchTier::ExactNormalized);
  assert(catalog::ComputeFixtureNameMatchTier("BLINDER 400", "Blinder 4 PRO") !=
         catalog::FixtureNameMatchTier::None);
}

// Verifies official GDTF Share mode fields survive parsing and affect selection.
static void VerifyOfficialCatalogContractAndModeRanking() {
  const std::string payload = R"json({
    "result": true, "timestamp": 1672531200, "list": [
      {"rid": 12345, "fixture": "Example Fixture",
       "manufacturer": "Example Manufacturer", "lastModified": "1672531200",
       "rating": "4.5", "modes": [
         {"name": "Mode 8ch", "dmxfootprint": 8},
         {"name": "Mode 30ch", "dmxfootprint": 30}]},
      {"rid": 99999, "fixture": "Example Fixture",
       "manufacturer": "Other", "lastModified": "1999999999", "rating": "5",
       "modes": [{"name": "Mode 8ch", "dmxfootprint": 8}]}
    ]})json";
  const auto entries = parser::ParseCatalogEntries(payload);
  assert(entries.size() == 2);
  assert(entries[0].rid == "12345");
  assert(entries[0].fixtureName == "Example Fixture");
  assert(entries[0].manufacturer == "Example Manufacturer");
  assert(entries[0].modes.size() == 2);
  assert(entries[0].modes[0].name == "Mode 8ch" && entries[0].modes[0].footprint == 8);
  assert(entries[0].modes[1].name == "Mode 30ch" && entries[0].modes[1].footprint == 30);
  const auto match = catalog::SelectBestDownloadMatch(
      "Object 1", "Example Fixture", "Mode 30ch", "Example Manufacturer Ltd", 30,
      entries);
  assert(match.rid == "12345");
  assert(match.modeName == "Mode 30ch");
}

// Verifies structured GDTF manufacturer metadata reaches the real request builder.
static void VerifyManufacturerPropagation() {
  const auto request = import_matching::BuildDownloadRequest(
      "Instance 12", "Beam 200", "Basic", "Acme GmbH", 16);
  assert(request.manufacturer == "Acme GmbH");
  const std::vector<catalog::GdtfCatalogEntry> entries = {
      {"new-wrong", "Other", "Beam 200", {{"Basic", 16}}, 500, 5.0f},
      {"old-right", "Acme Ltd", "Beam 200", {{"Basic", 16}}, 100, 3.0f}};
  assert(catalog::SelectBestDownloadMatch(request, entries).rid == "old-right");
}

// Verifies catalog selection is stable when otherwise equivalent entries reorder.
static void VerifyDeterministicTieBreak() {
  std::vector<catalog::GdtfCatalogEntry> entries = {
      {"b", "", "Beam 200", {}, 100, 4.0f},
      {"a", "", "Beam 200", {}, 100, 4.0f}};
  assert(catalog::SelectBestDownloadMatch("", "Beam 200", "", "", 0, entries).rid == "a");
  std::reverse(entries.begin(), entries.end());
  assert(catalog::SelectBestDownloadMatch("", "Beam 200", "", "", 0, entries).rid == "a");
}

// Verifies MVR spec extraction remains independent from the object alias.
static void VerifyMvrIdentityExtraction() {
  assert(import_matching::ExtractFixtureNameFromGdtfSpec(
             "Fixtures\\MAC Quantum Profile.gdtf") == "MAC Quantum Profile");
  assert(import_matching::SelectFallbackFixtureTypeName(
             "Fixture 101", "Fixtures/Beam 200.gdtf") == "Beam 200");
}

// Runs focused MVR GDTF catalog parsing and semantic matching coverage.
int main() {
  VerifyAuthoritativeIdentityWins();
  VerifyExactMacModelsWin();
  VerifyPlaceholderAliasRescue();
  VerifyNumericModelEvidence();
  VerifyOfficialCatalogContractAndModeRanking();
  VerifyManufacturerPropagation();
  VerifyDeterministicTieBreak();
  VerifyMvrIdentityExtraction();
  return 0;
}

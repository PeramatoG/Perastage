#include "gdtf_catalog_matcher.h"
#include "gdtf_catalog_parser.h"
#include "gdtf_import_matching.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <random>
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
  const auto compatibility = [](const std::string &lhs, const std::string &rhs) {
    return catalog::ComputeNumericTokenCompatibility(
        catalog::BuildCanonicalFixtureModel(lhs),
        catalog::BuildCanonicalFixtureModel(rhs));
  };
  assert(compatibility("Beam 200", "Beam 200") ==
         catalog::NumericTokenCompatibility::Exact);
  assert(compatibility("Beam 200", "Beam 2000") ==
         catalog::NumericTokenCompatibility::Different);
  assert(compatibility("XBeam 17", "XBeam 17 V2") ==
         catalog::NumericTokenCompatibility::Exact);
  assert(compatibility("XBeam 17", "XBeam 19") ==
         catalog::NumericTokenCompatibility::Different);
  assert(compatibility("K15", "K-15") ==
         catalog::NumericTokenCompatibility::Exact);
  assert(compatibility("K15", "K25") ==
         catalog::NumericTokenCompatibility::Different);
  assert(compatibility("Beam", "Beam 200") ==
         catalog::NumericTokenCompatibility::Missing);
}

// Verifies noisy descriptive collisions cannot establish fixture identity.
static void VerifyConservativeNoisyCatalogMatching() {
  std::vector<catalog::GdtfCatalogEntry> macEntries = {
      {"wrong-new", "Vari-Lite", "VL3600 Profile IP", {{"Mode", 40}}, 900, 5.0f},
      {"wrong-profile", "Other", "Example Profile", {{"Mode", 40}}, 800, 5.0f},
      {"correct", "Martin", "Martin MAC Quantum Profile", {}, 100, 3.0f}};
  const auto mac = catalog::SelectBestDownloadMatch(
      "", "MAC Quantum Profile (Bulb=LED)", "Mode", "Martin Professional", 40,
      macEntries);
  assert(mac.found && mac.rid == "correct");
  macEntries.resize(2);
  assert(!catalog::SelectBestDownloadMatch(
              "", "MAC Quantum Profile (Bulb=LED)", "Mode", "Martin", 40,
              macEntries).found);

  assert(!catalog::SelectBestDownloadMatch(
              "", "LED-BL4 (Bulb=LED)", "", "", 0,
              {{"bulb", "Astera", "Astera NYX Bulb", {}, 900, 5.0f}}).found);
  assert(!catalog::SelectBestDownloadMatch(
              "", "RoHS 18x10 LED Par RGBWA (Bulb=LED)", "", "", 0,
              {{"rgbwa", "Other", "Tourstick 72 RGBWA", {}, 900, 5.0f}}).found);
  std::vector<catalog::GdtfCatalogEntry> ambiguous = {
      {"a", "", "Tour Hazer II Aqua", {}, 100, 4.0f},
      {"b", "", "Tour Hazer II Pro", {}, 200, 5.0f}};
  assert(!catalog::SelectBestDownloadMatch(
              "", "Tour Hazer 2", "", "", 0, ambiguous).found);
  std::reverse(ambiguous.begin(), ambiguous.end());
  assert(!catalog::SelectBestDownloadMatch(
              "", "Tour Hazer 2", "", "", 0, ambiguous).found);
}

// Verifies useful structural fuzzy matches remain eligible without aliases.
static void VerifyUsefulFuzzyMatches() {
  const auto matches = [](const std::string &request, const std::string &manufacturer,
                          const std::string &candidate) {
    return catalog::SelectBestDownloadMatch(
        "", request, "", manufacturer, 0,
        {{"candidate", manufacturer, candidate, {}, 1, 1.0f}}).found;
  };
  assert(matches("XBeam 17 CMY (Bulb=Sirius HRI 440W)", "Clay Paky",
                 "XBEAM 17 V2"));
  assert(matches("HY B-EYE K15", "Clay Paky", "HY B-EYE K-15 Aqua"));
  assert(matches("MDG The Fan", "MDG", "MDG / theFAN"));
  assert(matches("Tour Hazer 2", "", "Tour Hazer II"));
}

// Verifies official GDTF Share mode fields survive parsing and affect selection.
static void VerifyOfficialCatalogContractAndModeRanking() {
  const std::string payload = R"json({
    "result": true, "timestamp": 1672531200, "list": [
      {"rid": 12345, "fixture": "Example Fixture",
       "manufacturer": "Example Manufacturer", "uuid": "fixture-type-id",
       "uploader": "Example Manufacturer", "lastModified": "1672531200",
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
  assert(entries[0].uuid == "fixture-type-id");
  assert(entries[0].uploader == "Example Manufacturer");
  assert(entries[0].modes.size() == 2);
  assert(entries[0].modes[0].name == "Mode 8ch" && entries[0].modes[0].footprint == 8);
  assert(entries[0].modes[1].name == "Mode 30ch" && entries[0].modes[1].footprint == 30);
  const auto match = catalog::SelectBestDownloadMatch(
      "Object 1", "Example Fixture", "Mode 30ch", "Example Manufacturer Ltd", 30,
      entries);
  assert(match.rid == "12345");
  assert(match.modeName == "Mode 30ch");
}

// Verifies search and automatic matching consume one shared MAC catalog parse.
static void VerifySharedMacCatalogPipeline() {
  const std::string payload = R"json({"data":{"list":[
    {"rid":"black","manufacturer":"Black Light Design","fixture":"Martin MAC Quantum Profile","uuid":"share-black","uploader":"Black Light Design","modes":[{"name":"Standard","dmxfootprint":42}],"lastModified":100,"rating":"3"},
    {"rid":"lpl","manufacturer":"LPL","fixture":"MAC Quantum Profile","uuid":"share-lpl","uploader":"LPL","modes":[{"name":"Standard","dmxfootprint":42}],"lastModified":200,"rating":"4"},
    {"rid":"official","manufacturer":"Martin Professional","fixture":"MAC Quantum Profile","uuid":"share-official","uploader":"Martin Professional","modes":[{"name":"Standard","dmxfootprint":42}],"lastModified":300,"rating":"5"},
    {"rid":"ogson","manufacturer":"OGSON fixtures","fixture":"Martin MAC Quantum Profile","uuid":"share-ogson","uploader":"OGSON fixtures","modes":[{"name":"Standard","dmxfootprint":42}],"lastModified":400,"rating":"4"},
    {"rid":"wrong","manufacturer":"Vari-Lite","fixture":"VL3600 Profile IP","lastModified":999,"rating":"5"},
    {"rid":"other","manufacturer":"Other","fixture":"Example Profile","lastModified":999,"rating":"5"},
    {"manufacturer":"Martin Professional","fixture":"MAC Quantum Profile","uuid":"display-only"}
  ]}})json";
  const auto parsed = parser::ParseCatalog(payload);
  assert(parsed.entries.size() == 7);
  assert(parsed.payloadBytes == payload.size());
  assert(!parsed.payloadFingerprint.empty());
  const auto searchMatches = parser::FilterCatalogEntries(
      parsed.entries, "", "mac quantum profile");
  assert(searchMatches.size() == 5);
  assert(!parsed.entries[searchMatches.back()].downloadable);
  for (std::size_t index = 0; index < 4; ++index) {
    const auto &entry = parsed.entries[searchMatches[index]];
    assert(entry.downloadable && !entry.rid.empty());
    assert(!entry.uuid.empty() && !entry.uploader.empty());
    assert(entry.modes.size() == 1 && entry.modes.front().footprint == 42);
  }

  const auto select = [&](const std::string &manufacturer) {
    return catalog::SelectBestDownloadMatch(
        "", "MAC Quantum Profile (Bulb=LED)", "Standard", manufacturer, 42,
        parsed.entries);
  };
  assert(select("Martin Professional").rid == "official");
  assert(select("Martin").rid == "official");
  const auto unknownManufacturer = select("");
  assert(unknownManufacturer.found && unknownManufacturer.rid == "official");

  std::vector<catalog::GdtfCatalogEntry> shuffled = parsed.entries;
  std::mt19937 generator(7);
  for (int iteration = 0; iteration < 20; ++iteration) {
    std::shuffle(shuffled.begin(), shuffled.end(), generator);
    assert(catalog::SelectBestDownloadMatch(
               "", "MAC Quantum Profile (Bulb=LED)", "Standard", "", 42,
               shuffled).rid == unknownManufacturer.rid);
  }
}

// Verifies top-level and compatibility wrapper shapes use the same parser.
static void VerifyCatalogWrapperCompatibility() {
  const std::string record =
      R"json({"rid":"1","manufacturer":"Acme","fixture":"Beam 200"})json";
  assert(parser::ParseCatalog("[" + record + "]").entries.size() == 1);
  assert(parser::ParseCatalog("{\"data\":{\"list\":[" + record + "]}}")
             .entries.size() == 1);
  assert(parser::ParseCatalog("{\"results\":{\"items\":[" + record + "]}}")
             .entries.size() == 1);
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
  std::mt19937 generator(42);
  for (int iteration = 0; iteration < 20; ++iteration) {
    std::shuffle(entries.begin(), entries.end(), generator);
    assert(catalog::SelectBestDownloadMatch("", "Beam 200", "", "", 0, entries).rid == "a");
  }
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
  VerifyConservativeNoisyCatalogMatching();
  VerifyUsefulFuzzyMatches();
  VerifyOfficialCatalogContractAndModeRanking();
  VerifySharedMacCatalogPipeline();
  VerifyCatalogWrapperCompatibility();
  VerifyManufacturerPropagation();
  VerifyDeterministicTieBreak();
  VerifyMvrIdentityExtraction();
  return 0;
}

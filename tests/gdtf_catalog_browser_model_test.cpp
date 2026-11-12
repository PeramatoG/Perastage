#include "gdtf_catalog_browser_model.h"
#include "../mvr/gdtf_catalog_parser.h"
#include <cassert>
#include <algorithm>
#include "json.hpp"
#include <cmath>

namespace parser = mvr::gdtf_catalog_parser;
namespace browser = gdtf_catalog_browser;

// Exercise the public parser so text, metadata and modes share the real path.
static void VerifyRatings() {
  using Json = nlohmann::json;
  const auto parseRating = [](const Json &rating) {
    return parser::ParseCatalog(Json{{"list", Json::array({
        {{"rid", "1"}, {"fixture", "Spot"}, {"manufacturer", "Acme"},
         {"rating", rating}, {"creator", "designer"},
         {"modes", Json::array({{{"name", "Standard"}, {"dmxfootprint", 12}}})}}
    })}}.dump());
  };
  for (const auto &text : {"N/A", "", "invalid", "4.5invalid", "4.5 ",
                           "nan", "NaN", "inf", "-Infinity", "1e1000",
                           "-1e1000", "3.5e38", "1e-1000", "4e", "+-4", "4,5"}) {
    const auto result = parseRating(text);
    assert(result.IsUsable() && result.entries.size() == 1);
    const auto &entry = result.entries[0];
    assert(entry.rating == 0 && entry.ratingText == text);
    assert(entry.manufacturer == "Acme" && entry.creator == "designer");
    assert(entry.modes.size() == 1 && entry.modes[0].name == "Standard");
    assert(entry.modes[0].footprint == 12 && entry.downloadable);
  }
  for (const auto &rating : {Json(4.5), Json("4.5"), Json("+4.5"),
                             Json(" \t4.5"), Json("45e-1")}) {
    const auto result = parseRating(rating);
    assert(result.entries[0].rating == 4.5f);
    assert(result.entries[0].ratingText ==
           (rating.is_string() ? rating.get<std::string>() : rating.dump()));
  }
  for (const auto &rating : {Json(), Json(true), Json::object(), Json(1e100),
                             Json(-1e100)})
    assert(parseRating(rating).entries[0].rating == 0);
  const auto numeric = parseRating(4.5);
  const auto text = parseRating("4.5");
  assert(parser::CatalogsEquivalent(numeric, text));
  assert(numeric.entries[0].rating == text.entries[0].rating);

  Json rows = Json::array();
  for (int i = 0; i < 12000; ++i) {
    Json row = {{"rid", i}, {"fixture", "Spot"},
                {"modes", Json::array({{{"name", "Standard"}, {"dmxfootprint", 12}}})}};
    if (i % 3 == 0) row["rating"] = "N/A";
    if (i % 3 == 1) row["rating"] = "invalid";
    rows.push_back(std::move(row));
  }
  const auto bulk = parser::ParseCatalog(rows.dump());
  assert(bulk.IsUsable() && bulk.usableEntryCount == 12000);
  assert(bulk.entries.size() == 12000);
  for (std::size_t i = 0; i < bulk.entries.size(); ++i) {
    const auto &entry = bulk.entries[i];
    assert(entry.rating == 0 && std::isfinite(entry.rating));
    assert(entry.ratingText == (i % 3 == 0 ? "N/A" : i % 3 == 1 ? "invalid" : ""));
    assert(entry.modes[0].footprint == 12);
  }
}

int main() {
  VerifyRatings();
  const auto parsed = parser::ParseCatalog(R"({"timestamp":1700000000,"list":[
    {"rid":17,"manufacturer":"Martin Professional","fixture":"VDO Sceptron 10",
     "revision":"Tour 2026","uuid":"abc-123-def","filesize":"1048576",
     "creationDate":1690000000,"lastModified":"1700000000","uploader":"Manuf.",
     "creator":"designer","version":"1.2","rating":"N/A",
     "modes":[{"name":"Extended RGB","dmxfootprint":"36"}]},
    {"rid":"18","manufacturer":"Other","fixture":"Sceptron","uuid":null,
     "filesize":2048,"rating":4.5,"modes":[{"name":"Standard","dmxfootprint":12}]},
    {"rid":"19","fixture":"Spot","filesize":null},
    {"rid":"20","fixture":"Wash","filesize":"invalid","rating":null}
  ]})");
  assert(parsed.IsUsable() && parsed.catalogTimestamp == "1700000000");
  const auto &entry = parsed.entries.front();
  assert(entry.fileSizeBytes == 1048576);
  assert(entry.creationDate == "1690000000" && entry.lastModifiedUnix == 1700000000);
  assert(entry.creator == "designer" && entry.uploader == "Manuf." && entry.version == "1.2");
  assert(entry.modes[0].name == "Extended RGB" && entry.modes[0].footprint == 36);
  assert(entry.ratingText == "N/A" && entry.rating == 0);
  assert(parsed.entries[1].uuid.empty() && parsed.entries[1].fileSizeBytes == 2048);
  assert(parsed.entries[1].modes[0].footprint == 12);
  assert(!parsed.entries[2].fileSizeBytes && parsed.entries[2].ratingText.empty());
  assert(!parsed.entries[3].fileSizeBytes && parsed.entries[3].ratingText.empty());
  assert(parser::ParseCatalog(R"({"timestamp":"1700000000","list":[{"rid":1,"fixture":"Spot"}]})")
             .catalogTimestamp == "1700000000");
  const auto legacy = parser::ParseCatalog(R"({"data":{"rows":[
    {"revisionId":1,"brand":"Legacy","model":"Spot","userName":"owner",
     "dmxModes":[{"name":"Mode","dmxFootprint":"16"}]}]}})");
  assert(legacy.IsUsable() && legacy.catalogTimestamp.empty());
  assert(legacy.entries[0].creator == "owner" && legacy.entries[0].modes[0].footprint == 16);
  for (const auto &invalid : {"-1", "\"12garbage\"", "18446744073709551615", "1e40"}) {
    const auto result = parser::ParseCatalog(
        "{\"list\":[{\"rid\":1,\"fixture\":\"Spot\",\"filesize\":" + std::string(invalid) + "}]}");
    assert(result.IsUsable() && !result.entries[0].fileSizeBytes);
  }

  browser::SearchIndex index(parsed.entries);
  const std::vector<std::size_t> first{0};
  assert(index.Search({"martin sceptron", {}, {}}) == first);
  assert(index.Search({" SCEPTRON\tMartin\n", {}, {}}) == first);
  assert(index.Search({"123-def", {}, {}}) == first);
  assert(index.Search({"sceptron tour", {}, {}}) == first);
  assert(index.Search({"tour sceptron", {}, {}}) == first);
  assert(index.Search({"extended martin", {}, {}}) == first);
  assert(index.Search({"rgb", "martin", "vdo"}) == first);
  assert(index.Search({"martin", "other", {}}).empty());
  assert(index.Search({"martin unknown", {}, {}}).empty());
  assert(index.Search({{}, {}, "sceptron"}).size() == 2);
  assert(index.Search({{}, {}, {}}).size() == 4);

  auto refreshed = parsed;
  refreshed.catalogTimestamp = "different-marker";
  refreshed.payloadFingerprint.clear();
  assert(parser::CatalogsEquivalent(parsed, refreshed));
  std::rotate(refreshed.entries.begin(), refreshed.entries.begin() + 1, refreshed.entries.end());
  assert(!parser::CatalogsEquivalent(parsed, refreshed));
  browser::SearchIndex refreshedIndex(refreshed.entries);
  const auto matches = refreshedIndex.Search({});
  assert(refreshedIndex.PageForRid(matches, "17", 2) == 1);
  assert(!refreshedIndex.PageForRid(matches, "missing", 2));
  assert(!refreshedIndex.PageForRid(matches, "17", 0));
  auto changedMetadata = parsed;
  changedMetadata.payloadFingerprint.clear();
  changedMetadata.entries[0].fileSizeBytes = 2;
  assert(!parser::CatalogsEquivalent(parsed, changedMetadata));
  changedMetadata = parsed;
  changedMetadata.payloadFingerprint.clear();
  changedMetadata.entries[0].modes[0].footprint = 99;
  assert(!parser::CatalogsEquivalent(parsed, changedMetadata));

  assert(browser::FormatFileSize(1048576) == "1.0 MiB");
  assert(browser::FormatFileSize(12) == "12 B");
  assert(browser::FormatFileSize(std::nullopt).empty());
  assert(browser::FormatFileSize(0).empty());
  assert(!browser::MeaningfulRating("N/A") && !browser::MeaningfulRating(""));
  assert(!browser::MeaningfulRating("nan") && !browser::MeaningfulRating("4oops"));
  assert(browser::MeaningfulRating("4.5") == "4.5");
  assert(browser::MeaningfulRating("0") == "0");
}

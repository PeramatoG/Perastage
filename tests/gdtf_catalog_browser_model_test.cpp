#include "gdtf_catalog_browser_model.h"
#include "../mvr/gdtf_catalog_parser.h"
#include <cassert>
#include <algorithm>

namespace parser = mvr::gdtf_catalog_parser;
namespace browser = gdtf_catalog_browser;

int main() {
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

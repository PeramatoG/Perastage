#include "gdtf_resolution.h"
#include "gdtf_patch_evidence.h"
#include "gdtf_import_matching.h"
#include <algorithm>
#include <cassert>
#include <random>

namespace catalog = mvr::gdtf_catalog_matcher;
using namespace gdtf_resolution;

static catalog::GdtfDownloadRequest Request(const std::string &model,
    const std::string &manufacturer = "Custom", const std::string &mode = "Standard",
    int footprint = 16) {
  return mvr::gdtf_import_matching::BuildDownloadRequest(
      "Instance 12", model, mode, manufacturer, footprint);
}

static void RankingRegressions() {
  std::vector<catalog::GdtfCatalogEntry> entries = {
    {"old", "Martin Professional", "MAC Aura XB", {{"Standard", 16}}, 10, 1},
    {"new", "Martin Professional", "MAC Aura XB", {{"Standard", 16}}, 20, 2},
    {"wrong-mode", "Martin", "MAC Aura XB", {{"Extended", 25}}, 50, 5},
    {"variant", "Martin", "MAC Aura", {{"Standard", 16}}, 60, 5},
    {"third-party", "Other", "Martin MAC Aura XB", {{"Standard",16}}, 999, 5},
    {"unrelated", "Other", "Beam 200", {{"Standard", 16}}, 999, 5}};
  auto result = catalog::RankDownloadCandidates(Request("Martin Mac Aura XB"), entries);
  assert(result.recommendedRid == "new");
  assert(result.confidence == catalog::MatchConfidence::Safe);
  assert(result.candidates.size() == 5);
  assert(result.candidates.front().selectedMode == "Standard");
  assert(result.candidates.front().footprint == 16);
  assert(std::find(result.candidates.front().reasons.begin(),
                   result.candidates.front().reasons.end(), "manufacturer-inferred-from-prefix") !=
         result.candidates.front().reasons.end());
  std::mt19937 random(15);
  for (int i = 0; i < 12; ++i) {
    std::shuffle(entries.begin(), entries.end(), random);
    auto shuffled = catalog::RankDownloadCandidates(Request("Martin Mac Aura XB"), entries);
    assert(shuffled.recommendedRid == result.recommendedRid);
    for (size_t j = 0; j < result.candidates.size(); ++j)
      assert(shuffled.candidates[j].rid == result.candidates[j].rid);
  }
  assert(catalog::RankDownloadCandidates(Request("Martin Mac Aura XB"), entries, 2)
             .candidates.size() == 2);
  auto wrong = catalog::RankDownloadCandidates(Request("MAC Aura XB", "Martin"),
      {{"x", "Martin", "MAC Aura XB", {{"Different", 16}}, 1, 1}});
  assert(!wrong.recommendedRid && wrong.confidence == catalog::MatchConfidence::Review);
  wrong = catalog::RankDownloadCandidates(Request("MAC Aura XB", "Martin"),
      {{"x", "Martin", "MAC Aura XB", {{"Standard", 32}}, 1, 1}});
  assert(!wrong.recommendedRid);
  assert(!catalog::RankDownloadCandidates(Request("Different Model"), entries).recommendedRid);
  auto sameFootprint = catalog::RankDownloadCandidates(Request("ELP PAR"),
      {{"wrong", "Other", "Beam 200", {{"Standard", 16}}, 1, 1}});
  assert(sameFootprint.candidates.empty());
  auto numeric = catalog::RankDownloadCandidates(Request("Beam 200", "Acme"),
      {{"wrong", "Acme", "Beam 2000", {{"Standard", 16}}, 1, 1}});
  assert(!numeric.recommendedRid);
  auto roman = catalog::RankDownloadCandidates(Request("Smoke Factory Tour Hazer 2"),
      {{"tour", "Smoke Factory", "Tour Hazer II", {{"Standard", 16}}, 1, 1}});
  assert(roman.recommendedRid == "tour");
  auto ambiguous = catalog::RankDownloadCandidates(Request("Tour Hazer 2", ""),
      {{"a", "Smoke Factory", "Tour Hazer II Aqua", {{"Standard",16}}, 2, 5},
       {"b", "Smoke Factory", "Tour Hazer II Pro", {{"Standard",16}}, 1, 1}});
  assert(!ambiguous.recommendedRid && ambiguous.confidence == catalog::MatchConfidence::Ambiguous);
  auto manufacturerCollision = catalog::RankDownloadCandidates(Request("Beam 200"),
      {{"a", "Acme", "Beam 200", {{"Standard",16}}, 2, 5},
       {"b", "Other", "Beam 200", {{"Standard",16}}, 1, 1}});
  assert(!manufacturerCollision.recommendedRid);

  // Deterministic metadata stand-ins; the original Tivoli binary is not used.
  for (const auto &[model, manufacturer] : std::vector<std::pair<std::string, std::string>>{
      {"Martin Atomic 3000 LED", "Martin"}, {"Martin ELP PAR", "Martin"},
      {"Exterior Linear-1200 Graze", "Martin"}, {"Martin JEM ZR45", "Martin"},
      {"Smoke Factory Tour Hazer II Boxed", "Smoke Factory"},
      {"MAC Viper XIP", "Martin Professional"}, {"MAC Ultra Performance", "Martin Professional"},
      {"VDO Atomic Dot WRM", "Martin Professional"}}) {
    const auto candidate = catalog::RankDownloadCandidates(Request(model),
        {{"real", manufacturer, model, {{"Default",16}, {"DMX Mode",16}}, 1, 1}});
    assert(!candidate.candidates.empty());
    assert(!candidate.recommendedRid); // Mode names alone do not establish equivalence.
  }
  auto multipleModes = Request("Beam 200", "Acme");
  multipleModes.importedModes = {"Standard", "Extended"};
  const auto multi = catalog::RankDownloadCandidates(multipleModes,
      {{"x", "Acme", "Beam 200", {{"Standard",16}, {"Extended",32}}, 1, 1}});
  assert(!multi.recommendedRid && !multi.candidates.empty());
  auto request = Request("Beam 200", "Acme");
  request.fixtureTypeId = "catalog-uuid";
  entries = {{"not-related", "Other", "Unrelated", {{"Standard",16}}, 1, 1}};
  entries.front().uuid = request.fixtureTypeId;
  assert(catalog::RankDownloadCandidates(request, entries).candidates.empty());
}

static void ResolutionLifecycle() {
  const Choice original{Source::OriginalMvr, {}, "/original.gdtf", "Standard"};
  const Choice app{Source::Application, {}, "/app.gdtf", "Standard"};
  const Choice local{Source::Local, {}, "/local.gdtf", "Custom mode"};
  const std::vector<catalog::GdtfCatalogEntry> entries = {
    {"a", "Acme", "Beam 200", {{"Standard",16}}, 1, 1},
    {"b", "Acme", "Beam 200", {{"Standard",16}}, 2, 2}};
  auto result = Analyze(Request("Beam 200", "Acme"), entries, {original, app, local}, app);
  assert(result.selected.revisionId == "b" && result.applied == app);
  const auto automatic = result.selected;
  assert(Complete(result, automatic, "/download-b.gdtf", "Standard"));
  assert(result.status == Status::Downloaded && result.applied.revisionId == "b");
  Choice manual{Source::Catalog, "a", {}, "Standard"};
  assert(Select(result, manual));
  assert(result.origin == SelectionOrigin::User && result.status == Status::Pending);
  assert(!Complete(result, automatic, "/wrong.gdtf", "Standard"));
  assert(!Complete(result, manual, "/wrong-mode.gdtf", "Other"));
  Fail(result, manual, "offline");
  assert(result.applied == app && result.status == Status::DownloadFailed);
  assert(result.selected == manual); // Preserve the proposed revision for retry/review.
  assert(Select(result, local));
  assert(result.applied == local && result.status == Status::Local);
  assert(Select(result, manual));
  Fail(result, manual, "offline");
  assert(result.applied == local); // Explicit fallback wins over previous defaults.
  assert(Select(result, original));
  assert(result.status == Status::OriginalMvr);
  assert(!Select(result, {Source::Local, {}, "/not-validated.gdtf", "Standard"}));
  assert(!Select(result, manual, SelectionOrigin::Automatic));

  auto independent = Analyze(Request("Beam 200", "Acme"), entries, {original}, original);
  assert(independent.origin == SelectionOrigin::Automatic);
  assert(Select(result, manual));
  Cancel(result);
  assert(result.applied == original && result.status == Status::Canceled);
  const auto independentChoice = independent.selected;
  assert(Complete(independent, independentChoice, "/download-b.gdtf", "Standard", true));
  Cancel(independent);
  assert(independent.status == Status::Reused);
  assert(ReplacementKey(independentChoice) == ReplacementKey(automatic));
  auto aliasRequest = Request("Beam 200", "Acme");
  aliasRequest.displayTypeKey = "Another alias";
  auto alias = Analyze(aliasRequest, entries, {original}, original, manual);
  assert(alias.selected == manual && alias.origin == SelectionOrigin::User);
  assert(ReplacementKey(alias.selected) == ReplacementKey(manual));
  auto noMatch = Analyze(Request("No useful model"), entries, {original}, original);
  assert(noMatch.applied == original && noMatch.status == Status::OriginalMvr);
  assert(noMatch.ranking.confidence == catalog::MatchConfidence::NoMatch);
}

int main() {
  using mvr::gdtf_patch_evidence::RepeatedSpacing;
  assert(RepeatedSpacing({{"Aura", "DMX Mode", "1.1"}, {"Aura", "DMX Mode", "1.26"},
                          {"Aura", "DMX Mode", "1.51"}}).at("Aura") == 25);
  assert(RepeatedSpacing({{"Aura", "DMX Mode", "1.1"}, {"Aura", "DMX Mode", "1.26"}}).empty());
  assert(RepeatedSpacing({{"Aura", "DMX Mode", "1.1"}, {"Aura", "DMX Mode", "1.26"},
                          {"Aura", "DMX Mode", "1.52"}}).empty());
  assert(RepeatedSpacing({{"Aura", "A", "1.1"}, {"Aura", "A", "1.26"},
                          {"Aura", "B", "1.51"}}).empty());
  assert(RepeatedSpacing({{"Aura", "A", "1.1"}, {"Aura", "A", "1.26"},
                          {"Aura", "A", "1.51"}, {"Other", "A", "1.26"}}).empty());
  int cancelChecks = 0;
  std::vector<catalog::GdtfCatalogEntry> largeCatalog(10000,
      {"a", "Acme", "Beam 200", {{"Standard",16}}, 1, 1});
  const auto canceled = catalog::RankDownloadCandidates(Request("Beam 200", "Acme"),
      largeCatalog, 8, [&] { return ++cancelChecks >= 4; });
  assert(cancelChecks <= 5 && canceled.candidates.empty() && !canceled.recommendedRid);
  RankingRegressions();
  ResolutionLifecycle();
}

#include "project_symbol_cache_snapshot.h"

#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <wx/init.h>
#include <wx/mstream.h>
#include <wx/zipstrm.h>

namespace {

// Creates an in-memory ZIP archive from deterministic entry payloads.
std::vector<std::uint8_t>
BuildZip(const std::map<std::string, std::vector<std::uint8_t>> &entries) {
  wxMemoryOutputStream memory;
  {
    wxZipOutputStream zip(memory);
    for (const auto &[name, bytes] : entries) {
      assert(zip.PutNextEntry(name));
      if (!bytes.empty())
        zip.Write(bytes.data(), bytes.size());
      assert(zip.CloseEntry());
    }
    assert(zip.Close());
  }
  std::vector<std::uint8_t> bytes(memory.GetSize());
  memory.CopyTo(bytes.data(), bytes.size());
  return bytes;
}

// Converts text into an archive byte payload.
std::vector<std::uint8_t> Bytes(const std::string &text) {
  return {text.begin(), text.end()};
}

// Builds a canonical minimal GDTF with an optional missing side symbol.
std::vector<std::uint8_t> BuildGdtf(bool includeSide = true) {
  const std::string description =
      "<GDTF><FixtureType Name=\"Roundtrip Type\">"
      "<PerastageMutationAudit SchemaVersion=\"1\"/>"
      "<Models><Model Name=\"Main\" File=\"main\"/></Models>"
      "</FixtureType></GDTF>";
  std::map<std::string, std::vector<std::uint8_t>> entries = {
      {"description.xml", Bytes(description)},
      {"models/svg/main.svg", Bytes("top")},
      {"models/svg/main_bottom.svg", Bytes("bottom")},
      {"models/svg_front/main.svg", Bytes("front")}};
  if (includeSide)
    entries["models/svg_side/main.svg"] = Bytes("side");
  return BuildZip(entries);
}

// Builds an MVR that references the supplied exact packaged GDTF bytes.
std::vector<std::uint8_t> BuildMvr(const std::vector<std::uint8_t> &gdtf) {
  const std::string xml =
      "<GeneralSceneDescription><Scene><Layers><Layer><ChildList>"
      "<Fixture uuid=\"fixture-0001\"><GDTFSpec>RoundTrip.gdtf</GDTFSpec>"
      "</Fixture><Fixture uuid=\"fixture-0002\">"
      "<GDTFSpec>RoundTrip.gdtf</GDTFSpec></Fixture>"
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
  return BuildZip({{"GeneralSceneDescription.xml", Bytes(xml)},
                   {"RoundTrip.gdtf", gdtf}});
}

// Verifies exact packaged proof, stale replacement, deduplication, and planning.
void TestValidatedSnapshotAndPlanner() {
  const auto mvr = BuildMvr(BuildGdtf());
  const std::vector<symbol_cache::ProjectFixtureSymbolIdentity> identities = {
      {"fixture-0001", "Roundtrip Type", "Roundtrip Type"},
      {"fixture-0002", "Roundtrip Type", "Roundtrip Type"}};

  symbol_cache::SymbolCacheManifest stale;
  symbol_cache::ValidationRequest staleRequest;
  staleRequest.fixtureKey = "Roundtrip Type";
  staleRequest.fixtureTypeName = "Roundtrip Type";
  staleRequest.gdtfSpec = "old.gdtf";
  staleRequest.gdtfContentHash = "stale";
  staleRequest.requiredViews = symbol_cache::RequiredPerastageSymbolViews();
  stale.MarkFixtureSymbolsValid(staleRequest, "2026-01-01T00:00:00Z");

  const auto snapshot = symbol_cache::BuildProjectSymbolCacheSnapshot(
      mvr, identities, &stale, "2026-02-01T00:00:00Z");
  assert(snapshot.sceneValid);
  assert(snapshot.validatedCount == 1);
  assert(snapshot.manifest.Entries().size() == 1);
  const auto &entry = snapshot.manifest.Entries().front();
  assert(entry.gdtfSpec == "RoundTrip.gdtf");
  assert(entry.gdtfContentHash != "stale");

  symbol_cache::ValidationRequest reloadRequest;
  reloadRequest.fixtureKey = entry.fixtureKey;
  reloadRequest.fixtureTypeName = entry.fixtureTypeName;
  reloadRequest.gdtfSpec = entry.gdtfSpec;
  reloadRequest.gdtfContentHash = entry.gdtfContentHash;
  reloadRequest.requiredViews = symbol_cache::RequiredPerastageSymbolViews();
  assert(snapshot.manifest.ValidateFixture(reloadRequest).valid);
  assert(symbol_cache::PlanFixtureSymbolCacheMisses(snapshot.manifest,
                                                     {reloadRequest})
             .empty());
}

// Verifies incomplete packaged symbols are omitted rather than guessed valid.
void TestIncompleteSymbolsAreOmitted() {
  const auto snapshot = symbol_cache::BuildProjectSymbolCacheSnapshot(
      BuildMvr(BuildGdtf(false)),
      {{"fixture-0001", "Roundtrip Type", "Roundtrip Type"}}, nullptr,
      "2026-02-01T00:00:00Z");
  assert(snapshot.sceneValid);
  assert(snapshot.validatedCount == 0);
  assert(snapshot.omittedCount == 1);
  assert(snapshot.manifest.Entries().empty());
}

// Verifies malformed scene payloads fail the correctness stage.
void TestMalformedSceneFails() {
  const auto snapshot = symbol_cache::BuildProjectSymbolCacheSnapshot(
      Bytes("not a zip"),
      {{"fixture-0001", "Roundtrip Type", "Roundtrip Type"}});
  assert(!snapshot.sceneValid);
  assert(!snapshot.errorMessage.empty());
}

} // namespace

// Runs project symbol-cache snapshot regression coverage.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());
  TestValidatedSnapshotAndPlanner();
  TestIncompleteSymbolsAreOmitted();
  TestMalformedSceneFails();
  return 0;
}

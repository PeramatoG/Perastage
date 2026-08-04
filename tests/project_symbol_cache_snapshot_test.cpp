#include "project_symbol_cache_snapshot.h"

#include <algorithm>
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
      auto *entry = new wxZipEntry();
      entry->SetName(wxString::FromUTF8(name), wxPATH_UNIX);
      assert(zip.PutNextEntry(entry));
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

// Replaces every fixed-width ZIP name occurrence without changing record offsets.
void ReplaceArchiveName(std::vector<std::uint8_t> &archive,
                        const std::string &from, const std::string &to) {
  assert(from.size() == to.size());
  const std::vector<std::uint8_t> source(from.begin(), from.end());
  for (auto position = std::search(archive.begin(), archive.end(), source.begin(),
                                   source.end());
       position != archive.end();
       position = std::search(position + to.size(), archive.end(), source.begin(),
                              source.end())) {
    std::copy(to.begin(), to.end(), position);
  }
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
      {"models/svg_front/main.svg", Bytes("front")},
      {"resources/Información.txt", Bytes("unicode")}};
  if (includeSide)
    entries["models/svg_side/main.svg"] = Bytes("side");
  return BuildZip(entries);
}

// Builds an MVR that references the supplied exact packaged GDTF bytes.
std::vector<std::uint8_t> BuildMvr(const std::vector<std::uint8_t> &gdtf) {
  const std::string xml =
      "<GeneralSceneDescription><Scene><Layers><Layer><ChildList>"
      "<Fixture uuid=\"fixture-0001\"><GDTFSpec>RoundTrip.gdtf</GDTFSpec>"
      "<GDTFMode>Mode A</GDTFMode>"
      "</Fixture><Fixture uuid=\"fixture-0002\">"
      "<GDTFSpec>RoundTrip.gdtf</GDTFSpec><GDTFMode>Mode A</GDTFMode></Fixture>"
      "</ChildList></Layer></Layers></Scene></GeneralSceneDescription>";
  return BuildZip({{"GeneralSceneDescription.xml", Bytes(xml)},
                   {"RoundTrip.gdtf", gdtf}});
}

// Verifies exact packaged proof, stale replacement, deduplication, and planning.
void TestValidatedSnapshotAndPlanner() {
  const auto mvr = BuildMvr(BuildGdtf());
  const std::vector<symbol_cache::ProjectFixtureSymbolIdentity> identities = {
      {"fixture-0001", "Roundtrip Type", "Mode A"},
      {"fixture-0002", "Renamed Type", "Mode A"}};

  symbol_cache::SymbolCacheManifest stale;
  symbol_cache::ValidationRequest staleRequest;
  std::string identityError;
  assert(symbol_cache::BuildFixtureSymbolGenerationIdentity(
      "old.gdtf", "Mode A", symbol_cache::kCurrentPerastageSymbolFormatVersion,
      "stale", "Roundtrip Type", staleRequest.generationIdentity, identityError));
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
  reloadRequest.generationIdentity = entry.generationIdentity;
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
      {{"fixture-0001", "Roundtrip Type", "Mode A"}}, nullptr,
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
      {{"fixture-0001", "Roundtrip Type", "Mode A"}});
  assert(!snapshot.sceneValid);
  assert(!snapshot.errorMessage.empty());
}

// Verifies raw unsafe and ambiguous ZIP names remain rejected before normalization.
void TestUnsafeRawArchiveNamesFail() {
  const std::string sceneXml = "<GeneralSceneDescription/>";
  for (const auto &[safeName, unsafeName] :
       std::vector<std::pair<std::string, std::string>>{
           {"aa/unsafe.txt", "../unsafe.txt"},
           {"xunsafe.txt", "/unsafe.txt"},
           {"xx/unsafe.txt", "C:/unsafe.txt"}}) {
    auto archive = BuildZip({{"GeneralSceneDescription.xml", Bytes(sceneXml)},
                             {safeName, Bytes("unsafe")}});
    ReplaceArchiveName(archive, safeName, unsafeName);
    const auto snapshot = symbol_cache::BuildProjectSymbolCacheSnapshot(
        archive, {});
    assert(!snapshot.sceneValid);
  }

  const auto collision = BuildZip(
      {{"GeneralSceneDescription.xml", Bytes(sceneXml)},
       {"Resources/Fixture.gdtf", Bytes("first")},
       {"resources/fixture.gdtf", Bytes("second")}});
  assert(!symbol_cache::BuildProjectSymbolCacheSnapshot(collision, {}).sceneValid);

  auto gdtf = BuildGdtf();
  ReplaceArchiveName(gdtf, "models/svg/main.svg", "models\\svg\\main.svg");
  const auto snapshot = symbol_cache::BuildProjectSymbolCacheSnapshot(
      BuildMvr(gdtf),
      {{"fixture-0001", "Roundtrip Type", "Mode A"}});
  assert(snapshot.sceneValid);
  assert(snapshot.validatedCount == 0);
  assert(snapshot.omittedCount == 1);
}

} // namespace

// Runs project symbol-cache snapshot regression coverage.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());
  TestValidatedSnapshotAndPlanner();
  TestIncompleteSymbolsAreOmitted();
  TestMalformedSceneFails();
  TestUnsafeRawArchiveNamesFail();
  return 0;
}

#include "symbols/standard_gdtf_completion.h"
#include "symbols/fixture_symbol_resource_contract.h"
#include "symbols/PerastageSvgSymbol.h"
#include "fixture_gdtf_derivative_contract.h"
#include "gdtf_archive_reader.h"
#include "gdtf_canonicalizer.h"
#include "gdtf_mutation_audit.h"
#include "inspection/xml_schema_validation.h"
#include "gdtf_test_fixture_builder.h"
#include "zip_test_utils.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

#include <tinyxml2.h>

namespace fs = std::filesystem;
using symbols::StandardGdtfSvgCandidate;
using symbols::StandardGdtfViewState;

std::string ReadFile(const fs::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::string Entry(const fs::path &path, const std::string &name) {
  std::string bytes;
  assert(tests::zip::ReadEntry(path, name, &bytes));
  return bytes;
}

int RevisionCount(const fs::path &path) {
  const std::string bytes = Entry(path, "description.xml");
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(bytes.c_str(), bytes.size()) == tinyxml2::XML_SUCCESS);
  auto *fixture = doc.FirstChildElement("GDTF")->FirstChildElement("FixtureType");
  auto *revisions = fixture->FirstChildElement("Revisions");
  int count = 0;
  for (auto *revision = revisions ? revisions->FirstChildElement("Revision") : nullptr;
       revision; revision = revision->NextSiblingElement("Revision"))
    ++count;
  return count;
}

void WithRevision(const fs::path &source, const fs::path &destination,
                  const std::string &text) {
  const auto result = GdtfCanonicalizer::RewriteArchiveDescription(
      source, destination, [&](tinyxml2::XMLDocument &document) {
        GdtfMutationAudit::AppendRevision(
            GdtfMutationAudit::EnsureFixtureType(document), document, text,
            "Perastage 1.5", 0, "2025-01-01T00:00:00Z",
            GdtfMutationAudit::RevisionPolicy::RecordEffectiveChange);
        return true;
      });
  assert(result.success);
}

fixture_gdtf::PreparedDerivative Prepare(const fs::path &source,
                                         const fs::path &destination) {
  fixture_gdtf::PreparedDerivative prepared;
  std::string error;
  assert(fixture_gdtf::PrepareOwnedDerivative(
      source, destination, destination.filename().string(), prepared, error));
  return prepared;
}

std::vector<StandardGdtfSvgCandidate> Candidates() {
  symbols::Symbol2D symbol;
  symbol.bounds = {{-20, -30}, {50, 40}, true};
  symbol.fill = {{{{-20, -30}, {50, -30}, {50, 40}, {-20, 40}},
                  {{{0, 0}, {10, 0}, {10, 10}, {0, 10}}}}};
  std::vector<StandardGdtfSvgCandidate> result;
  std::string error;
  for (const auto view : {SymbolViewKind::Top, SymbolViewKind::Left,
                          SymbolViewKind::Front}) {
    StandardGdtfSvgCandidate candidate;
    assert(symbols::SerializeStandardGdtfSvg(symbol, view, candidate, error));
    assert(candidate.svg.find("data-perastage-") == std::string::npos);
    assert(candidate.svg.find("70mm") != std::string::npos);
    assert(candidate.svg.find("fill-rule=\"evenodd\"") != std::string::npos);
    assert(candidate.offsetXmm == 20 && candidate.offsetYmm == 30);
    result.push_back(std::move(candidate));
  }
  StandardGdtfSvgCandidate bottom;
  assert(!symbols::SerializeStandardGdtfSvg(symbol, SymbolViewKind::Bottom,
                                            bottom, error));
  return result;
}

void CheckStandardOutput(const fs::path &path, const std::string &modelFile) {
  const auto archive = gdtf::ReadGdtfArchive(path);
  assert(archive.Success());
  for (const auto &entry : archive.entries) {
    assert(entry.path.find("perastage/") == std::string::npos);
    assert(entry.path.find("svg_bottom/") == std::string::npos);
    assert(entry.path.find("_bottom.svg") == std::string::npos);
    if (entry.path.ends_with(".svg"))
      assert(Entry(path, entry.path).find("data-perastage-") == std::string::npos);
  }
  tinyxml2::XMLDocument doc;
  assert(doc.Parse(archive.descriptionXml.c_str()) == tinyxml2::XML_SUCCESS);
  const auto *fixture = doc.FirstChildElement("GDTF")->FirstChildElement("FixtureType");
  assert(std::string(fixture->Attribute("FixtureTypeID")) ==
         tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId);
  const auto *model = fixture->FirstChildElement("Models")->FirstChildElement("Model");
  assert(std::string(model->Attribute("File")) == modelFile);
  for (const auto *attribute : {"SVGOffsetX", "SVGSideOffsetX", "SVGFrontOffsetX"})
    assert(model->DoubleAttribute(attribute) == 20);
  for (const auto *attribute : {"SVGOffsetY", "SVGSideOffsetY", "SVGFrontOffsetY"})
    assert(model->DoubleAttribute(attribute) == 30);
  const auto validation = perastage::inspection::ValidateXmlAgainstSchema(
      archive.descriptionXml, perastage::inspection::Gdtf12Schema());
  assert(validation.schema.status == perastage::inspection::ValidationStatus::Valid);
  std::string error;
  assert(fixture_gdtf::ValidatePublishedDerivative(path.string(), error));
}

int main() {
  const fs::path root = fs::temp_directory_path() / "perastage-standard-gdtf-completion";
  fs::remove_all(root);
  fs::create_directories(root);
  const std::string modelFile = "Case.Light.v2";
  const auto candidates = Candidates();
  const fs::path source = root / "Source.gdtf";
  const fs::path published = root / "Maker@Fixture@Perastage.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource(modelFile)
      .WithArchiveEntry("wheels/open.png", "authored wheel")
      .WriteArchive(source);
  const std::string original = ReadFile(source);
  const auto inspected = symbols::InspectStandardGdtfViews(source);
  assert(inspected.success && inspected.views.size() == 3);
  for (const auto &view : inspected.views)
    assert(view.state == StandardGdtfViewState::Missing);
  assert(ReadFile(source) == original);
  assert(RevisionCount(source) == 0);

  auto prepared = Prepare(source, published);
  auto result = symbols::CompleteStandardGdtfViews(
      source, prepared, candidates, gdtf::MutationPolicy::PreserveImported);
  assert(result.success && !result.changed && !fs::exists(published));
  assert(!fs::exists(prepared.workingPath) && ReadFile(source) == original);

  prepared = Prepare(source, published);
  result = symbols::CompleteStandardGdtfViews(
      source, prepared, candidates, gdtf::MutationPolicy::CompleteAndImprove);
  assert(result.success && result.changed && result.publishedPath == published);
  assert(!fs::exists(prepared.workingPath));
  assert(ReadFile(source) == original);
  assert(RevisionCount(published) == 1);
  CheckStandardOutput(published, modelFile);
  assert(Entry(published, "wheels/open.png") == "authored wheel");
  for (const auto &candidate : candidates)
    assert(Entry(published, symbols::StandardGdtfSvgPath(modelFile, candidate.viewKind)) ==
           candidate.svg);
  const std::string completed = ReadFile(published);
  prepared = Prepare(published, published);
  bool invokedPublisher = false;
  result = symbols::CompleteStandardGdtfViews(
      published, prepared, candidates, gdtf::MutationPolicy::CompleteAndImprove,
      "", [&](const auto &, auto &) { invokedPublisher = true; return false; });
  assert(result.success && !result.changed && !invokedPublisher);
  assert(ReadFile(published) == completed && RevisionCount(published) == 1);

  // Invalid existing is never mistaken for absent, and authored bytes survive.
  const std::string authored = "<svg viewBox=\"0 0 10 10\"><polygon points=\"0,0 10,0 10,10\"/></svg>";
  const fs::path mixed = root / "Mixed.gdtf";
  const fs::path improved = root / "Mixed@Perastage.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource(modelFile)
      .WithArchiveEntry("models/svg/" + modelFile + ".svg", authored)
      .WithArchiveEntry("models/svg_side/" + modelFile + ".svg", "<svg")
      .WriteArchive(mixed);
  const std::string mixedOriginal = ReadFile(mixed);
  const auto mixedInspection = symbols::InspectStandardGdtfViews(mixed);
  assert(mixedInspection.views[0].state == StandardGdtfViewState::ExistingUsable);
  assert(mixedInspection.views[1].state == StandardGdtfViewState::ExistingUnusable);
  assert(mixedInspection.views[2].state == StandardGdtfViewState::Missing);
  prepared = Prepare(mixed, improved);
  result = symbols::CompleteStandardGdtfViews(
      mixed, prepared, candidates, gdtf::MutationPolicy::CompleteAndImprove);
  assert(result.success && result.changed && !result.diagnostics.empty());
  assert(RevisionCount(improved) == 1);
  assert(Entry(improved, "description.xml").find("Added missing standard SVG Front view") !=
         std::string::npos);
  assert(Entry(improved, "models/svg/" + modelFile + ".svg") == authored);
  assert(Entry(improved, "models/svg_side/" + modelFile + ".svg") == "<svg");
  assert(ReadFile(mixed) == mixedOriginal);
  prepared = Prepare(improved, improved);
  result = symbols::ReplaceStandardGdtfViews(
      improved, prepared, {candidates[1]}, symbols::StandardGdtfReplacementIntent::RepairInvalid);
  assert(result.success && result.changed && RevisionCount(improved) == 2);
  assert(Entry(improved, "description.xml").find("after explicit user repair") != std::string::npos);
  const std::string repaired = ReadFile(improved);
  prepared = Prepare(improved, improved);
  result = symbols::ReplaceStandardGdtfViews(
      improved, prepared, {candidates[0]}, symbols::StandardGdtfReplacementIntent::RepairInvalid);
  assert(!result.success && ReadFile(improved) == repaired);
  prepared = Prepare(improved, improved);
  result = symbols::ReplaceStandardGdtfViews(
      improved, prepared, {candidates[0]}, symbols::StandardGdtfReplacementIntent::ReplaceExisting);
  assert(result.success && result.changed && RevisionCount(improved) == 3);
  const std::string explicitlyReplaced = ReadFile(improved);
  prepared = Prepare(improved, improved);
  result = symbols::ReplaceStandardGdtfViews(
      improved, prepared, {candidates[0]}, symbols::StandardGdtfReplacementIntent::ReplaceExisting);
  assert(result.success && !result.changed && ReadFile(improved) == explicitlyReplaced);

  // Legacy extensions are copied out of intentional output, never source-mutated.
  const fs::path legacy = root / "Legacy.gdtf";
  const fs::path legacyOutput = root / "Legacy@Perastage.gdtf";
  const std::string marked = "<svg data-perastage-symbol-version=\"1\" viewBox=\"0 0 10 10\"><polygon points=\"0,0 10,0 10,10\"/></svg>";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource(modelFile)
      .WithEditor("Perastage 1.5")
      .WithArchiveEntry("perastage/symbols/old/bottom.svg", marked)
      .WithArchiveEntry("models/svg_bottom/old.svg", marked)
      .WithArchiveEntry("models/svg/old_bottom.svg", marked)
      .WriteArchive(legacy);
  const std::string legacyBytes = ReadFile(legacy);
  prepared = Prepare(legacy, legacyOutput);
  result = symbols::CompleteStandardGdtfViews(
      legacy, prepared, candidates, gdtf::MutationPolicy::CompleteAndImprove);
  assert(result.success && result.changed && RevisionCount(legacyOutput) == 1);
  assert(ReadFile(legacy) == legacyBytes);
  CheckStandardOutput(legacyOutput, modelFile);

  // A rejected candidate or publisher never damages the existing destination.
  auto invalidCandidate = candidates[0];
  invalidCandidate.offsetXmm = std::numeric_limits<double>::infinity();
  prepared = Prepare(source, published);
  result = symbols::CompleteStandardGdtfViews(
      source, prepared, {invalidCandidate}, gdtf::MutationPolicy::CompleteAndImprove);
  assert(!result.success && ReadFile(published) == completed);
  assert(!fs::exists(prepared.workingPath));
  prepared = Prepare(source, published);
  result = symbols::CompleteStandardGdtfViews(
      source, prepared, candidates, gdtf::MutationPolicy::CompleteAndImprove,
      "", [](const auto &, std::string &error) { error = "Test publication refused"; return false; });
  assert(!result.success && ReadFile(published) == completed && ReadFile(source) == original);
  assert(!fs::exists(prepared.workingPath));

  const fs::path noFile = root / "NoModelFile.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WriteArchive(noFile);
  assert(!symbols::InspectStandardGdtfViews(noFile).success);
  const fs::path unsafe = root / "UnsafeModel.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("../escape").WriteArchive(unsafe);
  assert(!symbols::InspectStandardGdtfViews(unsafe).success);

  // An authored resource in an unsupported Bottom location has no ownership
  // evidence. Publication refuses it instead of silently deleting its bytes.
  const fs::path unknownBottom = root / "UnknownBottom.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("body")
      .WithArchiveEntry("models/svg_bottom/body.svg", authored)
      .WriteArchive(unknownBottom);
  const std::string unknownBytes = ReadFile(unknownBottom);
  prepared = Prepare(unknownBottom, root / "UnknownBottom@Perastage.gdtf");
  result = symbols::CompleteStandardGdtfViews(
      unknownBottom, prepared, candidates, gdtf::MutationPolicy::CompleteAndImprove);
  assert(!result.success && result.errorMessage.find("explicit repair") != std::string::npos);
  assert(ReadFile(unknownBottom) == unknownBytes && !fs::exists(prepared.publishedPath));

  // A valid exact Model.File may itself contain _bottom: its Top is standard.
  const fs::path namedBottom = root / "NamedBottom.gdtf";
  const fs::path namedOutput = root / "NamedBottom@Perastage.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("foo_bottom")
      .WithArchiveEntry("models/svg/foo_bottom.svg", marked)
      .WriteArchive(namedBottom);
  const std::string namedBytes = ReadFile(namedBottom);
  prepared = Prepare(namedBottom, namedOutput);
  result = symbols::CompleteStandardGdtfViews(
      namedBottom, prepared, candidates, gdtf::MutationPolicy::CompleteAndImprove);
  assert(result.success && result.changed && RevisionCount(namedOutput) == 1);
  assert(Entry(namedOutput, "models/svg/foo_bottom.svg").find("polygon") != std::string::npos);
  assert(Entry(namedOutput, "models/svg/foo_bottom.svg").find("data-perastage-") == std::string::npos);
  assert(tests::zip::ReadEntry(namedOutput, "models/svg_side/foo_bottom.svg"));
  assert(tests::zip::ReadEntry(namedOutput, "models/svg_front/foo_bottom.svg"));
  assert(ReadFile(namedBottom) == namedBytes);

  // Historical unmarked Bottom is removable only with exact revision evidence.
  const fs::path unmarkedLegacy = root / "UnmarkedLegacyBottom.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("body")
      .WithArchiveEntry("models/svg/body_bottom.svg", authored)
      .WriteArchive(unmarkedLegacy);
  const fs::path evidenceSource = root / "UnmarkedLegacyEvidence.gdtf";
  const auto evidence = GdtfCanonicalizer::RewriteArchiveDescription(
      unmarkedLegacy, evidenceSource, [](tinyxml2::XMLDocument &document) {
        auto *fixture = document.FirstChildElement("GDTF")->FirstChildElement("FixtureType");
        auto *revisions = document.NewElement("Revisions");
        auto *revision = document.NewElement("Revision");
        revision->SetAttribute("Text", "Applied fixture SVG symbol views (bottom)");
        revision->SetAttribute("ModifiedBy", "Perastage 1.5");
        revision->SetAttribute("Date", "2025-01-01T00:00:00Z");
        revision->SetAttribute("UserID", 0);
        revisions->InsertEndChild(revision);
        fixture->InsertAfterChild(fixture->FirstChildElement("DMXModes"), revisions);
        return true;
      });
  assert(evidence.success);
  prepared = Prepare(evidenceSource, root / "UnmarkedLegacy@Perastage.gdtf");
  result = symbols::CompleteStandardGdtfViews(
      evidenceSource, prepared, candidates, gdtf::MutationPolicy::CompleteAndImprove);
  assert(result.success && result.changed && RevisionCount(prepared.publishedPath) == 2);
  assert(!tests::zip::ReadEntry(prepared.publishedPath, "models/svg/body_bottom.svg"));

  // Ownership recognition requires exact known audit actions, not a broad label.
  assert(IsPerastageStandardSvgMutationRevision(
      "Perastage 1.7", "Added missing standard SVG Top, Side and Front views"));
  assert(!IsPerastageStandardSvgMutationRevision("Vendor", "Added missing standard SVG Top view"));
  assert(!IsPerastageStandardSvgMutationRevision("Perastage", "Edited something else"));
  assert(IsPerastageStandardSvgMutationRevision(
      "Perastage 1.7", "Added missing standard SVG Front, Top and Side views"));
  assert(IsPerastageStandardSvgMutationRevisionForView(
      "Perastage", "Replaced standard SVG Front and Side views after explicit user repair",
      SymbolViewKind::Right));
  assert(!IsPerastageStandardSvgMutationRevisionForView(
      "Perastage", "Replaced standard SVG Front and Side views after explicit user repair",
      SymbolViewKind::Top));
  assert(!IsPerastageStandardSvgMutationRevision(
      "Perastage", "Added missing standard SVG Top and Top views"));
  assert(IsPerastageStandardSvgCleanupRevision("Perastage",
      "Updated GDTF document fields from Perastage; standardized legacy private symbol resources"));
  assert(!IsPerastageStandardSvgCleanupRevision("Vendor",
      "Updated GDTF document fields from Perastage; standardized legacy private symbol resources"));

  // New standard output has deterministic audit order even when capture order differs.
  const fs::path reorderedOutput = root / "Reordered@Perastage.gdtf";
  prepared = Prepare(source, reorderedOutput);
  result = symbols::CompleteStandardGdtfViews(
      source, prepared, {candidates[2], candidates[0], candidates[1]},
      gdtf::MutationPolicy::CompleteAndImprove);
  assert(result.success && result.changed);
  assert(Entry(reorderedOutput, "description.xml").find(
      "Added missing standard SVG Top, Side and Front views") != std::string::npos);

  // A later per-view standard replacement supersedes that view's legacy audit.
  const fs::path historicalBase = root / "HistoricalBase.gdtf";
  const fs::path historical = root / "Historical.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("history")
      .WithArchiveEntry("models/svg/history.svg", authored)
      .WithArchiveEntry("models/svg_side/history.svg", authored)
      .WithArchiveEntry("models/svg_front/history.svg", authored)
      .WriteArchive(historicalBase);
  WithRevision(historicalBase, historical,
               "Applied fixture SVG symbol views (top, side, front)");
  const std::string historicalBytes = ReadFile(historical);
  FixtureSymbolResourceInspection resourceInspection;
  assert(InspectFixtureSymbolResources(historical.string(), resourceInspection));
  assert(!resourceInspection.FindStandardView(SymbolViewKind::Front)->usable);
  const fs::path historicalOutput = root / "Historical@Perastage.gdtf";
  prepared = Prepare(historical, historicalOutput);
  result = symbols::ReplaceStandardGdtfViews(
      historical, prepared, {candidates[2]},
      symbols::StandardGdtfReplacementIntent::ReplaceExisting);
  assert(result.success && result.changed && ReadFile(historical) == historicalBytes);
  assert(Entry(historicalOutput, "models/svg_front/history.svg") == candidates[2].svg);
  assert(Entry(historicalOutput, "models/svg/history.svg") == authored);
  assert(InspectFixtureSymbolResources(historicalOutput.string(), resourceInspection));
  assert(resourceInspection.FindStandardView(SymbolViewKind::Front)->usable);
  assert(!resourceInspection.FindStandardView(SymbolViewKind::Top)->usable);
  PerastageSvgSymbolData repairedFront;
  assert(LoadPerastageSvgSymbolFromGdtf(historicalOutput.string(), SymbolViewKind::Front,
      repairedFront, nullptr, FixtureSymbolResolutionPurpose::StandardGdtf));
  assert(repairedFront.resourceSet == FixtureSymbolResourceSet::StandardGdtf);
  assert(repairedFront.offsetXmm == 20 && repairedFront.offsetYmm == 30);
  assert(!repairedFront.fills.empty() && !repairedFront.fills.front().holes.empty());

  // A later legacy operation can still identify new compatibility input.
  const fs::path newerLegacy = root / "NewerLegacy.gdtf";
  WithRevision(historicalOutput, newerLegacy, "Applied fixture SVG symbol views (front)");
  assert(InspectFixtureSymbolResources(newerLegacy.string(), resourceInspection));
  assert(!resourceInspection.FindStandardView(SymbolViewKind::Front)->usable);

  // Cleanup publication converts marked official legacy paths to standard views.
  const fs::path cleanupBase = root / "CleanupBase.gdtf";
  const fs::path cleanupSource = root / "CleanupSource.gdtf";
  const fs::path cleanupOutput = root / "Cleanup@Perastage.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("history")
      .WithArchiveEntry("models/svg/history.svg", marked)
      .WithArchiveEntry("models/svg_side/history.svg", marked)
      .WithArchiveEntry("models/svg_front/history.svg", marked)
      .WithArchiveEntry("perastage/symbols/history/bottom.svg", marked)
      .WriteArchive(cleanupBase);
  WithRevision(cleanupBase, cleanupSource,
               "Applied fixture SVG symbol views (top, side, front)");
  const std::string cleanupBytes = ReadFile(cleanupSource);
  std::string cleanupError;
  assert(fixture_gdtf::PublishCanonicalGdtfCopy(cleanupSource, cleanupOutput, cleanupError));
  assert(InspectFixtureSymbolResources(cleanupOutput.string(), resourceInspection));
  assert(resourceInspection.standardViewsUsable && ReadFile(cleanupSource) == cleanupBytes);
  assert(!tests::zip::ReadEntry(cleanupOutput, "perastage/symbols/history/bottom.svg"));

  // Existing markers remain legacy evidence even after a claimed cleanup audit.
  const fs::path claimedCleanup = root / "ClaimedCleanup.gdtf";
  WithRevision(cleanupSource, claimedCleanup,
               "Removed legacy Perastage private symbol extensions");
  assert(InspectFixtureSymbolResources(claimedCleanup.string(), resourceInspection));
  assert(!resourceInspection.standardViewsUsable);

  // Document mutation uses its own audited cleanup suffix for the same output.
  const fs::path documentCleanup = root / "DocumentCleanup.gdtf";
  WithRevision(historical, documentCleanup,
      "Updated fixture physical properties; standardized legacy private symbol resources");
  assert(InspectFixtureSymbolResources(documentCleanup.string(), resourceInspection));
  assert(resourceInspection.standardViewsUsable);

  // A combined completion/cleanup also supersedes legacy at retained views.
  const fs::path partialBase = root / "PartialLegacyBase.gdtf";
  const fs::path partialLegacy = root / "PartialLegacy.gdtf";
  const fs::path partialOutput = root / "PartialLegacy@Perastage.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("history")
      .WithArchiveEntry("models/svg/history.svg", marked)
      .WithArchiveEntry("models/svg_side/history.svg", marked)
      .WriteArchive(partialBase);
  WithRevision(partialBase, partialLegacy, "Applied fixture SVG symbol views (top, side)");
  prepared = Prepare(partialLegacy, partialOutput);
  result = symbols::CompleteStandardGdtfViews(
      partialLegacy, prepared, candidates, gdtf::MutationPolicy::CompleteAndImprove);
  assert(result.success && result.changed && RevisionCount(partialOutput) == 2);
  assert(InspectFixtureSymbolResources(partialOutput.string(), resourceInspection));
  assert(resourceInspection.standardViewsUsable);

  // Project filename ownership must not be inferred from @Perastage spelling.
  const fs::path project = root / "project";
  fs::create_directories(project / "fixtures");
  const fs::path authoredNamedSource = project / "fixtures" / "Maker@Fixture@Perastage.gdtf";
  fs::copy_file(source, authoredNamedSource);
  const std::string authoredNamedBytes = ReadFile(authoredNamedSource);
  std::string error;
  assert(fixture_gdtf::PrepareProjectDerivative(
      authoredNamedSource, project, authoredNamedSource.filename(), prepared, error));
  assert(prepared.publishedPath != authoredNamedSource);
  assert(prepared.publishedReference.starts_with("fixtures/"));
  result = symbols::CompleteStandardGdtfViews(
      authoredNamedSource, prepared, candidates, gdtf::MutationPolicy::CompleteAndImprove);
  assert(result.success && result.changed && ReadFile(authoredNamedSource) == authoredNamedBytes);
  const fs::path ownedProjectDerivative = result.publishedPath;
  const std::string ownedBytes = ReadFile(ownedProjectDerivative);
  assert(fixture_gdtf::PrepareProjectDerivative(
      ownedProjectDerivative, project, authoredNamedSource.filename(), prepared, error));
  assert(prepared.publishedPath == ownedProjectDerivative);
  result = symbols::CompleteStandardGdtfViews(
      ownedProjectDerivative, prepared, candidates, gdtf::MutationPolicy::CompleteAndImprove);
  assert(result.success && !result.changed && ReadFile(ownedProjectDerivative) == ownedBytes);

  // A preexisting unrelated destination also remains byte-for-byte intact.
  const fs::path foreignDestination = project / "fixtures" / "Maker@Shared@Perastage.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("Other", "Other", "ffffffff-1234-4234-9234-123456789abc")
      .WithModelResource("other").WriteArchive(foreignDestination);
  const std::string foreignBytes = ReadFile(foreignDestination);
  assert(fixture_gdtf::PrepareProjectDerivative(
      source, project, foreignDestination.filename(), prepared, error));
  assert(prepared.publishedPath != foreignDestination);
  result = symbols::CompleteStandardGdtfViews(
      source, prepared, candidates, gdtf::MutationPolicy::CompleteAndImprove);
  assert(result.success && result.changed && ReadFile(foreignDestination) == foreignBytes);
  assert(ReadFile(source) == original);
  fs::remove_all(root);
}

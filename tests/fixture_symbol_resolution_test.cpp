#include "symbols/fixture_symbol_availability.h"
#include "symbols/fixture_symbol_svg_cache.h"
#include "support/gdtf_test_fixture_builder.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <wx/init.h>

namespace {
namespace fs = std::filesystem;
using Purpose = FixtureSymbolResolutionPurpose;
using Reason = FixtureSymbolFallbackReason;
using Set = FixtureSymbolResourceSet;
using Provenance = FixtureSymbolProvenance;
using View = SymbolViewKind;

const std::string kSvg =
    "<svg viewBox=\"0 0 10 10\"><polygon points=\"0,0 10,0 10,10\"/></svg>";
const std::string kInternal =
    "<svg data-perastage-symbol-version=\"1\" "
    "data-perastage-offset-x-mm=\"3\" data-perastage-offset-y-mm=\"4\" "
    "viewBox=\"0 0 20 10\"><polygon points=\"0,0 20,0 20,10\"/></svg>";

struct Fixtures {
  fs::path root = fs::temp_directory_path() /
      ("fixture-symbol-resolution-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
  Fixtures() { fs::create_directory(root); }
  ~Fixtures() { std::error_code error; fs::remove_all(root, error); }

  std::string Write(const std::string &name, const tests::gdtf::FixtureBuilder &builder) {
    const auto path = root / (name + ".gdtf");
    builder.WriteArchive(path);
    return path.string();
  }
};

tests::gdtf::FixtureBuilder Fixture() {
  auto builder = tests::gdtf::BuildMinimalValidFixture();
  builder.WithModelResource("body");
  return builder;
}

std::string ReadBytes(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

// Every successful resolution must agree with loading, including cache purpose,
// exact archive path, offsets, and rendering orientation.
FixtureSymbolResolution Check(const std::string &path, View view, Purpose purpose,
                              const std::string &expectedPath, Set set,
                              Provenance provenance, Reason reason = Reason::None,
                              View actual = View::Top, bool viewFallback = false) {
  const auto bytes = ReadBytes(path);
  const auto availability = symbol_cache::InspectFixtureSymbolAvailability(path);
  const auto &result = purpose == Purpose::StandardGdtf
      ? availability.standardViews[static_cast<size_t>(view)]
      : availability.internalViews[static_cast<size_t>(view)];
  assert(result.requestedView == view);
  assert(result.resolvedView == actual);
  assert(result.archivePath == expectedPath);
  assert(result.resourceSet == set);
  assert(result.provenance == provenance);
  assert(result.exists && result.usable);
  assert(result.fallbackReason == reason);
  assert(result.usedViewFallback == viewFallback);
  std::string error;
  const auto loaded = symbol_cache::LoadUsableFixtureSymbol(path, view, &error, purpose);
  assert(loaded && error.empty());
  assert(loaded->sourcePath == result.archivePath);
  assert(loaded->resourceSet == result.resourceSet);
  assert(loaded->provenance == result.provenance);
  assert(loaded->offsetXmm == result.offsetXmm);
  assert(loaded->offsetYmm == result.offsetYmm);
  assert(loaded->usedViewFallback == result.usedViewFallback);
  assert(loaded->viewKind == (view == View::Right && actual == View::Left
                                ? View::Right : actual));
  assert(ReadBytes(path) == bytes);
  return result;
}

void CheckRuntime(const std::string &path, View view, Purpose purpose) {
  FixtureSymbolResourceInspection inspection;
  assert(InspectFixtureSymbolResources(path, inspection));
  const auto result = ResolveFixtureSymbolView(inspection, view, purpose);
  assert(result.requestedView == view);
  assert(result.resolvedView == view);
  assert(!result.usable && !result.exists && result.archivePath.empty());
  assert(result.provenance == Provenance::RuntimeFallback);
  assert(result.fallbackReason == Reason::RuntimeGeometry);
  assert(!result.diagnostic.empty());
  assert(!result.usedViewFallback);
  std::string error;
  assert(!symbol_cache::LoadUsableFixtureSymbol(path, view, &error, purpose));
  assert(error == result.diagnostic);
}

void TestAuthoredAndCoexistence(Fixtures &fixtures) {
  const auto top = fixtures.Write("top", Fixture().WithArchiveEntry("models/svg/body.svg", kSvg));
  Check(top, View::Top, Purpose::StandardGdtf, "models/svg/body.svg",
        Set::StandardGdtf, Provenance::AuthoredGdtf);
  CheckRuntime(top, View::Front, Purpose::StandardGdtf);
  Check(top, View::Front, Purpose::InternalRendering, "models/svg/body.svg",
        Set::StandardGdtf, Provenance::AuthoredGdtf, Reason::TopView, View::Top, true);
  Check(top, View::Right, Purpose::InternalRendering, "models/svg/body.svg",
        Set::StandardGdtf, Provenance::AuthoredGdtf, Reason::TopView, View::Top, true);
  Check(top, View::Back, Purpose::InternalRendering, "models/svg/body.svg",
        Set::StandardGdtf, Provenance::AuthoredGdtf, Reason::TopView, View::Top, true);

  const auto complete = fixtures.Write("standard", Fixture()
      .WithArchiveEntry("models/svg/body.svg", kSvg)
      .WithArchiveEntry("models/svg_side/body.svg", kSvg)
      .WithArchiveEntry("models/svg_front/body.svg", kSvg));
  assert(symbol_cache::InspectFixtureSymbolAvailability(complete).resources.standardViewsUsable);
  for (const auto view : {View::Top, View::Left, View::Front}) {
    const std::string folder = view == View::Left ? "svg_side" : view == View::Front ? "svg_front" : "svg";
    Check(complete, view, Purpose::StandardGdtf, "models/" + folder + "/body.svg",
          Set::StandardGdtf, Provenance::AuthoredGdtf, Reason::None, view);
  }
  Check(complete, View::Right, Purpose::StandardGdtf, "models/svg_side/body.svg",
        Set::StandardGdtf, Provenance::AuthoredGdtf, Reason::SideCompatibility, View::Left);
  CheckRuntime(complete, View::Bottom, Purpose::StandardGdtf);
  Check(complete, View::Bottom, Purpose::InternalRendering, "models/svg/body.svg",
        Set::StandardGdtf, Provenance::AuthoredGdtf, Reason::TopView, View::Top, true);

  const auto both = fixtures.Write("both", Fixture()
      .WithArchiveEntry("models/svg/body.svg", kSvg)
      .WithArchiveEntry("perastage/symbols/body/top.svg", kInternal)
      .WithArchiveEntry("perastage/symbols/body/side.svg", kInternal));
  const auto internal = Check(both, View::Top, Purpose::InternalRendering,
      "perastage/symbols/body/top.svg", Set::Perastage, Provenance::GeneratedPerastage);
  assert(internal.offsetXmm == 3.0 && internal.offsetYmm == 4.0);
  Check(both, View::Top, Purpose::StandardGdtf, "models/svg/body.svg",
        Set::StandardGdtf, Provenance::AuthoredGdtf);
  Check(both, View::Right, Purpose::InternalRendering, "perastage/symbols/body/side.svg",
        Set::Perastage, Provenance::GeneratedPerastage, Reason::SideCompatibility, View::Left);
  CheckRuntime(both, View::Left, Purpose::StandardGdtf);
  const auto missing = symbol_cache::InspectFixtureSymbolAvailability(both);
  assert(!missing.resources.FindStandardView(View::Left)->exists);
}

void TestLegacyAndRecovery(Fixtures &fixtures) {
  const std::string marked = "<svg data-perastage-symbol-version=\"1\" "
      "viewBox=\"0 0 10 10\"><polygon points=\"0,0 10,0 10,10\"/></svg>";
  const auto legacy = fixtures.Write("legacy", Fixture()
      .WithArchiveEntry("models/svg/body.svg", marked)
      .WithArchiveEntry("models/svg_bottom/body.svg", kSvg));
  for (const auto purpose : {Purpose::StandardGdtf, Purpose::InternalRendering}) {
    Check(legacy, View::Top, purpose, "models/svg/body.svg",
          Set::Perastage, Provenance::LegacyPerastage, Reason::LegacyResource);
    Check(legacy, View::Bottom, purpose, "models/svg_bottom/body.svg",
          Set::Perastage, Provenance::LegacyPerastage, Reason::LegacyResource, View::Bottom);
  }
  const auto malformed = fixtures.Write("malformed_internal", Fixture()
      .WithArchiveEntry("models/svg/body.svg", kSvg)
      .WithArchiveEntry("perastage/symbols/body/top.svg", "<svg>"));
  const auto fallback = Check(malformed, View::Top, Purpose::InternalRendering,
      "models/svg/body.svg", Set::StandardGdtf, Provenance::AuthoredGdtf,
      Reason::StandardRenderingResource);
  assert(fallback.diagnostic.find("malformed") != std::string::npos);
  const auto malformedLegacy = fixtures.Write("malformed_then_legacy", Fixture()
      .WithArchiveEntry("models/svg/body.svg", marked)
      .WithArchiveEntry("perastage/symbols/body/top.svg", "<svg>"));
  Check(malformedLegacy, View::Top, Purpose::InternalRendering,
        "models/svg/body.svg", Set::Perastage, Provenance::LegacyPerastage, Reason::LegacyResource);
  const auto dedicatedLegacy = fixtures.Write("dedicated_and_legacy", Fixture()
      .WithArchiveEntry("models/svg/body.svg", marked)
      .WithArchiveEntry("perastage/symbols/body/top.svg", kInternal)
      .WithArchiveEntry("models/svg_bottom/body.svg", kSvg)
      .WithArchiveEntry("perastage/symbols/body/bottom.svg", kInternal));
  Check(dedicatedLegacy, View::Top, Purpose::StandardGdtf, "models/svg/body.svg",
        Set::Perastage, Provenance::LegacyPerastage, Reason::LegacyResource);
  Check(dedicatedLegacy, View::Top, Purpose::InternalRendering,
        "perastage/symbols/body/top.svg", Set::Perastage, Provenance::GeneratedPerastage);
  for (const auto purpose : {Purpose::StandardGdtf, Purpose::InternalRendering})
    Check(dedicatedLegacy, View::Bottom, purpose, "perastage/symbols/body/bottom.svg",
          Set::Perastage, Provenance::GeneratedPerastage, Reason::None, View::Bottom);
  const auto malformedBottom = fixtures.Write("malformed_bottom", Fixture()
      .WithArchiveEntry("models/svg_bottom/body.svg", kSvg)
      .WithArchiveEntry("perastage/symbols/body/bottom.svg", "<svg>"));
  for (const auto purpose : {Purpose::StandardGdtf, Purpose::InternalRendering})
    Check(malformedBottom, View::Bottom, purpose, "models/svg_bottom/body.svg",
          Set::Perastage, Provenance::LegacyPerastage, Reason::LegacyResource, View::Bottom);
  const auto empty = fixtures.Write("empty", Fixture());
  CheckRuntime(empty, View::Bottom, Purpose::InternalRendering);
  CheckRuntime(empty, View::Right, Purpose::InternalRendering);
}

void TestStandardCandidatePriority(Fixtures &fixtures) {
  const std::string generated = "<svg data-perastage-symbol-version=\"1\" "
      "data-perastage-resource-set=\"standard-gdtf\" "
      "viewBox=\"0 0 30 10\"><polygon points=\"0,0 30,0 30,10\"/></svg>";
  // The existing inspector recognizes exact and extension-stripped model names.
  // Authored content must win over generated content even if enumerated second.
  const auto authored = fixtures.Write("standard_priority", Fixture()
      .WithModelResource("body.svg")
      .WithArchiveEntry("models/svg/body.svg.svg", generated)
      .WithArchiveEntry("models/svg/body.svg", kSvg));
  Check(authored, View::Top, Purpose::StandardGdtf, "models/svg/body.svg",
        Set::StandardGdtf, Provenance::AuthoredGdtf);
  const auto recovered = fixtures.Write("standard_recovery", Fixture()
      .WithModelResource("body.svg")
      .WithArchiveEntry("models/svg/body.svg.svg", "<svg>")
      .WithArchiveEntry("models/svg/body.svg", generated));
  const auto resolution = Check(recovered, View::Top, Purpose::StandardGdtf,
      "models/svg/body.svg", Set::StandardGdtf, Provenance::GeneratedPerastage);
  assert(resolution.diagnostic.find("malformed") != std::string::npos);
  // An available legacy alternative cannot displace usable standard content.
  const auto standardLegacy = fixtures.Write("standard_and_legacy", Fixture()
      .WithModelResource("body.svg")
      .WithArchiveEntry("models/svg/body.svg.svg", generated)
      .WithArchiveEntry("models/svg/body.svg", kInternal));
  Check(standardLegacy, View::Top, Purpose::StandardGdtf, "models/svg/body.svg.svg",
        Set::StandardGdtf, Provenance::GeneratedPerastage);
}
} // namespace

int main() {
  wxInitializer wx;
  assert(wx.IsOk());
  Fixtures fixtures;
  TestAuthoredAndCoexistence(fixtures);
  TestLegacyAndRecovery(fixtures);
  TestStandardCandidatePriority(fixtures);
  symbol_cache::ClearFixtureSymbolRuntimeCaches();
}

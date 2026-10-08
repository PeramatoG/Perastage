#include "symbols/fixture_symbol_resource_contract.h"
#include "support/gdtf_test_fixture_builder.h"
#include "wx_path_utils.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {
namespace fs = std::filesystem;
using Entries = std::vector<std::pair<std::string, std::string>>;

constexpr std::array<SymbolViewKind, 3> kStandardViews = {
    SymbolViewKind::Top, SymbolViewKind::Left, SymbolViewKind::Front};
constexpr std::array<SymbolViewKind, 4> kPerastageViews = {
    SymbolViewKind::Top, SymbolViewKind::Left, SymbolViewKind::Front,
    SymbolViewKind::Bottom};
const std::string kAuthoredSvg =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10 10\">"
    "<polygon points=\"0,0 10,0 10,10 0,10\"/></svg>";

void Require(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

std::string ResourcePath(const std::string &base, SymbolViewKind view) {
  if (view == SymbolViewKind::Front)
    return "models/svg_front/" + base + ".svg";
  if (view == SymbolViewKind::Left)
    return "models/svg_side/" + base + ".svg";
  return "models/svg/" + base +
         (view == SymbolViewKind::Bottom ? "_bottom.svg" : ".svg");
}

std::string PerastagePath(const std::string &base, SymbolViewKind view) {
  std::string name = "top";
  if (view == SymbolViewKind::Left)
    name = "side";
  else if (view == SymbolViewKind::Front)
    name = "front";
  else if (view == SymbolViewKind::Bottom)
    name = "bottom";
  return "perastage/symbols/" + base + "/" + name + ".svg";
}

std::string MarkedSvg(const std::string &version = "1",
                      bool standardGdtf = false) {
  std::string svg = kAuthoredSvg;
  svg.insert(4, " data-perastage-symbol-version=\"" + version + "\"");
  if (standardGdtf)
    svg.insert(4, " data-perastage-resource-set=\"standard-gdtf\"");
  return svg;
}

Entries StandardEntries(const std::string &base, size_t count = 3) {
  Entries entries;
  for (size_t i = 0; i < count; ++i)
    entries.emplace_back(ResourcePath(base, kStandardViews[i]), kAuthoredSvg);
  return entries;
}

Entries PerastageEntries(const std::string &base) {
  Entries entries;
  for (const auto view : kPerastageViews)
    entries.emplace_back(PerastagePath(base, view), MarkedSvg());
  return entries;
}

tests::gdtf::FixtureBuilder AuthoredFixture(const std::string &base,
                                          const Entries &entries) {
  auto builder = tests::gdtf::BuildMinimalValidFixture();
  builder.WithFixtureIdentity("Manufacturer fixture", "Manufacturer",
                              tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId)
      .WithModelResource(base);
  for (const auto &[path, bytes] : entries)
    builder.WithArchiveEntry(path, bytes);
  return builder;
}

// Keeps XML construction in the shared builder while adding historical metadata.
void WriteWithMetadata(const fs::path &path, const std::string &base,
                       const Entries &entries, const std::string &metadata) {
  std::string xml = AuthoredFixture(base, {}).BuildDescriptionXml();
  xml.insert(xml.find("    <Models>"), metadata);
  wxFileOutputStream output(WxPathUtils::WxStringFromFilesystemPath(path));
  Require(output.IsOk(), "Could not open metadata fixture");
  wxZipOutputStream zip(output);
  Require(zip.PutNextEntry("description.xml"), "Could not write description.xml");
  zip.Write(xml.data(), xml.size());
  for (const auto &[entryPath, bytes] : entries) {
    Require(zip.PutNextEntry(wxString::FromUTF8(entryPath)),
            "Could not write metadata resource");
    zip.Write(bytes.data(), bytes.size());
  }
  Require(zip.Close(), "Could not close metadata fixture");
}

std::string ReadBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  Require(input.is_open(), "Could not read source fixture bytes");
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

class Fixtures {
public:
  Fixtures() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned int attempt = 0; attempt < 100; ++attempt) {
      root = fs::temp_directory_path() /
             ("fixture_symbol_contract_" + std::to_string(stamp) + "_" +
              std::to_string(attempt));
      if (fs::create_directory(root))
        return;
    }
    throw std::runtime_error("Could not create fixture test directory");
  }

  ~Fixtures() {
    std::error_code error;
    fs::remove_all(root, error);
  }

  fs::path Path(const std::string &name) const { return root / (name + ".gdtf"); }

  FixtureSymbolResourceInspection Inspect(const fs::path &path) const {
    const std::string before = ReadBytes(path);
    const auto modificationTime = fs::last_write_time(path);
    FixtureSymbolResourceInspection inspection;
    Require(InspectFixtureSymbolResources(path.string(), inspection),
            path.filename().string() + ": " + inspection.diagnostic);
    Require(ReadBytes(path) == before, "Inspection changed source archive bytes");
    Require(fs::last_write_time(path) == modificationTime,
            "Inspection changed source archive modification time");
    return inspection;
  }

private:
  fs::path root;
};

const FixtureSymbolResource &View(const FixtureSymbolResourceInspection &inspection,
                                  SymbolViewKind view, bool standardGdtf = true) {
  const auto *resource = standardGdtf ? inspection.FindStandardView(view)
                                     : inspection.FindPerastageView(view);
  Require(resource != nullptr, "Inspection omitted a view");
  return *resource;
}

void CheckResource(const FixtureSymbolResourceInspection &inspection,
                   const std::string &base, SymbolViewKind view, bool exists,
                   FixtureSymbolProvenance provenance, bool usable = true,
                   bool standardGdtf = true,
                   const std::string &legacyPath = {}) {
  const auto &resource = View(inspection, view, standardGdtf);
  Require(resource.viewKind == view, "Resource has the wrong view kind");
  const std::string expectedPath = !legacyPath.empty()
                                       ? legacyPath
                                       : standardGdtf ? ResourcePath(base, view)
                                                      : PerastagePath(base, view);
  Require(resource.archivePath == expectedPath,
          "Resource has the wrong archive path");
  Require(resource.exists == exists, "Resource presence is incorrect");
  Require(resource.usable == (exists && usable), "Resource usability is incorrect");
  Require(resource.provenance == provenance, "Resource provenance is incorrect");
  Require(resource.standardGdtf == standardGdtf,
          "Resource has the wrong standard/Perastage category");
  Require(resource.resourceSet ==
              (standardGdtf ? FixtureSymbolResourceSet::StandardGdtf
                            : FixtureSymbolResourceSet::Perastage),
          "Resource belongs to the wrong resource set");
  Require(resource.PerastageOwned() ==
              (provenance == FixtureSymbolProvenance::GeneratedPerastage ||
               provenance == FixtureSymbolProvenance::LegacyPerastage),
          "Resource ownership disagrees with its provenance");
  if (!exists || !usable)
    Require(!resource.diagnostic.empty(), "Unavailable view lacks a diagnostic");
}

void CheckAuthoredStandardViews(const FixtureSymbolResourceInspection &inspection,
                                const std::string &base, size_t count = 3) {
  Require(inspection.standardViewsUsable == (count == 3),
          "Standard completeness must depend only on Top, Side and Front");
  for (size_t i = 0; i < kStandardViews.size(); ++i)
    CheckResource(inspection, base, kStandardViews[i], i < count,
                  i < count ? FixtureSymbolProvenance::AuthoredGdtf
                            : FixtureSymbolProvenance::None);
}

void CheckAbsentPerastageViews(const FixtureSymbolResourceInspection &inspection,
                              const std::string &base) {
  Require(!inspection.perastageViewsUsable,
          "Absent Perastage resources must not complete the internal set");
  for (const auto view : kPerastageViews)
    CheckResource(inspection, base, view, false, FixtureSymbolProvenance::None,
                  true, false);
}

void CheckStandardCases(Fixtures &fixtures) {
  // Includes authored Top only, Top/Side, no views, and a complete standard set.
  for (size_t count = 0; count <= 3; ++count) {
    const auto path = fixtures.Path("authored_" + std::to_string(count));
    AuthoredFixture("manufacturer_body", StandardEntries("manufacturer_body", count))
        .WriteArchive(path);
    const auto inspection = fixtures.Inspect(path);
    CheckAuthoredStandardViews(inspection, "manufacturer_body", count);
    CheckAbsentPerastageViews(inspection, "manufacturer_body");
    Require(inspection.FindStandardView(SymbolViewKind::Bottom) == nullptr,
            "Bottom must not be part of the official GDTF resource set");
    Require(inspection.FindStandardView(SymbolViewKind::Right) ==
                inspection.FindStandardView(SymbolViewKind::Left),
            "Both side orientations must refer to the standard Side resource");
    if (count < 3)
      Require(!inspection.diagnostic.empty(), "Missing standard views lack diagnostics");
  }

  for (const std::string base : {"base", "main", "Body"}) {
    const auto path = fixtures.Path("conventional_" + base);
    AuthoredFixture(base, StandardEntries(base)).WriteArchive(path);
    CheckAuthoredStandardViews(fixtures.Inspect(path), base);
  }
}

void CheckGeneratedCases(Fixtures &fixtures) {
  const auto own = PerastageEntries("base");
  auto entries = StandardEntries("base");
  entries.insert(entries.end(), own.begin(), own.end());
  const auto path = fixtures.Path("both_sets");
  AuthoredFixture("base", entries).WriteArchive(path);
  const auto inspection = fixtures.Inspect(path);
  CheckAuthoredStandardViews(inspection, "base");
  Require(inspection.perastageViewsUsable, "Dedicated internal set is incomplete");
  for (const auto view : kPerastageViews)
    CheckResource(inspection, "base", view, true,
                  FixtureSymbolProvenance::GeneratedPerastage, true, false);
  Require(View(inspection, SymbolViewKind::Top).archivePath !=
              View(inspection, SymbolViewKind::Top, false).archivePath,
          "The same logical view must have distinct standard and internal resources");

  const auto internalOnly = fixtures.Path("internal_only");
  AuthoredFixture("base", own).WriteArchive(internalOnly);
  const auto internal = fixtures.Inspect(internalOnly);
  CheckAuthoredStandardViews(internal, "base", 0);
  Require(internal.perastageViewsUsable,
          "An internal set must not be presented as standard GDTF views");

  entries = StandardEntries("base", 2);
  entries.insert(entries.end(), own.begin(), own.end());
  const auto partial = fixtures.Path("partial_standard_complete_internal");
  AuthoredFixture("base", entries).WriteArchive(partial);
  const auto partialInspection = fixtures.Inspect(partial);
  CheckAuthoredStandardViews(partialInspection, "base", 2);
  Require(partialInspection.perastageViewsUsable,
          "Missing standard Front must not hide the independent internal Front");

  // Describes output of a future standard converter without implementing it.
  entries = StandardEntries("base");
  entries[0].second = MarkedSvg("1", true);
  entries.insert(entries.end(), own.begin(), own.end());
  const auto converted = fixtures.Path("generated_standard_and_internal");
  AuthoredFixture("base", entries).WriteArchive(converted);
  const auto futureStandard = fixtures.Inspect(converted);
  Require(futureStandard.standardViewsUsable && futureStandard.perastageViewsUsable,
          "Generated standard and internal resources must be independently complete");
  CheckResource(futureStandard, "base", SymbolViewKind::Top, true,
                FixtureSymbolProvenance::GeneratedPerastage);
  CheckResource(futureStandard, "base", SymbolViewKind::Top, true,
                FixtureSymbolProvenance::GeneratedPerastage, true, false);

  const auto futurePath = fixtures.Path("future_internal_marker");
  AuthoredFixture("base", {{PerastagePath("base", SymbolViewKind::Top), MarkedSvg("99")}})
      .WriteArchive(futurePath);
  const auto future = fixtures.Inspect(futurePath);
  CheckResource(future, "base", SymbolViewKind::Top, true,
                FixtureSymbolProvenance::GeneratedPerastage, true, false);
  Require(!View(future, SymbolViewKind::Top, false).diagnostic.empty(),
          "An unknown Perastage resource version must report a diagnostic");
}

void CheckLegacyCases(Fixtures &fixtures) {
  const auto editorPath = fixtures.Path("generic_editor");
  AuthoredFixture("base", StandardEntries("base"))
      .WithEditor("Perastage 1.0")
      .WriteArchive(editorPath);
  CheckAuthoredStandardViews(fixtures.Inspect(editorPath), "base");

  const auto revisionPath = fixtures.Path("generic_revision");
  WriteWithMetadata(
      revisionPath, "main", StandardEntries("main"),
      "    <PerastageMutationAudit SchemaVersion=\"1\"/>\n"
      "    <Revisions><Revision Date=\"2026-01-01T00:00:00\" ModifiedBy=\"Perastage 1.0\" "
      "Text=\"Updated physical properties\"/></Revisions>\n");
  CheckAuthoredStandardViews(fixtures.Inspect(revisionPath), "main");

  auto entries = StandardEntries("manufacturer_body");
  entries.emplace_back(ResourcePath("manufacturer_body", SymbolViewKind::Bottom),
                       kAuthoredSvg);
  const auto bottomPath = fixtures.Path("legacy_bottom");
  AuthoredFixture("manufacturer_body", entries).WriteArchive(bottomPath);
  const auto bottomInspection = fixtures.Inspect(bottomPath);
  CheckAuthoredStandardViews(bottomInspection, "manufacturer_body");
  CheckResource(bottomInspection, "manufacturer_body", SymbolViewKind::Bottom,
                true, FixtureSymbolProvenance::LegacyPerastage, true, false,
                ResourcePath("manufacturer_body", SymbolViewKind::Bottom));

  const auto legacyPath = fixtures.Path("legacy_applied_views");
  WriteWithMetadata(
      legacyPath, "manufacturer_body", entries,
      "    <Revisions><Revision Date=\"2026-01-01T00:00:00\" ModifiedBy=\"Perastage 1.0\" "
      "Text=\"Applied fixture SVG symbol views (front, bottom)\"/></Revisions>\n");
  const auto legacy = fixtures.Inspect(legacyPath);
  Require(!legacy.standardViewsUsable,
          "Legacy internal Front must not complete the standard GDTF set");
  CheckResource(legacy, "manufacturer_body", SymbolViewKind::Top, true,
                FixtureSymbolProvenance::AuthoredGdtf);
  CheckResource(legacy, "manufacturer_body", SymbolViewKind::Left, true,
                FixtureSymbolProvenance::AuthoredGdtf);
  CheckResource(legacy, "manufacturer_body", SymbolViewKind::Front, false,
                FixtureSymbolProvenance::None);
  for (const auto view : {SymbolViewKind::Front, SymbolViewKind::Bottom})
    CheckResource(legacy, "manufacturer_body", view, true,
                  FixtureSymbolProvenance::LegacyPerastage, true, false,
                  ResourcePath("manufacturer_body", view));

  const auto markedLegacy = fixtures.Path("legacy_marker_in_official_location");
  AuthoredFixture("base", {{ResourcePath("base", SymbolViewKind::Top), MarkedSvg()}})
      .WriteArchive(markedLegacy);
  const auto marked = fixtures.Inspect(markedLegacy);
  CheckResource(marked, "base", SymbolViewKind::Top, false,
                FixtureSymbolProvenance::None);
  CheckResource(marked, "base", SymbolViewKind::Top, true,
                FixtureSymbolProvenance::LegacyPerastage, true, false,
                ResourcePath("base", SymbolViewKind::Top));

  const auto currentRevision = fixtures.Path("internal_revision_does_not_claim_standard");
  WriteWithMetadata(currentRevision, "base", StandardEntries("base"),
      "    <Revisions><Revision ModifiedBy=\"Perastage 1.0\" "
      "Text=\"Applied Perastage fixture SVG symbol views (top, side, front, bottom)\"/>"
      "</Revisions>\n");
  CheckAuthoredStandardViews(fixtures.Inspect(currentRevision), "base");

}

void CheckRecoveryCases(Fixtures &fixtures) {
  const auto path = fixtures.Path("malformed_present");
  AuthoredFixture("base", {{ResourcePath("base", SymbolViewKind::Top), "<svg>"}})
      .WriteArchive(path);
  const auto inspection = fixtures.Inspect(path);
  Require(!inspection.standardViewsUsable, "Malformed view completed a standard set");
  CheckResource(inspection, "base", SymbolViewKind::Top, true,
                FixtureSymbolProvenance::AuthoredGdtf, false);
  CheckResource(inspection, "base", SymbolViewKind::Front, false,
                FixtureSymbolProvenance::None);
  const auto malformedBottom = fixtures.Path("malformed_optional_bottom");
  auto entries = StandardEntries("base");
  entries.emplace_back(ResourcePath("base", SymbolViewKind::Bottom), "<svg>");
  AuthoredFixture("base", entries).WriteArchive(malformedBottom);
  const auto optional = fixtures.Inspect(malformedBottom);
  CheckAuthoredStandardViews(optional, "base");
  CheckResource(optional, "base", SymbolViewKind::Bottom, true,
                FixtureSymbolProvenance::LegacyPerastage, false, false,
                ResourcePath("base", SymbolViewKind::Bottom));

  std::string badOffsetSvg = MarkedSvg();
  badOffsetSvg.insert(4, " data-perastage-offset-x-mm=\"invalid\"");
  const auto invalidOffset = fixtures.Path("invalid_internal_offset");
  AuthoredFixture("base", {{PerastagePath("base", SymbolViewKind::Top), badOffsetSvg}})
      .WriteArchive(invalidOffset);
  CheckResource(fixtures.Inspect(invalidOffset), "base", SymbolViewKind::Top,
                true, FixtureSymbolProvenance::GeneratedPerastage, false, false);

}
} // namespace

int main(int argc, char **argv) {
  wxInitializer wx;
  if (!wx.IsOk()) {
    std::cerr << "Could not initialize wxWidgets\n";
    return 1;
  }
  try {
    Fixtures fixtures;
    const std::string group = argc == 2 ? argv[1] : "all";
    Require(group == "all" || group == "standard" || group == "generated" ||
                group == "legacy" || group == "recovery",
            "Unknown fixture symbol contract test group");
    if (group == "all" || group == "standard")
      CheckStandardCases(fixtures);
    if (group == "all" || group == "generated")
      CheckGeneratedCases(fixtures);
    if (group == "all" || group == "legacy")
      CheckLegacyCases(fixtures);
    if (group == "all" || group == "recovery")
      CheckRecoveryCases(fixtures);
  } catch (const std::exception &error) {
    std::cerr << "Fixture symbol resource contract: " << error.what() << '\n';
    return 1;
  }
  return 0;
}

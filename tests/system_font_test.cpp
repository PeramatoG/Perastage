#include "support/system_font_test_utils.h"
#include "typography/composition.h"
#include "typography/glyph_outline.h"
#include <future>
#include <iostream>

int main(int argc, char **argv) {
  using namespace typography;
  using namespace system_font_test;
  if (argc == 2) {
    const std::filesystem::path watched(argv[1]);
    const auto discover = [] { return GetFontCatalog(PERASTAGE_TEST_RESOURCE_DIR, FontRefresh::All); };
    const auto files = Sans();
    const auto uninstalled = discover();
    assert(uninstalled->Resolve(sans, false).issue == FontIssue::Missing);
    std::filesystem::copy_file(files.path, watched);
    const auto installed = discover();
    const auto reference = installed->ReferenceForFamily(sans);
    const auto face = installed->Resolve(reference, false).face;
    assert(face && face->source == FontSource::InstalledSystem);
    assert(installed->Signature() != uninstalled->Signature());
    std::filesystem::remove(watched);
    const auto removed = discover();
    assert(removed->Resolve(reference, false).issue == FontIssue::Missing);
    assert(installed->Resolve(reference, false).face->fingerprint == face->fingerprint);
    std::filesystem::copy_file(files.path, watched);
    assert(discover()->Resolve(reference, false).face->fingerprint == face->fingerprint);
    return 0;
  }
  const auto now = std::chrono::steady_clock::now;
  const auto report = [&](const char *name, auto start) {
    std::cout << name << ": " << std::chrono::duration<double, std::milli>(now() - start).count() << " ms\n";
  };
  const auto firstDiscovery = now();
  const auto native = DiscoverSystemFonts();
  report("Native discovery", firstDiscovery);
  assert(!native.empty());
  std::cout << "Native records: " << native.size() << '\n';
  const auto warmDiscovery = now();
  auto snapshot = GetFontCatalog(PERASTAGE_TEST_RESOURCE_DIR);
  assert(snapshot == GetFontCatalog(PERASTAGE_TEST_RESOURCE_DIR));
  report("Build native catalog and reuse snapshot", warmDiscovery);

  assert(ClassifyEmbedding(4, 0).right == EmbeddingRight::Installable);
  assert(ClassifyEmbedding(4, 2).right == EmbeddingRight::Restricted);
  assert(ClassifyEmbedding(4, 4).right == EmbeddingRight::PreviewPrint);
  assert(ClassifyEmbedding(4, 8).right == EmbeddingRight::Editable);
  assert(ClassifyEmbedding(4, 0x108).noSubsetting && ClassifyEmbedding(4, 0x108).PermitsPdf());
  assert(ClassifyEmbedding(4, 0x208).bitmapOnly && !ClassifyEmbedding(4, 0x208).PermitsPdf());
  assert(!ClassifyEmbedding(4, 6).PermitsPdf() && !ClassifyEmbedding(4, 0x10).PermitsPdf());
  assert(ClassifyEmbedding(2, 6).right == EmbeddingRight::PreviewPrint);
  assert(ClassifyEmbedding(1, 0x208).right == EmbeddingRight::Editable &&
         !ClassifyEmbedding(1, 0x208).bitmapOnly);

  Files files;
  auto restricted = Sans();
  restricted.family = "Restricción de prueba";
  restricted.path = files.ChangeEmbedding("fuente-é.ttf", 2);
  auto bitmap = Sans();
  bitmap.family = "Bitmap";
  bitmap.path = files.ChangeEmbedding("bitmap.ttf", 0x200);
  auto inaccessible = Sans();
  inaccessible.family = "Removed";
  inaccessible.path = files.root / "removed.ttf";
  auto corrupt = Sans();
  corrupt.family = "Corrupt";
  corrupt.path = files.root / "corrupt.ttf";
  std::ofstream(corrupt.path) << "\0\1";
  auto variable = Sans();
  variable.family = "Variable";
  variable.path = std::filesystem::path(PERASTAGE_SYSTEM_FONT_FIXTURES) / "PerastageTestVariable.ttf";
  auto records = std::vector{Sans(), Cff(), BoldCollection(), restricted, bitmap, inaccessible, corrupt, variable};
  for (int i = 0; i < 450; ++i) {
    auto other = inaccessible;
    other.family = "Unused " + std::to_string(i);
    records.push_back(std::move(other));
  }

  const auto startCatalog = now();
  FontCatalog catalog(PERASTAGE_TEST_RESOURCE_DIR, {}, records);
  report("Build catalog with over 450 metadata records", startCatalog);
  assert(catalog.SystemFamilies().size() == records.size() - 1);
  assert(catalog.Families().size() == 4);
  const auto select = now();
  const auto installed = catalog.Resolve(sans, false);
  report("Validate selected face", select);
  assert(installed.issue == FontIssue::None && installed.face->source == FontSource::InstalledSystem);
  assert(installed.face->CanEmbedPdf());
  auto reference = catalog.ReferenceForFamily(sans);
  assert(DecodeFontReference(reference).PostScriptName(false, false) == "PerastageTestSans-Regular");
  assert(catalog.Resolve(reference, true).face->metrics.faceIndex == 1);
  const auto bold = catalog.Resolve(reference, true);
  assert(bold.face->bold && bold.face->collection && !bold.face->CanEmbedPdf());
  assert(catalog.Resolve(reference, true, false, FontUsage::Pdf).issue == FontIssue::PdfFormatUnsupported);
  const auto cff = catalog.Resolve(adobeStyle, false);
  assert(cff.issue == FontIssue::None && cff.face->outline == FontOutline::Cff);
  assert(catalog.Resolve(adobeStyle, false, false, FontUsage::Pdf).issue == FontIssue::PdfFormatUnsupported);
  assert(catalog.Resolve(restricted.family, false).issue == FontIssue::None);
  assert(catalog.Resolve(restricted.family, false, false, FontUsage::Pdf).issue == FontIssue::EmbeddingUnverified);
  assert(catalog.Resolve(bitmap.family, false, false, FontUsage::Pdf).issue == FontIssue::EmbeddingUnverified);
  assert(catalog.Resolve(inaccessible.family, false).issue == FontIssue::Inaccessible);
  assert(catalog.Resolve(corrupt.family, false).issue == FontIssue::Invalid);
  assert(catalog.Resolve(variable.family, false).issue == FontIssue::UnsupportedFormat);
  assert(catalog.Resolve(restricted.family, false).face->fingerprint != installed.face->fingerprint);
  assert(catalog.Resolve(catalog.ReferenceForFamily(restricted.family), false, false, FontUsage::Pdf).issue ==
         FontIssue::EmbeddingUnverified);

  const auto glyphs = Compose("AV é", {20}, catalog, reference);
  const auto portable = Compose("AV é", {20}, catalog, {});
  assert(glyphs.resourceId == installed.face->fingerprint && glyphs.lines[0].width != portable.lines[0].width);
  for (const auto request : {std::string(sans), std::string(adobeStyle)}) {
    const auto run = Compose("AV", {16}, catalog, request);
    const auto outline = ResolveGlyphOutline(catalog, run.glyphs[0].resourceId, run.glyphs[0].glyphId);
    assert(outline && !outline->substituted && !outline->path.empty());
  }
  const auto build = now();
  std::vector<std::future<void>> concurrent;
  for (int i = 0; i < 4; ++i)
    concurrent.push_back(std::async(std::launch::async, [&] {
      for (int j = 0; j < 250; ++j)
        assert(Compose("AV é", {20}, catalog, reference).resourceId == glyphs.resourceId);
    }));
  for (auto &worker : concurrent) worker.get();
  report("1000 repeated shaping calls across four threads", build);

  FontCatalog absent(PERASTAGE_TEST_RESOURCE_DIR, {});
  const auto fallback = absent.Resolve(reference, false);
  assert(fallback.issue == FontIssue::Missing && fallback.face->family == kDefaultFamily);
  auto renamed = Sans();
  renamed.family = "Localized Family Name";
  FontCatalog refreshed(PERASTAGE_TEST_RESOURCE_DIR, {}, {renamed});
  assert(refreshed.Signature() != absent.Signature());
  assert(refreshed.Resolve(reference, false).face->fingerprint == installed.face->fingerprint);
}

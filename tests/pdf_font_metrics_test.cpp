#include "typography/composition.h"
#include "typography/font_resources.h"
#include "typography/glyph_outline.h"
#include "typography/text_block.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>

int main() {
  namespace fs = std::filesystem;
  using namespace typography;
  const fs::path resources(PERASTAGE_TEST_RESOURCE_DIR);
  const auto start = std::chrono::steady_clock::now();
  const FontCatalog catalog(resources);
  const auto elapsed = [](auto since) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - since).count();
  };
  std::cout << "Complete font catalog: " << elapsed(start) << " ms\n";
  assert(catalog.IsComplete());
  assert((catalog.Families() == std::vector<std::string_view>{kDefaultFamily, "Noto Serif", "Source Sans 3", "Noto Sans Mono"}));
  std::set<double> widths;
  std::set<std::string_view> fingerprints;
  for (const auto family : catalog.Families()) {
    for (const bool bold : {false, true}) {
      const auto resolved = catalog.Resolve(family, bold);
      assert(resolved.issue == FontIssue::None && resolved.face && resolved.face->bold == bold);
      const auto &face = *resolved.face;
      assert(CheckStaticTrueType(face.metrics.data) == FontIssue::None);
      assert(face.license == "OFL-1.1" && !face.provenance.empty());
      assert(fingerprints.insert(face.fingerprint).second);
      const auto run = Compose("Escena Málaga € — Ω", {12, TextUnit::PaperPoints, bold}, catalog, family);
      assert(run.unsupportedBytes.empty() && run.glyphs.size() == 19);
      assert(run.resourceId == face.fingerprint);
      assert(run.bounds.right - run.bounds.left >= run.lines.front().width - 1e-9);
      assert(run.bounds.top >= run.ascent && run.bounds.bottom <= -run.descent);
      if (!bold) widths.insert(run.lines.front().width);
      for (const auto &glyph : run.glyphs) {
        assert(glyph.glyphId > 0);
        assert(ResolveGlyphOutline(catalog, glyph.resourceId, glyph.glyphId));
      }
    }
  }
  assert(widths.size() == 4);
  for (const auto family : {kDefaultFamily, std::string_view("Noto Serif"), std::string_view("Source Sans 3")}) {
    for (const bool bold : {false, true}) {
      const auto italic = catalog.Resolve(family, bold, true);
      assert(italic.issue == FontIssue::None && italic.face->italic && italic.face->bold == bold);
      assert(italic.face->family == family);
    }
  }
  assert(catalog.Resolve("Noto Sans Mono", true, true).issue == FontIssue::FaceUnavailable);
  const auto &mono = catalog.Resolve("Noto Sans Mono", false).face->metrics;
  assert(mono.glyphAdvances[mono.GlyphFor('i')] == mono.glyphAdvances[mono.GlyphFor('W')]);
  const auto &sans = catalog.Resolve(kDefaultFamily, false).face->metrics;
  assert(sans.glyphAdvances[sans.GlyphFor(0x20ac)] == 572);
  assert(sans.glyphAdvances[sans.GlyphFor(0x2014)] == 1000);

  // Certified byte identity, not the filename, controls discovery and embedding.
  const auto root = fs::temp_directory_path() /
      ("perastage-fonts-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto system = root / "installed";
  fs::create_directories(root / "fonts");
  fs::create_directories(system);
  fs::copy_file(resources / "fonts/PerastageSans.ttf", root / "fonts/PerastageSans.ttf");
  fs::copy_file(resources / "fonts/PerastageSans-Bold.ttf", root / "fonts/PerastageSans-Bold.ttf");
  for (int i = 0; i < 250; ++i)
    std::ofstream(system / ("unapproved-" + std::to_string(i) + ".ttf")) << "Unapproved data";
  const auto missingStart = std::chrono::steady_clock::now();
  const FontCatalog absent(root, {system});
  std::cout << "Incomplete font catalog: " << elapsed(missingStart) << " ms\n";
  const auto unavailable = absent.Resolve("Noto Serif", true);
  assert(unavailable.issue == FontIssue::Missing && unavailable.face->family == kDefaultFamily);
  assert(absent.Families().size() == 1);
  auto substituted = Compose("Español", {10, TextUnit::PaperPoints, true}, absent, "Noto Serif");
  assert(substituted.fontIssue == FontIssue::Missing && !substituted.glyphs.empty());

  fs::copy_file(resources / "fonts/NotoSerif-Bold.ttf", system / "renamed.ttf");
  fs::copy_file(resources / "fonts/NotoSerif-Regular.ttf", system / "arbitrary-name.ttf");
  const auto testFont = root / "fonts/SourceSans3-Regular.ttf";
  fs::copy_file(resources / "fonts/NotoSerif-Regular.ttf", testFont);
  const FontCatalog recovered(root, {system});
  assert(recovered.Resolve("Noto Serif", true).face->source == FontSource::ApprovedSystem);
  assert(recovered.Resolve("Noto Serif", true).face->fingerprint == catalog.Resolve("Noto Serif", true).face->fingerprint);
  assert(recovered.Resolve("Source Sans 3", false).issue == FontIssue::Unapproved);
  auto bytes = catalog.Resolve("Source Sans 3", false).face->metrics.data;
  uint32_t offset = 0, length = 0;
  assert(FindTable(bytes, 0x4f532f32, offset, length));
  bytes[offset + 9] = 2;
  std::ofstream(testFont, std::ios::binary).write(bytes.data(), bytes.size());
  const FontCatalog restricted(root, {});
  assert(restricted.Resolve("Source Sans 3", false).issue == FontIssue::EmbeddingUnverified);
  assert(restricted.Resolve("Source Sans 3", false).face->family == kDefaultFamily);
  const auto cached = GetFontCatalog(root);
  const auto cachedStart = std::chrono::steady_clock::now();
  for (int i = 0; i < 1000; ++i) assert(GetFontCatalog(root) == cached);
  std::cout << "One thousand cached catalog lookups: " << elapsed(cachedStart) << " ms\n";
  fs::copy_file(resources / "fonts/SourceSans3-Regular.ttf", testFont,
                fs::copy_options::overwrite_existing);
  assert(GetFontCatalog(root) == cached);
  const auto refreshed = GetFontCatalog(root, FontRefresh::All);
  assert(refreshed->Resolve("Source Sans 3", false).issue == FontIssue::None);
  fs::remove_all(root);

  const auto compose = [&](const std::string &text, double maxWidth = 0) {
    return Compose(text, {18, TextUnit::PaperPoints, false, maxWidth}, catalog, "Noto Serif");
  };
  for (const auto &[a, b] : {std::pair{"é", "é"}, std::pair{"Å", "Å"}}) {
    const auto composed = compose(a), decomposed = compose(b);
    assert(composed.glyphs.size() == 1 && decomposed.glyphs.size() == 1);
    assert(composed.glyphs[0].glyphId == decomposed.glyphs[0].glyphId);
    assert(composed.glyphs[0].advance == decomposed.glyphs[0].advance);
    assert(decomposed.glyphs[0].text == b && decomposed.glyphs[0].sourceEnd == std::string_view(b).size());
  }
  for (const auto pair : {"AV", "To"})
    assert(compose(pair).lines[0].width < compose({pair[0]}).lines[0].width + compose({pair[1]}).lines[0].width);
  for (const auto text : {"fi", "ffi"}) {
    const auto run = compose(text, 1);
    assert(run.lines.size() == 1 && run.glyphs.size() == 1 && run.glyphs[0].text == text);
  }
  for (const auto sample : {"España · Málaga · Iluminación · Producción escénica · À È É Ñ Ü ß € £ © ™",
                             "Αθήνα — Москва — Журнал"}) {
    const auto run = compose(sample);
    assert(run.unsupportedBytes.empty());
  }
  const auto marks = compose("x́");
  assert(marks.glyphs.size() == 2 && marks.glyphs.back().clusterContinuation);
  assert(marks.glyphs.back().advance == 0 && marks.glyphs.back().offsetX != 0);
  assert(marks.glyphs[0].sourceEnd == marks.glyphs[1].sourceEnd);

  const auto checkRtl = [&](const char *text, const char *family, const char *firstVisual) {
    const auto run = compose(text);
    assert(run.unsupportedBytes.empty());
    const auto rtl = std::find_if(run.glyphs.begin(), run.glyphs.end(), [](const auto &g) { return g.rightToLeft; });
    assert(rtl != run.glyphs.end() && rtl->text == firstVisual);
    assert(rtl->resourceId == catalog.Resolve(family, false).face->fingerprint);
    assert(std::next(rtl)->sourceBegin < rtl->sourceBegin);
    return run;
  };
  checkRtl("Scene 12 — שלום", "Noto Sans Hebrew", "ם");
  const auto arabic = checkRtl("Fixture 24 — مرحبا", "Noto Sans Arabic", "ا");
  assert(arabic.glyphs.back().glyphId != catalog.Resolve("Noto Sans Arabic", false).face->metrics.GlyphFor(0x0645));
  const auto missing = compose("灯😀");
  assert(missing.unsupportedBytes.size() == 2 && missing.glyphs[0].glyphId == 0);
  assert(missing.glyphs[1].text == "😀");
  assert(Compose("🙼", {18, TextUnit::PaperPoints, false}, catalog, "Noto Sans Mono").unsupportedBytes.empty());

  TextBlock block;
  block.wrap = true;
  block.lines.push_back({{{"Title", 18, true, false, "Source Sans 3"},
                          {" Wörds", 10, false, false, "Noto Serif"}}});
  const auto mixed = ComposeTextBlock(block, 150, 100, catalog);
  assert(mixed.glyphs.front().baseline == mixed.glyphs.back().baseline);
  assert(mixed.glyphs.front().glyph.resourceId != mixed.glyphs.back().glyph.resourceId);
  const auto wrapped = ComposeTextBlock(block, 58, 120, catalog);
  assert(wrapped.glyphs.front().baseline < wrapped.glyphs.back().baseline);
}

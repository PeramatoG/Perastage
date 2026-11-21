#include "typography/composition.h"
#include "typography/font_resources.h"
#include "typography/glyph_outline.h"
#include "typography/text_block.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>
#include <set>

int main() {
  namespace fs = std::filesystem;
  using namespace typography;
  const fs::path resources(PERASTAGE_TEST_RESOURCE_DIR);
  const FontCatalog catalog(resources);
  assert((catalog.Families() == std::vector<std::string_view>{kDefaultFamily, "Noto Serif", "Source Sans 3"}));
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
      if (!bold) widths.insert(run.lines.front().width);
      for (const auto &glyph : run.glyphs) {
        assert(glyph.glyphId > 0);
        assert(ResolveGlyphOutline(catalog, glyph.resourceId, glyph.glyphId));
      }
    }
  }
  assert(widths.size() == 3);
  assert(catalog.Resolve("Noto Serif", true, true).face->postScriptName == "NotoSans-BoldItalic");
  assert(catalog.Resolve("Noto Serif", true, true).issue == FontIssue::FaceUnavailable);
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
  const FontCatalog absent(root, {});
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
  fs::remove_all(root);

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

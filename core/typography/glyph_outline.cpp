#include "glyph_outline.h"
#include "font_resources.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <map>
#include <mutex>

namespace typography {
namespace {
struct FreeType {
  FT_Library library = nullptr;
  FreeType() { FT_Init_FreeType(&library); }
  ~FreeType() { if (library) FT_Done_FreeType(library); }
};

struct OutlineBuilder {
  GlyphOutline &outline;
  double em, area = 0, x = 0, y = 0, startX = 0, startY = 0;
  bool active = false, reverse = false;

  void Add(double nextX, double nextY) {
    area += x * nextY - nextX * y;
    x = nextX;
    y = nextY;
  }

  void Close() {
    if (!active) return;
    Add(startX, startY);
    outline.path.push_back({GlyphPathElement::Kind::Close, 0, 0, 0, 0, (area > 0) != reverse});
    active = false;
  }

  static int Move(const FT_Vector *point, void *context) {
    auto &b = *static_cast<OutlineBuilder *>(context);
    b.Close();
    b.x = b.startX = point->x / b.em;
    b.y = b.startY = point->y / b.em;
    b.active = true;
    b.area = 0;
    b.outline.path.push_back({GlyphPathElement::Kind::Move, b.x, b.y});
    return 0;
  }

  static int Line(const FT_Vector *point, void *context) {
    auto &b = *static_cast<OutlineBuilder *>(context);
    b.Add(point->x / b.em, point->y / b.em);
    b.outline.path.push_back({GlyphPathElement::Kind::Line, b.x, b.y});
    return 0;
  }

  static int Quad(const FT_Vector *control, const FT_Vector *point, void *context) {
    auto &b = *static_cast<OutlineBuilder *>(context);
    b.Add(control->x / b.em, control->y / b.em);
    const auto cx = b.x, cy = b.y;
    b.Add(point->x / b.em, point->y / b.em);
    b.outline.path.push_back({GlyphPathElement::Kind::Quadratic, b.x, b.y, cx, cy});
    return 0;
  }

  static int Cubic(const FT_Vector *c1, const FT_Vector *c2, const FT_Vector *point, void *context) {
    auto &b = *static_cast<OutlineBuilder *>(context);
    b.Add(c1->x / b.em, c1->y / b.em);
    const auto cx = b.x, cy = b.y;
    b.Add(c2->x / b.em, c2->y / b.em);
    const auto dx = b.x, dy = b.y;
    b.Add(point->x / b.em, point->y / b.em);
    b.outline.path.push_back({GlyphPathElement::Kind::Cubic, b.x, b.y, cx, cy, false, dx, dy});
    return 0;
  }
};
}

std::shared_ptr<const GlyphOutline> ReadGlyphOutlineById(const TtfFontMetrics &font, uint16_t glyph) {
  if (!font.valid || font.unitsPerEm <= 0 || glyph >= font.glyphAdvances.size()) return {};
  thread_local FreeType ft;
  FT_Face raw = nullptr;
  if (!ft.library || FT_New_Memory_Face(ft.library,
      reinterpret_cast<const FT_Byte *>(font.data.data()), font.data.size(), font.faceIndex, &raw)) return {};
  const std::unique_ptr<std::remove_pointer_t<FT_Face>, decltype(&FT_Done_Face)> face(raw, FT_Done_Face);
  if (FT_Load_Glyph(raw, glyph, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP) ||
      raw->glyph->format != FT_GLYPH_FORMAT_OUTLINE) return {};
  auto result = std::make_shared<GlyphOutline>();
  auto &outline = raw->glyph->outline;
  OutlineBuilder builder{*result, static_cast<double>(font.unitsPerEm)};
  builder.reverse = FT_Outline_Get_Orientation(&outline) == FT_ORIENTATION_POSTSCRIPT;
  const FT_Outline_Funcs funcs{OutlineBuilder::Move, OutlineBuilder::Line, OutlineBuilder::Quad,
                               OutlineBuilder::Cubic, 0, 0};
  if (FT_Outline_Decompose(&outline, &funcs, &builder)) return {};
  builder.Close();
  return result;
}

std::shared_ptr<const GlyphOutline> ResolveGlyphOutline(const FontCatalog &catalog,
                                                       std::string_view resource,
                                                       uint16_t glyph) {
  const auto face = catalog.FindResource(resource);
  if (!face) return {};
  using Key = std::pair<std::string, uint16_t>;
  static std::mutex mutex;
  static std::map<Key, std::shared_ptr<const GlyphOutline>> cache;
  const Key key{face->fingerprint, glyph};
  {
    const std::lock_guard lock(mutex);
    if (const auto it = cache.find(key); it != cache.end()) return it->second;
  }
  auto outline = ReadGlyphOutlineById(face->metrics, glyph);
  if (!outline && glyph) {
    if (const auto missing = ReadGlyphOutlineById(face->metrics, 0)) {
      auto replacement = std::make_shared<GlyphOutline>(*missing);
      replacement->substituted = true;
      outline = std::move(replacement);
    }
  }
  const std::lock_guard lock(mutex);
  if (cache.size() >= 4096) cache.clear();
  return cache.emplace(key, std::move(outline)).first->second;
}
}

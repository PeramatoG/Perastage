#include "document_text_renderer.h"
#include "typography/glyph_outline.h"
#include <wx/graphics.h>

namespace gui {
void DrawComposedDocumentText(wxGraphicsContext &graphics,
    const typography::Composition &run, const typography::TtfFontMetrics &font,
    double baselineX, double baselineY, double outputScale) {
  for (const auto &glyph : run.glyphs) {
    const auto encoded = typography::EncodeWinAnsi(glyph.text);
    const auto outline = typography::ReadGlyphOutline(font, static_cast<unsigned char>(encoded[0]));
    if (!outline) continue;
    auto path = graphics.CreatePath();
    const auto x = [&](double value) { return (baselineX + glyph.x + value * run.style.size) * outputScale; };
    const auto y = [&](double value) { return (baselineY - glyph.baseline - value * run.style.size) * outputScale; };
    for (const auto &point : outline->path) {
      using Kind = typography::GlyphPathElement::Kind;
      switch (point.kind) {
      case Kind::Move: path.MoveToPoint(x(point.x), y(point.y)); break;
      case Kind::Line: path.AddLineToPoint(x(point.x), y(point.y)); break;
      case Kind::Quadratic: path.AddQuadCurveToPoint(x(point.cx), y(point.cy), x(point.x), y(point.y)); break;
      case Kind::Close: path.CloseSubpath(); break;
      }
    }
    graphics.FillPath(path, wxWINDING_RULE);
  }
}
}

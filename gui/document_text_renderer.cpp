#include "document_text_renderer.h"
#include "typography/glyph_outline.h"
#include <wx/graphics.h>
#include "logger.h"
#include <set>

namespace gui {
void ReportDocumentTypographyDiagnostic(const std::string &diagnostic) {
  // Called on the GUI thread. Keep one report per issue during the session.
  static std::set<std::string> reported;
  if (reported.size() < 128 && reported.insert(diagnostic).second)
    Logger::Instance().Log(diagnostic);
}

// Converts the shared TrueType contour geometry into the wx graphics backend.
wxGraphicsPath BuildDocumentGlyphPath(wxGraphicsContext &graphics,
    const typography::GlyphOutline &outline, double baselineX,
    double baselineY, double size) {
  auto path = graphics.CreatePath();
  const auto x = [&](double value) { return baselineX + value * size; };
  const auto y = [&](double value) { return baselineY - value * size; };
  for (const auto &point : outline.path) {
    using Kind = typography::GlyphPathElement::Kind;
    switch (point.kind) {
    case Kind::Move: path.MoveToPoint(x(point.x), y(point.y)); break;
    case Kind::Line: path.AddLineToPoint(x(point.x), y(point.y)); break;
    case Kind::Quadratic: path.AddQuadCurveToPoint(x(point.cx), y(point.cy), x(point.x), y(point.y)); break;
    case Kind::Cubic: path.AddCurveToPoint(x(point.cx), y(point.cy), x(point.dx), y(point.dy), x(point.x), y(point.y)); break;
    case Kind::Close: path.CloseSubpath(); break;
    }
  }
  return path;
}

void DrawComposedDocumentText(wxGraphicsContext &graphics,
    const typography::Composition &run, const typography::FontCatalog &catalog,
    double baselineX, double baselineY, double outputScale) {
  for (const auto &glyph : run.glyphs) {
    const auto outline = typography::ResolveGlyphOutline(catalog, glyph.resourceId, glyph.glyphId);
    if (!outline || outline->substituted)
      ReportDocumentTypographyDiagnostic("A document glyph contour is unavailable in the approved face.");
    if (!outline) continue;
    const auto path = BuildDocumentGlyphPath(graphics, *outline,
        (baselineX + glyph.x) * outputScale,
        (baselineY - glyph.baseline) * outputScale, run.style.size * outputScale);
    graphics.FillPath(path, wxWINDING_RULE);
  }
}
}

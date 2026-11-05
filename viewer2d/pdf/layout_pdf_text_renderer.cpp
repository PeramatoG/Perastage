#include "layout_pdf_text_renderer.h"
#include "layout_pdf_page_mapping.h"
#include <algorithm>
#include <cmath>
namespace layout_pdf_internal {
namespace {
double ComputePdfRunAscent(const PdfFontDefinition *font, double fontSize) {
  if (!font || !font->embedded || font->metrics.unitsPerEm <= 0)
    return fontSize * 0.8;
  const double unitsPerEm = static_cast<double>(font->metrics.unitsPerEm);
  const double ascentUnits = std::max(0.0, static_cast<double>(font->metrics.ascent));
  if (ascentUnits <= 0.0)
    return fontSize * 0.8;
  return (ascentUnits / unitsPerEm) * fontSize;
}

double ComputePdfRunLineHeight(const PdfFontDefinition *font, double fontSize) {
  if (!font || !font->embedded || font->metrics.unitsPerEm <= 0)
    return fontSize * 1.2;
  const double unitsPerEm = static_cast<double>(font->metrics.unitsPerEm);
  const double ascentUnits = std::max(0.0, static_cast<double>(font->metrics.ascent));
  const double descentUnits =
      std::abs(std::min(0.0, static_cast<double>(font->metrics.descent)));
  const double heightUnits = ascentUnits + descentUnits;
  if (heightUnits <= 0.0)
    return fontSize * 1.2;
  return (heightUnits / unitsPerEm) * fontSize;
}

}
// Preserve legacy runs, metrics, alignment and italic skew in document points.
void RenderLayoutTextPdf(std::ostringstream &contentStream, const LayoutTextExportData &text,
    double pageH, double scaleX, double scaleY, const PdfFontCatalog &fontCatalog,
    const FloatFormatter &formatter) {
    const double frameW = static_cast<double>(text.frame.width);
    const double frameH = static_cast<double>(text.frame.height);
    if (frameW <= 0.0 || frameH <= 0.0)
      return;
    ScopedDocumentPageMapping pageMapping(contentStream, pageH, scaleX, scaleY, formatter);
    const double frameX = static_cast<double>(text.frame.x);
    const double frameY =
        pageH - text.frame.y - static_cast<double>(text.frame.height);

    contentStream << "q\n" << formatter.Format(frameX) << ' '
                  << formatter.Format(frameY) << ' '
                  << formatter.Format(frameW) << ' '
                  << formatter.Format(frameH) << " re W n\n";
    if (text.solidBackground) {
      contentStream << "1 1 1 rg " << formatter.Format(frameX) << ' '
                    << formatter.Format(frameY) << ' '
                    << formatter.Format(frameW) << ' '
                    << formatter.Format(frameH) << " re f\n";
    }

    const double padding = 4.0;
    const double availableH = std::max(0.0, frameH - padding * 2.0);
    constexpr double kItalicSkew = 0.2;
    auto appendTextRun = [&](double x, double y, const std::string &line,
                             const char *fontKey, double size, bool italic) {
      contentStream << "BT\n/" << fontKey << ' ' << formatter.Format(size)
                    << " Tf\n0 0 0 rg\n";
      if (italic) {
        contentStream << "1 0 " << formatter.Format(kItalicSkew) << " 1 "
                      << formatter.Format(x) << ' ' << formatter.Format(y)
                      << " Tm\n(" << EscapePdfLiteralBytes(line) << ") Tj\nET\n";
        return;
      }
      contentStream << formatter.Format(x) << ' ' << formatter.Format(y)
                    << " Td\n(" << EscapePdfLiteralBytes(line) << ") Tj\nET\n";
    };

    double usedHeight = 0.0;
    for (const auto &line : text.lines) {
      double lineFontSize =
          text.fontSize > 0 ? static_cast<double>(text.fontSize) : 12.0;
      double lineAscent = ComputePdfRunAscent(fontCatalog.regular, lineFontSize);
      double lineHeight = ComputePdfRunLineHeight(fontCatalog.regular, lineFontSize);
      for (const auto &run : line.runs) {
        const double runSize =
            run.fontSize > 0
                ? static_cast<double>(run.fontSize)
                : lineFontSize;
        lineFontSize = std::max(lineFontSize, runSize);
        const PdfFontDefinition *runFont =
            run.bold ? fontCatalog.bold : fontCatalog.regular;
        lineAscent = std::max(lineAscent, ComputePdfRunAscent(runFont, runSize));
        lineHeight =
            std::max(lineHeight, ComputePdfRunLineHeight(runFont, runSize));
      }
      if (usedHeight + lineHeight > availableH && usedHeight > 0.0)
        break;
      double lineWidth = 0.0;
      for (const auto &run : line.runs) {
        const double runSize =
            run.fontSize > 0
                ? static_cast<double>(run.fontSize)
                : lineFontSize;
        const PdfFontDefinition *font =
            run.bold ? fontCatalog.bold : fontCatalog.regular;
        const std::string encoded = EncodeWinAnsi(run.text);
        lineWidth += MeasureTextWidth(encoded, runSize, font);
      }
      double x = frameX + padding;
      if (text.alignment == LayoutTextExportData::Alignment::Center) {
        x = frameX + std::max(0.0, (frameW - lineWidth) * 0.5);
      } else if (text.alignment == LayoutTextExportData::Alignment::Right) {
        x = frameX + frameW - padding - lineWidth;
      }
      const double y = frameY + frameH - padding - usedHeight - lineAscent;
      double cursorX = x;
      if (line.runs.empty()) {
        usedHeight += lineHeight;
        continue;
      }
      for (const auto &run : line.runs) {
        const double runSize =
            run.fontSize > 0
                ? static_cast<double>(run.fontSize)
                : lineFontSize;
        const char *fontKey = run.bold ? "F2" : "F1";
        const PdfFontDefinition *font =
            run.bold ? fontCatalog.bold : fontCatalog.regular;
        const std::string encoded = EncodeWinAnsi(run.text);
        const double runWidth = MeasureTextWidth(encoded, runSize, font);
        appendTextRun(cursorX, y, encoded, fontKey, runSize, run.italic);
        cursorX += runWidth;
      }
      usedHeight += lineHeight;
    }
    contentStream << "Q\n";

    if (text.drawFrame) {
      contentStream << "q\n0 0 0 RG 0.5 w " << formatter.Format(frameX) << ' '
                    << formatter.Format(frameY) << ' '
                    << formatter.Format(frameW) << ' '
                    << formatter.Format(frameH) << " re S\nQ\n";
    }
}
}

#include "layouttextdialog.h"
#include "layout_pdf_text_renderer.h"
#include "pdf_unicode_fonts.h"
#include "support/legacy_layout_text_fixture.h"
#include "layouttextutils.h"
#include "typography/font_resources.h"
#include "typography/glyph_outline.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>

#include <wx/app.h>
#include <wx/richtext/richtextctrl.h>

namespace {


void CheckInk(const wxImage &image, const typography::TextBlockPlan &plan,
              const typography::FontCatalog &catalog, double scale, bool solid) {
  assert(image.IsOk() && image.HasAlpha());
  assert(image.GetAlpha(image.GetWidth() - 1, image.GetHeight() - 1) ==
         (solid ? 255 : 0));
  if (solid)
    assert(image.GetRed(image.GetWidth() - 1, image.GetHeight() - 1) == 255);
  std::string letters;
  bool coloredEdge = false;
  for (const auto &entry : plan.glyphs) {
    if (entry.glyph.text == " ") continue;
    letters += entry.glyph.text;
    const auto outline = typography::ResolveGlyphOutline(
        catalog, entry.glyph.resourceId, entry.glyph.glyphId);
    assert(outline && !outline->substituted && !outline->path.empty());
    double left = image.GetWidth(), top = image.GetHeight(), right = 0, bottom = 0;
    for (const auto &point : outline->path) {
      if (point.kind == typography::GlyphPathElement::Kind::Close) continue;
      const auto x = (entry.x + point.x * entry.size) * scale;
      const auto y = (entry.baseline - point.y * entry.size) * scale;
      left = std::min(left, x);
      top = std::min(top, y);
      right = std::max(right, x);
      bottom = std::max(bottom, y);
    }
    assert(left >= 0 && top >= 0 && right < image.GetWidth() && bottom < image.GetHeight());
    assert(right - left > 4 && bottom - top > 4);
    if (solid && entry.color == std::array<uint8_t, 3>{255, 255, 255}) continue;

    int inkPixels = 0;
    for (int y = static_cast<int>(top); y <= std::ceil(bottom); ++y) {
      for (int x = static_cast<int>(left); x <= std::ceil(right); ++x) {
        const auto alpha = image.GetAlpha(x, y);
        const bool matchesInk = std::abs(image.GetRed(x, y) - entry.color[0]) <= 16 &&
            std::abs(image.GetGreen(x, y) - entry.color[1]) <= 16 &&
            std::abs(image.GetBlue(x, y) - entry.color[2]) <= 16;
        if (alpha >= 240 && matchesInk)
          ++inkPixels;
        if (!solid && entry.glyph.text == "X" && alpha >= 55 && alpha <= 190)
          coloredEdge |= matchesInk;
      }
    }
    assert(inkPixels > (right - left) * (bottom - top) / 12);
    if (!solid && entry.glyph.text == "O")
      assert(image.GetAlpha(static_cast<int>((left + right) / 2),
                            static_cast<int>((top + bottom) / 2)) == 0);
  }
  assert(letters == "PABOÉZX");
  if (!solid) assert(coloredEdge);
}

void CheckPdf(const LayoutTextExportData &text, const typography::TextBlockPlan &preview,
              const std::shared_ptr<const typography::FontCatalog> &catalog) {
  using namespace layout_pdf_internal;
  constexpr double pageHeight = 240;
  PdfUnicodeFonts fonts(catalog);
  FloatFormatter fmt(6);
  std::ostringstream stream;
  RenderLayoutTextPdf(stream, text, pageHeight, 1, 1, fonts, fmt);
  const auto pdf = stream.str();
  assert(fonts.Diagnostics() == preview.diagnostics);
  std::size_t next = 0;
  for (const auto &entry : preview.glyphs) {
    const auto color = fmt.Format(entry.color[0] / 255.0) + ' ' +
        fmt.Format(entry.color[1] / 255.0) + ' ' +
        fmt.Format(entry.color[2] / 255.0) + " rg\nBT\n";
    const auto fill = pdf.find(color, next);
    assert(fill != std::string::npos);
    assert(pdf.find("BT\n", next) == fill + color.size() - 3);
    const auto placement = "1 0 0 1 " + fmt.Format(text.frame.x + entry.x) + ' ' +
        fmt.Format(pageHeight - text.frame.y - entry.baseline) + " Tm\n";
    const auto matrix = pdf.find(placement, fill);
    assert(matrix != std::string::npos && matrix < pdf.find("ET\n", fill));
    next = pdf.find("ET\n", matrix) + 3;
  }
  assert(pdf.find("BT\n", next) == std::string::npos);

  std::vector<PdfObject> objects;
  std::ostringstream resources;
  fonts.WriteResources(objects, resources);
  for (const auto *unicode : {"0050", "0041", "0042", "004F", "00C9", "005A", "0058"})
    assert(std::any_of(objects.begin(), objects.end(), [&](const auto &object) {
      return object.body.find(std::string("> <") + unicode + ">\n") != std::string::npos;
    }));
}

} // namespace

void TestLayoutTextRendering() {
  const auto original = wxString::FromUTF8(kLegacyLayoutText);
  layouts::LayoutTextDefinition text;
  text.richText = kLegacyLayoutText;
  text.text = "Different fallback";
  text.frame = {17, 29, 280, 120};
  const auto exportData = layouttext::BuildLayoutTextExportData(text);
  assert(exportData.lines.size() == 2);
  assert(exportData.lines[0].runs.size() == 2);
  assert(exportData.lines[0].runs[0].text == "P");
  assert(exportData.lines[0].runs[1].text == " A B O");
  assert(exportData.lines[0].runs[1].color[0] == 255);
  assert(exportData.lines[1].runs[0].text == "É Z" && exportData.lines[1].runs[0].bold);
  assert(exportData.lines[1].runs[1].color[0] == 80);
  const auto catalog = std::make_shared<const typography::FontCatalog>(PERASTAGE_TEST_RESOURCE_DIR);
  const auto plan = typography::ComposeTextBlock(exportData, 280, 120, *catalog);
  CheckPdf(exportData, plan, catalog);
  for (const bool solid : {true, false}) {
    text.solidBackground = solid;
    CheckInk(layouttext::RenderTextImage(text, {420, 180}, {280, 120}, 1.5),
             plan, *catalog, 1.5, solid);
  }

  LayoutTextDialog editor(nullptr, original, "Different fallback", true, false);
  editor.Show();
  wxYield();
  assert(editor.FindWindowByName("layoutTextLegacyNotice", &editor));
  assert(editor.GetRichText() == original);
  assert(editor.GetPlainText() == wxString::FromUTF8("P A B O\nÉ Z X"));
  wxRichTextCtrl *control = nullptr;
  for (auto *child : editor.GetChildren())
    if (auto *rich = dynamic_cast<wxRichTextCtrl *>(child)) control = rich;
  assert(control && control->GetLastPosition() > 8);
  assert(control->GetBuffer().GetLineAtPosition(6, false));
  assert(control->GetBackgroundColour().Red() < 220);
  wxRichTextAttr white;
  assert(control->GetStyle(2, white) && white.GetTextColour() == *wxWHITE);
  control->SetInsertionPointEnd();
  control->WriteText("!");
  const auto edited = editor.GetRichText();
  assert(edited != original && edited.Contains("Perastage legacy rich text"));
  assert(editor.GetPlainText() == wxString::FromUTF8("P A B O\nÉ Z X!"));
  control->Undo();
  assert(editor.GetRichText() == original);
  control->Redo();
  assert(editor.GetRichText() == edited);
  editor.Hide();

  LayoutTextDialog plain(nullptr, {}, "Plain text\nSecond line", true, false);
  plain.Show();
  wxYield();
  assert(plain.GetRichText().empty());
  assert(plain.GetPlainText() == "Plain text\nSecond line");
  plain.Hide();
}

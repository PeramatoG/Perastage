#include "pdf_draw_commands.h"
#include "typography/composition.h"
#include "typography/font_resources.h"
#include "typography/text_outline.h"
#include "typography/glyph_outline.h"
#include <cassert>
#include <cmath>
#include "pdf_objects.h"
#include "pdf_writer.h"
#include "support/pdf_test_fixture_builder.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <sstream>

namespace {

class CommaDecimalPunctuation : public std::numpunct<char> {
protected:
  // Supplies a comma decimal point without requiring an installed OS locale.
  char do_decimal_point() const override { return ','; }
};

class TemporaryDirectory {
public:
  // Creates a unique test-owned temporary directory.
  TemporaryDirectory() {
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("perastage_pdf_writer_" + std::to_string(unique));
    std::error_code error;
    std::filesystem::remove_all(path_, error);
    error.clear();
    std::filesystem::create_directories(path_, error);
    if (error)
      creationError_ = error.message();
  }

  // Removes all test output after callers have closed their streams.
  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
    if (error)
      std::cerr << "Unable to clean PDF writer temporary directory: "
                << error.message() << '\n';
  }

  // Returns the temporary directory path.
  const std::filesystem::path &Path() const { return path_; }

  // Returns a diagnostic when temporary directory creation failed.
  const std::string &CreationError() const { return creationError_; }

private:
  std::filesystem::path path_;
  std::string creationError_;
};

// Verifies locale-independent float and draw-command serialization.
bool CheckLocaleIndependentNumbers() {
  const std::locale original = std::locale();
  std::locale::global(std::locale(original, new CommaDecimalPunctuation));
  struct LocaleRestore {
    std::locale original;
    ~LocaleRestore() { std::locale::global(original); }
  } restore{original};

  layout_pdf_internal::FloatFormatter formatter(3);
  if (formatter.Format(1.5) != "1.500" ||
      formatter.Format(-0.25) != "-0.250") {
    std::cerr << "FloatFormatter used locale-dependent decimal punctuation.\n";
    return false;
  }

  layout_pdf_internal::Mapping mapping;
  mapping.scale = 1.0;
  mapping.flipY = false;
  layout_pdf_internal::Transform transform;
  layout_pdf_internal::RenderOptions options;
  layout_pdf_internal::GraphicsStateCache cache;
  LineCommand line;
  line.x0 = 0.0f;
  line.y0 = 0.0f;
  line.x1 = 10.0f;
  line.y1 = 10.0f;
  line.stroke.width = 1.0f;
  line.stroke.color = {0.0f, 0.0f, 0.0f};

  std::ostringstream content;
  layout_pdf_internal::EmitCommandStroke(content, cache, formatter, mapping,
                                         transform, CanvasCommand{line}, options);
  const std::string expected =
      "1 j\n1 J\n0.000 0.000 0.000 RG\n1.000 w\n"
      "0.000 0.000 m\n10.000 10.000 l\nS\n";
  if (content.str() != expected || content.str().find(',') != std::string::npos) {
    std::cerr << "Draw command output changed or contains a comma decimal separator.\n"
              << content.str();
    return false;
  }
  return true;
}

// Verifies PDF structure, root selection, stream lifecycle, and cleanup.
bool CheckWriterSerialization() {
  TemporaryDirectory temporary;
  if (!temporary.CreationError().empty()) {
    std::cerr << "Unable to create PDF writer temporary directory: "
              << temporary.CreationError() << '\n';
    return false;
  }
  const auto outputPath = temporary.Path() / "document.pdf";
  std::error_code errorCode;
  std::filesystem::remove(outputPath, errorCode);
  if (errorCode) {
    std::cerr << "Unable to remove stale PDF output: " << errorCode.message() << '\n';
    return false;
  }

  const std::vector<PdfObject> objects = {
      {"<< /Type /Catalog /Pages 2 0 R >>"},
      {"<< /Type /Pages /Kids [3 0 R] /Count 1 >>"},
      {"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 100] /Contents 4 0 R >>"},
      {"<< /Length 0 >>\nstream\nendstream"}};
  std::string error;
  if (!WritePdfDocument(outputPath, objects, 1, error)) {
    std::cerr << "PDF writer stage failed: " << error << '\n';
    return false;
  }
  std::string data;
  {
    std::ifstream input(outputPath, std::ios::binary);
    if (!input.is_open()) {
      std::cerr << "Unable to open serialized PDF for validation.\n";
      return false;
    }
    data.assign(std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>());
    if (input.bad()) {
      std::cerr << "Unable to read serialized PDF for validation.\n";
      return false;
    }
  }

  if (!pdf_test_support::ValidatePdfStructure(data, error) ||
      data.find("trailer\n<< /Size 5 /Root 1 0 R >>") == std::string::npos) {
    std::cerr << "Serialized PDF structure failed validation: " << error << '\n';
    return false;
  }
  error.clear();
  if (WritePdfDocument(outputPath, objects, 0, error) || error.empty()) {
    std::cerr << "Invalid catalog object index was not diagnosed.\n";
    return false;
  }

  std::filesystem::remove(outputPath, errorCode);
  if (errorCode || std::filesystem::exists(outputPath)) {
    std::cerr << "Unable to remove closed PDF output: " << errorCode.message() << '\n';
    return false;
  }
  return true;
}

} // namespace

// Runs PDF writer portability and serialization controls.
int main() {
  const typography::FontPair fonts(PERASTAGE_TEST_RESOURCE_DIR);
  typography::TextStyle style; style.size = 12; style.unit = typography::TextUnit::PaperPoints;
  style.maxWidth = 45;
  const auto a = typography::Compose("Euro € — Süd\nÉclairage", style, fonts.regular);
  const auto b = typography::Compose("Euro € — Süd\nÉclairage", style, fonts.regular);
  assert(a.unsupportedBytes.empty() && a.lines.size() == 4 && a.lines.size() == b.lines.size());
  const std::array<std::string,4> expectedLines{"Euro € ", "— Süd", "Éclairag", "e"};
  const std::array<double,4> expectedWidths{39.408,36.504,44.424,6.768};
  for (size_t i=0; i<4; ++i) {
    std::string line;
    for (size_t j=a.lines[i].first; j<a.lines[i].first+a.lines[i].count; ++j) line += a.glyphs[j].text;
    assert(line == expectedLines[i]);
    assert(std::abs(a.lines[i].width - expectedWidths[i]) < 0.000001);
    assert(std::abs(a.lines[i].baseline + i*16.344) < 0.000001);
  }
  assert(fonts.regular.advanceWidths[0x80] == 572); // Euro, not U+0080.
  assert(fonts.regular.advanceWidths[0x97] == 1000); // Em dash.
  bool missingFont = false;
  try { typography::FontPair missing("/perastage/nonexistent/resources"); }
  catch (const std::runtime_error &) { missingFont = true; }
  assert(missingFont);
  for (size_t i=0; i<a.lines.size(); ++i) {
    assert(a.lines[i].first == b.lines[i].first && a.lines[i].count == b.lines[i].count);
    assert(a.lines[i].baseline == b.lines[i].baseline && a.lines[i].width <= 45);
  }
  assert(!typography::Compose("灯", style, fonts.regular).unsupportedBytes.empty());
  assert(!typography::Compose(std::string("\xc0\x80",2), style, fonts.regular).unsupportedBytes.empty());
  assert(typography::Compose("a\n\n", style, fonts.regular).lines.size() == 3);
  layout_pdf_internal::PdfFontDefinition regular;
  regular.metrics = fonts.regular; regular.embedded = true; regular.key = "F3";
  layout_pdf_internal::PdfFontCatalog catalog; catalog.labelRegular = &regular;
  assert(catalog.Resolve("perastage-sans") == &regular);
  const layout_pdf_internal::FloatFormatter fmt(6);
  for (const auto &glyph : a.glyphs) {
    TextCommand text; text.text = glyph.text; text.style.fontFamily = "perastage-sans";
    text.style.fontSize = 12; text.style.ascent = a.ascent; text.style.descent = a.descent;
    std::ostringstream out;
    layout_pdf_internal::AppendText(out, fmt, {glyph.x, glyph.baseline}, text, text.style, 1, &catalog);
    // Shared positions are serialized directly: error below 0.000001 PDF points.
    assert(out.str().find(fmt.Format(glyph.x) + " " + fmt.Format(glyph.baseline)) != std::string::npos);
  }

  // Continuous contours cover the bounded repertoire, including composite accents.
  for (const auto *font : {&fonts.regular, &fonts.bold}) {
    for (unsigned code = 32; code < 256; ++code) {
      const auto outline = typography::ReadGlyphOutline(*font, code);
      for (const auto &p : outline->path)
        assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.cx) && std::isfinite(p.cy));
    }
  }
  const auto accent = typography::ReadGlyphOutline(fonts.regular, 0xc9);
  double accentTop = 0;
  for (const auto &p : accent->path) accentTop = std::max(accentTop, p.y);
  assert(std::abs(accentTop - .944) < 1e-9);
  const auto round = typography::ReadGlyphOutline(fonts.regular, 'O');
  assert(std::count_if(round->path.begin(), round->path.end(), [](const auto &p) {
    return p.kind == typography::GlyphPathElement::Kind::Close;
  }) == 2);
  assert(std::count_if(round->path.begin(), round->path.end(), [](const auto &p) {
    return p.kind == typography::GlyphPathElement::Kind::Close && p.hole;
  }) == 1);
  assert(std::any_of(round->path.begin(), round->path.end(), [](const auto &p) {
    return p.kind == typography::GlyphPathElement::Kind::Quadratic;
  }));
  auto truncated = fonts.regular; truncated.data.resize(4);
  bool invalidContour = false;
  try { typography::ReadGlyphOutline(truncated, 'A'); }
  catch (const std::runtime_error &) { invalidContour = true; }
  assert(invalidContour);

  TextCommand outlined;
  outlined.text = "A"; outlined.style.fontFamily = "perastage-sans";
  outlined.style.fontSize = 12; outlined.style.ascent = a.ascent;
  outlined.style.descent = a.descent; outlined.style.outlineWidth = .013f;
  outlined.style.outlineColor = {.9f,.8f,.7f,1}; outlined.style.color = {.2f,.3f,.4f,1};
  for (double zoom : {.25,.5,1.,2.,4.}) {
    for (double dpi : {1.,1.5,2.,3.}) {
      const auto radius = typography::kFixtureLabelOutlineReferenceMetres;
      const double screenStroke = typography::ScreenOutlineStrokeWidth(radius,dpi);
      assert(std::abs(screenStroke*dpi-2) < 1e-6);
      assert(std::abs(typography::ScreenOutlineStrokeWidth(2*radius,dpi)*dpi-4) < 1e-6);
      assert(typography::ScreenOutlineStrokeWidth(0,dpi)==0);
      // Printing still uses the authored physical radius at its own mapping scale.
      std::ostringstream physicalPdf;
      layout_pdf_internal::AppendText(physicalPdf,fmt,{1,2},outlined,outlined.style,20*zoom,&catalog);
      assert(physicalPdf.str().find(fmt.Format(2*outlined.style.outlineWidth*20*zoom)+" w")!=std::string::npos);
    }
  }
  assert(typography::ScreenOutlineStrokeWidth(.004,0)==0);
  std::ostringstream outlinedPdf;
  layout_pdf_internal::AppendText(outlinedPdf,fmt,{1,2},outlined,outlined.style,20,&catalog);
  const auto output = outlinedPdf.str();
  assert(output.find("1 Tr") != std::string::npos);
  assert(output.find(fmt.Format(2*outlined.style.outlineWidth*20)+" w") != std::string::npos);
  assert(output.find("1.000000 2.000000 Td") != std::string::npos);
  assert(output.find("0.900000 0.800000 0.700000 RG") != std::string::npos);
  const auto firstPaint = output.find("(A) Tj");
  const auto secondPaint = output.find("(A) Tj", firstPaint + 1);
  assert(firstPaint != std::string::npos && secondPaint != std::string::npos);
  assert(output.find("(A) Tj", secondPaint + 1) == std::string::npos);
  assert(output.find("0.200000 0.300000 0.400000 rg") != std::string::npos);
  auto adjacent = outlined;
  adjacent.x = .07f; adjacent.text = "B";
  std::vector<CanvasCommand> run{outlined, adjacent, SaveCommand{}};
  std::ostringstream layeredPdf;
  assert(layout_pdf_internal::AppendComposedTextRun(layeredPdf,fmt,{}, {},run,
      {"label:fixture","label:fixture","label:fixture"},0,&catalog) == 2);
  // A neighbouring glyph's halo must never paint over a completed text fill.
  const auto layered = layeredPdf.str();
  assert(layered.rfind("0.900000 0.800000 0.700000 RG") <
         layered.find("0.200000 0.300000 0.400000 rg"));
  outlined.style.outlineWidth = 0;
  std::ostringstream plainPdf;
  layout_pdf_internal::AppendText(plainPdf,fmt,{1,2},outlined,outlined.style,20,&catalog);
  assert(plainPdf.str().find("1 Tr") == std::string::npos);
  return CheckWriterSerialization() && CheckLocaleIndependentNumbers() ? 0 : 1;
}

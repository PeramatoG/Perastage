#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

#include "symbols/FixtureSymbolDiagnostics.h"
#include "symbols/Symbol2DImageBuilder.h"
#include "symbols/SymbolGeometrySimplifier.h"
#include "tools/symbol_physical_calibration.h"
#include "windows/symbol_preview_exporter.h"

namespace {

// Returns the stable lowercase label for a symbol view.
std::string ViewName(symbols::SymbolView view) {
  switch (view) {
  case symbols::SymbolView::Top:
    return "top";
  case symbols::SymbolView::Bottom:
    return "bottom";
  case symbols::SymbolView::Front:
    return "front";
  case symbols::SymbolView::Left:
    return "side";
  }
  return "unknown";
}

// Constructs a small asymmetric RGBA fixture without image decoder
// dependencies.
symbols::RenderedSymbolImage MakeRender(symbols::SymbolView view, int variant) {
  symbols::RenderedSymbolImage render{
      view, 14, 12, std::vector<unsigned char>(14 * 12 * 4, 255)};
  auto pixel = [&](int x, int y, unsigned char r, unsigned char g,
                   unsigned char b) {
    const size_t at = static_cast<size_t>((y * render.width + x) * 4);
    render.rgba[at] = r;
    render.rgba[at + 1] = g;
    render.rgba[at + 2] = b;
    render.rgba[at + 3] = 255;
  };
  for (int y = 2; y <= 8 + (variant % 2); ++y)
    for (int x = 2; x <= 7 + variant; ++x)
      if (!(x >= 4 && x <= 5 && y >= 4 && y <= 5))
        pixel(x, y, 63, 169, 245);
  for (int x = 7; x <= 10 + (variant % 2); ++x)
    pixel(x, 8 - variant, 63, 169, 245);
  for (int y = 2; y <= 6 + variant; ++y)
    pixel(9 + (variant % 2), y, 0, 0, 0);
  return render;
}

// Formats all review-relevant symbol structure with fixed four-decimal
// precision.
std::string Snapshot(const symbols::Symbol2D &symbol) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::fixed << std::setprecision(4);
  out << "view " << ViewName(symbol.view) << '\n';
  out << "bounds " << symbol.bounds.min.x << ' ' << symbol.bounds.min.y << ' '
      << symbol.bounds.max.x << ' ' << symbol.bounds.max.y << '\n';
  out << "dimensions " << symbol.bounds.max.x - symbol.bounds.min.x << ' '
      << symbol.bounds.max.y - symbol.bounds.min.y << '\n';
  out << "offset " << symbol.bounds.min.x << ' ' << symbol.bounds.min.y << '\n';
  out << "stroke_width " << symbol.strokeWidthPx << '\n';
  out << "fills " << symbol.fill.size() << '\n';
  auto points = [&](const symbols::Polyline2D &line) {
    for (const auto &point : line)
      out << ' ' << point.x << ',' << point.y;
    out << '\n';
  };
  for (size_t i = 0; i < symbol.fill.size(); ++i) {
    out << "fill " << i << " outer " << symbol.fill[i].outer.size();
    points(symbol.fill[i].outer);
    out << "fill " << i << " holes " << symbol.fill[i].holes.size() << '\n';
    for (size_t h = 0; h < symbol.fill[i].holes.size(); ++h) {
      out << "hole " << i << '.' << h << ' ' << symbol.fill[i].holes[h].size();
      points(symbol.fill[i].holes[h]);
    }
  }
  out << "strokes " << symbol.strokes.size() << '\n';
  for (size_t i = 0; i < symbol.strokes.size(); ++i) {
    out << "stroke " << i << ' ' << symbol.strokes[i].size();
    points(symbol.strokes[i]);
  }
  std::string svg, error;
  if (!symbol_preview::ExportSymbolToSvgString(symbol, svg, error))
    return {};
  out << "svg_begin\n" << svg << "svg_end\n";
  return out.str();
}

// Reads a checked-in structural baseline as raw UTF-8 text.
std::string ReadFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

// Verifies production conversion, simplification, calibration, and SVG
// snapshots.
bool TestStructuralBaselines() {
  const std::array views = {
      symbols::SymbolView::Top, symbols::SymbolView::Bottom,
      symbols::SymbolView::Front, symbols::SymbolView::Left};
  std::vector<symbols::RenderedSymbolImage> renders;
  for (size_t i = 0; i < views.size(); ++i)
    renders.push_back(MakeRender(views[i], static_cast<int>(i)));
  auto generated =
      symbols::Symbol2DImageBuilder::BuildFromRenderedImagesSequential(renders);
  for (auto &symbol : generated)
    symbols::SimplifySymbolGeometry(symbol, 1.0f);
  tools::FixtureGeometryBounds bounds;
  bounds.min = {-40.0f, -25.0f, -60.0f};
  bounds.max = {80.0f, 45.0f, 90.0f};
  bounds.valid = true;
  std::string error;
  if (!tools::CalibrateFixtureSymbolsToPhysicalUnits(bounds, generated, error))
    return false;
  bool ok = generated.size() == views.size();
  for (const auto &symbol : generated) {
    const std::string actual = Snapshot(symbol);
    const auto path = std::filesystem::path(FIXTURE_SYMBOL_BASELINE_DIR) /
                      (ViewName(symbol.view) + ".txt");
    const std::string expected = ReadFile(path);
    if (actual != expected) {
      std::cerr << "SNAPSHOT " << path.filename().string() << "\n" << actual;
      ok = false;
    }
  }
  if (!generated.empty()) {
    std::string altered = Snapshot(generated.front());
    altered[altered.find("view top")] = 'V';
    ok = ok && altered !=
                   ReadFile(std::filesystem::path(FIXTURE_SYMBOL_BASELINE_DIR) /
                            "top.txt");
  }
  return ok;
}

// Verifies the runtime-owned view sequence, bottom override, and side mirror.
bool TestCapturePlan() {
  const auto &plan = symbols::FixtureSymbolCapturePlan();
  return plan[0].viewerView == symbols::SymbolCaptureViewerView::Front &&
         plan[0].symbolView == symbols::SymbolView::Front &&
         !plan[0].mirrorHorizontally &&
         plan[1].symbolView == symbols::SymbolView::Top &&
         plan[2].viewerView == symbols::SymbolCaptureViewerView::Side &&
         plan[2].symbolView == symbols::SymbolView::Left &&
         plan[2].mirrorHorizontally &&
         plan[3].viewerView == symbols::SymbolCaptureViewerView::Top &&
         plan[3].symbolView == symbols::SymbolView::Bottom &&
         plan[3].forceBottomViewForTopFixtures && !plan[3].mirrorHorizontally;
}

// Verifies timing accumulation, absent phases, outcomes, order, and locale
// stability.
bool TestTimings() {
  symbols::FixtureSymbolTimings timings(true);
  timings.Add(symbols::FixtureSymbolPhase::Resolve,
              std::chrono::microseconds(3));
  timings.Add(symbols::FixtureSymbolPhase::Resolve,
              std::chrono::microseconds(4));
  timings.Add(symbols::FixtureSymbolPhase::Fingerprint,
              std::chrono::microseconds(5));
  const std::string skipped =
      timings.Format("fixture", "key", symbols::FixtureSymbolOutcome::Skipped);
  bool ok =
      timings.Elapsed(symbols::FixtureSymbolPhase::Resolve).count() == 7 &&
      !timings.Has(symbols::FixtureSymbolPhase::Capture) &&
      skipped.find("resolve_us=7 fingerprint_us=5 inspect_us=- bounds_us=- "
                   "capture_us=-") != std::string::npos;
  symbols::FixtureSymbolTimings generated(true);
  for (size_t i = 0;
       i < static_cast<size_t>(symbols::FixtureSymbolPhase::Count); ++i)
    generated.Add(static_cast<symbols::FixtureSymbolPhase>(i),
                  std::chrono::microseconds(i + 1));
  ok =
      ok && generated.Format("x", "y", symbols::FixtureSymbolOutcome::Generated)
                    .find("outcome=generated") != std::string::npos;
  symbols::FixtureSymbolTimings failed(true);
  failed.Add(symbols::FixtureSymbolPhase::Inspect,
             std::chrono::microseconds(9));
  ok = ok && failed.Has(symbols::FixtureSymbolPhase::Inspect) &&
       !failed.Has(symbols::FixtureSymbolPhase::Capture) &&
       failed.Format("x", "y", symbols::FixtureSymbolOutcome::Failed)
               .find("outcome=failed") != std::string::npos;
  try {
    std::locale::global(std::locale(""));
  } catch (...) {
  }
  return ok && timings.Format("fixture", "key",
                              symbols::FixtureSymbolOutcome::Skipped)
                       .find("resolve_us=7") != std::string::npos;
}

} // namespace

// Runs the fixture-symbol structural and timing regression contracts.
int main() {
  return TestStructuralBaselines() && TestCapturePlan() && TestTimings() ? 0
                                                                         : 1;
}

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

// Constructs concave, disconnected, holed, and diagonally touching fill contours.
symbols::RenderedSymbolImage MakeContourRender() {
  symbols::RenderedSymbolImage render{
      symbols::SymbolView::Top, 42, 32,
      std::vector<unsigned char>(42 * 32 * 4, 255)};
  auto setPixel = [&](int x, int y, unsigned char r, unsigned char g,
                      unsigned char b) {
    const size_t at = static_cast<size_t>((y * render.width + x) * 4);
    render.rgba[at] = r;
    render.rgba[at + 1] = g;
    render.rgba[at + 2] = b;
  };
  auto fill = [&](int x0, int y0, int x1, int y1, bool foreground) {
    for (int y = y0; y <= y1; ++y)
      for (int x = x0; x <= x1; ++x)
        setPixel(x, y, foreground ? 63 : 255, foreground ? 169 : 255,
                 foreground ? 245 : 255);
  };
  fill(2, 2, 25, 27, true);
  fill(19, 11, 25, 18, false);
  fill(6, 7, 10, 11, false);
  fill(13, 18, 17, 22, false);
  fill(31, 3, 36, 8, true);
  fill(30, 9, 30, 9, true);
  return render;
}

// Computes the signed area of one implicit polygon ring.
double RingArea(const symbols::Polyline2D &ring) {
  double twiceArea = 0.0;
  for (size_t index = 0; index < ring.size(); ++index) {
    const auto &a = ring[index];
    const auto &b = ring[(index + 1) % ring.size()];
    twiceArea += static_cast<double>(a.x) * b.y -
                 static_cast<double>(b.x) * a.y;
  }
  return twiceArea * 0.5;
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
  if (!input.is_open()) {
    std::cerr << "Could not open fixture-symbol baseline: " << path << '\n';
    return {};
  }
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

// Verifies deterministic contour topology, collection order, and exact filled area.
bool TestDeterministicContours() {
  symbols::ImageBuildParams params;
  params.fillGapClosurePixels = 0;
  params.lineGapClosurePixels = 0;
  const auto render = MakeContourRender();
  const auto expected =
      symbols::Symbol2DImageBuilder::BuildFromRenderedImage(render, params);
  if (!expected || expected->fill.size() != 3 ||
      expected->fill.front().holes.size() != 2) {
    std::cerr << "Contour topology mismatch: expected three outer rings and two "
                 "holes in the largest ring\n";
    return false;
  }

  double filledArea = 0.0;
  for (const auto &polygon : expected->fill) {
    if (polygon.outer.size() < 3 ||
        (polygon.outer.front().x == polygon.outer.back().x &&
         polygon.outer.front().y == polygon.outer.back().y)) {
      std::cerr << "Outer ring closure or unique-vertex invariant failed\n";
      return false;
    }
    filledArea += std::abs(RingArea(polygon.outer));
    for (const auto &hole : polygon.holes) {
      if (hole.size() < 3) {
        std::cerr << "Hole unique-vertex invariant failed\n";
        return false;
      }
      filledArea -= std::abs(RingArea(hole));
    }
  }
  size_t foregroundPixels = 0;
  for (size_t at = 0; at < render.rgba.size(); at += 4)
    if (render.rgba[at] == 63 && render.rgba[at + 1] == 169)
      ++foregroundPixels;
  if (std::fabs(filledArea - static_cast<double>(foregroundPixels)) > 1e-6) {
    std::cerr << "Contour area mismatch: expected " << foregroundPixels
              << " actual " << filledArea << '\n';
    return false;
  }

  const std::string expectedText = Snapshot(*expected);
  for (int repetition = 0; repetition < 20; ++repetition) {
    const auto actual =
        symbols::Symbol2DImageBuilder::BuildFromRenderedImage(render, params);
    if (!actual || Snapshot(*actual) != expectedText) {
      std::cerr << "Canonical contour sequence mismatch at repetition "
                << repetition << '\n';
      return false;
    }
  }

  std::vector<symbols::RenderedSymbolImage> renders(4, render);
  renders[0].view = symbols::SymbolView::Front;
  renders[1].view = symbols::SymbolView::Top;
  renders[2].view = symbols::SymbolView::Left;
  renders[3].view = symbols::SymbolView::Bottom;
  const auto sequential =
      symbols::Symbol2DImageBuilder::BuildFromRenderedImagesSequential(renders,
                                                                       params);
  const auto parallel =
      symbols::Symbol2DImageBuilder::BuildFromRenderedImages(renders, params);
  if (sequential.size() != parallel.size()) {
    std::cerr << "Parallel contour result count mismatch\n";
    return false;
  }
  for (size_t index = 0; index < sequential.size(); ++index) {
    if (Snapshot(sequential[index]) != Snapshot(parallel[index])) {
      std::cerr << "Parallel canonical contour mismatch for view "
                << ViewName(sequential[index].view) << "\nexpected:\n"
                << Snapshot(sequential[index]) << "actual:\n"
                << Snapshot(parallel[index]);
      return false;
    }
  }
  return true;
}

// Simulates an early archive-rewrite failure handled by the scoped timer.
void RecordFailedArchiveRewrite(symbols::FixtureSymbolTimings &timings) {
  symbols::ScopedFixtureSymbolPhase phase(
      &timings, symbols::FixtureSymbolPhase::ArchiveRewrite);
}

// Verifies timing totals, disabled behavior, outcomes, order, and locale stability.
bool TestTimings() {
  symbols::FixtureSymbolTimings disabled;
  {
    symbols::ScopedFixtureSymbolPhase phase(
        &disabled, symbols::FixtureSymbolPhase::Capture);
  }
  bool ok = !disabled.Enabled() &&
            disabled.Total() == symbols::FixtureSymbolTimings::Duration::zero() &&
            !disabled.Has(symbols::FixtureSymbolPhase::Capture);

  const auto start = symbols::FixtureSymbolTimings::Clock::time_point{};
  const auto current = start + std::chrono::microseconds(100);
  symbols::FixtureSymbolTimings controlled(start, current);
  controlled.Add(symbols::FixtureSymbolPhase::Resolve,
                 std::chrono::microseconds(7));
  ok = ok && controlled.Total() == std::chrono::microseconds(100) &&
       controlled.Elapsed(symbols::FixtureSymbolPhase::Resolve) ==
           std::chrono::microseconds(7);

  symbols::FixtureSymbolTimings timings(true);
  timings.Add(symbols::FixtureSymbolPhase::Resolve,
              std::chrono::microseconds(3));
  timings.Add(symbols::FixtureSymbolPhase::Resolve,
              std::chrono::microseconds(4));
  timings.Add(symbols::FixtureSymbolPhase::Fingerprint,
              std::chrono::microseconds(5));
  timings.Add(symbols::FixtureSymbolPhase::Validation,
              std::chrono::microseconds(6));
  const std::string skipped =
      timings.Format("fixture", "key", symbols::FixtureSymbolOutcome::Skipped);
  ok = ok && timings.Elapsed(symbols::FixtureSymbolPhase::Resolve).count() == 7 &&
      !timings.Has(symbols::FixtureSymbolPhase::Capture) &&
      skipped.find("resolve_us=7 fingerprint_us=5 inspect_us=- bounds_us=- "
                   "capture_us=- vectorization_us=- calibration_us=- "
                   "archive_rewrite_us=- validation_us=6 refresh_us=-") !=
          std::string::npos;
  symbols::FixtureSymbolTimings generated(start, current);
  for (size_t i = 0;
       i < static_cast<size_t>(symbols::FixtureSymbolPhase::Count); ++i)
    generated.Add(static_cast<symbols::FixtureSymbolPhase>(i),
                  std::chrono::microseconds(i + 1));
  const std::string generatedText = generated.Format(
      "x", "y", symbols::FixtureSymbolOutcome::Generated);
  const std::string orderedPhases =
      "resolve_us=1 fingerprint_us=2 inspect_us=3 bounds_us=4 capture_us=5 "
      "vectorization_us=6 calibration_us=7 archive_rewrite_us=8 "
      "validation_us=9 refresh_us=10";
  ok = ok && generatedText.find("outcome=generated total_us=100 " +
                                orderedPhases) != std::string::npos;

  symbols::FixtureSymbolTimings failed(start, current);
  RecordFailedArchiveRewrite(failed);
  ok = ok && failed.Has(symbols::FixtureSymbolPhase::ArchiveRewrite) &&
       !failed.Has(symbols::FixtureSymbolPhase::Validation) &&
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
  return TestStructuralBaselines() && TestCapturePlan() &&
                 TestDeterministicContours() && TestTimings()
             ? 0
             : 1;
}

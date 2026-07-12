#include "gdtf/gdtf_color_cie.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace gdtf {
namespace {
// Adds a color diagnostic with source context.
void AddColorDiagnostic(std::vector<GdtfModeDiagnostic> &diagnostics,
                        GdtfDiagnosticSeverity severity, std::string message,
                        std::string rawValue = {}) {
  diagnostics.push_back({severity, std::move(message), "ColorCIE", std::move(rawValue), {}});
}

// Parses a comma or whitespace separated double list.
bool ParseTriple(const std::string &raw, double &x, double &y, double &Y) {
  std::string normalized = raw;
  std::replace(normalized.begin(), normalized.end(), ',', ' ');
  std::stringstream input(normalized);
  return (input >> x >> y >> Y) && input.eof();
}

// Returns display-relative luminance from GDTF CIE xyY luminance.
double NormalizeDisplayLuminance(double value) {
  return value > 1.0 ? value / 100.0 : value;
}

// Applies the display sRGB transfer function.
double EncodeSrgb(double value) {
  if (value <= 0.0031308)
    return 12.92 * value;
  return 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
}
} // namespace

// Parses a GDTF CIE xyY color while preserving the exact source text.
GdtfColorCie ParseGdtfColorCie(const std::string &raw, GdtfValueOrigin origin) {
  GdtfColorCie color;
  color.raw = raw;
  color.origin = origin;
  if (raw.empty()) {
    AddColorDiagnostic(color.diagnostics, GdtfDiagnosticSeverity::Info,
                       "No CIE xyY color value is specified.");
    return color;
  }
  if (!ParseTriple(raw, color.x, color.y, color.Y)) {
    AddColorDiagnostic(color.diagnostics, GdtfDiagnosticSeverity::Warning,
                       "Malformed CIE xyY color value.", raw);
    return color;
  }
  if (!std::isfinite(color.x) || !std::isfinite(color.y) || !std::isfinite(color.Y) ||
      color.y <= 0.0) {
    AddColorDiagnostic(color.diagnostics, GdtfDiagnosticSeverity::Warning,
                       "CIE xyY color contains non-finite values or y <= 0.", raw);
    return color;
  }
  color.valid = true;
  return color;
}

// Converts CIE xyY to a clipped display sRGB preview without changing source values.
GdtfSrgbPreview ConvertCieXyyToSrgb(const GdtfColorCie &color) {
  GdtfSrgbPreview preview;
  if (!color.valid) {
    AddColorDiagnostic(preview.diagnostics, GdtfDiagnosticSeverity::Info,
                       "No valid CIE xyY value is available for preview.", color.raw);
    return preview;
  }
  const double luminance = NormalizeDisplayLuminance(color.Y);
  const double X = color.x * luminance / color.y;
  const double Z = (1.0 - color.x - color.y) * luminance / color.y;
  double r = 3.2406 * X - 1.5372 * luminance - 0.4986 * Z;
  double g = -0.9689 * X + 1.8758 * luminance + 0.0415 * Z;
  double b = 0.0557 * X - 0.2040 * luminance + 1.0570 * Z;
  auto clip = [&](double value) {
    if (value < 0.0 || value > 1.0)
      preview.clipped = true;
    return std::clamp(value, 0.0, 1.0);
  };
  preview.red = EncodeSrgb(clip(r));
  preview.green = EncodeSrgb(clip(g));
  preview.blue = EncodeSrgb(clip(b));
  preview.valid = true;
  if (preview.clipped)
    AddColorDiagnostic(preview.diagnostics, GdtfDiagnosticSeverity::Info,
                       "Display sRGB preview was clipped to the monitor gamut.", color.raw);
  return preview;
}

} // namespace gdtf

#include "gdtf/svg_preview_geometry.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>

#include <tinyxml2.h>

namespace gui::gdtf {
namespace {

// Parses one finite SVG number after a valid whitespace or comma separator.
bool ParseViewportNumber(const char *&cursor, double &value, bool first) {
  bool whitespace = false;
  while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor))) {
    whitespace = true;
    ++cursor;
  }
  if (!first && *cursor == ',') {
    ++cursor;
    while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)))
      ++cursor;
  } else if (!first && !whitespace) {
    return false;
  } else if (first && *cursor == ',') {
    return false;
  }
  char *end = nullptr;
  value = std::strtod(cursor, &end);
  if (end == cursor || !std::isfinite(value))
    return false;
  cursor = end;
  return true;
}

// Parses a positive absolute SVG length into CSS pixels.
std::optional<double> ParseSvgLength(const char *text) {
  if (!text)
    return std::nullopt;
  while (std::isspace(static_cast<unsigned char>(*text)))
    ++text;
  char *end = nullptr;
  const double value = std::strtod(text, &end);
  if (end == text || !std::isfinite(value) || value <= 0.0)
    return std::nullopt;
  while (std::isspace(static_cast<unsigned char>(*end)))
    ++end;
  std::string unit(end);
  while (!unit.empty() &&
         std::isspace(static_cast<unsigned char>(unit.back())))
    unit.pop_back();
  double factor = 0.0;
  if (unit.empty() || unit == "px")
    factor = 1.0;
  else if (unit == "in")
    factor = 96.0;
  else if (unit == "cm")
    factor = 96.0 / 2.54;
  else if (unit == "mm")
    factor = 96.0 / 25.4;
  else if (unit == "q")
    factor = 96.0 / 101.6;
  else if (unit == "pt")
    factor = 96.0 / 72.0;
  else if (unit == "pc")
    factor = 16.0;
  const double pixels = value * factor;
  return factor > 0.0 && std::isfinite(pixels)
             ? std::optional<double>(pixels)
             : std::nullopt;
}

} // namespace

// Reports whether both fitted bitmap dimensions are usable.
bool SvgPreviewSize::IsValid() const { return width > 0 && height > 0; }

// Reads validated viewport dimensions from the SVG root element.
std::optional<SvgViewport> ParseSvgViewport(std::string_view svgText) {
  tinyxml2::XMLDocument document;
  if (document.Parse(svgText.data(), svgText.size()) != tinyxml2::XML_SUCCESS)
    return std::nullopt;
  const tinyxml2::XMLElement *root = document.RootElement();
  if (!root)
    return std::nullopt;
  const std::string_view name(root->Name());
  if (name != "svg" &&
      !(name.size() > 4 && name.substr(name.size() - 4) == ":svg"))
    return std::nullopt;

  if (const char *viewBox = root->Attribute("viewBox")) {
    const char *cursor = viewBox;
    std::array<double, 4> values{};
    bool valid = true;
    for (std::size_t index = 0; index < values.size(); ++index)
      valid = valid && ParseViewportNumber(cursor, values[index], index == 0);
    while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)))
      ++cursor;
    if (valid && *cursor == '\0' && values[2] > 0.0 && values[3] > 0.0)
      return SvgViewport{values[2], values[3]};
  }

  const auto width = ParseSvgLength(root->Attribute("width"));
  const auto height = ParseSvgLength(root->Attribute("height"));
  if (width && height)
    return SvgViewport{*width, *height};
  return std::nullopt;
}

// Calculates an aspect-preserving SVG raster size bounded by the target.
SvgPreviewSize FitSvgPreviewSize(double sourceWidth, double sourceHeight,
                                 int targetWidth, int targetHeight,
                                 int maximumDimension) {
  if (!std::isfinite(sourceWidth) || !std::isfinite(sourceHeight) ||
      sourceWidth <= 0.0 || sourceHeight <= 0.0 || targetWidth <= 0 ||
      targetHeight <= 0 || maximumDimension <= 0 ||
      targetWidth > maximumDimension || targetHeight > maximumDimension)
    return {};

  const double scale =
      std::min(static_cast<double>(targetWidth) / sourceWidth,
               static_cast<double>(targetHeight) / sourceHeight);
  if (!std::isfinite(scale) || scale <= 0.0)
    return {};

  return {std::clamp(static_cast<int>(std::lround(sourceWidth * scale)), 1,
                     targetWidth),
          std::clamp(static_cast<int>(std::lround(sourceHeight * scale)), 1,
                     targetHeight)};
}

} // namespace gui::gdtf
